# Source Project Analysis

## Detailed Analysis of Four Game Server Projects

**Version**: 0.1.0.0
**Last Updated**: 2026-02-03
**Purpose**: Technical reference for integration decisions

---

## 1. Overview

This document provides detailed technical analysis of the four source projects being integrated into Common Game Server.

| Project | Type | Lines of Code | Status |
|---------|------|---------------|--------|
| common_game_server_system | Specification | ~200K (docs) | Complete |
| game_server | Implementation | ~150K | Phase 2 |
| unified_game_server | Implementation | ~80K | Phase 4 Complete |
| game_server_system | Placeholder | ~5K | Setup |

---

## 2. Common Game Server System (CGSS)

### 2.1 Project Overview

**Location**: `/Users/raphaelshin/Sources/common_game_server_system`

**Purpose**: Standardized specifications and development guidelines for game server development using 7 foundation systems.

### 2.2 Documentation Structure

```
common_game_server_system/
├── docs/
│   ├── PRD.md                    # Product Requirements
│   ├── PRD.kr.md                 # Korean version
│   ├── guidelines.md             # Development guidelines (MANDATORY)
│   ├── guidelines.kr.md          # Korean version
│   ├── srs/                      # Software Requirements Specification
│   │   ├── core/                 # 7 Foundation Systems
│   │   │   ├── common_system.md
│   │   │   ├── thread_system.md
│   │   │   ├── logger_system.md
│   │   │   ├── network_system.md
│   │   │   ├── database_system.md
│   │   │   ├── container_system.md
│   │   │   └── monitoring_system.md
│   │   ├── sdk/                  # High-level Game Features
│   │   ├── nfr.md               # Non-Functional Requirements
│   │   ├── interfaces.md        # API Specifications
│   │   ├── data.md              # Data Models
│   │   └── constraints.md       # Design Constraints
│   └── reference/
│       └── features/             # 15 Game Feature Specifications
└── work_plan/                    # Development Plans
```

### 2.3 Seven Foundation Systems

| System | Purpose | Performance Target |
|--------|---------|-------------------|
| **common_system** | Base types, Result<T,E>, DI, EventBus | 150K events/sec |
| **thread_system** | Thread pool, job scheduling | 1.24M jobs/sec |
| **logger_system** | Structured async logging | 4.3M msg/sec |
| **network_system** | TCP/UDP/WebSocket | 305K msg/sec |
| **database_system** | Multi-backend DB abstraction | <50ms p99 |
| **container_system** | Message serialization | 2M containers/sec |
| **monitoring_system** | Metrics, distributed tracing | 10M+ ops/sec |

### 2.4 Mandatory Guidelines

**CRITICAL**: All code must use only 7 foundation systems.

```cpp
// ✅ CORRECT - Using foundation systems
#include <kcenon/logger/logger.h>
auto& logger = kcenon::logger::LoggerFactory::get("game");
logger.info("Player {} connected", player_id);

// ❌ PROHIBITED - Direct external library usage
#include <spdlog/spdlog.h>  // PROHIBITED
spdlog::info("Player {} connected", player_id);
```

**Prohibited External Libraries**:

| Category | Prohibited | Use Instead |
|----------|------------|-------------|
| Threading | std::thread, pthread, boost::thread | thread_system |
| Logging | spdlog, log4cxx, glog | logger_system |
| JSON | nlohmann/json, rapidjson | container_system |
| Network | boost::asio (direct) | network_system |
| Database | Raw PostgreSQL/MySQL drivers | database_system |

### 2.5 Key Contributions to Integration

| Contribution | Value | Integration Priority |
|--------------|-------|---------------------|
| Foundation system specs | Standard API definitions | P0 |
| Result<T,E> pattern | Type-safe error handling | P0 |
| Performance targets | Benchmark requirements | P0 |
| Development guidelines | Code quality standards | P1 |
| Feature specifications | Game feature requirements | P2 |

---

## 3. Game Server (game_server)

### 3.1 Project Overview

**Location**: `/Users/raphaelshin/Sources/game_server`

**Purpose**: Traditional MMORPG server implementation inspired by MaNGOS architecture.

### 3.2 Project Structure

```
game_server/
├── src/
│   ├── realmd/                   # Realm Server (Authentication)
│   │   ├── auth_session.cpp
│   │   ├── auth_codes.cpp
│   │   └── realm_list.cpp
│   ├── mangosd/                  # World Server (Game Logic)
│   │   ├── world.cpp
│   │   ├── world_session.cpp
│   │   └── world_runnable.cpp
│   ├── game/                     # Game Logic
│   │   ├── Object/               # Object System
│   │   │   ├── Object.cpp
│   │   │   ├── Unit.cpp
│   │   │   ├── Player.cpp
│   │   │   ├── Creature.cpp
│   │   │   └── GameObject.cpp
│   │   ├── Combat/               # Combat System
│   │   │   ├── Spell.cpp
│   │   │   ├── Aura.cpp
│   │   │   ├── DamageCalculator.cpp
│   │   │   └── ThreatManager.cpp
│   │   ├── World/                # World System
│   │   │   ├── Map.cpp
│   │   │   ├── Zone.cpp
│   │   │   ├── Grid.cpp
│   │   │   └── Visibility.cpp
│   │   ├── AI/                   # AI System
│   │   │   ├── Brain.cpp
│   │   │   ├── BehaviorTree.cpp
│   │   │   └── Pathfinding.cpp
│   │   └── Content/              # Content Loading
│   │       ├── DBCFile.cpp
│   │       └── ContentLoader.cpp
│   └── shared/
│       └── integration/          # Foundation Integration Layer
│           ├── game_logger.cpp
│           ├── game_network.cpp
│           ├── game_database.cpp
│           ├── game_thread_pool.cpp
│           └── game_monitor.cpp
├── sql/                          # Database Schemas
│   ├── auth/
│   ├── characters/
│   └── world/
├── config/
│   ├── realmd.conf
│   └── mangosd.conf
└── docker/
    ├── Dockerfile.realmd
    └── Dockerfile.mangosd
```

### 3.3 Object System Analysis

**Class Hierarchy**:

```
Object (Base)
├── WorldObject
│   ├── Unit
│   │   ├── Player
│   │   └── Creature
│   │       ├── Pet
│   │       └── Totem
│   ├── GameObject
│   ├── DynamicObject
│   └── Corpse
└── Item
    └── Bag
```

**Key Object Fields**:

```cpp
class Object {
protected:
    ObjectGUID guid_;
    ObjectType type_;
    uint32_t entry_;
    float scale_;

    // Update fields (networked state)
    std::array<uint32_t, OBJECT_END> update_fields_;

public:
    virtual void Update(float dt) = 0;
    virtual void BuildCreatePacket(Packet& packet) = 0;
    virtual void BuildUpdatePacket(Packet& packet) = 0;
};

class Unit : public Object {
protected:
    UnitStats stats_;           // Health, mana, attributes
    Faction faction_;           // Faction affiliation
    UnitFlags flags_;           // State flags
    CombatState combat_;        // Combat information
    MovementInfo movement_;     // Position, velocity
    SpellHistory spells_;       // Cast history
    AuraList auras_;            // Active effects

public:
    void DealDamage(Unit* target, int amount, DamageType type);
    void CastSpell(SpellId spell, Unit* target);
    void ApplyAura(Aura* aura);
};

class Player : public Unit {
protected:
    AccountId account_id_;
    CharacterId character_id_;
    Inventory inventory_;
    QuestLog quests_;
    SkillList skills_;
    Reputation reputations_;
    Achievements achievements_;
    GuildMembership guild_;

public:
    void SendPacket(const Packet& packet);
    void Teleport(MapId map, Vector3 position);
    void LearnSpell(SpellId spell);
};
```

### 3.4 Combat System Analysis

**Damage Calculation Flow**:

```cpp
// DamageCalculator.cpp
DamageResult DamageCalculator::Calculate(Unit* attacker, Unit* victim,
                                          SpellId spell, DamageType type) {
    DamageResult result;

    // 1. Base damage
    int base_damage = CalculateBaseDamage(attacker, spell);

    // 2. Apply attacker modifiers
    base_damage = ApplyAttackerModifiers(base_damage, attacker, spell);

    // 3. Apply victim modifiers (armor, resistance)
    int mitigated = ApplyVictimMitigation(base_damage, victim, type);

    // 4. Calculate critical hit
    bool is_crit = RollCritical(attacker, victim, spell);
    if (is_crit) {
        mitigated *= GetCritMultiplier(attacker, spell);
    }

    // 5. Apply absorbs
    int absorbed = ApplyAbsorbs(mitigated, victim);
    int final_damage = mitigated - absorbed;

    // 6. Build result
    result.damage = final_damage;
    result.absorbed = absorbed;
    result.is_critical = is_crit;
    result.damage_type = type;

    return result;
}
```

**Spell System Flow**:

```cpp
// Spell.cpp
void Spell::Cast() {
    // 1. Validate cast
    if (!ValidateCast()) {
        SendCastError();
        return;
    }

    // 2. Start cast time (if not instant)
    if (spell_info_->cast_time > 0) {
        StartCastTimer();
        return;
    }

    // 3. Execute spell
    Execute();
}

void Spell::Execute() {
    // 1. Consume resources
    ConsumeResources();

    // 2. Apply effects
    for (auto& effect : spell_info_->effects) {
        ApplyEffect(effect);
    }

    // 3. Trigger cooldown
    TriggerCooldown();

    // 4. Generate threat
    GenerateThreat();
}
```

### 3.5 World System Analysis

**Grid-Based Spatial Management**:

```cpp
// Grid.cpp
class Grid {
    static constexpr float CELL_SIZE = 33.3333f;  // ~533/16
    static constexpr int GRID_SIZE = 64;

    std::array<std::array<Cell, GRID_SIZE>, GRID_SIZE> cells_;

public:
    void AddObject(Object* obj, const Vector3& pos) {
        GridCoord coord = CalculateCoord(pos);
        cells_[coord.x][coord.y].AddObject(obj);
    }

    void RemoveObject(Object* obj) {
        GridCoord coord = obj->GetGridCoord();
        cells_[coord.x][coord.y].RemoveObject(obj);
    }

    void UpdateVisibility(Player* player) {
        GridCoord center = player->GetGridCoord();
        int range = CalculateVisibilityRange(player);

        std::set<Object*> visible;
        for (int x = center.x - range; x <= center.x + range; ++x) {
            for (int y = center.y - range; y <= center.y + range; ++y) {
                if (IsValidCoord(x, y)) {
                    for (auto* obj : cells_[x][y].GetObjects()) {
                        if (player->CanSee(obj)) {
                            visible.insert(obj);
                        }
                    }
                }
            }
        }

        player->UpdateVisibleObjects(visible);
    }
};
```

### 3.6 Key Contributions to Integration

| Contribution | Value | Integration Priority |
|--------------|-------|---------------------|
| Object hierarchy | Proven MMORPG entity model | P0 |
| Combat mechanics | Battle-tested damage/spell system | P0 |
| World/Grid system | Efficient spatial management | P0 |
| AI/Pathfinding | Working NPC behavior | P1 |
| Integration layer | Foundation adapter patterns | P0 |
| Database schemas | Production-ready data models | P1 |

---

## 4. Unified Game Server (UGS)

### 4.1 Project Overview

**Location**: `/Users/raphaelshin/Sources/unified_game_server`

**Purpose**: Modern microservices-based game server with ECS architecture and plugin system.

### 4.2 Project Structure

```
unified_game_server/
├── include/ugs/
│   ├── core/                     # ECS Core
│   │   ├── entity.hpp
│   │   ├── component.hpp
│   │   ├── system.hpp
│   │   ├── world.hpp
│   │   ├── resource.hpp
│   │   └── event.hpp
│   ├── foundation/               # Foundation Adapters
│   │   ├── common_adapter.hpp
│   │   ├── thread_adapter.hpp
│   │   ├── logger_adapter.hpp
│   │   ├── network_adapter.hpp
│   │   ├── database_adapter.hpp
│   │   ├── container_adapter.hpp
│   │   └── monitoring_adapter.hpp
│   ├── network/                  # Network Layer
│   │   ├── game_server.hpp
│   │   ├── tcp_server.hpp
│   │   ├── tcp_socket.hpp
│   │   ├── session.hpp
│   │   └── packet_serializer.hpp
│   ├── services/                 # Microservices
│   │   ├── auth_service.hpp
│   │   ├── gateway_service.hpp
│   │   ├── game_service.hpp
│   │   ├── lobby_service.hpp
│   │   └── dbproxy_service.hpp
│   ├── api/                      # Game API
│   │   └── game_api.hpp
│   └── plugins/                  # Plugin System
│       ├── plugin_interface.hpp
│       ├── plugin_manager.hpp
│       └── mmorpg/               # MMORPG Reference Plugin
│           ├── character/
│           ├── combat/
│           ├── inventory/
│           ├── quest/
│           ├── guild/
│           ├── spell/
│           ├── aura/
│           ├── stats/
│           ├── progression/
│           └── loot/
├── src/
│   ├── core/
│   ├── foundation/
│   ├── network/
│   ├── services/
│   └── plugins/
├── test/
├── configs/
├── docs/                         # Extensive Documentation
│   ├── REQUIREMENTS.md           # 1,293 lines
│   ├── ARCHITECTURE.md           # 1,821 lines
│   ├── PROTOCOL.md               # 1,458 lines
│   ├── API_REFERENCE.md          # 939 lines
│   ├── PLUGIN_DEVELOPMENT.md     # 852 lines
│   ├── PHASE_1_PLAN.md
│   ├── PHASE_2_PLAN.md
│   ├── PHASE_3_PLAN.md
│   └── diagrams/                 # 15 Mermaid diagrams
└── deployment/                   # Kubernetes
    ├── Dockerfile
    ├── docker-compose.yml
    └── kubernetes/
```

### 4.3 ECS Implementation Analysis

**Entity Management**:

```cpp
// entity.hpp
using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY = 0;

class EntityManager {
    std::vector<EntityId> free_ids_;
    uint32_t next_id_ = 1;
    std::vector<uint32_t> generations_;  // For entity recycling

public:
    EntityId create() {
        if (!free_ids_.empty()) {
            EntityId id = free_ids_.back();
            free_ids_.pop_back();
            return id;
        }
        return next_id_++;
    }

    void destroy(EntityId id) {
        generations_[id]++;
        free_ids_.push_back(id);
    }

    bool is_valid(EntityId id) const {
        return id != INVALID_ENTITY && id < next_id_;
    }
};
```

**Component Storage (SparseSet)**:

```cpp
// component.hpp
template<typename T>
class ComponentPool {
    std::vector<T> dense_;           // Actual component data
    std::vector<EntityId> dense_to_entity_;  // Entity for each component
    std::vector<size_t> sparse_;     // Entity -> dense index

public:
    T& add(EntityId entity) {
        size_t index = dense_.size();
        dense_.push_back(T{});
        dense_to_entity_.push_back(entity);

        if (entity >= sparse_.size()) {
            sparse_.resize(entity + 1, INVALID_INDEX);
        }
        sparse_[entity] = index;

        return dense_.back();
    }

    T& get(EntityId entity) {
        return dense_[sparse_[entity]];
    }

    bool has(EntityId entity) const {
        return entity < sparse_.size() && sparse_[entity] != INVALID_INDEX;
    }

    // Iterator for system queries
    auto begin() { return dense_.begin(); }
    auto end() { return dense_.end(); }
};
```

**System Interface**:

```cpp
// system.hpp
class System {
public:
    virtual ~System() = default;

    virtual void update(World& world, float delta_time) = 0;
    virtual std::string_view name() const = 0;

    // Optional lifecycle hooks
    virtual void on_init(World& world) {}
    virtual void on_shutdown(World& world) {}

    // Dependencies for parallel execution
    virtual std::vector<std::string_view> read_components() const { return {}; }
    virtual std::vector<std::string_view> write_components() const { return {}; }
};

// System execution
class SystemRunner {
    std::vector<std::unique_ptr<System>> systems_;
    bool parallel_enabled_ = false;

public:
    void update(World& world, float delta_time) {
        if (parallel_enabled_) {
            update_parallel(world, delta_time);
        } else {
            for (auto& system : systems_) {
                system->update(world, delta_time);
            }
        }
    }
};
```

**World Container**:

```cpp
// world.hpp
class World {
    EntityManager entities_;
    std::unordered_map<std::type_index, std::unique_ptr<ComponentPoolBase>> pools_;
    SystemRunner systems_;

public:
    EntityId create_entity() {
        return entities_.create();
    }

    template<typename T>
    T& add_component(EntityId entity) {
        return get_pool<T>().add(entity);
    }

    template<typename T>
    T& get_component(EntityId entity) {
        return get_pool<T>().get(entity);
    }

    template<typename... Components>
    auto view() {
        return View<Components...>(*this);
    }

    void update(float delta_time) {
        systems_.update(*this, delta_time);
    }
};
```

### 4.4 Plugin System Analysis

**Plugin Interface**:

```cpp
// plugin_interface.hpp
class GamePlugin {
public:
    virtual ~GamePlugin() = default;

    // Lifecycle
    virtual Result<void> on_load(PluginContext& ctx) = 0;
    virtual Result<void> on_unload() = 0;
    virtual Result<void> on_enable(World& world) = 0;
    virtual Result<void> on_disable(World& world) = 0;

    // Metadata
    virtual PluginInfo info() const = 0;

    // Optional hooks
    virtual void on_tick(World& world, float dt) {}
    virtual void on_player_join(World& world, EntityId player) {}
    virtual void on_player_leave(World& world, EntityId player) {}
};

struct PluginInfo {
    std::string name;
    std::string version;
    std::string description;
    std::vector<std::string> dependencies;
    std::vector<std::string> optional_dependencies;
};
```

**Plugin Manager**:

```cpp
// plugin_manager.hpp
class PluginManager {
    std::vector<std::unique_ptr<GamePlugin>> plugins_;
    std::unordered_map<std::string, GamePlugin*> plugin_map_;
    PluginContext context_;

public:
    Result<void> load_plugin(const std::filesystem::path& path) {
        auto plugin = load_shared_library(path);
        if (!plugin) {
            return Result<void>::error(Error::PluginLoadFailed);
        }

        auto result = plugin->on_load(context_);
        if (!result) {
            return result;
        }

        plugin_map_[std::string(plugin->info().name)] = plugin.get();
        plugins_.push_back(std::move(plugin));
        return Result<void>::ok();
    }

    Result<void> enable_all(World& world) {
        // Topological sort by dependencies
        auto sorted = resolve_dependencies();

        for (auto* plugin : sorted) {
            auto result = plugin->on_enable(world);
            if (!result) {
                return result;
            }
        }
        return Result<void>::ok();
    }
};
```

### 4.5 Microservices Architecture

**Service Ports and Responsibilities**:

| Service | Port | Responsibility |
|---------|------|----------------|
| AuthServer | 8081 | Login, tokens, sessions |
| GatewayServer | 8080 | Client routing, load balancing |
| GameServer | 8082 | World simulation, game logic |
| LobbyServer | 8083 | Matchmaking, parties, chat |
| DBProxy | 8085 | Connection pooling, caching |
| AnalyticsCollector | 8084 | Event collection, analytics |

**Inter-Service Communication**:

```cpp
// Internal service communication uses gRPC
service AuthService {
    rpc Login(LoginRequest) returns (LoginResponse);
    rpc ValidateToken(TokenRequest) returns (TokenResponse);
    rpc RefreshToken(RefreshRequest) returns (RefreshResponse);
}

service GatewayService {
    rpc RouteMessage(GameMessage) returns (RouteResponse);
    rpc GetServerStatus(StatusRequest) returns (StatusResponse);
}
```

### 4.6 Key Contributions to Integration

| Contribution | Value | Integration Priority |
|--------------|-------|---------------------|
| ECS implementation | Cache-friendly entity management | P0 |
| Plugin system | Extensible game logic | P0 |
| Foundation adapters | Clean separation of concerns | P0 |
| Microservices | Scalable architecture | P0 |
| Kubernetes deployment | Cloud-native ready | P1 |
| Documentation | 11,000+ lines of docs | P1 |

---

## 5. Game Server System

### 5.1 Project Overview

**Location**: `/Users/raphaelshin/Sources/game_server_system`

**Purpose**: Intended as the integration target for combining all projects. Currently in early setup phase.

### 5.2 Current Status

```
game_server_system/
├── include/game/
│   ├── common/
│   │   ├── error_codes.hpp     ✅ Implemented
│   │   └── error_info.hpp      ✅ Implemented
│   ├── foundation/             🚧 In Progress
│   │   ├── game_database.hpp
│   │   ├── game_logger.hpp
│   │   ├── game_monitor.hpp
│   │   ├── game_network.hpp
│   │   └── game_thread_pool.hpp
│   ├── realm/                  📋 Planned
│   └── world/                  📋 Planned
├── src/
├── tests/
├── config/
├── sql/
└── docker/
```

### 5.3 Completed Work

**Ticket #001: Error Codes** - ✅ Complete

```cpp
// error_codes.hpp
namespace game {

enum class ErrorCode : uint32_t {
    // Common errors (0x0001-0x0FFF)
    Success = 0x0000,
    Unknown = 0x0001,
    InvalidInput = 0x0002,
    NotFound = 0x0003,

    // Foundation errors (0x1000-0x1FFF)
    DatabaseError = 0x1000,
    NetworkError = 0x1100,
    ThreadError = 0x1200,

    // Realm errors (0x2000-0x2FFF)
    AuthFailed = 0x2000,
    InvalidToken = 0x2001,
    SessionExpired = 0x2002,

    // World errors (0x3000-0x3FFF)
    MapNotFound = 0x3000,
    PositionInvalid = 0x3001,

    // Combat errors (0x4000-0x4FFF)
    SpellNotFound = 0x4000,
    InvalidTarget = 0x4001,
    OnCooldown = 0x4002,

    // Chat errors (0x5000-0x5FFF)
    ChannelNotFound = 0x5000,
    Muted = 0x5001,
};

struct ErrorInfo {
    ErrorCode code;
    std::string message;
    std::string context;
    std::source_location location;
};

}
```

### 5.4 Role in Integration

This project serves as the **integration target** where components from all three source projects will be combined. The new Common Game Server project will supersede this role.

---

## 6. Comparison Matrix

### 6.1 Architecture Comparison

| Aspect | CGSS | game_server | UGS |
|--------|------|-------------|-----|
| Type | Specification | Implementation | Implementation |
| Architecture | N/A | Monolithic | Microservices |
| Entity Model | N/A | OOP Hierarchy | ECS |
| Scalability | Defined | Limited | High |
| Cloud-Native | Specified | Basic | Full |
| Plugin Support | N/A | No | Yes |

### 6.2 Performance Comparison

| Metric | CGSS Target | game_server | UGS |
|--------|-------------|-------------|-----|
| Message Throughput | 300K/sec | ~150K/sec | 305K/sec |
| Entity Update | 10K @ <5ms | ~10K @ 15ms | 10K @ 5ms |
| DB Query p99 | <50ms | ~80ms | 35ms |
| Concurrent Users | 10K+ | ~3K | 10K+ |

### 6.3 Feature Comparison

| Feature | CGSS | game_server | UGS |
|---------|------|-------------|-----|
| Object System | Spec | ✅ Complete | Via Plugin |
| Combat System | Spec | ✅ Complete | Via Plugin |
| World System | Spec | ✅ Complete | Via Plugin |
| AI System | Spec | ✅ Complete | Via Plugin |
| Quest System | Spec | 🔵 Partial | Via Plugin |
| Guild System | Spec | 🔵 Partial | Via Plugin |
| Plugin System | N/A | ❌ No | ✅ Complete |
| Microservices | Spec | ❌ No | ✅ Complete |
| K8s Deployment | Spec | Basic | ✅ Complete |

---

## 7. Integration Decision Summary

### 7.1 What to Take from Each Project

| From CGSS | From game_server | From UGS |
|-----------|------------------|----------|
| 7 Foundation system specs | Object system implementation | ECS architecture |
| Result<T,E> pattern | Combat mechanics | Plugin system |
| Performance targets | World/Grid system | Microservices |
| Development guidelines | AI/Pathfinding | Foundation adapters |
| Error code ranges | Database schemas | K8s deployment |
| Feature specifications | Integration patterns | Documentation style |

### 7.2 Integration Priority

1. **P0 (Critical)**: Foundation + ECS + Core Game Logic
2. **P1 (High)**: Services + Plugin Framework
3. **P2 (Medium)**: MMORPG Plugin + Additional Features
4. **P3 (Low)**: Documentation + Examples

---

## 8. Appendices

### 8.1 Code Statistics

| Project | C++ Files | Header Files | Test Files | Total Lines |
|---------|-----------|--------------|------------|-------------|
| CGSS | 0 | 0 | 0 | ~200K (docs) |
| game_server | ~120 | ~150 | ~40 | ~150K |
| UGS | ~60 | ~80 | ~30 | ~80K |
| game_server_system | ~10 | ~15 | ~5 | ~5K |

### 8.2 Related Documents

- [ARCHITECTURE.md](../ARCHITECTURE.md) - Target architecture
- [INTEGRATION_STRATEGY.md](../INTEGRATION_STRATEGY.md) - Integration approach
- [ECS_DESIGN.md](./ECS_DESIGN.md) - ECS details
- [PLUGIN_SYSTEM.md](./PLUGIN_SYSTEM.md) - Plugin architecture

---

*Analysis Version*: 1.0.0
*Last Updated*: 2026-02-03
