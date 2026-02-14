#pragma once

/// @file game_server.hpp
/// @brief Game Server for ECS-based world simulation, player session
///        management, and game system orchestration.
///
/// GameServer is the main entry point for game logic. It owns the
/// ECS world (EntityManager, ComponentStorages, SystemScheduler),
/// wires the 6 game systems, and runs them through a fixed-rate
/// GameLoop. Player lifecycle (join, leave) is mapped to ECS entities.
///
/// @see SRS-SVC-003
/// @see SDS-MOD-032

#include "cgs/ecs/entity.hpp"
#include "cgs/foundation/game_result.hpp"
#include "cgs/foundation/types.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace cgs::service {

// -- Configuration -----------------------------------------------------------

/// Configuration for the Game Server.
struct GameServerConfig {
    /// Tick rate in ticks per second (default: 20 Hz).
    uint32_t tickRate = 20;

    /// Maximum number of map instances.
    uint32_t maxInstances = 1000;

    /// World spatial cell size for spatial indexing.
    float spatialCellSize = 32.0f;

    /// Default AI tick interval in seconds.
    float aiTickInterval = 0.1f;
};

// -- Player session ----------------------------------------------------------

/// Maps a player to their in-world entity and map instance.
struct PlayerSession {
    cgs::foundation::PlayerId playerId;
    cgs::ecs::Entity entity;
    uint32_t instanceId = 0;
};

// -- Statistics ---------------------------------------------------------------

/// Runtime statistics snapshot for the Game Server.
struct GameServerStats {
    /// Game loop tick metrics.
    uint64_t totalTicks = 0;
    float lastUpdateTimeMs = 0.0f;
    float lastBudgetUtilization = 0.0f;

    /// Entity and instance counts.
    std::size_t entityCount = 0;
    std::size_t playerCount = 0;
    uint32_t activeInstances = 0;
    uint32_t drainingInstances = 0;

    /// System-level counters.
    uint64_t playersJoined = 0;
    uint64_t playersLeft = 0;
};

// -- Game Server -------------------------------------------------------------

/// Game Server orchestrating ECS world simulation and player lifecycle.
///
/// Usage:
/// @code
///   GameServerConfig config;
///   config.tickRate = 20;
///   GameServer server(config);
///   auto r = server.start();
///
///   // Create a map instance and add a player.
///   auto instId = server.createInstance(1);     // mapId = 1
///   server.addPlayer(playerId, instId.value());
///
///   // Tick manually for tests, or let the loop run.
///   auto metrics = server.tick();
///
///   server.removePlayer(playerId);
///   server.stop();
/// @endcode
class GameServer {
public:
    explicit GameServer(GameServerConfig config);
    ~GameServer();

    GameServer(const GameServer&) = delete;
    GameServer& operator=(const GameServer&) = delete;
    GameServer(GameServer&&) noexcept;
    GameServer& operator=(GameServer&&) noexcept;

    // -- Lifecycle ------------------------------------------------------------

    /// Start the game loop on a dedicated thread.
    [[nodiscard]] cgs::foundation::GameResult<void> start();

    /// Stop the game loop and shut down.
    void stop();

    /// Check if the server is running.
    [[nodiscard]] bool isRunning() const noexcept;

    // -- Manual tick (for testing) --------------------------------------------

    /// Execute a single game tick manually.
    ///
    /// The game loop must NOT be running on a thread when calling this.
    /// @return The tick metrics.
    [[nodiscard]] cgs::foundation::GameResult<void> tick();

    // -- Instance management --------------------------------------------------

    /// Create a new map instance for the given map ID.
    [[nodiscard]] cgs::foundation::GameResult<uint32_t> createInstance(uint32_t mapId,
                                                                       uint32_t maxPlayers = 100);

    /// Destroy a map instance (must have zero players).
    [[nodiscard]] cgs::foundation::GameResult<void> destroyInstance(uint32_t instanceId);

    /// Get instance IDs that can accept new players for a given map.
    [[nodiscard]] std::vector<uint32_t> availableInstances(uint32_t mapId) const;

    // -- Player lifecycle -----------------------------------------------------

    /// Add a player to a map instance.
    ///
    /// Creates an ECS entity with default components (Transform, Identity,
    /// Stats, Movement, MapMembership, QuestLog, Inventory, Equipment).
    [[nodiscard]] cgs::foundation::GameResult<cgs::ecs::Entity> addPlayer(
        cgs::foundation::PlayerId playerId, uint32_t instanceId);

    /// Remove a player from the world.
    ///
    /// Destroys the player's ECS entity and releases the instance slot.
    [[nodiscard]] cgs::foundation::GameResult<void> removePlayer(
        cgs::foundation::PlayerId playerId);

    /// Get the player session info for a player.
    [[nodiscard]] std::optional<PlayerSession> getPlayerSession(
        cgs::foundation::PlayerId playerId) const;

    /// Transfer a player to a different map instance.
    [[nodiscard]] cgs::foundation::GameResult<void> transferPlayer(
        cgs::foundation::PlayerId playerId, uint32_t targetInstanceId);

    // -- Statistics -----------------------------------------------------------

    /// Get a snapshot of current game server statistics.
    [[nodiscard]] GameServerStats stats() const;

    /// Get the configuration.
    [[nodiscard]] const GameServerConfig& config() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cgs::service
