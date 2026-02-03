# Software Design Specification (SDS)

## Common Game Server - Unified Game Server Framework

**Version**: 0.1.0.0
**Last Updated**: 2026-02-03
**Status**: Draft
**Based On**: SRS v0.1.0.0

---

## 1. Introduction

### 1.1 Purpose

This Software Design Specification (SDS) provides the detailed technical design for the Common Game Server framework. It translates the requirements defined in SRS v0.1.0.0 into architectural decisions, component designs, and implementation guidelines.

### 1.2 Scope

This document covers:
- System architecture and component decomposition
- Module and class designs
- Data structures and database schemas
- API specifications
- Deployment architecture

### 1.3 Document Conventions

- **SDS-XXX-YYY**: Design element identifier
  - XXX: Category (ARC=Architecture, MOD=Module, CLS=Class, API=API, DAT=Data, DEP=Deployment)
  - YYY: Sequential number
- **SRS Trace**: Reference to originating SRS requirement(s)

### 1.4 References

| Document | Version | Description |
|----------|---------|-------------|
| SRS.md | 0.1.0.0 | Software Requirements Specification |
| PRD.md | 0.1.0.0 | Product Requirements Document |

---

## 2. System Architecture

### 2.1 Architecture Overview

| ID | SDS-ARC-001 |
|----|-------------|
| **SRS Trace** | SRS-FND-001 ~ SRS-FND-007, SRS-ECS-001 ~ SRS-ECS-005 |
| **Description** | Layered architecture with plugin extensibility |

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Client Layer                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                          │
│  │  Game    │  │   Web    │  │  Admin   │                          │
│  │  Client  │  │  Client  │  │  Tools   │                          │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                          │
└───────┼─────────────┼─────────────┼─────────────────────────────────┘
        │ TCP/TLS     │ WebSocket   │ REST
        ▼             ▼             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Gateway Layer                                   │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                    GatewayServer                              │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐             │  │
│  │  │ Connection │  │   Auth     │  │  Message   │             │  │
│  │  │  Manager   │  │ Validator  │  │  Router    │             │  │
│  │  └────────────┘  └────────────┘  └────────────┘             │  │
│  └──────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
        │ gRPC
        ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Service Layer                                   │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐   │
│  │ AuthServer │  │ GameServer │  │LobbyServer │  │  DBProxy   │   │
│  └────────────┘  └─────┬──────┘  └────────────┘  └────────────┘   │
└────────────────────────┼────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Game Core Layer                                 │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                    Plugin Manager                             │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐                   │  │
│  │  │  MMORPG  │  │  Action  │  │  Battle  │  ...              │  │
│  │  │  Plugin  │  │  Plugin  │  │  Plugin  │                   │  │
│  │  └──────────┘  └──────────┘  └──────────┘                   │  │
│  └──────────────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                      ECS Core                                 │  │
│  │  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐│  │
│  │  │ World  │  │ Entity │  │Component│  │ System │  │ Query  ││  │
│  │  └────────┘  └────────┘  └────────┘  └────────┘  └────────┘│  │
│  └──────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Foundation Adapter Layer                          │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐│
│  │ Common │ │ Thread │ │ Logger │ │Network │ │Database│ │Monitor ││
│  │Adapter │ │Adapter │ │Adapter │ │Adapter │ │Adapter │ │Adapter ││
│  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └────────┘│
└─────────────────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Foundation Systems Layer                          │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐│
│  │ common │ │ thread │ │ logger │ │network │ │database│ │monitor ││
│  │ _system│ │ _system│ │ _system│ │ _system│ │ _system│ │ _system││
│  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └────────┘│
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 Package Structure

| ID | SDS-ARC-002 |
|----|-------------|
| **SRS Trace** | All SRS requirements |
| **Description** | Source code organization |

```
common_game_server/
├── CMakeLists.txt
├── conanfile.py
├── src/
│   ├── cgs/                          # Main namespace
│   │   ├── foundation/               # Foundation adapters
│   │   │   ├── common_adapter.hpp
│   │   │   ├── thread_adapter.hpp
│   │   │   ├── logger_adapter.hpp
│   │   │   ├── network_adapter.hpp
│   │   │   ├── database_adapter.hpp
│   │   │   ├── monitoring_adapter.hpp
│   │   │   └── container_adapter.hpp
│   │   ├── ecs/                      # ECS core
│   │   │   ├── entity.hpp
│   │   │   ├── component.hpp
│   │   │   ├── system.hpp
│   │   │   ├── query.hpp
│   │   │   ├── world.hpp
│   │   │   └── scheduler.hpp
│   │   ├── plugin/                   # Plugin system
│   │   │   ├── plugin_interface.hpp
│   │   │   ├── plugin_manager.hpp
│   │   │   ├── plugin_context.hpp
│   │   │   └── event_bus.hpp
│   │   ├── game/                     # Game logic components
│   │   │   ├── components/
│   │   │   ├── systems/
│   │   │   └── events/
│   │   └── services/                 # Microservices
│   │       ├── auth/
│   │       ├── gateway/
│   │       ├── game/
│   │       ├── lobby/
│   │       └── dbproxy/
│   └── main.cpp
├── plugins/                          # Game-specific plugins
│   └── mmorpg/
├── tests/
├── benchmarks/
└── docs/
```

### 2.3 Design Principles

| Principle | Description | Applied To |
|-----------|-------------|------------|
| **Dependency Inversion** | High-level modules depend on abstractions | Foundation Adapters |
| **Single Responsibility** | Each class has one reason to change | All components |
| **Open/Closed** | Open for extension, closed for modification | Plugin System |
| **Interface Segregation** | Many specific interfaces over general ones | ECS Components |
| **Data-Oriented Design** | Optimize for cache efficiency | ECS Storage |

---

## 3. Foundation Adapter Design

### 3.1 Common System Adapter

| ID | SDS-MOD-001 |
|----|-------------|
| **SRS Trace** | SRS-FND-001.1 ~ SRS-FND-001.4 |
| **Description** | Adapter for common_system types and utilities |

**Class Diagram**:

```
┌─────────────────────────────────────┐
│         GameError                    │
├─────────────────────────────────────┤
│ - code_: ErrorCode                   │
│ - message_: std::string              │
│ - context_: std::any                 │
├─────────────────────────────────────┤
│ + Code() const -> ErrorCode          │
│ + Message() const -> string_view     │
│ + Context<T>() const -> const T*     │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│      ServiceLocator                  │
├─────────────────────────────────────┤
│ - services_: unordered_map<TypeId,  │
│              unique_ptr<void>>       │
├─────────────────────────────────────┤
│ + Register<T>(unique_ptr<T>)        │
│ + Get<T>() -> T*                    │
│ + Has<T>() -> bool                  │
│ + Remove<T>()                       │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│      ConfigManager                   │
├─────────────────────────────────────┤
│ - config_: YAML::Node                │
│ - watchers_: vector<Callback>        │
├─────────────────────────────────────┤
│ + Load(path) -> GameResult<void>    │
│ + Get<T>(key) -> GameResult<T>      │
│ + Set<T>(key, value)                │
│ + Watch(key, callback)              │
└─────────────────────────────────────┘
```

**Error Code Enumeration**:

```cpp
namespace cgs::foundation {
    enum class ErrorCode : uint32_t {
        // General (0x0000 - 0x00FF)
        Success = 0x0000,
        Unknown = 0x0001,
        InvalidArgument = 0x0002,
        NotFound = 0x0003,
        AlreadyExists = 0x0004,

        // Network (0x0100 - 0x01FF)
        NetworkError = 0x0100,
        ConnectionFailed = 0x0101,
        ConnectionLost = 0x0102,
        Timeout = 0x0103,

        // Database (0x0200 - 0x02FF)
        DatabaseError = 0x0200,
        QueryFailed = 0x0201,
        TransactionFailed = 0x0202,

        // ECS (0x0300 - 0x03FF)
        EntityNotFound = 0x0300,
        ComponentNotFound = 0x0301,
        SystemError = 0x0302,

        // Plugin (0x0400 - 0x04FF)
        PluginLoadFailed = 0x0400,
        PluginNotFound = 0x0401,
        DependencyError = 0x0402,

        // Auth (0x0500 - 0x05FF)
        AuthenticationFailed = 0x0500,
        TokenExpired = 0x0501,
        InvalidToken = 0x0502,
        PermissionDenied = 0x0503,
    };
}
```

### 3.2 Thread System Adapter

| ID | SDS-MOD-002 |
|----|-------------|
| **SRS Trace** | SRS-FND-002.1 ~ SRS-FND-002.4 |
| **Description** | Game-specific job scheduling interface |

**Class Diagram**:

```
┌─────────────────────────────────────┐
│       GameJobScheduler               │
├─────────────────────────────────────┤
│ - scheduler_: thread::Scheduler*     │
│ - tickJobs_: vector<TickJob>         │
│ - nextJobId_: atomic<JobId>          │
├─────────────────────────────────────┤
│ + Schedule(job, priority) -> JobId   │
│ + ScheduleAfter(dep, job) -> JobId   │
│ + ScheduleTick(interval, job)        │
│ + Wait(id)                           │
│ + Cancel(id)                         │
│ + ProcessTick(deltaTime)             │
└─────────────────────────────────────┘
         │
         │ uses
         ▼
┌─────────────────────────────────────┐
│      thread_system::Scheduler        │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│         TickJob                      │
├─────────────────────────────────────┤
│ + id: JobId                          │
│ + interval: milliseconds             │
│ + lastExecuted: time_point           │
│ + func: JobFunc                      │
│ + enabled: bool                      │
└─────────────────────────────────────┘
```

### 3.3 Logger System Adapter

| ID | SDS-MOD-003 |
|----|-------------|
| **SRS Trace** | SRS-FND-003.1 ~ SRS-FND-003.4 |
| **Description** | Structured logging with game-specific categories |

**Log Categories and Levels**:

| Category | Description | Default Level |
|----------|-------------|---------------|
| Core | Core framework operations | Info |
| ECS | Entity-Component-System | Debug |
| Network | Network communication | Info |
| Database | Database operations | Info |
| Plugin | Plugin lifecycle | Info |
| Combat | Combat calculations | Debug |
| World | World/map operations | Info |
| AI | AI behavior | Debug |

**Log Context Structure**:

```cpp
struct LogContext {
    std::optional<EntityId> entityId;
    std::optional<PlayerId> playerId;
    std::optional<SessionId> sessionId;
    std::optional<std::string> traceId;
    std::unordered_map<std::string, std::string> extra;
};
```

### 3.4 Network System Adapter

| ID | SDS-MOD-004 |
|----|-------------|
| **SRS Trace** | SRS-FND-004.1 ~ SRS-FND-004.5 |
| **Description** | Network communication abstraction |

**Class Diagram**:

```
┌─────────────────────────────────────────────────────┐
│              GameNetworkManager                      │
├─────────────────────────────────────────────────────┤
│ - tcpServer_: network::TcpServer*                   │
│ - udpServer_: network::UdpServer*                   │
│ - wsServer_: network::WebSocketServer*              │
│ - sessions_: unordered_map<SessionId, Session>      │
│ - handlers_: unordered_map<uint16_t, Handler>       │
├─────────────────────────────────────────────────────┤
│ + Listen(port, protocol)                            │
│ + Close(session)                                    │
│ + Send(session, msg)                                │
│ + Broadcast(msg)                                    │
│ + RegisterHandler(opcode, handler)                  │
├─────────────────────────────────────────────────────┤
│ + OnConnected: Signal<SessionId>                    │
│ + OnDisconnected: Signal<SessionId>                 │
│ + OnError: Signal<SessionId, ErrorCode>             │
└─────────────────────────────────────────────────────┘
         │
         │ creates
         ▼
┌─────────────────────────────────────────────────────┐
│                   Session                            │
├─────────────────────────────────────────────────────┤
│ + id: SessionId                                     │
│ + protocol: Protocol                                │
│ + remoteAddress: string                             │
│ + connectedAt: time_point                           │
│ + lastActivity: time_point                          │
│ + userData: any                                     │
└─────────────────────────────────────────────────────┘
```

**Message Format**:

```
┌────────────────────────────────────────────┐
│            Network Message                  │
├──────────┬──────────┬──────────────────────┤
│  Length  │  Opcode  │      Payload         │
│  4 bytes │ 2 bytes  │    N bytes           │
├──────────┴──────────┴──────────────────────┤
│  Total: 6 + N bytes                        │
└────────────────────────────────────────────┘
```

### 3.5 Database System Adapter

| ID | SDS-MOD-005 |
|----|-------------|
| **SRS Trace** | SRS-FND-005.1 ~ SRS-FND-005.5 |
| **Description** | Database abstraction with connection pooling |

**Class Diagram**:

```
┌─────────────────────────────────────────────────────┐
│                 GameDatabase                         │
├─────────────────────────────────────────────────────┤
│ - pool_: database::ConnectionPool*                  │
│ - config_: DatabaseConfig                           │
├─────────────────────────────────────────────────────┤
│ + Connect(config) -> GameResult<void>               │
│ + Disconnect()                                      │
│ + Query(sql) -> GameResult<QueryResult>             │
│ + QueryAsync(sql) -> Future<GameResult<QueryResult>>│
│ + Prepare(sql) -> GameResult<PreparedStatement>     │
│ + Execute(stmt, args...) -> GameResult<QueryResult> │
│ + BeginTransaction() -> GameResult<Transaction>     │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│                 Transaction                          │
├─────────────────────────────────────────────────────┤
│ - conn_: Connection*                                │
│ - committed_: bool                                  │
├─────────────────────────────────────────────────────┤
│ + Query(sql) -> GameResult<QueryResult>             │
│ + Execute(stmt, args...) -> GameResult<QueryResult> │
│ + Commit() -> GameResult<void>                      │
│ + Rollback()                                        │
│ + ~Transaction() // auto-rollback if not committed  │
└─────────────────────────────────────────────────────┘
```

### 3.6 Monitoring System Adapter

| ID | SDS-MOD-006 |
|----|-------------|
| **SRS Trace** | SRS-FND-006.1 ~ SRS-FND-006.4 |
| **Description** | Metrics collection and distributed tracing |

**Predefined Metrics**:

| Metric Name | Type | Description |
|-------------|------|-------------|
| `cgs_connected_clients` | Gauge | Current CCU |
| `cgs_messages_total` | Counter | Total messages processed |
| `cgs_message_latency_ms` | Histogram | Message processing latency |
| `cgs_tick_duration_ms` | Histogram | World tick duration |
| `cgs_entity_count` | Gauge | Active entity count |
| `cgs_db_query_duration_ms` | Histogram | Database query latency |
| `cgs_db_pool_connections` | Gauge | Active DB connections |

### 3.7 Container System Adapter

| ID | SDS-MOD-007 |
|----|-------------|
| **SRS Trace** | SRS-FND-007.1 ~ SRS-FND-007.4 |
| **Description** | Serialization for network and persistence |

**Serialization Schema**:

```cpp
// Schema registration macro
#define CGS_SERIALIZABLE(Type, Version, ...) \
    template<> struct Serializable<Type> { \
        static constexpr uint32_t version = Version; \
        static constexpr auto fields = std::make_tuple(__VA_ARGS__); \
    }

// Usage example
CGS_SERIALIZABLE(PlayerData, 1,
    Field("id", &PlayerData::id),
    Field("name", &PlayerData::name),
    Field("level", &PlayerData::level),
    Field("position", &PlayerData::position)
);
```

---

## 4. ECS Core Design

### 4.1 Entity Design

| ID | SDS-MOD-010 |
|----|-------------|
| **SRS Trace** | SRS-ECS-002.1 ~ SRS-ECS-002.4 |
| **Description** | Entity representation and management |

**Entity Structure**:

```cpp
struct Entity {
    uint32_t id : 24;      // 16M entities max
    uint32_t version : 8;   // 256 versions before wrap

    static constexpr Entity Invalid() {
        return Entity{0xFFFFFF, 0xFF};
    }

    bool IsValid() const {
        return id != 0xFFFFFF;
    }
};
static_assert(sizeof(Entity) == 4);
```

**EntityManager Design**:

```
┌─────────────────────────────────────────────────────┐
│               EntityManager                          │
├─────────────────────────────────────────────────────┤
│ - entities_: vector<uint8_t>      // versions       │
│ - freeList_: vector<uint32_t>     // recycled IDs   │
│ - pendingDestroy_: vector<Entity> // deferred       │
│ - count_: size_t                                    │
├─────────────────────────────────────────────────────┤
│ + Create() -> Entity                                │
│ + Destroy(entity)                                   │
│ + DestroyImmediate(entity)                          │
│ + IsAlive(entity) -> bool                           │
│ + Count() -> size_t                                 │
│ + FlushPendingDestroys()                            │
└─────────────────────────────────────────────────────┘
```

### 4.2 Component Storage Design

| ID | SDS-MOD-011 |
|----|-------------|
| **SRS Trace** | SRS-ECS-001.1 ~ SRS-ECS-001.4 |
| **Description** | Sparse set-based component storage |

**Sparse Set Structure**:

```
Sparse Array (indexed by entity.id):
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ 2 │ - │ 0 │ - │ 1 │ - │ - │ 3 │  → Dense index or invalid
└───┴───┴───┴───┴───┴───┴───┴───┘
  0   1   2   3   4   5   6   7     Entity IDs

Dense Array (packed components):
┌───────────┬───────────┬───────────┬───────────┐
│Component 0│Component 1│Component 2│Component 3│
└───────────┴───────────┴───────────┴───────────┘
     ↓            ↓           ↓           ↓
Entity ID Array:
┌───┬───┬───┬───┐
│ 2 │ 4 │ 0 │ 7 │  → Corresponding entity IDs
└───┴───┴───┴───┘
```

**ComponentStorage Class**:

```cpp
template<typename T>
class ComponentStorage {
    std::vector<uint32_t> sparse_;    // entity.id -> dense index
    std::vector<T> dense_;            // packed components
    std::vector<uint32_t> entities_;  // dense index -> entity.id
    std::vector<uint32_t> versions_;  // change detection

public:
    T& Add(Entity entity, T&& component);
    T& Get(Entity entity);
    bool Has(Entity entity) const;
    void Remove(Entity entity);

    // Cache-friendly iteration
    auto begin() { return dense_.begin(); }
    auto end() { return dense_.end(); }

    // Version for change detection
    uint32_t GetVersion(Entity entity) const;
    bool HasChanged(Entity entity, uint32_t sinceVersion) const;
};
```

### 4.3 System Scheduler Design

| ID | SDS-MOD-012 |
|----|-------------|
| **SRS Trace** | SRS-ECS-003.1 ~ SRS-ECS-003.4, SRS-ECS-004.1 ~ SRS-ECS-004.4 |
| **Description** | System execution with dependency management |

**Scheduler Architecture**:

```
┌─────────────────────────────────────────────────────┐
│               SystemScheduler                        │
├─────────────────────────────────────────────────────┤
│ - systems_: vector<SystemEntry>                     │
│ - stageGroups_: map<Stage, vector<SystemId>>        │
│ - dependencies_: DAG<SystemId>                      │
│ - parallelGroups_: vector<ParallelGroup>            │
├─────────────────────────────────────────────────────┤
│ + Register<T>()                                     │
│ + AddDependency(before, after)                      │
│ + SetEnabled(system, enabled)                       │
│ + Execute(deltaTime)                                │
│ - BuildExecutionPlan()                              │
│ - DetectConflicts()                                 │
└─────────────────────────────────────────────────────┘

Execution Flow:
┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐
│PreUpdate│ → │ Update  │ → │PostUpdate│ → │  Fixed  │
│  Stage  │   │  Stage  │   │  Stage   │   │ Update  │
└─────────┘   └─────────┘   └─────────┘   └─────────┘
     │             │             │             │
     ▼             ▼             ▼             ▼
 [Systems]    [Systems]     [Systems]     [Systems]
  in order    in order      in order      in order
  or parallel or parallel   or parallel   or parallel
```

**Parallel Execution Groups**:

```cpp
struct SystemAccessInfo {
    std::set<TypeId> reads;    // Components read
    std::set<TypeId> writes;   // Components written
};

// Systems can run in parallel if:
// 1. No write-write conflicts
// 2. No read-write conflicts on same component
// 3. No explicit dependency between them
```

### 4.4 Query Design

| ID | SDS-MOD-013 |
|----|-------------|
| **SRS Trace** | SRS-ECS-005.1 ~ SRS-ECS-005.4 |
| **Description** | Component queries with filtering |

**Query Implementation**:

```cpp
template<typename... Includes>
class Query {
    World* world_;
    std::tuple<ComponentStorage<Includes>*...> storages_;

    // Filters
    std::vector<TypeId> excludes_;
    std::vector<TypeId> optionals_;

    // Cache
    mutable std::vector<Entity> cachedEntities_;
    mutable uint64_t cacheVersion_ = 0;

public:
    Query& Exclude() {
        excludes_.push_back(TypeId::Of<T>());
        return *this;
    }

    template<typename T>
    Query& Optional() {
        optionals_.push_back(TypeId::Of<T>());
        return *this;
    }

    void ForEach(auto&& func) {
        RefreshCache();
        for (Entity e : cachedEntities_) {
            func(e, std::get<ComponentStorage<Includes>*>(storages_)->Get(e)...);
        }
    }

    size_t Count() const {
        RefreshCache();
        return cachedEntities_.size();
    }
};
```

---

## 5. Plugin System Design

### 5.1 Plugin Interface Design

| ID | SDS-MOD-020 |
|----|-------------|
| **SRS Trace** | SRS-PLG-001.1 ~ SRS-PLG-001.4 |
| **Description** | Plugin interface and metadata |

**Plugin Interface**:

```cpp
namespace cgs::plugin {
    constexpr uint32_t PLUGIN_API_VERSION = 1;

    struct Version {
        uint16_t major;
        uint16_t minor;
        uint16_t patch;

        bool IsCompatibleWith(const Version& other) const;
    };

    struct PluginInfo {
        std::string name;
        std::string description;
        Version version;
        std::vector<std::string> dependencies;  // "name>=1.0.0"
        uint32_t apiVersion;
    };

    class IPlugin {
    public:
        virtual ~IPlugin() = default;
        virtual const PluginInfo& GetInfo() const = 0;
        virtual bool OnLoad(PluginContext& ctx) = 0;
        virtual bool OnInit() = 0;
        virtual void OnUpdate(float deltaTime) = 0;
        virtual void OnShutdown() = 0;
        virtual void OnUnload() = 0;
    };
}
```

### 5.2 Plugin Manager Design

| ID | SDS-MOD-021 |
|----|-------------|
| **SRS Trace** | SRS-PLG-002.1 ~ SRS-PLG-002.4, SRS-PLG-004.1 ~ SRS-PLG-004.4 |
| **Description** | Plugin lifecycle and dependency management |

**Class Diagram**:

```
┌─────────────────────────────────────────────────────┐
│               PluginManager                          │
├─────────────────────────────────────────────────────┤
│ - plugins_: map<string, PluginEntry>                │
│ - loadOrder_: vector<string>                        │
│ - context_: PluginContext                           │
├─────────────────────────────────────────────────────┤
│ + LoadPlugin(path) -> GameResult<void>              │
│ + UnloadPlugin(name) -> GameResult<void>            │
│ + InitializeAll() -> GameResult<void>               │
│ + UpdateAll(deltaTime)                              │
│ + ShutdownAll()                                     │
│ + GetPlugin(name) -> IPlugin*                       │
│ - ResolveDependencies() -> GameResult<vector<string>>│
│ - ValidateVersion(dep, available) -> bool           │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│                PluginEntry                           │
├─────────────────────────────────────────────────────┤
│ + plugin: unique_ptr<IPlugin>                       │
│ + handle: void* (dynamic library handle)            │
│ + state: PluginState                                │
│ + loadedAt: time_point                              │
└─────────────────────────────────────────────────────┘

enum class PluginState {
    Unloaded,
    Loaded,
    Initialized,
    Active,
    ShuttingDown,
    Error
};
```

**Plugin State Machine**:

```
              LoadPlugin()           InitializeAll()
[Unloaded] ──────────────→ [Loaded] ──────────────→ [Initialized]
     ↑                         │                          │
     │                         │ Error                    │ Start
     │                         ▼                          ▼
     │                      [Error]                   [Active]
     │                         │                          │
     │                         │                          │ ShutdownAll()
     └─────────────────────────┴──────────────────────────┘
                         UnloadPlugin()
```

### 5.3 Event Bus Design

| ID | SDS-MOD-022 |
|----|-------------|
| **SRS Trace** | SRS-PLG-003.1 ~ SRS-PLG-003.4 |
| **Description** | Type-safe event communication |

**Event Bus Implementation**:

```cpp
class EventBus {
    using HandlerId = uint64_t;

    struct HandlerEntry {
        HandlerId id;
        int priority;
        std::function<void(const void*)> handler;
    };

    std::unordered_map<TypeId, std::vector<HandlerEntry>> handlers_;
    std::queue<std::pair<TypeId, std::any>> deferredEvents_;
    std::atomic<HandlerId> nextHandlerId_{0};

public:
    template<typename E>
    HandlerId Subscribe(std::function<void(const E&)> handler, int priority = 0) {
        auto id = nextHandlerId_++;
        handlers_[TypeId::Of<E>()].push_back({
            id, priority,
            [handler](const void* e) { handler(*static_cast<const E*>(e)); }
        });
        SortByPriority(handlers_[TypeId::Of<E>()]);
        return id;
    }

    void Unsubscribe(HandlerId id);

    template<typename E>
    void Publish(const E& event) {
        auto it = handlers_.find(TypeId::Of<E>());
        if (it != handlers_.end()) {
            for (auto& entry : it->second) {
                entry.handler(&event);
            }
        }
    }

    template<typename E>
    void PublishDeferred(E event) {
        deferredEvents_.emplace(TypeId::Of<E>(), std::move(event));
    }

    void ProcessDeferred();
};
```

---

## 6. Game Logic Component Design

### 6.1 Core Components

| ID | SDS-MOD-030 |
|----|-------------|
| **SRS Trace** | SRS-GML-001.1 ~ SRS-GML-001.5 |
| **Description** | Base game object components |

**Component Definitions**:

```cpp
namespace cgs::game {
    // Transform component
    struct Transform {
        Vector3 position{0, 0, 0};
        Quaternion rotation{0, 0, 0, 1};
        Vector3 scale{1, 1, 1};

        Matrix4x4 ToMatrix() const;
        Vector3 Forward() const;
    };

    // Identity component
    struct Identity {
        GUID guid;
        std::string name;
        ObjectType type;
        uint32_t entry;       // Template/entry ID
        uint32_t displayId;   // Visual appearance
    };

    // Stats component
    struct Stats {
        int32_t health;
        int32_t maxHealth;
        int32_t mana;
        int32_t maxMana;
        std::array<int32_t, 32> attributes;  // STR, AGI, INT, etc.

        float HealthPercent() const;
        bool IsAlive() const;
    };

    // Movement component
    struct Movement {
        float speed;
        float baseSpeed;
        Vector3 velocity;
        MovementFlags flags;
        MovementState state;
    };

    enum class MovementState : uint8_t {
        Idle, Walking, Running, Jumping, Falling, Swimming, Flying
    };
}
```

### 6.2 Combat Components

| ID | SDS-MOD-031 |
|----|-------------|
| **SRS Trace** | SRS-GML-002.1 ~ SRS-GML-002.5 |
| **Description** | Combat system components |

```cpp
namespace cgs::game {
    // Spell casting
    struct SpellCaster {
        struct CastInfo {
            uint32_t spellId;
            Entity target;
            Vector3 targetPos;
            CastState state;
            float castTime;
            float remaining;
        };
        std::optional<CastInfo> currentCast;
        std::vector<uint32_t> cooldowns;  // spell_id -> end_time
    };

    // Aura/buff system
    struct AuraHolder {
        struct Aura {
            uint32_t auraId;
            Entity caster;
            int32_t stacks;
            float duration;
            float remaining;
            AuraFlags flags;
        };
        std::vector<Aura> auras;

        bool HasAura(uint32_t auraId) const;
        void AddAura(Aura aura);
        void RemoveAura(uint32_t auraId);
        void UpdateAuras(float deltaTime);
    };

    // Combat state
    struct CombatState {
        Entity target;
        std::vector<Entity> threatList;
        float inCombatTimer;
        bool inCombat;
    };
}
```

### 6.3 World Components

| ID | SDS-MOD-032 |
|----|-------------|
| **SRS Trace** | SRS-GML-003.1 ~ SRS-GML-003.5 |
| **Description** | World and spatial components |

```cpp
namespace cgs::game {
    // Map instance
    struct MapInstance {
        uint32_t mapId;
        uint32_t instanceId;
        MapType type;          // World, Dungeon, Arena, etc.
        uint32_t maxPlayers;
        time_point createdAt;
    };

    // Zone information
    struct ZoneInfo {
        uint32_t zoneId;
        uint32_t areaId;
        ZoneType type;
        ZoneFlags flags;       // PvP, Sanctuary, etc.
    };

    // Grid cell membership
    struct GridPosition {
        int32_t cellX;
        int32_t cellY;
    };
}

// Spatial Index (non-component, world-level)
class SpatialIndex {
    static constexpr float CELL_SIZE = 50.0f;  // meters

    struct Cell {
        std::vector<Entity> entities;
    };

    std::unordered_map<uint64_t, Cell> cells_;

public:
    void Insert(Entity entity, const Vector3& pos);
    void Update(Entity entity, const Vector3& oldPos, const Vector3& newPos);
    void Remove(Entity entity, const Vector3& pos);

    std::vector<Entity> QueryRadius(const Vector3& center, float radius);
    std::vector<Entity> QueryRect(const AABB& bounds);

private:
    static uint64_t CellKey(int32_t x, int32_t y);
    static std::pair<int32_t, int32_t> PosToCell(const Vector3& pos);
};
```

---

## 7. Microservices Design

### 7.1 Service Communication

| ID | SDS-MOD-040 |
|----|-------------|
| **SRS Trace** | SRS-SVC-001 ~ SRS-SVC-005 |
| **Description** | Inter-service communication protocol |

**gRPC Service Definitions**:

```protobuf
// auth.proto
service AuthService {
    rpc ValidateToken(ValidateTokenRequest) returns (ValidateTokenResponse);
    rpc RefreshToken(RefreshTokenRequest) returns (TokenResponse);
    rpc RevokeToken(RevokeTokenRequest) returns (RevokeResponse);
}

// gateway.proto
service GatewayService {
    rpc RegisterSession(RegisterSessionRequest) returns (RegisterSessionResponse);
    rpc RouteMessage(RouteMessageRequest) returns (RouteMessageResponse);
    rpc TransferSession(TransferRequest) returns (TransferResponse);
}

// game.proto
service GameService {
    rpc JoinWorld(JoinWorldRequest) returns (JoinWorldResponse);
    rpc LeaveWorld(LeaveWorldRequest) returns (LeaveWorldResponse);
    rpc GetServerStatus(Empty) returns (ServerStatusResponse);
}

// lobby.proto
service LobbyService {
    rpc JoinQueue(JoinQueueRequest) returns (JoinQueueResponse);
    rpc LeaveQueue(LeaveQueueRequest) returns (LeaveQueueResponse);
    rpc CreateParty(CreatePartyRequest) returns (PartyResponse);
}
```

### 7.2 Authentication Server Design

| ID | SDS-MOD-041 |
|----|-------------|
| **SRS Trace** | SRS-SVC-001.1 ~ SRS-SVC-001.6 |
| **Description** | JWT-based authentication service |

**Token Structure**:

```
Access Token (JWT):
{
  "header": {
    "alg": "RS256",
    "typ": "JWT"
  },
  "payload": {
    "sub": "user_id",
    "iss": "cgs-auth",
    "aud": "cgs-services",
    "exp": 1234567890,
    "iat": 1234567890,
    "jti": "unique_token_id",
    "roles": ["player"],
    "permissions": ["play", "chat"]
  }
}

Refresh Token:
{
  "token": "random_256bit_hex",
  "user_id": "user_id",
  "device_id": "device_fingerprint",
  "expires_at": "2026-02-10T00:00:00Z",
  "created_at": "2026-02-03T00:00:00Z"
}
```

### 7.3 Gateway Server Design

| ID | SDS-MOD-042 |
|----|-------------|
| **SRS Trace** | SRS-SVC-002.1 ~ SRS-SVC-002.5 |
| **Description** | Client connection gateway |

**Connection Flow**:

```
Client                    Gateway                   Auth                    Game
  │                          │                        │                       │
  │──── Connect ────────────→│                        │                       │
  │                          │                        │                       │
  │←─── Challenge ───────────│                        │                       │
  │                          │                        │                       │
  │──── Auth(token) ────────→│                        │                       │
  │                          │─── ValidateToken ────→│                       │
  │                          │←── TokenValid ────────│                       │
  │                          │                        │                       │
  │←─── AuthSuccess ─────────│                        │                       │
  │                          │                        │                       │
  │──── EnterWorld ─────────→│                        │                       │
  │                          │────────── JoinWorld ──────────────────────────→│
  │                          │←───────── WorldState ─────────────────────────│
  │←─── WorldData ───────────│                        │                       │
  │                          │                        │                       │
```

### 7.4 Game Server Design

| ID | SDS-MOD-043 |
|----|-------------|
| **SRS Trace** | SRS-SVC-003.1 ~ SRS-SVC-003.5 |
| **Description** | World simulation server |

**Game Loop**:

```cpp
class GameServer {
    World world_;
    PluginManager plugins_;
    std::chrono::milliseconds tickInterval_{50};  // 20 Hz

public:
    void Run() {
        auto lastTick = Clock::now();
        float accumulator = 0.0f;

        while (running_) {
            auto now = Clock::now();
            float deltaTime = (now - lastTick).count() / 1000.0f;
            lastTick = now;
            accumulator += deltaTime;

            // Process network messages
            ProcessIncomingMessages();

            // Fixed timestep update
            while (accumulator >= tickInterval_.count() / 1000.0f) {
                FixedUpdate(tickInterval_.count() / 1000.0f);
                accumulator -= tickInterval_.count() / 1000.0f;
            }

            // Variable update
            Update(deltaTime);

            // Send state updates to clients
            BroadcastWorldState();

            // Sleep if we have time
            SleepUntilNextTick();
        }
    }
};
```

---

## 8. Data Design

### 8.1 Database Schema

| ID | SDS-DAT-001 |
|----|-------------|
| **SRS Trace** | SRS-SVC-001, SRS-SVC-003.5 |
| **Description** | PostgreSQL database schema |

**Core Tables**:

```sql
-- Users and authentication
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    username VARCHAR(32) UNIQUE NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    last_login TIMESTAMPTZ,
    status VARCHAR(20) DEFAULT 'active'
);

CREATE TABLE refresh_tokens (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id) ON DELETE CASCADE,
    token_hash VARCHAR(64) NOT NULL,
    device_id VARCHAR(64),
    expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    revoked_at TIMESTAMPTZ
);

-- Characters
CREATE TABLE characters (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id) ON DELETE CASCADE,
    name VARCHAR(32) UNIQUE NOT NULL,
    class_id INT NOT NULL,
    level INT DEFAULT 1,
    experience BIGINT DEFAULT 0,
    map_id INT NOT NULL,
    position_x REAL NOT NULL,
    position_y REAL NOT NULL,
    position_z REAL NOT NULL,
    rotation REAL DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    played_time BIGINT DEFAULT 0
);

CREATE TABLE character_stats (
    character_id UUID PRIMARY KEY REFERENCES characters(id) ON DELETE CASCADE,
    health INT NOT NULL,
    max_health INT NOT NULL,
    mana INT NOT NULL,
    max_mana INT NOT NULL,
    attributes JSONB NOT NULL DEFAULT '{}'
);

-- Inventory
CREATE TABLE character_inventory (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    character_id UUID REFERENCES characters(id) ON DELETE CASCADE,
    slot INT NOT NULL,
    item_id INT NOT NULL,
    count INT DEFAULT 1,
    durability INT,
    enchants JSONB,
    UNIQUE(character_id, slot)
);

-- Indexes
CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_characters_user ON characters(user_id);
CREATE INDEX idx_inventory_character ON character_inventory(character_id);
CREATE INDEX idx_refresh_tokens_user ON refresh_tokens(user_id);
```

---

## 9. Deployment Design

### 9.1 Kubernetes Architecture

| ID | SDS-DEP-001 |
|----|-------------|
| **SRS Trace** | SRS-NFR-007, SRS-NFR-009 |
| **Description** | Kubernetes deployment configuration |

**Deployment Diagram**:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Kubernetes Cluster                            │
├─────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    Ingress Controller                        │   │
│  │                   (nginx / traefik)                          │   │
│  └───────────────────────────┬─────────────────────────────────┘   │
│                              │                                      │
│  ┌───────────────────────────┼─────────────────────────────────┐   │
│  │                    Gateway Service                           │   │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐                      │   │
│  │  │Gateway-1│  │Gateway-2│  │Gateway-N│   (HPA: 2-10)        │   │
│  │  └─────────┘  └─────────┘  └─────────┘                      │   │
│  └───────────────────────────┬─────────────────────────────────┘   │
│                              │                                      │
│  ┌───────────────────────────┼─────────────────────────────────┐   │
│  │                    Internal Services                         │   │
│  │                                                              │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │   │
│  │  │ AuthServer   │  │ LobbyServer  │  │  DBProxy     │       │   │
│  │  │  (2 replicas)│  │ (2 replicas) │  │ (2 replicas) │       │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘       │   │
│  │                                                              │   │
│  │  ┌──────────────────────────────────────────────────────┐   │   │
│  │  │                  GameServer Pool                      │   │   │
│  │  │  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐      │   │   │
│  │  │  │ Game-1 │  │ Game-2 │  │ Game-3 │  │ Game-N │      │   │   │
│  │  │  │(World1)│  │(World2)│  │(Inst1) │  │(InstN) │      │   │   │
│  │  │  └────────┘  └────────┘  └────────┘  └────────┘      │   │   │
│  │  └──────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    Data Layer                                │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │   │
│  │  │  PostgreSQL  │  │    Redis     │  │ Prometheus   │       │   │
│  │  │  (Primary +  │  │  (Cluster)   │  │ + Grafana    │       │   │
│  │  │   Replica)   │  │              │  │              │       │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘       │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 9.2 Resource Requirements

| Service | CPU Request | CPU Limit | Memory Request | Memory Limit | Replicas |
|---------|-------------|-----------|----------------|--------------|----------|
| Gateway | 500m | 2000m | 512Mi | 2Gi | 2-10 (HPA) |
| AuthServer | 250m | 1000m | 256Mi | 1Gi | 2 |
| GameServer | 2000m | 4000m | 2Gi | 8Gi | Variable |
| LobbyServer | 500m | 2000m | 512Mi | 2Gi | 2 |
| DBProxy | 500m | 2000m | 512Mi | 2Gi | 2 |

---

## 10. Traceability Matrix

### 10.1 SRS to SDS Traceability

| SRS ID | SRS Description | SDS IDs |
|--------|-----------------|---------|
| SRS-FND-001.1 ~ .4 | Common System Adapter | SDS-MOD-001 |
| SRS-FND-002.1 ~ .4 | Thread System Adapter | SDS-MOD-002 |
| SRS-FND-003.1 ~ .4 | Logger System Adapter | SDS-MOD-003 |
| SRS-FND-004.1 ~ .5 | Network System Adapter | SDS-MOD-004 |
| SRS-FND-005.1 ~ .5 | Database System Adapter | SDS-MOD-005 |
| SRS-FND-006.1 ~ .4 | Monitoring System Adapter | SDS-MOD-006 |
| SRS-FND-007.1 ~ .4 | Container System Adapter | SDS-MOD-007 |
| SRS-ECS-001.1 ~ .4 | Component Storage | SDS-MOD-011 |
| SRS-ECS-002.1 ~ .4 | Entity Management | SDS-MOD-010 |
| SRS-ECS-003.1 ~ .4 | System Scheduling | SDS-MOD-012 |
| SRS-ECS-004.1 ~ .4 | Parallel Execution | SDS-MOD-012 |
| SRS-ECS-005.1 ~ .4 | Component Queries | SDS-MOD-013 |
| SRS-PLG-001.1 ~ .4 | Plugin Interface | SDS-MOD-020 |
| SRS-PLG-002.1 ~ .4 | Plugin Lifecycle | SDS-MOD-021 |
| SRS-PLG-003.1 ~ .4 | Plugin Communication | SDS-MOD-022 |
| SRS-PLG-004.1 ~ .4 | Plugin Dependencies | SDS-MOD-021 |
| SRS-GML-001.1 ~ .5 | Object System | SDS-MOD-030 |
| SRS-GML-002.1 ~ .5 | Combat System | SDS-MOD-031 |
| SRS-GML-003.1 ~ .5 | World System | SDS-MOD-032 |
| SRS-SVC-001.1 ~ .6 | Authentication Server | SDS-MOD-041, SDS-DAT-001 |
| SRS-SVC-002.1 ~ .5 | Gateway Server | SDS-MOD-042 |
| SRS-SVC-003.1 ~ .5 | Game Server | SDS-MOD-043 |
| SRS-SVC-004.1 ~ .4 | Lobby Server | SDS-MOD-040 |
| SRS-SVC-005.1 ~ .4 | Database Proxy | SDS-MOD-040 |
| SRS-NFR-007, SRS-NFR-009 | Scalability | SDS-DEP-001 |

### 10.2 Coverage Summary

| Category | SRS Requirements | SDS Modules | Coverage |
|----------|------------------|-------------|----------|
| Foundation | 26 | 7 | 100% |
| ECS | 17 | 4 | 100% |
| Plugin | 17 | 3 | 100% |
| Game Logic | 23 | 3 | 100% |
| Services | 24 | 4 | 100% |
| Data | - | 1 | 100% |
| Deployment | 3 | 1 | 100% |
| **Total** | **110+** | **23** | **100%** |

---

## 11. Appendices

### 11.1 Change History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1.0.0 | 2026-02-03 | - | Initial draft based on SRS v0.1.0.0 |

### 11.2 Related Documents

- [SRS.md](./SRS.md) - Software Requirements Specification
- [PRD.md](./PRD.md) - Product Requirements Document
- [ARCHITECTURE.md](./ARCHITECTURE.md) - Architecture overview

---

*Document Approval*

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Technical Lead | | | |
| Architect | | | |
| Senior Developer | | | |
