# Integration Strategy Document

## Merging Four Projects into Common Game Server

**Version**: 0.1.0.0
**Last Updated**: 2026-02-03
**Status**: Draft

---

## 1. Executive Summary

This document outlines the strategy for integrating four game server projects into a unified framework:

| Source Project | Key Contribution | Integration Priority |
|----------------|------------------|---------------------|
| **common_game_server_system (CGSS)** | Standards, specifications, guidelines | Foundation |
| **game_server** | Proven MMORPG game logic | Game Logic Layer |
| **unified_game_server (UGS)** | Modern architecture, ECS, plugins | Core Architecture |
| **game_server_system** | Integration target | Placeholder |

---

## 2. Integration Principles

### 2.1 Core Principles

| Principle | Description | Rationale |
|-----------|-------------|-----------|
| **Standards First** | CGSS specifications as the foundation | Consistency |
| **Architecture from UGS** | ECS + microservices from UGS | Scalability |
| **Logic from game_server** | Port proven game mechanics | Reliability |
| **Adapter Pattern** | Isolate foundation from game logic | Maintainability |
| **Plugin Architecture** | Game-specific code as plugins | Extensibility |

### 2.2 Integration Decision Matrix

| Component | Source | Reason |
|-----------|--------|--------|
| 7 Foundation Systems | CGSS | Standardized, well-documented |
| Error handling (Result<T,E>) | CGSS | Type-safe error propagation |
| ECS implementation | UGS | Cache-friendly, scalable |
| Plugin system | UGS | Supports multiple game genres |
| Microservices | UGS | Cloud-native scaling |
| Object/Unit system | game_server | Battle-tested MMORPG logic |
| Combat system | game_server | Proven mechanics |
| World/Map system | game_server | Grid-based spatial management |
| AI/Pathfinding | game_server | Working implementation |
| Foundation Adapters | UGS + game_server | Combined best practices |

---

## 3. Detailed Integration Plan

### 3.1 Phase 1: Foundation Integration (Weeks 1-4)

#### 3.1.1 CGSS Integration

**Objective**: Establish foundation layer with 7 systems

```
Tasks:
├── Import common_system
│   ├── Result<T, E> pattern
│   ├── ServiceContainer (DI)
│   ├── EventBus
│   └── Configuration management
│
├── Import thread_system
│   ├── TypedThreadPool
│   ├── Job scheduling
│   └── Metrics integration
│
├── Import logger_system
│   ├── Structured logging
│   ├── Log levels and filtering
│   └── Multiple sinks
│
├── Import network_system
│   ├── TCP/UDP/WebSocket servers
│   ├── Session management
│   └── Protocol handling
│
├── Import database_system
│   ├── Connection pooling
│   ├── Query builder
│   └── Transaction support
│
├── Import container_system
│   ├── Message serialization
│   ├── Type-safe containers
│   └── Binary protocols
│
└── Import monitoring_system
    ├── Metrics collection
    ├── Distributed tracing
    └── Health checks
```

**Deliverables**:
- [ ] Foundation systems integrated and building
- [ ] Unit tests passing for all systems
- [ ] Benchmark suite operational
- [ ] Documentation updated

#### 3.1.2 Foundation Adapter Layer

**Objective**: Create game-specific wrappers for foundation systems

```cpp
// Directory: include/game/foundation/

// game_logger.hpp
class GameLogger {
public:
    // Game-specific logging methods
    void log_player_login(PlayerId id, const std::string& ip);
    void log_player_logout(PlayerId id, LogoutReason reason);
    void log_combat_damage(EntityId source, EntityId target, int damage);
    void log_item_transaction(PlayerId player, ItemId item, TransactionType type);
    void log_world_event(WorldEventType type, const WorldEventData& data);

private:
    std::shared_ptr<kcenon::logger::Logger> logger_;
    LogContext context_;
};

// game_network.hpp
class GameNetwork {
public:
    // Game protocol handling
    Result<void> send_packet(SessionId session, const GamePacket& packet);
    Result<void> broadcast_to_map(MapId map, const GamePacket& packet);
    Result<void> broadcast_to_zone(ZoneId zone, const GamePacket& packet);
    Result<void> multicast(const std::vector<SessionId>& sessions, const GamePacket& packet);

    // Session management
    Result<SessionId> create_session(PlayerId player);
    Result<void> destroy_session(SessionId session);
    SessionInfo get_session_info(SessionId session);

private:
    std::shared_ptr<kcenon::network::TcpServer> server_;
    SessionManager sessions_;
    PacketRouter router_;
};

// game_database.hpp
class GameDatabase {
public:
    // Player operations
    Result<PlayerData> load_player(PlayerId id);
    Result<void> save_player(const PlayerData& data);
    Result<std::vector<CharacterSummary>> get_character_list(AccountId account);

    // Inventory operations
    Result<Inventory> load_inventory(CharacterId character);
    Result<void> save_inventory(CharacterId character, const Inventory& inventory);

    // World operations
    Result<MapData> load_map(MapId id);
    Result<std::vector<SpawnPoint>> load_spawn_points(MapId map);

private:
    std::shared_ptr<kcenon::database::Database> db_;
    QueryCache cache_;
};

// game_thread_pool.hpp
class GameThreadPool {
public:
    // Game-specific job scheduling
    JobHandle schedule_tick(WorldId world, TickFunction func);
    JobHandle schedule_player_action(PlayerId player, ActionFunction func);
    JobHandle schedule_ai_update(EntityId entity, AIFunction func);
    JobHandle schedule_background(BackgroundFunction func);

    // Priority management
    void set_tick_priority(Priority priority);
    void set_action_priority(Priority priority);

private:
    std::shared_ptr<kcenon::thread::TypedThreadPool> pool_;
    PriorityMapper priorities_;
};

// game_monitor.hpp
class GameMonitor {
public:
    // Game metrics
    void record_online_players(int count);
    void record_tick_duration(WorldId world, std::chrono::microseconds duration);
    void record_message_processed(MessageType type);
    void record_database_query(QueryType type, std::chrono::microseconds duration);
    void record_combat_action(CombatActionType type);

    // Tracing
    TraceContext start_trace(const std::string& operation);
    void end_trace(TraceContext& ctx);

private:
    std::shared_ptr<kcenon::monitoring::Metrics> metrics_;
    std::shared_ptr<kcenon::monitoring::Tracer> tracer_;
};
```

**Integration Pattern**:

```cpp
// Adapter isolates game code from foundation changes
// If foundation API changes, only adapter needs update

// Before (tight coupling)
void Player::save() {
    kcenon::database::Connection conn = db_.get_connection();
    conn.execute("INSERT INTO players...", this->to_row());
}

// After (loose coupling via adapter)
void Player::save() {
    game_db_.save_player(this->to_player_data());
}
```

### 3.2 Phase 2: Core ECS Integration (Weeks 5-8)

#### 3.2.1 UGS ECS Import

**Objective**: Integrate Entity-Component System from unified_game_server

```
Tasks:
├── Import ECS Core
│   ├── Entity management
│   ├── Component storage (SparseSet)
│   ├── System runner
│   └── World container
│
├── Adapt to Foundation
│   ├── Use thread_system for parallel updates
│   ├── Integrate with monitoring_system
│   └── Add logging via logger_system
│
└── Create Base Components
    ├── TransformComponent
    ├── IdentityComponent
    ├── NetworkComponent
    └── LifecycleComponent
```

**ECS Integration with Foundation**:

```cpp
// ECS uses foundation systems through adapters
class ECSWorld {
public:
    ECSWorld(GameFoundation& foundation)
        : foundation_(foundation)
        , entity_manager_()
        , system_runner_(foundation.thread_pool())
    {
        // Initialize with foundation services
        system_runner_.set_logger(foundation.logger());
        system_runner_.set_metrics(foundation.monitor());
    }

    void update(float delta_time) {
        auto trace = foundation_.monitor().start_trace("world_update");

        // Run systems using thread_pool for parallelism
        system_runner_.update(*this, delta_time);

        foundation_.monitor().record_tick_duration(
            id_, trace.elapsed()
        );
    }

private:
    GameFoundation& foundation_;
    EntityManager entity_manager_;
    SystemRunner system_runner_;
};
```

#### 3.2.2 Hybrid Object-ECS Bridge

**Objective**: Allow gradual migration from OOP to ECS

```cpp
// Bridge allows game_server objects to work with ECS
namespace game::bridge {

// Wrapper to use legacy Object in ECS
struct LegacyObjectComponent {
    std::shared_ptr<legacy::Object> object;
};

// System that updates legacy objects
class LegacyObjectSystem : public ecs::System {
public:
    void update(ecs::World& world, float delta_time) override {
        for (auto entity : world.view<LegacyObjectComponent>()) {
            auto& comp = world.get<LegacyObjectComponent>(entity);
            comp.object->Update(delta_time);
        }
    }
};

// Helper to convert legacy object to ECS entity
EntityId convert_to_ecs(ecs::World& world, legacy::Object* obj) {
    EntityId entity = world.create_entity();

    // Add transform component from object position
    auto& transform = world.add<TransformComponent>(entity);
    transform.position = obj->GetPosition();
    transform.rotation = obj->GetRotation();

    // Keep legacy object for gradual migration
    auto& legacy = world.add<LegacyObjectComponent>(entity);
    legacy.object = obj->shared_from_this();

    return entity;
}

}
```

### 3.3 Phase 3: Game Logic Integration (Weeks 9-14)

#### 3.3.1 Object System Port

**Objective**: Port game_server Object/Unit/Player systems to ECS

```cpp
// Original game_server structure (OOP)
class Object {
    ObjectGUID guid_;
    ObjectType type_;
    virtual void Update(float dt);
};

class Unit : public Object {
    UnitStats stats_;
    Faction faction_;
    virtual void Update(float dt) override;
};

class Player : public Unit {
    AccountId account_;
    virtual void Update(float dt) override;
};

// Ported to ECS Components
struct ObjectComponent {
    ObjectGUID guid;
    ObjectType type;
    std::string name;
};

struct UnitComponent {
    UnitStats stats;
    Faction faction;
    UnitFlags flags;
};

struct PlayerComponent {
    AccountId account_id;
    CharacterId character_id;
    SessionId session_id;
    PlayerFlags flags;
};

struct CreatureComponent {
    CreatureTemplateId template_id;
    SpawnId spawn_id;
    RespawnTimer respawn;
};

// ECS Systems replace virtual Update methods
class UnitUpdateSystem : public ecs::System {
public:
    void update(ecs::World& world, float delta_time) override {
        for (auto entity : world.view<UnitComponent, TransformComponent>()) {
            auto& unit = world.get<UnitComponent>(entity);
            auto& transform = world.get<TransformComponent>(entity);

            // Port logic from Unit::Update
            update_unit(unit, transform, delta_time);
        }
    }
};
```

**Migration Strategy**:

| Phase | Approach | Risk |
|-------|----------|------|
| 1. Wrapper | Wrap legacy objects in ECS | Low |
| 2. Parallel | Run both systems, compare results | Medium |
| 3. Replace | Replace legacy with pure ECS | Medium |
| 4. Remove | Remove legacy code | Low |

#### 3.3.2 Combat System Port

**Objective**: Port combat mechanics to ECS

```cpp
// Combat Components (from game_server mechanics)
struct CombatStateComponent {
    bool in_combat;
    EntityId current_target;
    std::chrono::steady_clock::time_point combat_start;
    std::chrono::steady_clock::time_point last_attack;
};

struct ThreatComponent {
    std::map<EntityId, float> threat_list;
    EntityId highest_threat;

    void add_threat(EntityId source, float amount);
    void remove_threat(EntityId source);
    EntityId get_top_threat() const;
};

struct DamageComponent {
    // Pending damage to apply
    struct PendingDamage {
        EntityId source;
        DamageType type;
        int amount;
        SpellId spell_id;  // 0 if melee
    };
    std::vector<PendingDamage> pending;
};

struct SpellCastComponent {
    SpellId spell_id;
    EntityId target;
    CastState state;  // PREPARING, CASTING, FINISHED, INTERRUPTED
    float cast_time;
    float elapsed;
};

struct AuraComponent {
    struct ActiveAura {
        AuraId id;
        EntityId caster;
        int stacks;
        float remaining_duration;
        std::vector<AuraEffect> effects;
    };
    std::vector<ActiveAura> auras;
};

// Combat Systems
class CombatStateSystem : public ecs::System {
    void update(ecs::World& world, float delta_time) override;
};

class ThreatSystem : public ecs::System {
    void update(ecs::World& world, float delta_time) override;
};

class DamageSystem : public ecs::System {
    void update(ecs::World& world, float delta_time) override;
};

class SpellCastSystem : public ecs::System {
    void update(ecs::World& world, float delta_time) override;
};

class AuraSystem : public ecs::System {
    void update(ecs::World& world, float delta_time) override;
};
```

#### 3.3.3 World System Port

**Objective**: Port Map/Zone/Grid systems to ECS

```cpp
// World Components
struct MapComponent {
    MapId id;
    std::string name;
    MapType type;
    Vector2 bounds;
};

struct ZoneComponent {
    ZoneId id;
    MapId map_id;
    std::string name;
    ZoneType type;
    Rectangle bounds;
};

struct GridCellComponent {
    GridCoord coord;
    MapId map_id;
    std::set<EntityId> entities;
};

struct PositionComponent {
    MapId map_id;
    ZoneId zone_id;
    GridCoord grid;
    Vector3 position;
    float orientation;
};

struct VisibilityComponent {
    float visibility_range;
    std::set<EntityId> visible_entities;
    std::set<EntityId> known_entities;  // Were visible recently
};

// World Systems
class GridUpdateSystem : public ecs::System {
    // Update entity grid positions when they move
    void update(ecs::World& world, float delta_time) override;
};

class VisibilitySystem : public ecs::System {
    // Update what each entity can see
    void update(ecs::World& world, float delta_time) override;
};

class ZoneTransitionSystem : public ecs::System {
    // Handle entities moving between zones
    void update(ecs::World& world, float delta_time) override;
};
```

### 3.4 Phase 4: Services Integration (Weeks 15-18)

#### 3.4.1 UGS Microservices Import

**Objective**: Integrate microservices architecture from UGS

```cpp
// Service base class
class GameService {
public:
    virtual ~GameService() = default;

    virtual Result<void> start() = 0;
    virtual Result<void> stop() = 0;
    virtual ServiceStatus status() const = 0;
    virtual std::string_view name() const = 0;

protected:
    GameFoundation& foundation_;
    ServiceConfig config_;
};

// Auth Service
class AuthService : public GameService {
public:
    Result<LoginResponse> login(const LoginRequest& request);
    Result<void> logout(SessionToken token);
    Result<TokenInfo> validate_token(SessionToken token);
    Result<SessionToken> refresh_token(SessionToken old_token);

private:
    TokenManager tokens_;
    AccountRepository accounts_;
};

// Gateway Service
class GatewayService : public GameService {
public:
    void on_client_connect(ConnectionId conn);
    void on_client_disconnect(ConnectionId conn);
    void on_client_message(ConnectionId conn, const Message& msg);

    Result<void> route_to_game(SessionId session, const GameMessage& msg);
    Result<void> route_to_lobby(SessionId session, const LobbyMessage& msg);

private:
    ConnectionManager connections_;
    ServiceRouter router_;
    LoadBalancer balancer_;
};

// Game Service
class GameServerService : public GameService {
public:
    Result<void> start() override;
    Result<void> stop() override;

    void on_player_enter(PlayerId player, WorldId world);
    void on_player_leave(PlayerId player);
    void on_player_input(PlayerId player, const PlayerInput& input);

private:
    std::vector<std::unique_ptr<ecs::World>> worlds_;
    InputQueue input_queue_;
    TickScheduler tick_scheduler_;
};

// Lobby Service
class LobbyService : public GameService {
public:
    Result<void> join_matchmaking(PlayerId player, const MatchCriteria& criteria);
    Result<void> leave_matchmaking(PlayerId player);
    Result<MatchInfo> get_match_info(MatchId match);

    Result<PartyId> create_party(PlayerId leader);
    Result<void> invite_to_party(PartyId party, PlayerId invitee);
    Result<void> leave_party(PlayerId player);

private:
    MatchmakingQueue queue_;
    PartyManager parties_;
};
```

### 3.5 Phase 5: Plugin System Integration (Weeks 19-22)

#### 3.5.1 Plugin Architecture

**Objective**: Enable game-specific logic as loadable plugins

```cpp
// Plugin interface
class GamePlugin {
public:
    virtual ~GamePlugin() = default;

    // Lifecycle
    virtual Result<void> on_load(PluginContext& ctx) = 0;
    virtual Result<void> on_unload() = 0;
    virtual Result<void> on_enable(ecs::World& world) = 0;
    virtual Result<void> on_disable(ecs::World& world) = 0;

    // Metadata
    virtual PluginInfo info() const = 0;

    // Optional hooks
    virtual void on_tick(ecs::World& world, float delta_time) {}
    virtual void on_player_join(ecs::World& world, EntityId player) {}
    virtual void on_player_leave(ecs::World& world, EntityId player) {}
};

// Plugin context provides access to framework
class PluginContext {
public:
    GameFoundation& foundation();
    PluginManager& plugins();
    EventBus& events();

    template<typename T>
    void register_component();

    template<typename T>
    void register_system();

    void register_packet_handler(PacketType type, PacketHandler handler);
    void register_command(const std::string& name, CommandHandler handler);
};

// MMORPG Plugin (combines game_server logic)
class MMORPGPlugin : public GamePlugin {
public:
    Result<void> on_load(PluginContext& ctx) override {
        // Register MMORPG-specific components
        ctx.register_component<CharacterComponent>();
        ctx.register_component<InventoryComponent>();
        ctx.register_component<QuestLogComponent>();
        ctx.register_component<SkillBarComponent>();
        ctx.register_component<GuildMemberComponent>();
        ctx.register_component<AchievementComponent>();

        // Register MMORPG systems
        ctx.register_system<CharacterProgressionSystem>();
        ctx.register_system<InventorySystem>();
        ctx.register_system<QuestSystem>();
        ctx.register_system<SkillSystem>();
        ctx.register_system<GuildSystem>();
        ctx.register_system<AchievementSystem>();

        // Register packet handlers
        ctx.register_packet_handler(CMSG_CAST_SPELL, &handle_cast_spell);
        ctx.register_packet_handler(CMSG_USE_ITEM, &handle_use_item);
        ctx.register_packet_handler(CMSG_QUEST_ACCEPT, &handle_quest_accept);

        return Result<void>::ok();
    }

    PluginInfo info() const override {
        return {
            .name = "mmorpg",
            .version = "1.0.0",
            .description = "MMORPG game mechanics",
            .dependencies = {}
        };
    }
};
```

### 3.6 Phase 6: Production Readiness (Weeks 23-26)

#### 3.6.1 Deployment Integration

```yaml
# Kubernetes deployment combining all services
# deployment/kubernetes/

# Gateway deployment
apiVersion: apps/v1
kind: Deployment
metadata:
  name: gateway
spec:
  replicas: 3
  template:
    spec:
      containers:
      - name: gateway
        image: common-game-server/gateway:1.0.0
        resources:
          requests:
            cpu: "500m"
            memory: "512Mi"
          limits:
            cpu: "2000m"
            memory: "2Gi"

# Game server stateful set
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: game-server
spec:
  serviceName: game-server
  replicas: 5
  template:
    spec:
      containers:
      - name: game
        image: common-game-server/game:1.0.0
        volumeMounts:
        - name: world-state
          mountPath: /data/world
```

---

## 4. Risk Mitigation

### 4.1 Technical Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| ECS performance regression | High | Medium | Continuous benchmarking, fallback to hybrid |
| Foundation API changes | Medium | Low | Adapter layer isolation |
| Plugin compatibility issues | Medium | Medium | Strict versioning, compatibility tests |
| Integration complexity | High | Medium | Incremental migration, feature flags |

### 4.2 Fallback Strategies

```cpp
// Feature flags for gradual rollout
namespace config {
    bool use_ecs_combat = false;      // Toggle ECS combat system
    bool use_ecs_movement = true;     // Toggle ECS movement
    bool enable_parallel_systems = false;  // Toggle parallel execution
}

// Hybrid execution based on flags
void WorldTick::update(float delta_time) {
    if (config::use_ecs_movement) {
        movement_system_.update(world_, delta_time);
    } else {
        legacy_movement_manager_.Update(delta_time);
    }

    if (config::use_ecs_combat) {
        combat_system_.update(world_, delta_time);
    } else {
        legacy_combat_manager_.Update(delta_time);
    }
}
```

---

## 5. Success Criteria

### 5.1 Phase Completion Criteria

| Phase | Criteria |
|-------|----------|
| **Phase 1** | All 7 foundation systems integrated, adapters complete, tests passing |
| **Phase 2** | ECS operational, 10K entities @ <5ms, basic systems working |
| **Phase 3** | Object/Combat/World systems ported, feature parity with game_server |
| **Phase 4** | All microservices operational, inter-service communication working |
| **Phase 5** | MMORPG plugin complete, plugin API stable |
| **Phase 6** | K8s deployment working, 10K CCU load test passed |

### 5.2 Quality Gates

| Gate | Requirement |
|------|-------------|
| Code Coverage | ≥80% |
| Performance | Meet all NFR targets |
| Security | Pass security audit |
| Documentation | All public APIs documented |

---

## 6. Appendices

### 6.1 Source Code Mapping

| Target Directory | Source | Content |
|------------------|--------|---------|
| `src/foundation/` | New | Foundation adapters |
| `src/core/ecs/` | UGS | ECS implementation |
| `src/game/object/` | game_server | Object system (ported) |
| `src/game/combat/` | game_server | Combat system (ported) |
| `src/game/world/` | game_server | World system (ported) |
| `src/services/` | UGS | Microservices |
| `src/plugins/mmorpg/` | game_server + UGS | MMORPG plugin |

### 6.2 Related Documents

- [ARCHITECTURE.md](./ARCHITECTURE.md) - System architecture
- [ROADMAP.md](./ROADMAP.md) - Implementation timeline
- [reference/PROJECT_ANALYSIS.md](./reference/PROJECT_ANALYSIS.md) - Source analysis

---

*Document Version*: 1.0.0
*Status*: Draft
