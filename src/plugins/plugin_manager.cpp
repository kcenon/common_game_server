/// @file plugin_manager.cpp
/// @brief PluginManager implementation: lifecycle, dynamic/static loading,
///        dependency resolution.
///
/// @see SDS-MOD-021

#include "cgs/plugin/plugin_manager.hpp"

#include <algorithm>
#include <queue>
#include <unordered_set>

#include "cgs/foundation/error_code.hpp"
#include "cgs/plugin/plugin_export.hpp"

// Platform-specific dynamic library loading.
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;

namespace cgs::plugin {

// ── Construction / destruction ──────────────────────────────────────────

PluginManager::PluginManager() = default;

PluginManager::~PluginManager() {
    ShutdownAll();
    UnloadAll();
}

PluginManager::PluginManager(PluginManager&&) noexcept = default;
PluginManager& PluginManager::operator=(PluginManager&&) noexcept = default;

// ── Context ─────────────────────────────────────────────────────────────

void PluginManager::SetContext(PluginContext ctx) {
    context_ = ctx;
}

PluginContext& PluginManager::GetContext() noexcept {
    return context_;
}

// ── Dynamic loading ─────────────────────────────────────────────────────

GameResult<void> PluginManager::LoadPlugin(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginLoadFailed,
                      "Plugin file not found: " + path.string()));
    }

    // Open the shared library.
#if defined(_WIN32)
    void* handle = static_cast<void*>(LoadLibraryW(path.c_str()));
#else
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif

    if (handle == nullptr) {
#if defined(_WIN32)
        auto errorMsg = "LoadLibrary failed for: " + path.string();
#else
        auto errorMsg = std::string(dlerror());
#endif
        return GameResult<void>::err(
            GameError(ErrorCode::PluginLoadFailed, std::move(errorMsg)));
    }

    // Look up the factory function.
    using CreateFunc = IPlugin* (*)();
#if defined(_WIN32)
    auto createFn = reinterpret_cast<CreateFunc>(
        GetProcAddress(static_cast<HMODULE>(handle), "CgsCreatePlugin"));
#else
    auto createFn = reinterpret_cast<CreateFunc>(dlsym(handle, "CgsCreatePlugin"));
#endif

    if (createFn == nullptr) {
        closeLibrary(handle);
        return GameResult<void>::err(
            GameError(ErrorCode::PluginLoadFailed,
                      "Symbol 'CgsCreatePlugin' not found in: " + path.string()));
    }

    // Create the plugin instance.
    std::unique_ptr<IPlugin> plugin(createFn());
    if (!plugin) {
        closeLibrary(handle);
        return GameResult<void>::err(
            GameError(ErrorCode::PluginLoadFailed,
                      "CgsCreatePlugin returned null for: " + path.string()));
    }

    return loadPluginInstance(std::move(plugin), handle);
}

// ── Static loading ──────────────────────────────────────────────────────

GameResult<void> PluginManager::RegisterStaticPlugins() {
    for (const auto& entry : StaticPluginRegistry()) {
        auto plugin = entry.factory();
        if (!plugin) {
            return GameResult<void>::err(
                GameError(ErrorCode::PluginLoadFailed,
                          "Static plugin factory returned null: " + entry.name));
        }
        auto result = loadPluginInstance(std::move(plugin), nullptr);
        if (result.hasError()) {
            return result;
        }
    }
    return GameResult<void>::ok();
}

// ── Lifecycle: Init ─────────────────────────────────────────────────────

GameResult<void> PluginManager::InitPlugin(std::string_view name) {
    auto it = plugins_.find(std::string(name));
    if (it == plugins_.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginNotFound,
                      "Plugin not found: " + std::string(name)));
    }

    auto& entry = it->second;
    if (entry.state != PluginState::Loaded) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginInvalidState,
                      "Plugin '" + std::string(name) +
                          "' is not in Loaded state (current: " +
                          std::to_string(static_cast<int>(entry.state)) + ")"));
    }

    if (!entry.plugin->OnInit()) {
        entry.state = PluginState::Error;
        return GameResult<void>::err(
            GameError(ErrorCode::PluginInitFailed,
                      "OnInit() failed for plugin: " + std::string(name)));
    }

    entry.state = PluginState::Initialized;
    return GameResult<void>::ok();
}

GameResult<void> PluginManager::InitializeAll() {
    auto orderResult = resolveDependencies();
    if (orderResult.hasError()) {
        return GameResult<void>::err(orderResult.error());
    }

    loadOrder_ = std::move(orderResult).value();

    for (const auto& name : loadOrder_) {
        auto it = plugins_.find(name);
        if (it == plugins_.end() || it->second.state != PluginState::Loaded) {
            continue;
        }
        auto result = InitPlugin(name);
        if (result.hasError()) {
            return result;
        }
    }

    return GameResult<void>::ok();
}

// ── Lifecycle: Activate ─────────────────────────────────────────────────

GameResult<void> PluginManager::ActivatePlugin(std::string_view name) {
    auto it = plugins_.find(std::string(name));
    if (it == plugins_.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginNotFound,
                      "Plugin not found: " + std::string(name)));
    }

    auto& entry = it->second;
    if (entry.state != PluginState::Initialized) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginInvalidState,
                      "Plugin '" + std::string(name) +
                          "' is not in Initialized state"));
    }

    entry.state = PluginState::Active;
    return GameResult<void>::ok();
}

GameResult<void> PluginManager::ActivateAll() {
    const auto& order = loadOrder_.empty()
                            ? GetAllPluginNames()
                            : loadOrder_;

    for (const auto& name : order) {
        auto it = plugins_.find(name);
        if (it == plugins_.end() || it->second.state != PluginState::Initialized) {
            continue;
        }
        auto result = ActivatePlugin(name);
        if (result.hasError()) {
            return result;
        }
    }
    return GameResult<void>::ok();
}

// ── Lifecycle: Update ───────────────────────────────────────────────────

void PluginManager::UpdateAll(float deltaTime) {
    const auto& order = loadOrder_.empty()
                            ? GetAllPluginNames()
                            : loadOrder_;

    for (const auto& name : order) {
        auto it = plugins_.find(name);
        if (it != plugins_.end() && it->second.state == PluginState::Active) {
            it->second.plugin->OnUpdate(deltaTime);
        }
    }
}

// ── Lifecycle: Shutdown ─────────────────────────────────────────────────

GameResult<void> PluginManager::ShutdownPlugin(std::string_view name) {
    auto it = plugins_.find(std::string(name));
    if (it == plugins_.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginNotFound,
                      "Plugin not found: " + std::string(name)));
    }

    auto& entry = it->second;
    if (entry.state != PluginState::Active &&
        entry.state != PluginState::Initialized) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginInvalidState,
                      "Plugin '" + std::string(name) +
                          "' is not in Active or Initialized state"));
    }

    entry.state = PluginState::ShuttingDown;
    entry.plugin->OnShutdown();
    entry.state = PluginState::Loaded;
    return GameResult<void>::ok();
}

void PluginManager::ShutdownAll() {
    // Shutdown in reverse dependency order.
    auto order = loadOrder_.empty()
                     ? GetAllPluginNames()
                     : loadOrder_;
    std::reverse(order.begin(), order.end());

    for (const auto& name : order) {
        auto it = plugins_.find(name);
        if (it != plugins_.end() &&
            (it->second.state == PluginState::Active ||
             it->second.state == PluginState::Initialized)) {
            it->second.state = PluginState::ShuttingDown;
            it->second.plugin->OnShutdown();
            it->second.state = PluginState::Loaded;
        }
    }
}

// ── Lifecycle: Unload ───────────────────────────────────────────────────

GameResult<void> PluginManager::UnloadPlugin(std::string_view name) {
    auto it = plugins_.find(std::string(name));
    if (it == plugins_.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginNotFound,
                      "Plugin not found: " + std::string(name)));
    }

    auto& entry = it->second;
    if (entry.state != PluginState::Loaded &&
        entry.state != PluginState::Unloaded &&
        entry.state != PluginState::Error) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginInvalidState,
                      "Plugin '" + std::string(name) +
                          "' must be shut down before unloading"));
    }

    entry.plugin->OnUnload();
    entry.plugin.reset();

    if (entry.libraryHandle != nullptr) {
        closeLibrary(entry.libraryHandle);
        entry.libraryHandle = nullptr;
    }

    plugins_.erase(it);

    // Remove from load order.
    auto orderIt = std::find(loadOrder_.begin(), loadOrder_.end(), std::string(name));
    if (orderIt != loadOrder_.end()) {
        loadOrder_.erase(orderIt);
    }

    return GameResult<void>::ok();
}

void PluginManager::UnloadAll() {
    // Unload in reverse dependency order.
    auto order = loadOrder_.empty()
                     ? GetAllPluginNames()
                     : loadOrder_;
    std::reverse(order.begin(), order.end());

    for (const auto& name : order) {
        auto it = plugins_.find(name);
        if (it != plugins_.end()) {
            it->second.plugin->OnUnload();
            it->second.plugin.reset();
            if (it->second.libraryHandle != nullptr) {
                closeLibrary(it->second.libraryHandle);
            }
        }
    }

    plugins_.clear();
    loadOrder_.clear();
}

// ── Queries ─────────────────────────────────────────────────────────────

IPlugin* PluginManager::GetPlugin(std::string_view name) const {
    auto it = plugins_.find(std::string(name));
    if (it == plugins_.end()) {
        return nullptr;
    }
    return it->second.plugin.get();
}

GameResult<PluginState> PluginManager::GetPluginState(std::string_view name) const {
    auto it = plugins_.find(std::string(name));
    if (it == plugins_.end()) {
        return GameResult<PluginState>::err(
            GameError(ErrorCode::PluginNotFound,
                      "Plugin not found: " + std::string(name)));
    }
    return GameResult<PluginState>::ok(it->second.state);
}

std::vector<std::string> PluginManager::GetAllPluginNames() const {
    std::vector<std::string> names;
    names.reserve(plugins_.size());
    for (const auto& [name, entry] : plugins_) {
        names.push_back(name);
    }
    return names;
}

std::size_t PluginManager::PluginCount() const noexcept {
    return plugins_.size();
}

// ── Private helpers ─────────────────────────────────────────────────────

GameResult<void> PluginManager::loadPluginInstance(
    std::unique_ptr<IPlugin> plugin, void* libraryHandle) {
    const auto& info = plugin->GetInfo();

    // Check for duplicate.
    if (plugins_.count(info.name) > 0) {
        if (libraryHandle != nullptr) {
            closeLibrary(libraryHandle);
        }
        return GameResult<void>::err(
            GameError(ErrorCode::PluginAlreadyLoaded,
                      "Plugin already loaded: " + info.name));
    }

    // Verify API version compatibility (major must match).
    if (info.apiVersion != kPluginApiVersion) {
        if (libraryHandle != nullptr) {
            closeLibrary(libraryHandle);
        }
        return GameResult<void>::err(
            GameError(ErrorCode::PluginVersionMismatch,
                      "API version mismatch for '" + info.name +
                          "': expected " + std::to_string(kPluginApiVersion) +
                          ", got " + std::to_string(info.apiVersion)));
    }

    // Call OnLoad.
    if (!plugin->OnLoad(context_)) {
        if (libraryHandle != nullptr) {
            closeLibrary(libraryHandle);
        }
        return GameResult<void>::err(
            GameError(ErrorCode::PluginLoadFailed,
                      "OnLoad() failed for plugin: " + info.name));
    }

    PluginEntry entry;
    entry.plugin = std::move(plugin);
    entry.libraryHandle = libraryHandle;
    entry.state = PluginState::Loaded;
    entry.loadedAt = std::chrono::steady_clock::now();

    plugins_.emplace(info.name, std::move(entry));
    return GameResult<void>::ok();
}

GameResult<std::vector<std::string>> PluginManager::resolveDependencies() const {
    // Build adjacency list: dependency -> dependents.
    std::unordered_map<std::string, std::vector<std::string>> graph;
    std::unordered_map<std::string, std::size_t> inDegree;

    for (const auto& [name, entry] : plugins_) {
        if (inDegree.find(name) == inDegree.end()) {
            inDegree[name] = 0;
        }
        for (const auto& dep : entry.plugin->GetInfo().dependencies) {
            // Parse dependency name (strip version spec like ">=1.0.0").
            auto spacePos = dep.find_first_of("><=!");
            std::string depName = (spacePos != std::string::npos)
                                      ? dep.substr(0, spacePos)
                                      : dep;

            graph[depName].push_back(name);
            inDegree[name]++;
            if (inDegree.find(depName) == inDegree.end()) {
                inDegree[depName] = 0;
            }
        }
    }

    // Kahn's algorithm for topological sort.
    std::queue<std::string> readyQueue;
    for (const auto& [name, degree] : inDegree) {
        if (degree == 0 && plugins_.count(name) > 0) {
            readyQueue.push(name);
        }
    }

    std::vector<std::string> sorted;
    sorted.reserve(plugins_.size());

    while (!readyQueue.empty()) {
        auto current = readyQueue.front();
        readyQueue.pop();

        if (plugins_.count(current) > 0) {
            sorted.push_back(current);
        }

        if (graph.count(current) > 0) {
            for (const auto& dependent : graph.at(current)) {
                inDegree[dependent]--;
                if (inDegree[dependent] == 0) {
                    readyQueue.push(dependent);
                }
            }
        }
    }

    if (sorted.size() < plugins_.size()) {
        return GameResult<std::vector<std::string>>::err(
            GameError(ErrorCode::DependencyError,
                      "Circular or unresolvable plugin dependency detected"));
    }

    return GameResult<std::vector<std::string>>::ok(std::move(sorted));
}

void PluginManager::closeLibrary(void* handle) {
    if (handle == nullptr) {
        return;
    }
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

} // namespace cgs::plugin
