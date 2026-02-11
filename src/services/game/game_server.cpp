/// @file game_server.cpp
/// @brief GameServer implementation orchestrating ECS world simulation,
///        player session management, and 6 game systems.

#include "cgs/service/game_server.hpp"

#include <atomic>
#include <mutex>
#include <unordered_map>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"
#include "cgs/game/ai_components.hpp"
#include "cgs/game/ai_system.hpp"
#include "cgs/game/combat_components.hpp"
#include "cgs/game/combat_system.hpp"
#include "cgs/game/components.hpp"
#include "cgs/game/inventory_components.hpp"
#include "cgs/game/inventory_system.hpp"
#include "cgs/game/object_system.hpp"
#include "cgs/game/quest_components.hpp"
#include "cgs/game/quest_system.hpp"
#include "cgs/game/world_components.hpp"
#include "cgs/game/world_system.hpp"
#include "cgs/service/game_loop.hpp"
#include "cgs/service/map_instance_manager.hpp"

namespace cgs::service {

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;
using cgs::foundation::PlayerId;

// -- Impl --------------------------------------------------------------------

struct GameServer::Impl {
    GameServerConfig config;

    // ECS core
    cgs::ecs::EntityManager entities;
    cgs::ecs::SystemScheduler scheduler;

    // Component storages (core)
    cgs::ecs::ComponentStorage<cgs::game::Transform> transforms;
    cgs::ecs::ComponentStorage<cgs::game::Identity> identities;
    cgs::ecs::ComponentStorage<cgs::game::Stats> stats;
    cgs::ecs::ComponentStorage<cgs::game::Movement> movements;

    // Component storages (world)
    cgs::ecs::ComponentStorage<cgs::game::MapInstance> mapInstances;
    cgs::ecs::ComponentStorage<cgs::game::MapMembership> memberships;
    cgs::ecs::ComponentStorage<cgs::game::VisibilityRange> visibilityRanges;
    cgs::ecs::ComponentStorage<cgs::game::Zone> zones;

    // Component storages (combat)
    cgs::ecs::ComponentStorage<cgs::game::SpellCast> spellCasts;
    cgs::ecs::ComponentStorage<cgs::game::AuraHolder> auraHolders;
    cgs::ecs::ComponentStorage<cgs::game::DamageEvent> damageEvents;
    cgs::ecs::ComponentStorage<cgs::game::ThreatList> threatLists;

    // Component storages (AI)
    cgs::ecs::ComponentStorage<cgs::game::AIBrain> aiBrains;

    // Component storages (quest)
    cgs::ecs::ComponentStorage<cgs::game::QuestLog> questLogs;
    cgs::ecs::ComponentStorage<cgs::game::QuestEvent> questEvents;

    // Component storages (inventory)
    cgs::ecs::ComponentStorage<cgs::game::Inventory> inventories;
    cgs::ecs::ComponentStorage<cgs::game::Equipment> equipment;
    cgs::ecs::ComponentStorage<cgs::game::DurabilityEvent> durabilityEvents;

    // Service-level components
    GameLoop gameLoop;
    MapInstanceManager instanceManager;

    // Player session tracking
    mutable std::mutex playerMutex;
    std::unordered_map<PlayerId, PlayerSession> playerSessions;

    // Instance ID → map entity mapping (avoids scanning component storage).
    std::unordered_map<uint32_t, cgs::ecs::Entity> instanceEntities;

    // Counters
    std::atomic<uint64_t> playersJoined{0};
    std::atomic<uint64_t> playersLeft{0};

    explicit Impl(GameServerConfig cfg)
        : config(std::move(cfg))
        , gameLoop(config.tickRate)
        , instanceManager(config.maxInstances) {}

    /// Register all component storages with the EntityManager for
    /// automatic cleanup on entity destruction.
    void registerStorages() {
        entities.RegisterStorage(&transforms);
        entities.RegisterStorage(&identities);
        entities.RegisterStorage(&stats);
        entities.RegisterStorage(&movements);
        entities.RegisterStorage(&mapInstances);
        entities.RegisterStorage(&memberships);
        entities.RegisterStorage(&visibilityRanges);
        entities.RegisterStorage(&zones);
        entities.RegisterStorage(&spellCasts);
        entities.RegisterStorage(&auraHolders);
        entities.RegisterStorage(&damageEvents);
        entities.RegisterStorage(&threatLists);
        entities.RegisterStorage(&aiBrains);
        entities.RegisterStorage(&questLogs);
        entities.RegisterStorage(&questEvents);
        entities.RegisterStorage(&inventories);
        entities.RegisterStorage(&equipment);
        entities.RegisterStorage(&durabilityEvents);
    }

    /// Register all 6 game systems with the scheduler.
    [[nodiscard]] bool registerSystems() {
        // PreUpdate stage
        scheduler.Register<cgs::game::WorldSystem>(
            transforms, memberships, mapInstances,
            visibilityRanges, zones, config.spatialCellSize);

        // Update stage
        scheduler.Register<cgs::game::ObjectUpdateSystem>(
            transforms, movements);

        scheduler.Register<cgs::game::CombatSystem>(
            spellCasts, auraHolders, damageEvents, stats, threatLists);

        scheduler.Register<cgs::game::AISystem>(
            aiBrains, transforms, movements, stats, threatLists,
            config.aiTickInterval);

        // PostUpdate stage
        scheduler.Register<cgs::game::QuestSystem>(
            questLogs, questEvents);

        scheduler.Register<cgs::game::InventorySystem>(
            inventories, equipment, durabilityEvents);

        return scheduler.Build();
    }

    /// Find the map entity for a given instanceId.
    std::optional<cgs::ecs::Entity> findMapEntity(uint32_t instanceId) const {
        auto it = instanceEntities.find(instanceId);
        if (it == instanceEntities.end()) {
            return std::nullopt;
        }
        return it->second;
    }
};

// -- Construction / destruction / move ----------------------------------------

GameServer::GameServer(GameServerConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

GameServer::~GameServer() {
    if (impl_ && impl_->gameLoop.isRunning()) {
        stop();
    }
}

GameServer::GameServer(GameServer&&) noexcept = default;
GameServer& GameServer::operator=(GameServer&&) noexcept = default;

// -- Lifecycle ----------------------------------------------------------------

GameResult<void> GameServer::start() {
    if (impl_->gameLoop.isRunning()) {
        return GameResult<void>::err(
            GameError(ErrorCode::GameLoopAlreadyRunning,
                      "game server is already running"));
    }

    impl_->registerStorages();

    if (!impl_->registerSystems()) {
        return GameResult<void>::err(
            GameError(ErrorCode::SystemSchedulerBuildFailed,
                      "failed to build system scheduler: "
                      + impl_->scheduler.GetLastError()));
    }

    impl_->gameLoop.setTickCallback([this](float dt) {
        impl_->scheduler.Execute(dt);
        impl_->entities.FlushDeferred();
    });

    if (!impl_->gameLoop.start()) {
        return GameResult<void>::err(
            GameError(ErrorCode::GameLoopAlreadyRunning,
                      "failed to start game loop"));
    }

    return GameResult<void>::ok();
}

void GameServer::stop() {
    impl_->gameLoop.stop();
}

bool GameServer::isRunning() const noexcept {
    return impl_->gameLoop.isRunning();
}

// -- Manual tick --------------------------------------------------------------

GameResult<void> GameServer::tick() {
    if (impl_->gameLoop.isRunning()) {
        return GameResult<void>::err(
            GameError(ErrorCode::GameLoopAlreadyRunning,
                      "cannot tick manually while game loop is running"));
    }

    // Ensure systems are wired on first manual tick.
    if (impl_->scheduler.SystemCount() == 0) {
        impl_->registerStorages();
        if (!impl_->registerSystems()) {
            return GameResult<void>::err(
                GameError(ErrorCode::SystemSchedulerBuildFailed,
                          "failed to build system scheduler: "
                          + impl_->scheduler.GetLastError()));
        }

        impl_->gameLoop.setTickCallback([this](float dt) {
            impl_->scheduler.Execute(dt);
            impl_->entities.FlushDeferred();
        });
    }

    (void)impl_->gameLoop.tick();
    return GameResult<void>::ok();
}

// -- Instance management ------------------------------------------------------

GameResult<uint32_t> GameServer::createInstance(
    uint32_t mapId, uint32_t maxPlayers) {
    auto result = impl_->instanceManager.createInstance(
        mapId, cgs::game::MapType::OpenWorld, maxPlayers);

    if (result.hasError()) {
        return result;
    }

    // Create a map entity in the ECS world for spatial indexing.
    auto mapEntity = impl_->entities.Create();
    cgs::game::MapInstance comp;
    comp.mapId = mapId;
    comp.instanceId = result.value();
    impl_->mapInstances.Add(mapEntity, comp);

    // Track the mapping for O(1) lookup.
    impl_->instanceEntities.emplace(result.value(), mapEntity);

    return result;
}

GameResult<void> GameServer::destroyInstance(uint32_t instanceId) {
    auto destroyResult = impl_->instanceManager.destroyInstance(instanceId);
    if (destroyResult.hasError()) {
        return destroyResult;
    }

    // Remove the corresponding map entity from ECS.
    auto mapEntity = impl_->findMapEntity(instanceId);
    if (mapEntity.has_value()) {
        impl_->entities.Destroy(*mapEntity);
    }
    impl_->instanceEntities.erase(instanceId);

    return GameResult<void>::ok();
}

std::vector<uint32_t> GameServer::availableInstances(uint32_t mapId) const {
    return impl_->instanceManager.findAvailableInstances(mapId);
}

// -- Player lifecycle ---------------------------------------------------------

GameResult<cgs::ecs::Entity> GameServer::addPlayer(
    PlayerId playerId, uint32_t instanceId) {
    std::lock_guard lock(impl_->playerMutex);

    // Check for duplicate player.
    if (impl_->playerSessions.contains(playerId)) {
        return GameResult<cgs::ecs::Entity>::err(
            GameError(ErrorCode::PlayerAlreadyInWorld,
                      "player is already in the world"));
    }

    // Verify instance exists and can accept players.
    auto instanceInfo = impl_->instanceManager.getInstance(instanceId);
    if (!instanceInfo.has_value()) {
        return GameResult<cgs::ecs::Entity>::err(
            GameError(ErrorCode::MapInstanceNotFound,
                      "map instance not found"));
    }

    if (!impl_->instanceManager.addPlayer(instanceId)) {
        return GameResult<cgs::ecs::Entity>::err(
            GameError(ErrorCode::InstanceFull,
                      "map instance is full or not active"));
    }

    // Find the map entity for this instance.
    auto mapEntity = impl_->findMapEntity(instanceId);
    if (!mapEntity.has_value()) {
        // Rollback the addPlayer counter.
        (void)impl_->instanceManager.removePlayer(instanceId);
        return GameResult<cgs::ecs::Entity>::err(
            GameError(ErrorCode::MapInstanceNotFound,
                      "map entity not found in ECS"));
    }

    // Create the player entity with default components.
    auto entity = impl_->entities.Create();

    cgs::game::Transform transform;
    impl_->transforms.Add(entity, transform);

    cgs::game::Identity identity;
    identity.guid = cgs::game::GenerateGUID();
    identity.type = cgs::game::ObjectType::Player;
    impl_->identities.Add(entity, std::move(identity));

    cgs::game::Stats playerStats;
    playerStats.health = 100;
    playerStats.maxHealth = 100;
    playerStats.mana = 100;
    playerStats.maxMana = 100;
    impl_->stats.Add(entity, playerStats);

    cgs::game::Movement movement;
    movement.baseSpeed = 7.0f;
    movement.speed = 7.0f;
    impl_->movements.Add(entity, movement);

    cgs::game::MapMembership membership;
    membership.mapEntity = *mapEntity;
    impl_->memberships.Add(entity, membership);

    cgs::game::QuestLog questLog;
    impl_->questLogs.Add(entity, std::move(questLog));

    cgs::game::Inventory inventory;
    inventory.Initialize();
    impl_->inventories.Add(entity, std::move(inventory));

    cgs::game::Equipment equip;
    impl_->equipment.Add(entity, std::move(equip));

    // Record player session.
    PlayerSession session;
    session.playerId = playerId;
    session.entity = entity;
    session.instanceId = instanceId;
    impl_->playerSessions.emplace(playerId, session);

    impl_->playersJoined.fetch_add(1, std::memory_order_relaxed);

    return GameResult<cgs::ecs::Entity>::ok(entity);
}

GameResult<void> GameServer::removePlayer(PlayerId playerId) {
    std::lock_guard lock(impl_->playerMutex);

    auto it = impl_->playerSessions.find(playerId);
    if (it == impl_->playerSessions.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PlayerNotInWorld,
                      "player is not in the world"));
    }

    auto session = it->second;
    impl_->playerSessions.erase(it);

    // Release the instance slot.
    (void)impl_->instanceManager.removePlayer(session.instanceId);

    // Destroy the entity (storages auto-cleanup components).
    impl_->entities.Destroy(session.entity);

    impl_->playersLeft.fetch_add(1, std::memory_order_relaxed);

    return GameResult<void>::ok();
}

std::optional<PlayerSession> GameServer::getPlayerSession(
    PlayerId playerId) const {
    std::lock_guard lock(impl_->playerMutex);

    auto it = impl_->playerSessions.find(playerId);
    if (it == impl_->playerSessions.end()) {
        return std::nullopt;
    }
    return it->second;
}

GameResult<void> GameServer::transferPlayer(
    PlayerId playerId, uint32_t targetInstanceId) {
    std::lock_guard lock(impl_->playerMutex);

    auto it = impl_->playerSessions.find(playerId);
    if (it == impl_->playerSessions.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PlayerNotInWorld,
                      "player is not in the world"));
    }

    auto& session = it->second;

    if (session.instanceId == targetInstanceId) {
        return GameResult<void>::ok();  // Already in the target instance.
    }

    // Verify the target instance exists and can accept players.
    if (!impl_->instanceManager.addPlayer(targetInstanceId)) {
        return GameResult<void>::err(
            GameError(ErrorCode::InstanceFull,
                      "target instance is full or not active"));
    }

    auto targetMapEntity = impl_->findMapEntity(targetInstanceId);
    if (!targetMapEntity.has_value()) {
        (void)impl_->instanceManager.removePlayer(targetInstanceId);
        return GameResult<void>::err(
            GameError(ErrorCode::MapInstanceNotFound,
                      "target map entity not found in ECS"));
    }

    // Release the old instance slot.
    (void)impl_->instanceManager.removePlayer(session.instanceId);

    // Update the entity's map membership.
    if (impl_->memberships.Has(session.entity)) {
        auto& membership = impl_->memberships.Get(session.entity);
        membership.mapEntity = *targetMapEntity;
        membership.zoneId = 0;
    }

    session.instanceId = targetInstanceId;

    return GameResult<void>::ok();
}

// -- Statistics ---------------------------------------------------------------

GameServerStats GameServer::stats() const {
    GameServerStats s;

    auto loopMetrics = impl_->gameLoop.lastMetrics();
    s.totalTicks = loopMetrics.tickNumber;
    s.lastUpdateTimeMs = static_cast<float>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            loopMetrics.updateTime).count()) / 1000.0f;
    s.lastBudgetUtilization = loopMetrics.budgetUtilization;

    s.entityCount = impl_->entities.Count();

    {
        std::lock_guard lock(impl_->playerMutex);
        s.playerCount = impl_->playerSessions.size();
    }

    s.activeInstances = impl_->instanceManager.instanceCount(
        InstanceState::Active);
    s.drainingInstances = impl_->instanceManager.instanceCount(
        InstanceState::Draining);

    s.playersJoined = impl_->playersJoined.load(std::memory_order_relaxed);
    s.playersLeft = impl_->playersLeft.load(std::memory_order_relaxed);

    return s;
}

const GameServerConfig& GameServer::config() const noexcept {
    return impl_->config;
}

} // namespace cgs::service
