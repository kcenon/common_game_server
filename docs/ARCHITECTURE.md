# Architecture Design Document

## Common Game Server - Unified Architecture

**Version**: 0.1.0.0
**Last Updated**: 2026-02-03
**Status**: Draft

---

## 1. Overview

### 1.1 Purpose

This document defines the unified architecture for the Common Game Server framework, combining the strengths of four source projects into a cohesive, production-ready system.

### 1.2 Scope

- System architecture and component design
- Layer definitions and responsibilities
- Integration patterns and data flow
- Deployment architecture

### 1.3 Design Principles

| Principle | Description | Application |
|-----------|-------------|-------------|
| **Modularity** | Separate concerns into distinct layers | 6-layer architecture |
| **Extensibility** | Support new game types without core changes | Plugin system |
| **Performance** | Optimize for game server workloads | ECS, cache-friendly design |
| **Reliability** | Graceful degradation, fault tolerance | Microservices, health checks |
| **Testability** | Easy to test in isolation | Dependency injection, adapters |

---

## 2. System Architecture

### 2.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         COMMON GAME SERVER                               │
├─────────────────────────────────────────────────────────────────────────┤
│  Layer 6: PLUGIN LAYER                                                   │
│  ┌───────────────┐ ┌───────────────┐ ┌───────────────┐ ┌─────────────┐  │
│  │ MMORPG Plugin │ │ Battle Royale │ │  RTS Plugin   │ │Custom Plugin│  │
│  │               │ │    Plugin     │ │               │ │             │  │
│  │ - Character   │ │ - Zone Shrink │ │ - Unit Control│ │ - Custom    │  │
│  │ - Combat      │ │ - Loot System │ │ - Base Build  │ │   Systems   │  │
│  │ - Quest       │ │ - Spectate    │ │ - Resource    │ │             │  │
│  │ - Guild       │ │               │ │               │ │             │  │
│  └───────────────┘ └───────────────┘ └───────────────┘ └─────────────┘  │
├─────────────────────────────────────────────────────────────────────────┤
│  Layer 5: GAME LOGIC LAYER (from game_server)                           │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │ Object System │ Combat System │ World System │ AI System        │    │
│  │ ────────────  │ ────────────  │ ────────────  │ ──────────       │    │
│  │ - Object      │ - Spell       │ - Map         │ - Brain          │    │
│  │ - Unit        │ - Aura        │ - Zone        │ - BehaviorTree   │    │
│  │ - Player      │ - Damage      │ - Grid        │ - Pathfinding    │    │
│  │ - Creature    │ - Threat      │ - Visibility  │ - Movement       │    │
│  │ - GameObject  │ - Combat Log  │ - Terrain     │                  │    │
│  └─────────────────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────────────────┤
│  Layer 4: CORE ECS LAYER                                                │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐       │
│  │   Entity    │ │  Component  │ │   System    │ │    World    │       │
│  │   Manager   │ │   Storage   │ │   Runner    │ │   Manager   │       │
│  │ ──────────  │ │ ──────────  │ │ ──────────  │ │ ──────────  │       │
│  │ - Create    │ │ - SparseSet │ │ - Schedule  │ │ - Multiple  │       │
│  │ - Destroy   │ │ - Archetype │ │ - Execute   │ │   Worlds    │       │
│  │ - Query     │ │ - ComponentPool│ - Parallel │ │ - Transfer  │       │
│  │ - Recycle   │ │             │ │             │ │             │       │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘       │
├─────────────────────────────────────────────────────────────────────────┤
│  Layer 3: SERVICE LAYER                                                  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐      │
│  │   Auth   │ │ Gateway  │ │   Game   │ │  Lobby   │ │ DBProxy  │      │
│  │ Service  │ │ Service  │ │ Service  │ │ Service  │ │ Service  │      │
│  │ ──────── │ │ ──────── │ │ ──────── │ │ ──────── │ │ ──────── │      │
│  │ - Login  │ │ - Route  │ │ - World  │ │ - Match  │ │ - Pool   │      │
│  │ - Token  │ │ - Balance│ │ - Tick   │ │ - Party  │ │ - Query  │      │
│  │ - Session│ │ - Protocol│ │ - State │ │ - Chat   │ │ - Cache  │      │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘      │
├─────────────────────────────────────────────────────────────────────────┤
│  Layer 2: FOUNDATION ADAPTER LAYER                                      │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │ GameLogger │ GameNetwork │ GameDatabase │ GameThread │ GameMonitor│  │
│  │ ────────── │ ──────────  │ ────────────  │ ──────────  │ ──────── │  │
│  │ Adapts     │ Adapts      │ Adapts        │ Adapts      │ Adapts   │  │
│  │ logger_    │ network_    │ database_     │ thread_     │ monitor_ │  │
│  │ system     │ system      │ system        │ system      │ system   │  │
│  └───────────────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────────────┤
│  Layer 1: FOUNDATION LAYER (7 Systems)                                  │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐           │
│  │ common  │ │ thread  │ │ logger  │ │ network │ │database │           │
│  │ _system │ │ _system │ │ _system │ │ _system │ │ _system │           │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘           │
│  ┌─────────┐ ┌─────────┐                                                │
│  │container│ │monitoring                                                │
│  │ _system │ │ _system │                                                │
│  └─────────┘ └─────────┘                                                │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Layer Responsibilities

| Layer | Responsibility |
|-------|---------------|
| **L1: Foundation** | Core infrastructure (logging, network, DB) |
| **L2: Adapter** | Game-specific wrappers for foundation |
| **L3: Service** | Microservices for game operations |
| **L4: Core ECS** | Entity-Component System runtime |
| **L5: Game Logic** | Reusable game systems (combat, world) |
| **L6: Plugin** | Game-specific implementations |

---

## 3. Component Design

### 3.1 Foundation Layer (Layer 1)

Seven foundation systems providing core infrastructure:

```cpp
// All systems follow consistent patterns
namespace foundation {

// common_system - Base types, Result<T,E>, DI
using Result = kcenon::common::Result<T, Error>;
using ServiceContainer = kcenon::common::ServiceContainer;

// thread_system - Job scheduling
using ThreadPool = kcenon::thread::TypedThreadPool;
using Job = kcenon::thread::Job;

// logger_system - Structured logging
using Logger = kcenon::logger::Logger;
using LogLevel = kcenon::logger::LogLevel;

// network_system - Network I/O
using TcpServer = kcenon::network::TcpServer;
using Session = kcenon::network::Session;

// database_system - Database access
using Database = kcenon::database::Database;
using Connection = kcenon::database::Connection;

// container_system - Serialization
using Container = kcenon::container::Container;

// monitoring_system - Metrics and tracing
using Metrics = kcenon::monitoring::Metrics;
using Tracer = kcenon::monitoring::Tracer;

}
```

### 3.2 Foundation Adapter Layer (Layer 2)

Adapters provide game-specific interfaces to foundation systems:

```cpp
namespace game::foundation {

// GameLogger - Game-aware logging
class GameLogger {
public:
    void log_player_action(PlayerId id, const std::string& action);
    void log_combat_event(const CombatEvent& event);
    void log_world_event(const WorldEvent& event);

private:
    kcenon::logger::Logger& logger_;
};

// GameNetwork - Game protocol handling
class GameNetwork {
public:
    Result<void> send_to_player(PlayerId id, const Packet& packet);
    Result<void> broadcast_to_zone(ZoneId id, const Packet& packet);
    Result<void> multicast_to_players(const std::vector<PlayerId>& ids, const Packet& packet);

private:
    kcenon::network::TcpServer& server_;
    SessionManager& sessions_;
};

// GameDatabase - Game data access
class GameDatabase {
public:
    Result<Player> load_player(PlayerId id);
    Result<void> save_player(const Player& player);
    Result<std::vector<Item>> load_inventory(PlayerId id);

private:
    kcenon::database::Database& db_;
};

// GameThreadPool - Game job scheduling
class GameThreadPool {
public:
    void schedule_world_tick(WorldId id, std::function<void()> tick);
    void schedule_player_action(PlayerId id, std::function<void()> action);
    void schedule_background_task(std::function<void()> task);

private:
    kcenon::thread::TypedThreadPool& pool_;
};

// GameMonitor - Game metrics
class GameMonitor {
public:
    void record_player_count(int count);
    void record_tick_duration(std::chrono::microseconds duration);
    void record_message_processed();

private:
    kcenon::monitoring::Metrics& metrics_;
};

}
```

### 3.3 Service Layer (Layer 3)

Microservices handling game operations:

```cpp
namespace game::services {

// AuthService - Authentication and sessions
class AuthService {
public:
    Result<SessionToken> login(const Credentials& creds);
    Result<void> logout(SessionToken token);
    Result<SessionInfo> validate_token(SessionToken token);
    Result<void> refresh_token(SessionToken& token);
};

// GatewayService - Client routing and load balancing
class GatewayService {
public:
    Result<void> route_message(SessionId session, const Message& msg);
    Result<ServiceEndpoint> get_game_server(SessionId session);
    void on_client_connected(SessionId session);
    void on_client_disconnected(SessionId session);
};

// GameService - World simulation
class GameService {
public:
    void start_world_loop();
    void stop_world_loop();
    Result<void> process_player_input(PlayerId id, const Input& input);
    WorldState get_world_state();
};

// LobbyService - Matchmaking and social
class LobbyService {
public:
    Result<void> join_queue(PlayerId id, const MatchCriteria& criteria);
    Result<void> leave_queue(PlayerId id);
    Result<MatchResult> create_match(const std::vector<PlayerId>& players);
};

// DBProxyService - Database connection pooling
class DBProxyService {
public:
    Result<QueryResult> execute_query(const Query& query);
    Result<void> execute_batch(const std::vector<Query>& queries);
    ConnectionStats get_stats();
};

}
```

### 3.4 Core ECS Layer (Layer 4)

Entity-Component System for game logic:

```cpp
namespace game::ecs {

// Entity - Unique identifier
using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY = 0;

// Component - Data storage (examples)
struct TransformComponent {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;
};

struct HealthComponent {
    int32_t max_health;
    int32_t current_health;
    bool is_dead() const { return current_health <= 0; }
};

struct MovementComponent {
    Vector3 velocity;
    float speed;
    bool is_moving;
};

// System - Logic processing
class System {
public:
    virtual ~System() = default;
    virtual void update(World& world, float delta_time) = 0;
    virtual std::string_view name() const = 0;
};

// World - Entity container
class World {
public:
    EntityId create_entity();
    void destroy_entity(EntityId id);
    bool is_valid(EntityId id) const;

    template<typename T>
    T& add_component(EntityId id);

    template<typename T>
    T& get_component(EntityId id);

    template<typename T>
    bool has_component(EntityId id) const;

    template<typename... Components>
    auto view() -> View<Components...>;
};

// SystemRunner - Executes systems
class SystemRunner {
public:
    void add_system(std::unique_ptr<System> system);
    void remove_system(std::string_view name);
    void update(World& world, float delta_time);
    void set_parallel_execution(bool enabled);
};

}
```

### 3.5 Game Logic Layer (Layer 5)

Reusable game systems ported from game_server:

```cpp
namespace game::logic {

// Object System Components
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
    PlayerFlags flags;
};

struct CreatureComponent {
    CreatureTemplate template_id;
    AIBrain brain;
    SpawnInfo spawn;
};

// Combat System Components
struct CombatComponent {
    bool in_combat;
    EntityId target;
    ThreatList threat_list;
};

struct SpellCastComponent {
    SpellId spell_id;
    EntityId target;
    CastState state;
    float cast_time_remaining;
};

struct AuraComponent {
    std::vector<Aura> active_auras;
};

// World System Components
struct PositionComponent {
    MapId map_id;
    ZoneId zone_id;
    GridCoord grid;
    Vector3 position;
    float orientation;
};

struct VisibilityComponent {
    std::set<EntityId> visible_entities;
    float visibility_range;
};

// Game Systems (ECS Systems)
class MovementSystem : public ecs::System {
    void update(ecs::World& world, float delta_time) override;
};

class CombatSystem : public ecs::System {
    void update(ecs::World& world, float delta_time) override;
};

class SpellSystem : public ecs::System {
    void update(ecs::World& world, float delta_time) override;
};

class AISystem : public ecs::System {
    void update(ecs::World& world, float delta_time) override;
};

class VisibilitySystem : public ecs::System {
    void update(ecs::World& world, float delta_time) override;
};

}
```

### 3.6 Plugin Layer (Layer 6)

Game-specific implementations:

```cpp
namespace game::plugins {

// Plugin Interface
class GamePlugin {
public:
    virtual ~GamePlugin() = default;

    // Lifecycle
    virtual Result<void> on_load() = 0;
    virtual Result<void> on_init(ecs::World& world) = 0;
    virtual void on_update(ecs::World& world, float delta_time) = 0;
    virtual void on_shutdown() = 0;

    // Metadata
    virtual std::string_view name() const = 0;
    virtual std::string_view version() const = 0;
    virtual std::vector<std::string_view> dependencies() const = 0;
};

// Plugin Manager
class PluginManager {
public:
    Result<void> load_plugin(const std::filesystem::path& path);
    Result<void> unload_plugin(std::string_view name);
    GamePlugin* get_plugin(std::string_view name);
    std::vector<GamePlugin*> get_all_plugins();
};

// MMORPG Plugin Example
class MMORPGPlugin : public GamePlugin {
public:
    Result<void> on_load() override {
        // Register components
        register_component<CharacterComponent>();
        register_component<InventoryComponent>();
        register_component<QuestLogComponent>();
        register_component<GuildMemberComponent>();
        return Result<void>::ok();
    }

    Result<void> on_init(ecs::World& world) override {
        // Register systems
        world.add_system<CharacterSystem>();
        world.add_system<InventorySystem>();
        world.add_system<QuestSystem>();
        world.add_system<GuildSystem>();
        return Result<void>::ok();
    }

    std::string_view name() const override { return "mmorpg"; }
    std::string_view version() const override { return "1.0.0"; }
};

}
```

---

## 4. Data Flow

### 4.1 Client Message Flow

```
┌────────┐     ┌─────────┐     ┌────────┐     ┌──────────┐     ┌─────────┐
│ Client │────>│ Gateway │────>│  Auth  │────>│   Game   │────>│   ECS   │
│        │     │ Service │     │ Service│     │  Service │     │  World  │
└────────┘     └─────────┘     └────────┘     └──────────┘     └─────────┘
    │               │               │               │               │
    │  1. Connect   │               │               │               │
    │──────────────>│               │               │               │
    │               │ 2. Validate   │               │               │
    │               │──────────────>│               │               │
    │               │   Session     │               │               │
    │               │<──────────────│               │               │
    │               │               │               │               │
    │  3. Input     │               │               │               │
    │──────────────>│ 4. Route      │               │               │
    │               │──────────────────────────────>│               │
    │               │               │               │ 5. Process    │
    │               │               │               │──────────────>│
    │               │               │               │               │
    │               │               │               │<──────────────│
    │               │               │               │ 6. State      │
    │<──────────────────────────────────────────────│               │
    │  7. Update    │               │               │               │
```

### 4.2 World Tick Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                       WORLD TICK (50ms)                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────┐                                                │
│  │ Input Phase │ ← Process queued player inputs                 │
│  └──────┬──────┘                                                │
│         │                                                        │
│         v                                                        │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              System Update Phase (Parallel)              │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐   │    │
│  │  │ Movement │ │   AI     │ │  Spell   │ │  Aura    │   │    │
│  │  │  System  │ │  System  │ │  System  │ │  System  │   │    │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘   │    │
│  └─────────────────────────────────────────────────────────┘    │
│         │                                                        │
│         v                                                        │
│  ┌─────────────┐                                                │
│  │Combat Phase │ ← Resolve combat, apply damage                 │
│  └──────┬──────┘                                                │
│         │                                                        │
│         v                                                        │
│  ┌─────────────┐                                                │
│  │Visibility   │ ← Update what each player can see             │
│  │Phase        │                                                │
│  └──────┬──────┘                                                │
│         │                                                        │
│         v                                                        │
│  ┌─────────────┐                                                │
│  │Output Phase │ ← Send state updates to clients                │
│  └─────────────┘                                                │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5. Deployment Architecture

### 5.1 Kubernetes Deployment

```yaml
┌─────────────────────────────────────────────────────────────────┐
│                     Kubernetes Cluster                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ Ingress Controller (nginx/traefik)                       │    │
│  │ - TLS termination                                        │    │
│  │ - WebSocket support                                      │    │
│  │ - Rate limiting                                          │    │
│  └────────────────────────┬────────────────────────────────┘    │
│                           │                                      │
│           ┌───────────────┼───────────────┐                     │
│           │               │               │                     │
│           v               v               v                     │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐                │
│  │  Gateway   │  │  Gateway   │  │  Gateway   │  ← HPA         │
│  │  Pod (1)   │  │  Pod (2)   │  │  Pod (3)   │    (2-10)      │
│  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘                │
│        └───────────────┼───────────────┘                        │
│                        │                                        │
│           ┌────────────┴────────────┐                          │
│           │                         │                          │
│           v                         v                          │
│  ┌─────────────────┐       ┌─────────────────┐                │
│  │   Auth Service  │       │  Lobby Service  │                │
│  │   Deployment    │       │   Deployment    │                │
│  │   (2 replicas)  │       │   (2 replicas)  │                │
│  └─────────────────┘       └─────────────────┘                │
│                                                                │
│  ┌────────────────────────────────────────────────────────┐   │
│  │ Game Server StatefulSet (game-server-0, 1, 2...)       │   │
│  │ - Persistent world state                                │   │
│  │ - Sticky sessions                                       │   │
│  │ - Graceful shutdown                                     │   │
│  └────────────────────────────────────────────────────────┘   │
│                                                                │
│  ┌─────────────────┐       ┌─────────────────┐                │
│  │ DBProxy Service │       │   PostgreSQL    │                │
│  │  (3 replicas)   │       │  StatefulSet    │                │
│  └─────────────────┘       └─────────────────┘                │
│                                                                │
│  ┌────────────────────────────────────────────────────────┐   │
│  │ Monitoring Stack                                        │   │
│  │ - Prometheus (metrics collection)                       │   │
│  │ - Grafana (dashboards)                                  │   │
│  │ - Jaeger (distributed tracing)                          │   │
│  └────────────────────────────────────────────────────────┘   │
│                                                                │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Service Communication

| From | To | Protocol | Purpose |
|------|-----|----------|---------|
| Client | Gateway | WebSocket/TCP | Game traffic |
| Gateway | Auth | gRPC | Session validation |
| Gateway | Game | TCP | Player commands |
| Gateway | Lobby | gRPC | Matchmaking |
| Game | DBProxy | TCP | Database queries |
| All | Prometheus | HTTP | Metrics export |

---

## 6. Security Architecture

### 6.1 Authentication Flow

```
┌────────┐                ┌────────┐                ┌────────┐
│ Client │                │  Auth  │                │   DB   │
│        │                │ Service│                │ Proxy  │
└───┬────┘                └───┬────┘                └───┬────┘
    │                         │                         │
    │  1. Login(user, pass)   │                         │
    │────────────────────────>│                         │
    │                         │  2. Verify credentials  │
    │                         │────────────────────────>│
    │                         │<────────────────────────│
    │                         │  3. Account data        │
    │                         │                         │
    │  4. JWT + Refresh Token │                         │
    │<────────────────────────│                         │
    │                         │                         │
    │  5. Game Request + JWT  │                         │
    │────────────────────────>│                         │
    │                         │  6. Validate JWT        │
    │                         │  (no DB lookup)         │
    │                         │                         │
    │  7. Request processed   │                         │
    │<────────────────────────│                         │
```

### 6.2 Security Measures

| Layer | Measure | Implementation |
|-------|---------|----------------|
| Transport | TLS 1.3 | All service communication |
| Authentication | JWT | RS256 signed tokens |
| Authorization | RBAC | Per-endpoint permissions |
| Input | Validation | All client inputs sanitized |
| Rate Limiting | Token bucket | Per-IP and per-account |
| Audit | Logging | All security events logged |

---

## 7. Performance Considerations

### 7.1 ECS Performance

```cpp
// Cache-friendly component storage using SparseSet
// Components stored contiguously in memory

// Bad: Object-oriented approach (cache misses)
for (auto* entity : entities) {
    entity->transform.update();  // Cache miss
    entity->health.update();     // Cache miss
}

// Good: ECS approach (cache friendly)
for (auto& transform : transforms) {
    transform.update();  // Sequential memory access
}
for (auto& health : healths) {
    health.update();     // Sequential memory access
}
```

### 7.2 Performance Targets

| Metric | Target | Achieved By |
|--------|--------|-------------|
| Tick Rate | 20 Hz | ECS batch processing |
| Entity Update | <5ms for 10K | Sparse set storage |
| Message Latency | <10ms | Zero-copy networking |
| Memory/Player | <100KB | Component pooling |

---

## 8. Error Handling

### 8.1 Result<T, E> Pattern

```cpp
// All operations return Result instead of throwing
Result<Player> load_player(PlayerId id) {
    auto conn = db_.get_connection();
    if (!conn) {
        return Result<Player>::error(
            Error::DatabaseConnection,
            "Failed to get database connection"
        );
    }

    auto query_result = conn->execute(
        "SELECT * FROM players WHERE id = ?", id
    );

    if (!query_result) {
        return Result<Player>::error(
            Error::QueryFailed,
            "Player query failed"
        );
    }

    if (query_result->empty()) {
        return Result<Player>::error(
            Error::NotFound,
            fmt::format("Player {} not found", id)
        );
    }

    return Result<Player>::ok(Player::from_row(query_result->front()));
}

// Usage
auto result = load_player(player_id);
if (!result) {
    logger.error("Failed to load player: {}", result.error().message);
    return;
}
Player& player = result.value();
```

### 8.2 Error Categories

| Category | Range | Example |
|----------|-------|---------|
| Common | 0x0001-0x0FFF | InvalidInput, NotFound |
| Network | 0x1000-0x1FFF | ConnectionFailed, Timeout |
| Database | 0x2000-0x2FFF | QueryFailed, DeadlockDetected |
| Game | 0x3000-0x3FFF | InvalidAction, InsufficientResources |
| Auth | 0x4000-0x4FFF | InvalidToken, PermissionDenied |

---

## 9. Appendices

### 9.1 C++20 Features Used

| Feature | Usage |
|---------|-------|
| Concepts | Type constraints for ECS |
| Coroutines | Async database operations |
| Ranges | Entity queries |
| std::span | Zero-copy buffers |
| std::format | String formatting |

### 9.2 Related Documents

- [PRD.md](./PRD.md) - Product requirements
- [INTEGRATION_STRATEGY.md](./INTEGRATION_STRATEGY.md) - Integration approach
- [reference/ECS_DESIGN.md](./reference/ECS_DESIGN.md) - ECS details
- [reference/PLUGIN_SYSTEM.md](./reference/PLUGIN_SYSTEM.md) - Plugin architecture

---

*Architecture Version*: 1.0.0
*Review Status*: Draft
