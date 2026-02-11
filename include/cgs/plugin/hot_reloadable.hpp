#pragma once

/// @file hot_reloadable.hpp
/// @brief Optional interface for plugins that support state preservation
///        across hot reloads.
///
/// @see SRS-PLG-005.3
/// @see SDS-MOD-023

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "cgs/foundation/game_result.hpp"
#include "cgs/plugin/plugin_types.hpp"

namespace cgs::plugin {

/// Snapshot of a plugin's serialized state for reload preservation.
struct PluginStateSnapshot {
    std::string pluginName;
    Version pluginVersion;
    uint32_t stateVersion = 0;
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point capturedAt;
};

/// Optional interface for plugins that wish to preserve state
/// across hot reloads.
///
/// Plugins may optionally implement this interface to support
/// state serialization/deserialization during reload cycles.
/// Plugins that do not implement this interface will simply be
/// reloaded without state restoration.
///
/// Usage:
/// @code
///   class MyPlugin : public IPlugin, public IHotReloadable {
///   public:
///       GameResult<std::vector<uint8_t>> SerializeState() override {
///           // Serialize important state to bytes.
///           return GameResult<std::vector<uint8_t>>::ok({...});
///       }
///
///       GameResult<void> DeserializeState(
///           const uint8_t* data, std::size_t size) override {
///           // Restore state from bytes.
///           return GameResult<void>::ok();
///       }
///
///       uint32_t GetStateVersion() const override { return 1; }
///   };
/// @endcode
class IHotReloadable {
public:
    virtual ~IHotReloadable() = default;

    /// Serialize the plugin's current state into a byte buffer.
    ///
    /// Called before the plugin is unloaded during a hot reload cycle.
    [[nodiscard]] virtual cgs::foundation::GameResult<std::vector<uint8_t>>
    SerializeState() = 0;

    /// Restore the plugin's state from a previously serialized buffer.
    ///
    /// Called after the plugin is reloaded and initialized.
    /// @param data  Pointer to the serialized state bytes.
    /// @param size  Number of bytes in the buffer.
    [[nodiscard]] virtual cgs::foundation::GameResult<void>
    DeserializeState(const uint8_t* data, std::size_t size) = 0;

    /// Return a version number for the serialized state format.
    ///
    /// If the version returned by a reloaded plugin differs from
    /// the stored snapshot, state restoration will be skipped.
    [[nodiscard]] virtual uint32_t GetStateVersion() const = 0;
};

} // namespace cgs::plugin
