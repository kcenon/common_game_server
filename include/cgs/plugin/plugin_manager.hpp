#pragma once

/// @file plugin_manager.hpp
/// @brief PluginManager: lifecycle management, dependency resolution,
///        and dynamic/static plugin loading.
///
/// @see SDS-MOD-021 (Plugin Manager Design)

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cgs/foundation/game_result.hpp"
#include "cgs/plugin/iplugin.hpp"
#include "cgs/plugin/plugin_types.hpp"

namespace cgs::plugin {

/// Manages the full plugin lifecycle: Load → Init → Active → Shutdown → Unload.
///
/// Supports two loading modes:
///   - **Dynamic**: load shared libraries (.so/.dll) at runtime via LoadPlugin().
///   - **Static**: register compile-time plugins via RegisterStaticPlugins().
///
/// The manager enforces a state machine for each plugin and resolves
/// dependency ordering before initialization.
class PluginManager {
public:
    PluginManager();
    ~PluginManager();

    // Non-copyable, movable.
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    PluginManager(PluginManager&&) noexcept;
    PluginManager& operator=(PluginManager&&) noexcept;

    // ── Context ────────────────────────────────────────────────────────

    /// Set the plugin context (service locator, etc.).
    ///
    /// Must be called before loading any plugins.
    void SetContext(PluginContext ctx);

    /// Retrieve the current plugin context.
    [[nodiscard]] PluginContext& GetContext() noexcept;

    // ── Dynamic loading ────────────────────────────────────────────────

    /// Load a plugin from a shared library file.
    ///
    /// Opens the library, calls CgsCreatePlugin(), verifies API version
    /// compatibility, and transitions the plugin to the Loaded state.
    ///
    /// @param path  Path to the shared library (.so, .dll, .dylib).
    /// @return GameResult<void> — error on file-not-found, symbol lookup
    ///         failure, API version mismatch, or duplicate load.
    [[nodiscard]] cgs::foundation::GameResult<void>
    LoadPlugin(const std::filesystem::path& path);

    // ── Static loading ─────────────────────────────────────────────────

    /// Load all plugins from the static registry (CGS_PLUGIN_REGISTER).
    ///
    /// Each registered factory is called and the resulting plugin is
    /// loaded the same way as a dynamic plugin (minus dlopen).
    [[nodiscard]] cgs::foundation::GameResult<void> RegisterStaticPlugins();

    // ── Lifecycle ──────────────────────────────────────────────────────

    /// Initialize a single plugin by name.
    ///
    /// Transitions from Loaded → Initialized.
    [[nodiscard]] cgs::foundation::GameResult<void>
    InitPlugin(std::string_view name);

    /// Initialize all loaded plugins in dependency order.
    ///
    /// Resolves dependency ordering via topological sort, then calls
    /// OnInit() on each plugin.  On failure, already-initialized plugins
    /// are not rolled back (caller should ShutdownAll + UnloadAll).
    [[nodiscard]] cgs::foundation::GameResult<void> InitializeAll();

    /// Activate a single initialized plugin (Initialized → Active).
    [[nodiscard]] cgs::foundation::GameResult<void>
    ActivatePlugin(std::string_view name);

    /// Activate all initialized plugins.
    [[nodiscard]] cgs::foundation::GameResult<void> ActivateAll();

    /// Update all active plugins.
    ///
    /// @param deltaTime  Frame delta time in seconds.
    void UpdateAll(float deltaTime);

    /// Shut down a single plugin (Active/Initialized → Loaded).
    [[nodiscard]] cgs::foundation::GameResult<void>
    ShutdownPlugin(std::string_view name);

    /// Shut down all active/initialized plugins in reverse dependency order.
    void ShutdownAll();

    /// Unload a single plugin (Loaded → Unloaded, removed from manager).
    [[nodiscard]] cgs::foundation::GameResult<void>
    UnloadPlugin(std::string_view name);

    /// Unload all plugins.
    void UnloadAll();

    // ── Queries ────────────────────────────────────────────────────────

    /// Retrieve a loaded plugin by name (nullptr if not found).
    [[nodiscard]] IPlugin* GetPlugin(std::string_view name) const;

    /// Return the state of a loaded plugin.
    [[nodiscard]] cgs::foundation::GameResult<PluginState>
    GetPluginState(std::string_view name) const;

    /// Return the names of all loaded plugins.
    [[nodiscard]] std::vector<std::string> GetAllPluginNames() const;

    /// Return the number of loaded plugins.
    [[nodiscard]] std::size_t PluginCount() const noexcept;

private:
    /// Internal bookkeeping for a loaded plugin.
    struct PluginEntry {
        std::unique_ptr<IPlugin> plugin;
        void* libraryHandle = nullptr;   ///< dlopen handle (nullptr for static).
        PluginState state = PluginState::Unloaded;
        std::chrono::steady_clock::time_point loadedAt;
    };

    /// Load a pre-created plugin instance into the manager.
    [[nodiscard]] cgs::foundation::GameResult<void>
    loadPluginInstance(std::unique_ptr<IPlugin> plugin, void* libraryHandle);

    /// Resolve dependency ordering via topological sort.
    [[nodiscard]] cgs::foundation::GameResult<std::vector<std::string>>
    resolveDependencies() const;

    /// Close a dynamic library handle.
    static void closeLibrary(void* handle);

    /// All loaded plugins keyed by name.
    std::unordered_map<std::string, PluginEntry> plugins_;

    /// Dependency-resolved load order (populated by InitializeAll).
    std::vector<std::string> loadOrder_;

    /// Plugin context shared with all plugins.
    PluginContext context_;
};

} // namespace cgs::plugin
