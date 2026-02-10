#pragma once

/// @file plugin_types.hpp
/// @brief Core types for the plugin system: Version, PluginInfo, PluginState, PluginContext.
///
/// @see SDS-MOD-020 (Plugin Interface Design)
/// @see SDS-MOD-021 (Plugin Manager Design)

#include <cstdint>
#include <string>
#include <vector>

#include "cgs/foundation/service_locator.hpp"

namespace cgs::plugin {

/// Plugin API version.  Plugins built against a different major version
/// are considered incompatible and will be rejected during loading.
constexpr uint32_t kPluginApiVersion = 1;

/// Semantic version for plugins.
struct Version {
    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t patch = 0;

    /// Two versions are compatible when their major versions match.
    [[nodiscard]] bool IsCompatibleWith(const Version& other) const noexcept {
        return major == other.major;
    }

    auto operator<=>(const Version&) const = default;
};

/// Metadata describing a plugin.
struct PluginInfo {
    std::string name;
    std::string description;
    Version version;
    std::vector<std::string> dependencies;
    uint32_t apiVersion = kPluginApiVersion;
};

/// Runtime context passed to plugins during their Load phase.
///
/// Provides access to foundation services (logger, config, etc.)
/// via the ServiceLocator.
struct PluginContext {
    cgs::foundation::ServiceLocator* services = nullptr;
};

/// Plugin lifecycle states.
///
/// State machine:
///   Unloaded ──LoadPlugin()──→ Loaded ──InitializeAll()──→ Initialized ──→ Active
///       ↑                                                                     │
///       └────────────────── UnloadPlugin() / ShutdownAll() ───────────────────┘
///                                          ↓ (on error)
///                                        Error
enum class PluginState : uint8_t {
    Unloaded,      ///< Not loaded; initial state.
    Loaded,        ///< Shared library opened, IPlugin created, OnLoad() succeeded.
    Initialized,   ///< OnInit() succeeded.
    Active,        ///< Running; receives OnUpdate() calls.
    ShuttingDown,  ///< OnShutdown() in progress.
    Error          ///< An error occurred during a lifecycle transition.
};

} // namespace cgs::plugin
