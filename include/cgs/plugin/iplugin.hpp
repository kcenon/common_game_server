#pragma once

/// @file iplugin.hpp
/// @brief IPlugin abstract interface that all plugins must implement.
///
/// @see SDS-MOD-020 (Plugin Interface Design)

#include "cgs/plugin/plugin_types.hpp"

namespace cgs::plugin {

/// Abstract base class for game server plugins.
///
/// Every plugin — whether loaded dynamically (.so/.dll) or registered
/// statically — must implement this interface.  The PluginManager drives
/// the lifecycle callbacks in order:
///
///   OnLoad() → OnInit() → OnUpdate()* → OnShutdown() → OnUnload()
///
/// @see PluginManager
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /// Return immutable metadata about this plugin.
    [[nodiscard]] virtual const PluginInfo& GetInfo() const = 0;

    /// Called when the plugin is first loaded.
    ///
    /// Use this to acquire resources and register components/systems.
    /// @param ctx  Runtime context providing access to foundation services.
    /// @return true on success, false to abort loading.
    virtual bool OnLoad(PluginContext& ctx) = 0;

    /// Called after all plugins are loaded and dependencies resolved.
    ///
    /// Use this to perform initialization that depends on other plugins.
    /// @return true on success, false to abort initialization.
    virtual bool OnInit() = 0;

    /// Called once per frame while the plugin is active.
    ///
    /// @param deltaTime  Frame delta time in seconds.
    virtual void OnUpdate(float deltaTime) = 0;

    /// Called when the plugin is being shut down.
    ///
    /// Release resources acquired during OnInit() and OnLoad().
    virtual void OnShutdown() = 0;

    /// Called just before the plugin is unloaded from memory.
    ///
    /// Final cleanup; after this call the plugin object will be destroyed.
    virtual void OnUnload() = 0;
};

}  // namespace cgs::plugin
