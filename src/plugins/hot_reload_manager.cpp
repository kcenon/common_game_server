/// @file hot_reload_manager.cpp
/// @brief HotReloadManager implementation: safe reload cycle with state preservation.
///
/// @see SDS-MOD-023

#include "cgs/plugin/hot_reload_manager.hpp"

#include "cgs/foundation/error_code.hpp"
#include "cgs/plugin/plugin_events.hpp"
#include "cgs/plugin/plugin_manager.hpp"

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;

namespace cgs::plugin {

// ── Construction / destruction ──────────────────────────────────────────

HotReloadManager::HotReloadManager(PluginManager& pluginManager) : pluginManager_(pluginManager) {
#ifdef CGS_HOT_RELOAD
    fileWatcher_.SetCallback([this](const std::filesystem::path& path) {
        // Find which plugin corresponds to this file.
        for (const auto& [name, watchedPath] : watchedPlugins_) {
            if (std::filesystem::equivalent(path, watchedPath)) {
                (void)doReload(name, watchedPath);
                break;
            }
        }
    });
#endif
}

HotReloadManager::~HotReloadManager() = default;

// ── Public API ──────────────────────────────────────────────────────────

GameResult<void> HotReloadManager::WatchPlugin(const std::string& pluginName,
                                               const std::filesystem::path& libraryPath) {
#ifdef CGS_HOT_RELOAD
    if (!std::filesystem::exists(libraryPath)) {
        return GameResult<void>::err(GameError(ErrorCode::PluginLoadFailed,
                                               "Library file not found: " + libraryPath.string()));
    }

    if (!fileWatcher_.Watch(libraryPath)) {
        return GameResult<void>::err(
            GameError(ErrorCode::HotReloadFailed, "Failed to watch: " + libraryPath.string()));
    }

    watchedPlugins_[pluginName] = libraryPath;
    return GameResult<void>::ok();
#else
    (void)pluginName;
    (void)libraryPath;
    return GameResult<void>::err(
        GameError(ErrorCode::HotReloadDisabled, "Hot reload is not available in this build"));
#endif
}

void HotReloadManager::UnwatchPlugin(const std::string& pluginName) {
#ifdef CGS_HOT_RELOAD
    auto it = watchedPlugins_.find(pluginName);
    if (it != watchedPlugins_.end()) {
        fileWatcher_.Unwatch(it->second);
        watchedPlugins_.erase(it);
    }
#else
    (void)pluginName;
#endif
}

void HotReloadManager::Poll() {
#ifdef CGS_HOT_RELOAD
    fileWatcher_.Poll();
#endif
}

GameResult<void> HotReloadManager::ReloadPlugin(const std::string& pluginName) {
#ifdef CGS_HOT_RELOAD
    auto it = watchedPlugins_.find(pluginName);
    if (it == watchedPlugins_.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginNotFound, "Plugin not watched: " + pluginName));
    }
    return doReload(pluginName, it->second);
#else
    (void)pluginName;
    return GameResult<void>::err(
        GameError(ErrorCode::HotReloadDisabled, "Hot reload is not available in this build"));
#endif
}

void HotReloadManager::SetDebounceMs(uint32_t ms) {
#ifdef CGS_HOT_RELOAD
    fileWatcher_.SetDebounceMs(ms);
#else
    (void)ms;
#endif
}

std::size_t HotReloadManager::WatchedPluginCount() const {
#ifdef CGS_HOT_RELOAD
    return watchedPlugins_.size();
#else
    return 0;
#endif
}

uint64_t HotReloadManager::ReloadCount() const noexcept {
#ifdef CGS_HOT_RELOAD
    return reloadCount_;
#else
    return 0;
#endif
}

const PluginStateSnapshot* HotReloadManager::GetSnapshot(const std::string& pluginName) const {
#ifdef CGS_HOT_RELOAD
    auto it = snapshots_.find(pluginName);
    if (it == snapshots_.end()) {
        return nullptr;
    }
    return &it->second;
#else
    (void)pluginName;
    return nullptr;
#endif
}

// ── Internal reload implementation ──────────────────────────────────────

#ifdef CGS_HOT_RELOAD

GameResult<void> HotReloadManager::doReload(const std::string& pluginName,
                                            const std::filesystem::path& libraryPath) {
    // 1. Capture state (if the plugin supports IHotReloadable).
    auto stateResult = captureState(pluginName);
    bool hasState = stateResult.hasValue();
    if (hasState) {
        snapshots_.insert_or_assign(pluginName, std::move(stateResult).value());
    }

    // 2. Shutdown the plugin (Active/Initialized → Loaded).
    auto stateCheck = pluginManager_.GetPluginState(pluginName);
    if (stateCheck.hasValue()) {
        auto state = stateCheck.value();
        if (state == PluginState::Active || state == PluginState::Initialized) {
            auto shutResult = pluginManager_.ShutdownPlugin(pluginName);
            if (shutResult.hasError()) {
                return shutResult;
            }
        }
    }

    // 3. Unload the old binary.
    auto unloadResult = pluginManager_.UnloadPlugin(pluginName);
    if (unloadResult.hasError()) {
        return unloadResult;
    }

    // 4. Load the new binary.
    auto loadResult = pluginManager_.LoadPlugin(libraryPath);
    if (loadResult.hasError()) {
        return GameResult<void>::err(GameError(
            ErrorCode::HotReloadFailed,
            "Failed to reload plugin '" + pluginName + "': " + loadResult.error().message()));
    }

    // 5. Initialize the reloaded plugin.
    auto initResult = pluginManager_.InitPlugin(pluginName);
    if (initResult.hasError()) {
        return initResult;
    }

    // 6. Activate the reloaded plugin.
    auto activateResult = pluginManager_.ActivatePlugin(pluginName);
    if (activateResult.hasError()) {
        return activateResult;
    }

    // 7. Restore state (if captured and version matches).
    if (hasState) {
        auto restoreResult = restoreState(pluginName, snapshots_.at(pluginName));
        // State restoration failure is non-fatal; the plugin runs fresh.
        (void)restoreResult;
    }

    ++reloadCount_;
    return GameResult<void>::ok();
}

GameResult<PluginStateSnapshot> HotReloadManager::captureState(const std::string& pluginName) {
    auto* plugin = pluginManager_.GetPlugin(pluginName);
    if (plugin == nullptr) {
        return GameResult<PluginStateSnapshot>::err(
            GameError(ErrorCode::PluginNotFound, "Plugin not found: " + pluginName));
    }

    auto* reloadable = dynamic_cast<IHotReloadable*>(plugin);
    if (reloadable == nullptr) {
        return GameResult<PluginStateSnapshot>::err(
            GameError(ErrorCode::StateSerializationFailed,
                      "Plugin does not support IHotReloadable: " + pluginName));
    }

    auto serResult = reloadable->SerializeState();
    if (serResult.hasError()) {
        return GameResult<PluginStateSnapshot>::err(serResult.error());
    }

    PluginStateSnapshot snapshot;
    snapshot.pluginName = pluginName;
    snapshot.pluginVersion = plugin->GetInfo().version;
    snapshot.stateVersion = reloadable->GetStateVersion();
    snapshot.data = std::move(serResult).value();
    snapshot.capturedAt = std::chrono::steady_clock::now();

    return GameResult<PluginStateSnapshot>::ok(std::move(snapshot));
}

GameResult<void> HotReloadManager::restoreState(const std::string& pluginName,
                                                const PluginStateSnapshot& snapshot) {
    auto* plugin = pluginManager_.GetPlugin(pluginName);
    if (plugin == nullptr) {
        return GameResult<void>::err(
            GameError(ErrorCode::PluginNotFound, "Plugin not found after reload: " + pluginName));
    }

    auto* reloadable = dynamic_cast<IHotReloadable*>(plugin);
    if (reloadable == nullptr) {
        return GameResult<void>::err(
            GameError(ErrorCode::StateDeserializationFailed,
                      "Reloaded plugin lost IHotReloadable: " + pluginName));
    }

    // Version mismatch: skip restoration silently.
    if (reloadable->GetStateVersion() != snapshot.stateVersion) {
        return GameResult<void>::ok();
    }

    return reloadable->DeserializeState(snapshot.data.data(), snapshot.data.size());
}

#endif  // CGS_HOT_RELOAD

}  // namespace cgs::plugin
