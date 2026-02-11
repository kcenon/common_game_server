#pragma once

/// @file plugin_events.hpp
/// @brief Plugin lifecycle event types for EventBus communication.
///
/// These events are emitted by PluginManager at each lifecycle
/// transition, allowing plugins to react to changes in the system.
///
/// @see SRS-PLG-003
/// @see SDS-MOD-017

#include <chrono>
#include <string>

#include "cgs/plugin/plugin_types.hpp"

namespace cgs::plugin {

/// Emitted when a plugin is successfully loaded.
struct PluginLoadedEvent {
    std::string pluginName;
    Version version;
    std::chrono::steady_clock::time_point timestamp =
        std::chrono::steady_clock::now();
};

/// Emitted when a plugin completes initialization.
struct PluginInitializedEvent {
    std::string pluginName;
    std::chrono::steady_clock::time_point timestamp =
        std::chrono::steady_clock::now();
};

/// Emitted when a plugin is activated and ready for updates.
struct PluginActivatedEvent {
    std::string pluginName;
    std::chrono::steady_clock::time_point timestamp =
        std::chrono::steady_clock::now();
};

/// Emitted when a plugin begins shutting down.
struct PluginShutdownEvent {
    std::string pluginName;
    std::chrono::steady_clock::time_point timestamp =
        std::chrono::steady_clock::now();
};

/// Emitted when a plugin encounters an error during a lifecycle transition.
struct PluginErrorEvent {
    std::string pluginName;
    std::string errorMessage;
    std::chrono::steady_clock::time_point timestamp =
        std::chrono::steady_clock::now();
};

} // namespace cgs::plugin
