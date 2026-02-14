/// @file plugin_manager.cpp
/// @brief PluginManager implementation: lifecycle, dynamic/static loading,
///        dependency resolution.
///
/// @see SDS-MOD-021

#include "cgs/plugin/plugin_manager.hpp"

#include "cgs/foundation/error_code.hpp"
#include "cgs/plugin/plugin_events.hpp"
#include "cgs/plugin/plugin_export.hpp"
#include "cgs/plugin/version_constraint.hpp"

#include <algorithm>
#include <queue>
#include <stack>
#include <unordered_set>

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

PluginManager::PluginManager() {
    context_.eventBus = &eventBus_;
}

PluginManager::~PluginManager() {
    ShutdownAll();
    UnloadAll();
}

PluginManager::PluginManager(PluginManager&& other) noexcept
    : plugins_(std::move(other.plugins_)),
      loadOrder_(std::move(other.loadOrder_)),
      context_(other.context_),
      eventBus_(std::move(other.eventBus_)) {
    context_.eventBus = &eventBus_;
}

PluginManager& PluginManager::operator=(PluginManager&& other) noexcept {
    if (this != &other) {
        ShutdownAll();
        UnloadAll();
        plugins_ = std::move(other.plugins_);
        loadOrder_ = std::move(other.loadOrder_);
        context_ = other.context_;
        eventBus_ = std::move(other.eventBus_);
        context_.eventBus = &eventBus_;
    }
    return *this;
}

// ── Context ─────────────────────────────────────────────────────────────

void PluginManager::SetContext(PluginContext ctx) {
    context_ = ctx;
    // Ensure the event bus always points to our owned instance.
    context_.eventBus = &eventBus_;
}

PluginContext& PluginManager::GetContext() noexcept {
    return context_;
}

// ── Dynamic loading ─────────────────────────────────────────────────────

GameResult<void> PluginManager::LoadPlugin(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginLoadFailed, "Plugin file not found: " + path.string()));
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
        return GameResult<void>::err(GameError(ErrorCode::PluginLoadFailed, std::move(errorMsg)));
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
        return GameResult<void>::err(GameError(
            ErrorCode::PluginLoadFailed, "CgsCreatePlugin returned null for: " + path.string()));
    }

    return loadPluginInstance(std::move(plugin), handle);
}

// ── Static loading ──────────────────────────────────────────────────────

GameResult<void> PluginManager::RegisterStaticPlugins() {
    for (const auto& entry : StaticPluginRegistry()) {
        auto plugin = entry.factory();
        if (!plugin) {
            return GameResult<void>::err(GameError(
                ErrorCode::PluginLoadFailed, "Static plugin factory returned null: " + entry.name));
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
            GameError(ErrorCode::PluginNotFound, "Plugin not found: " + std::string(name)));
    }

    auto& entry = it->second;
    if (entry.state != PluginState::Loaded) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginInvalidState,
                      "Plugin '" + std::string(name) + "' is not in Loaded state (current: " +
                          std::to_string(static_cast<int>(entry.state)) + ")"));
    }

    if (!entry.plugin->OnInit()) {
        entry.state = PluginState::Error;
        eventBus_.Publish(PluginErrorEvent{std::string(name), "OnInit() failed"});
        return GameResult<void>::err(GameError(ErrorCode::PluginInitFailed,
                                               "OnInit() failed for plugin: " + std::string(name)));
    }

    entry.state = PluginState::Initialized;
    eventBus_.Publish(PluginInitializedEvent{std::string(name)});
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
            GameError(ErrorCode::PluginNotFound, "Plugin not found: " + std::string(name)));
    }

    auto& entry = it->second;
    if (entry.state != PluginState::Initialized) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginInvalidState,
                      "Plugin '" + std::string(name) + "' is not in Initialized state"));
    }

    entry.state = PluginState::Active;
    eventBus_.Publish(PluginActivatedEvent{std::string(name)});
    return GameResult<void>::ok();
}

GameResult<void> PluginManager::ActivateAll() {
    const auto& order = loadOrder_.empty() ? GetAllPluginNames() : loadOrder_;

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
    const auto& order = loadOrder_.empty() ? GetAllPluginNames() : loadOrder_;

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
            GameError(ErrorCode::PluginNotFound, "Plugin not found: " + std::string(name)));
    }

    auto& entry = it->second;
    if (entry.state != PluginState::Active && entry.state != PluginState::Initialized) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginInvalidState,
                      "Plugin '" + std::string(name) + "' is not in Active or Initialized state"));
    }

    eventBus_.Publish(PluginShutdownEvent{std::string(name)});
    entry.state = PluginState::ShuttingDown;
    entry.plugin->OnShutdown();
    entry.state = PluginState::Loaded;
    return GameResult<void>::ok();
}

void PluginManager::ShutdownAll() {
    // Shutdown in reverse dependency order.
    auto order = loadOrder_.empty() ? GetAllPluginNames() : loadOrder_;
    std::reverse(order.begin(), order.end());

    for (const auto& name : order) {
        auto it = plugins_.find(name);
        if (it != plugins_.end() && (it->second.state == PluginState::Active ||
                                     it->second.state == PluginState::Initialized)) {
            eventBus_.Publish(PluginShutdownEvent{name});
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
            GameError(ErrorCode::PluginNotFound, "Plugin not found: " + std::string(name)));
    }

    auto& entry = it->second;
    if (entry.state != PluginState::Loaded && entry.state != PluginState::Unloaded &&
        entry.state != PluginState::Error) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginInvalidState,
                      "Plugin '" + std::string(name) + "' must be shut down before unloading"));
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
    auto order = loadOrder_.empty() ? GetAllPluginNames() : loadOrder_;
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
            GameError(ErrorCode::PluginNotFound, "Plugin not found: " + std::string(name)));
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

EventBus& PluginManager::GetEventBus() noexcept {
    return eventBus_;
}

// ── Private helpers ─────────────────────────────────────────────────────

GameResult<void> PluginManager::loadPluginInstance(std::unique_ptr<IPlugin> plugin,
                                                   void* libraryHandle) {
    const auto& info = plugin->GetInfo();

    // Check for duplicate.
    if (plugins_.count(info.name) > 0) {
        if (libraryHandle != nullptr) {
            closeLibrary(libraryHandle);
        }
        return GameResult<void>::err(
            GameError(ErrorCode::PluginAlreadyLoaded, "Plugin already loaded: " + info.name));
    }

    // Verify API version compatibility (major must match).
    if (info.apiVersion != kPluginApiVersion) {
        if (libraryHandle != nullptr) {
            closeLibrary(libraryHandle);
        }
        return GameResult<void>::err(GameError(
            ErrorCode::PluginVersionMismatch,
            "API version mismatch for '" + info.name + "': expected " +
                std::to_string(kPluginApiVersion) + ", got " + std::to_string(info.apiVersion)));
    }

    // Call OnLoad.
    if (!plugin->OnLoad(context_)) {
        if (libraryHandle != nullptr) {
            closeLibrary(libraryHandle);
        }
        return GameResult<void>::err(
            GameError(ErrorCode::PluginLoadFailed, "OnLoad() failed for plugin: " + info.name));
    }

    PluginEntry entry;
    entry.plugin = std::move(plugin);
    entry.libraryHandle = libraryHandle;
    entry.state = PluginState::Loaded;
    entry.loadedAt = std::chrono::steady_clock::now();

    auto pluginName = info.name;
    auto pluginVersion = info.version;
    plugins_.emplace(pluginName, std::move(entry));

    eventBus_.Publish(PluginLoadedEvent{pluginName, pluginVersion});

    return GameResult<void>::ok();
}

GameResult<std::vector<std::string>> PluginManager::resolveDependencies() const {
    auto report = ValidateDependencies();
    if (!report.success) {
        // Build a detailed error message from all issues.
        std::string msg;
        for (const auto& issue : report.issues) {
            if (!msg.empty()) {
                msg += "; ";
            }
            msg += issue.detail;
        }
        if (!report.cyclePath.empty()) {
            msg += " [cycle: ";
            for (std::size_t i = 0; i < report.cyclePath.size(); ++i) {
                if (i > 0) {
                    msg += " -> ";
                }
                msg += report.cyclePath[i];
            }
            msg += "]";
        }
        return GameResult<std::vector<std::string>>::err(
            GameError(ErrorCode::DependencyError, std::move(msg)));
    }

    return GameResult<std::vector<std::string>>::ok(std::move(report.loadOrder));
}

// ── Dependency validation ───────────────────────────────────────────────

PluginManager::DependencyReport PluginManager::ValidateDependencies() const {
    DependencyReport report;
    report.success = true;

    // Step 1: Validate version constraints and collect missing deps.
    validateVersionConstraints(report);

    // Step 2: Build adjacency list (dependency -> dependents).
    std::unordered_map<std::string, std::vector<std::string>> graph;
    std::unordered_map<std::string, std::size_t> inDegree;

    for (const auto& [name, entry] : plugins_) {
        if (inDegree.find(name) == inDegree.end()) {
            inDegree[name] = 0;
        }
        for (const auto& dep : entry.plugin->GetInfo().dependencies) {
            auto specResult = DependencySpec::Parse(dep);
            std::string depName;
            if (specResult.hasValue()) {
                depName = specResult.value().name;
            } else {
                auto opPos = dep.find_first_of("><=~");
                depName = (opPos != std::string::npos) ? dep.substr(0, opPos) : dep;
            }

            // Only add edges for loaded plugins (skip missing deps).
            if (plugins_.count(depName) > 0) {
                graph[depName].push_back(name);
                inDegree[name]++;
            }
            if (inDegree.find(depName) == inDegree.end()) {
                inDegree[depName] = 0;
            }
        }
    }

    // Step 3: Detect circular dependencies via DFS.
    auto cycle = detectCycle(graph);
    if (!cycle.empty()) {
        report.success = false;
        report.cyclePath = cycle;

        std::string cyclePath;
        for (std::size_t i = 0; i < cycle.size(); ++i) {
            if (i > 0) {
                cyclePath += " -> ";
            }
            cyclePath += cycle[i];
        }

        DependencyIssue issue;
        issue.kind = DependencyIssue::Kind::CircularDep;
        issue.detail = "Circular dependency detected: " + cyclePath;
        report.issues.push_back(std::move(issue));
        return report;
    }

    // Step 4: Topological sort (Kahn's algorithm).
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

    report.loadOrder = std::move(sorted);
    return report;
}

void PluginManager::validateVersionConstraints(DependencyReport& report) const {
    for (const auto& [name, entry] : plugins_) {
        for (const auto& dep : entry.plugin->GetInfo().dependencies) {
            auto specResult = DependencySpec::Parse(dep);
            if (specResult.hasError()) {
                // Fallback: extract plain name.
                continue;
            }

            const auto& spec = specResult.value();

            // Check if dependency is loaded.
            auto depIt = plugins_.find(spec.name);
            if (depIt == plugins_.end()) {
                report.success = false;
                DependencyIssue issue;
                issue.kind = DependencyIssue::Kind::Missing;
                issue.plugin = name;
                issue.dependency = spec.name;
                issue.detail = "Plugin '" + name + "' requires '" + spec.name + "' (" +
                               spec.ConstraintsToString() + ") but it is not loaded";
                report.issues.push_back(std::move(issue));
                continue;
            }

            // Check version constraints.
            if (!spec.constraints.empty()) {
                const auto& depVersion = depIt->second.plugin->GetInfo().version;
                if (!spec.IsSatisfiedBy(depVersion)) {
                    report.success = false;
                    DependencyIssue issue;
                    issue.kind = DependencyIssue::Kind::VersionMismatch;
                    issue.plugin = name;
                    issue.dependency = spec.name;
                    issue.detail = "Plugin '" + name + "' requires '" + spec.name + "' " +
                                   spec.ConstraintsToString() + " but loaded version is " +
                                   std::to_string(depVersion.major) + "." +
                                   std::to_string(depVersion.minor) + "." +
                                   std::to_string(depVersion.patch);
                    report.issues.push_back(std::move(issue));
                }
            }
        }
    }
}

std::vector<std::string> PluginManager::detectCycle(
    const std::unordered_map<std::string, std::vector<std::string>>& graph) const {
    // DFS-based cycle detection with path reconstruction.
    enum class Color : uint8_t { White, Gray, Black };
    std::unordered_map<std::string, Color> color;

    for (const auto& [name, entry] : plugins_) {
        color[name] = Color::White;
    }

    // parent map to reconstruct the cycle path.
    std::unordered_map<std::string, std::string> parent;
    std::string cycleStart;
    std::string cycleEnd;
    bool found = false;

    std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
        color[node] = Color::Gray;

        if (graph.count(node) > 0) {
            for (const auto& neighbor : graph.at(node)) {
                if (color.count(neighbor) == 0) {
                    continue;
                }
                if (color[neighbor] == Color::Gray) {
                    // Found a cycle.
                    cycleStart = neighbor;
                    cycleEnd = node;
                    found = true;
                    return true;
                }
                if (color[neighbor] == Color::White) {
                    parent[neighbor] = node;
                    if (dfs(neighbor)) {
                        return true;
                    }
                }
            }
        }

        color[node] = Color::Black;
        return false;
    };

    for (const auto& [name, entry] : plugins_) {
        if (color[name] == Color::White) {
            if (dfs(name)) {
                break;
            }
        }
    }

    if (!found) {
        return {};
    }

    // Reconstruct the cycle path: cycleStart -> ... -> cycleEnd -> cycleStart.
    std::vector<std::string> cycle;

    auto current = cycleEnd;
    while (current != cycleStart) {
        cycle.push_back(current);
        if (parent.count(current) == 0) {
            break;
        }
        current = parent[current];
    }
    cycle.push_back(cycleStart);
    std::reverse(cycle.begin(), cycle.end());
    cycle.push_back(cycleStart);  // Close the cycle.
    return cycle;
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

}  // namespace cgs::plugin
