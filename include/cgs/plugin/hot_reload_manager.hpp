#pragma once

/// @file hot_reload_manager.hpp
/// @brief HotReloadManager: orchestrates safe plugin reload during development.
///
/// Guarded by CGS_HOT_RELOAD — compiled out entirely in production builds.
///
/// @see SRS-PLG-005
/// @see SDS-MOD-023

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "cgs/foundation/game_result.hpp"
#include "cgs/plugin/file_watcher.hpp"
#include "cgs/plugin/hot_reloadable.hpp"

namespace cgs::plugin {

class PluginManager;

/// Manages plugin hot reload during development.
///
/// Orchestrates the safe reload cycle:
///   1. Detect file change (via FileWatcher)
///   2. Capture state (if IHotReloadable)
///   3. Shutdown → Unload
///   4. Load → Init → Activate (new binary)
///   5. Restore state (if version matches)
///
/// This entire class is a no-op shell when CGS_HOT_RELOAD is not defined,
/// ensuring zero overhead in production builds.
class HotReloadManager {
public:
    explicit HotReloadManager(PluginManager& pluginManager);
    ~HotReloadManager();

    HotReloadManager(const HotReloadManager&) = delete;
    HotReloadManager& operator=(const HotReloadManager&) = delete;
    HotReloadManager(HotReloadManager&&) = delete;
    HotReloadManager& operator=(HotReloadManager&&) = delete;

    /// Whether hot reload is available in this build.
    [[nodiscard]] static constexpr bool IsAvailable() noexcept {
#ifdef CGS_HOT_RELOAD
        return true;
#else
        return false;
#endif
    }

    /// Start monitoring a plugin shared library for changes.
    ///
    /// Associates a plugin name with its library file path so that
    /// file changes trigger a reload of that specific plugin.
    ///
    /// @param pluginName  The registered plugin name.
    /// @param libraryPath Path to the .so/.dll/.dylib file.
    /// @return Error if hot reload is disabled or file not found.
    [[nodiscard]] cgs::foundation::GameResult<void>
    WatchPlugin(const std::string& pluginName,
                const std::filesystem::path& libraryPath);

    /// Stop monitoring a plugin.
    void UnwatchPlugin(const std::string& pluginName);

    /// Poll for file changes and trigger reloads as needed.
    ///
    /// Call this periodically (e.g., once per frame in development).
    void Poll();

    /// Manually trigger a hot reload for a specific plugin.
    ///
    /// @param pluginName The plugin to reload.
    /// @return Error if the plugin is not found, not dynamic, or reload fails.
    [[nodiscard]] cgs::foundation::GameResult<void>
    ReloadPlugin(const std::string& pluginName);

    /// Set debounce duration for file change detection.
    void SetDebounceMs(uint32_t ms);

    /// Return the number of plugins being watched.
    [[nodiscard]] std::size_t WatchedPluginCount() const;

    /// Return the number of successful reloads performed.
    [[nodiscard]] uint64_t ReloadCount() const noexcept;

    /// Retrieve the most recent state snapshot for a plugin (if any).
    [[nodiscard]] const PluginStateSnapshot*
    GetSnapshot(const std::string& pluginName) const;

private:
#ifdef CGS_HOT_RELOAD
    /// Perform the full reload cycle for a single plugin.
    [[nodiscard]] cgs::foundation::GameResult<void>
    doReload(const std::string& pluginName,
             const std::filesystem::path& libraryPath);

    /// Capture state from an IHotReloadable plugin.
    [[nodiscard]] cgs::foundation::GameResult<PluginStateSnapshot>
    captureState(const std::string& pluginName);

    /// Restore state to an IHotReloadable plugin.
    [[nodiscard]] cgs::foundation::GameResult<void>
    restoreState(const std::string& pluginName,
                 const PluginStateSnapshot& snapshot);

    PluginManager& pluginManager_;
    FileWatcher fileWatcher_;

    /// Plugin name → library path mapping.
    std::unordered_map<std::string, std::filesystem::path> watchedPlugins_;

    /// Plugin name → most recent state snapshot.
    std::unordered_map<std::string, PluginStateSnapshot> snapshots_;

    uint64_t reloadCount_ = 0;
#else
    // Reference kept for API compatibility even when hot reload is disabled.
    [[maybe_unused]] PluginManager& pluginManager_;
#endif
};

} // namespace cgs::plugin
