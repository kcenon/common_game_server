#pragma once

/// @file map_instance_manager.hpp
/// @brief Map instance lifecycle management for the Game Server.
///
/// MapInstanceManager tracks logical map instances, their states,
/// and player counts.  It does not own ECS entities; the WorldSystem
/// manages the spatial data.  This class provides service-level
/// orchestration: instance creation, draining, and shutdown.
///
/// @see SRS-SVC-003.2
/// @see SDS-MOD-032

#include "cgs/foundation/game_result.hpp"
#include "cgs/game/world_types.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cgs::service {

/// Lifecycle state of a map instance.
enum class InstanceState : uint8_t {
    Active,       ///< Running normally, accepting new players.
    Draining,     ///< No new players, waiting for existing to leave.
    ShuttingDown  ///< About to be destroyed.
};

/// Metadata for a single map instance.
struct MapInstanceInfo {
    uint32_t instanceId = 0;
    uint32_t mapId = 0;
    cgs::game::MapType type = cgs::game::MapType::OpenWorld;
    InstanceState state = InstanceState::Active;
    uint32_t playerCount = 0;
    uint32_t maxPlayers = 100;
    std::chrono::steady_clock::time_point createdAt;
};

/// Manages map instance creation, state transitions, and player tracking.
///
/// Thread-safe: all public methods are guarded by an internal mutex.
class MapInstanceManager {
public:
    /// @param maxInstances  Maximum number of concurrent instances.
    explicit MapInstanceManager(uint32_t maxInstances = 1000);

    /// Create a new map instance.
    ///
    /// @param mapId       Logical map identifier.
    /// @param type        Map type classification.
    /// @param maxPlayers  Maximum players for this instance.
    /// @return The new instance ID, or error if limit reached.
    [[nodiscard]] cgs::foundation::GameResult<uint32_t> createInstance(
        uint32_t mapId,
        cgs::game::MapType type = cgs::game::MapType::OpenWorld,
        uint32_t maxPlayers = 100);

    /// Destroy an instance immediately.
    ///
    /// Fails if the instance has players (must drain first).
    [[nodiscard]] cgs::foundation::GameResult<void> destroyInstance(uint32_t instanceId);

    /// Transition an instance to a new state.
    ///
    /// Valid transitions: Active → Draining → ShuttingDown.
    [[nodiscard]] bool setInstanceState(uint32_t instanceId, InstanceState state);

    /// Get metadata for a specific instance.
    [[nodiscard]] std::optional<MapInstanceInfo> getInstance(uint32_t instanceId) const;

    /// Get all instances for a given map ID.
    [[nodiscard]] std::vector<MapInstanceInfo> getInstancesByMap(uint32_t mapId) const;

    /// Get all instances in a given state.
    [[nodiscard]] std::vector<MapInstanceInfo> getInstancesByState(InstanceState state) const;

    /// Increment the player count for an instance.
    ///
    /// Fails if the instance is not Active, does not exist, or is full.
    [[nodiscard]] bool addPlayer(uint32_t instanceId);

    /// Decrement the player count for an instance.
    ///
    /// Fails if the instance does not exist or has zero players.
    [[nodiscard]] bool removePlayer(uint32_t instanceId);

    /// Get the total number of instances.
    [[nodiscard]] uint32_t instanceCount() const;

    /// Get the number of instances in a given state.
    [[nodiscard]] uint32_t instanceCount(InstanceState state) const;

    /// Find all instances with zero players.
    [[nodiscard]] std::vector<uint32_t> findEmptyInstances() const;

    /// Find instances that can accept more players.
    [[nodiscard]] std::vector<uint32_t> findAvailableInstances(uint32_t mapId) const;

private:
    uint32_t maxInstances_;
    uint32_t nextInstanceId_ = 1;
    std::unordered_map<uint32_t, MapInstanceInfo> instances_;
    mutable std::mutex mutex_;
};

}  // namespace cgs::service
