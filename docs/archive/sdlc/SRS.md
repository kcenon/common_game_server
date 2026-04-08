# Software Requirements Specification (SRS)

## Common Game Server - Unified Game Server Framework

**Version**: 0.1.0.0
**Last Updated**: 2026-02-03
**Status**: Draft
**Based On**: PRD v0.1.0.0

---

## 1. Introduction

### 1.1 Purpose

This Software Requirements Specification (SRS) describes the functional and non-functional requirements for the Common Game Server framework. It serves as the primary reference for technical implementation and provides detailed specifications derived from the Product Requirements Document (PRD).

**Traceability**: This document maintains bidirectional traceability with PRD v0.1.0.0.

### 1.2 Scope

The Common Game Server is a unified, production-ready game server framework that:

- Integrates 7 foundation systems for core infrastructure
- Provides Entity-Component System (ECS) for game logic performance
- Supports plugin-based architecture for game-specific modules
- Enables microservices-based deployment for horizontal scaling

**Out of Scope** (per PRD Section 3.2):
- Game client development
- Game content creation tools
- Voice/video communication
- Payment/monetization systems
- Anti-cheat systems

### 1.3 Definitions, Acronyms, and Abbreviations

| Term | Definition |
|------|------------|
| **ECS** | Entity-Component System - data-oriented design pattern |
| **CCU** | Concurrent Connected Users |
| **Foundation System** | One of 7 core infrastructure libraries (common, thread, logger, network, database, monitoring, container) |
| **Plugin** | Loadable module containing game-specific logic |
| **Adapter** | Wrapper providing game-specific interface to foundation system |
| **SRS** | Software Requirements Specification |
| **PRD** | Product Requirements Document |

### 1.4 References

| Document | Version | Description |
|----------|---------|-------------|
| PRD.md | 0.1.0.0 | Product Requirements Document |
| ARCHITECTURE.md | - | Technical architecture design |
| ECS_DESIGN.md | - | ECS architecture details |
| PLUGIN_SYSTEM.md | - | Plugin system design |

### 1.5 Document Conventions

- **SRS-XXX-YYY**: Software requirement identifier
  - XXX: Category (FND=Foundation, ECS=Entity-Component, PLG=Plugin, GML=Game Logic, SVC=Service, NFR=Non-Functional)
  - YYY: Sequential number
- **Priority**: P0 (Critical), P1 (High), P2 (Medium), P3 (Low)
- **PRD Trace**: Reference to originating PRD requirement

---

## 2. Overall Description

### 2.1 Product Perspective

The Common Game Server unifies four existing projects:

```
┌─────────────────────────────────────────────────────────────┐
│                    Common Game Server                        │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │   Plugin    │  │   Plugin    │  │   Plugin    │  ...    │
│  │   MMORPG    │  │   Action    │  │   Battle    │         │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘         │
│         └────────────────┼────────────────┘                 │
│                          ▼                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Game Server Core (ECS)                   │  │
│  │  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐     │  │
│  │  │ World  │  │ Entity │  │ System │  │ Query  │     │  │
│  │  └────────┘  └────────┘  └────────┘  └────────┘     │  │
│  └──────────────────────────────────────────────────────┘  │
│                          ▼                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Foundation Adapters                      │  │
│  └──────────────────────────────────────────────────────┘  │
│                          ▼                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              7 Foundation Systems                     │  │
│  │  common │ thread │ logger │ network │ db │ mon │ cnt │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Product Functions

| Function | Description | PRD Reference |
|----------|-------------|---------------|
| Foundation Integration | Adapter layer for 7 foundation systems | FR-001 |
| ECS Core | Entity-Component-System game engine | FR-002 |
| Plugin Management | Dynamic loading of game modules | FR-003 |
| Game Logic | Ported game systems (Object, Combat, World) | FR-004 |
| Microservices | Distributed service architecture | FR-005 |

### 2.3 User Classes and Characteristics

| User Class | Technical Level | Primary Interactions | PRD Reference |
|------------|-----------------|---------------------|---------------|
| Game Developer | High | API, SDK, Documentation | Section 4.1 |
| System Architect | Expert | Architecture, Deployment | Section 4.1 |
| DevOps Engineer | High | K8s, Monitoring, Operations | Section 4.1 |
| Plugin Developer | High | Plugin API, Extension Points | Section 4.2 |
| QA Engineer | Medium | Testing Framework, Mocks | Section 4.2 |

### 2.4 Operating Environment

| Component | Specification | Notes |
|-----------|--------------|-------|
| OS | Linux (Ubuntu 22.04+), Windows Server 2019+ | Primary: Linux |
| CPU | x86_64, ARM64 | Multi-core required |
| Memory | 8GB+ per node | 16GB recommended |
| Network | 1Gbps+ | Low latency required |
| Storage | SSD recommended | For database and logging |

### 2.5 Design and Implementation Constraints

| Constraint | Description | PRD Reference |
|------------|-------------|---------------|
| C++20 | Primary language standard | Section 6.1 |
| CMake 3.20+ | Build system | Section 6.1 |
| Foundation Only | All features via foundation systems | Section 6.2 |
| No Direct Libraries | Use foundation wrappers | Section 6.2 |
| Result<T,E> Pattern | Error handling without exceptions | Section 6.2 |
| English Documentation | All code and docs | Section 6.2 |

### 2.6 Assumptions and Dependencies

**Assumptions**:
1. Foundation systems (v1.x) are stable and available
2. Target deployment platform supports Kubernetes
3. PostgreSQL 14+ available for production databases

**Dependencies** (from PRD Section 7):

| Dependency | Version | Category |
|------------|---------|----------|
| common_system | 1.x | Internal |
| thread_system | 1.x | Internal |
| logger_system | 1.x | Internal |
| network_system | 1.x | Internal |
| database_system | 1.x | Internal |
| monitoring_system | 1.x | Internal |
| container_system | 1.x | Internal |
| PostgreSQL | 14+ | External |
| OpenSSL | 1.1+ | External |
| yaml-cpp | 0.7+ | External |
| Google Test | 1.12+ | External |
| Google Benchmark | latest | External |

---

## 3. Specific Requirements

### 3.1 Foundation System Integration

#### SRS-FND-001: Common System Adapter

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-001.1 |
| **Priority** | P0 |
| **Description** | Integrate common_system for base types and Result<T,E> pattern |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-FND-001.1 | Shall provide game-specific type aliases for common_system types | Unit test |
| SRS-FND-001.2 | Shall wrap Result<T,E> with game-specific error types | Unit test |
| SRS-FND-001.3 | Shall provide dependency injection container interface | Integration test |
| SRS-FND-001.4 | Shall expose configuration management utilities | Integration test |

**Interface Definition**:

```cpp
namespace cgs::foundation {
    // Type aliases
    using EntityId = common::UniqueId;
    using PlayerId = common::UniqueId;

    // Result types
    template<typename T>
    using GameResult = common::Result<T, GameError>;

    // DI container
    class ServiceLocator {
    public:
        template<typename T>
        void Register(std::unique_ptr<T> service);

        template<typename T>
        T* Get();
    };
}
```

#### SRS-FND-002: Thread System Adapter

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-001.2 |
| **Priority** | P0 |
| **Description** | Integrate thread_system for job scheduling |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-FND-002.1 | Shall provide game tick scheduling API | Benchmark |
| SRS-FND-002.2 | Shall support job priorities (Critical, High, Normal, Low) | Unit test |
| SRS-FND-002.3 | Shall enable job dependencies and chaining | Unit test |
| SRS-FND-002.4 | Shall support at least 1.24M jobs/sec throughput | Performance test |

**Interface Definition**:

```cpp
namespace cgs::foundation {
    enum class JobPriority { Critical, High, Normal, Low };

    class GameJobScheduler {
    public:
        using JobId = uint64_t;
        using JobFunc = std::function<void()>;

        JobId Schedule(JobFunc job, JobPriority priority = JobPriority::Normal);
        JobId ScheduleAfter(JobId dependency, JobFunc job);
        void ScheduleTick(std::chrono::milliseconds interval, JobFunc job);
        void Wait(JobId id);
        void Cancel(JobId id);
    };
}
```

#### SRS-FND-003: Logger System Adapter

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-001.3 |
| **Priority** | P0 |
| **Description** | Integrate logger_system for structured logging |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-FND-003.1 | Shall provide game-specific log categories | Unit test |
| SRS-FND-003.2 | Shall support structured logging with context | Unit test |
| SRS-FND-003.3 | Shall enable log level filtering at runtime | Integration test |
| SRS-FND-003.4 | Shall support at least 4.3M msg/sec throughput | Performance test |

**Interface Definition**:

```cpp
namespace cgs::foundation {
    enum class LogCategory {
        Core, ECS, Network, Database, Plugin, Combat, World, AI
    };

    class GameLogger {
    public:
        void Log(LogLevel level, LogCategory cat, std::string_view msg);
        void LogWithContext(LogLevel level, LogCategory cat,
                           std::string_view msg, const LogContext& ctx);
        void SetCategoryLevel(LogCategory cat, LogLevel minLevel);
    };

    // Macros for convenience
    #define CGS_LOG_DEBUG(cat, msg) ...
    #define CGS_LOG_INFO(cat, msg) ...
    #define CGS_LOG_WARN(cat, msg) ...
    #define CGS_LOG_ERROR(cat, msg) ...
}
```

#### SRS-FND-004: Network System Adapter

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-001.4 |
| **Priority** | P0 |
| **Description** | Integrate network_system for TCP/UDP/WebSocket |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-FND-004.1 | Shall provide TCP connection management API | Integration test |
| SRS-FND-004.2 | Shall provide UDP packet handling API | Integration test |
| SRS-FND-004.3 | Shall provide WebSocket support for web clients | Integration test |
| SRS-FND-004.4 | Shall support message serialization/deserialization | Unit test |
| SRS-FND-004.5 | Shall support at least 305K msg/sec throughput | Performance test |

**Interface Definition**:

```cpp
namespace cgs::foundation {
    using SessionId = uint64_t;

    struct NetworkMessage {
        uint16_t opcode;
        std::vector<uint8_t> payload;
    };

    class GameNetworkManager {
    public:
        // Connection management
        void Listen(uint16_t port, Protocol protocol);
        void Close(SessionId session);
        void Broadcast(const NetworkMessage& msg);

        // Message handling
        void Send(SessionId session, const NetworkMessage& msg);
        void RegisterHandler(uint16_t opcode, MessageHandler handler);

        // Events
        Signal<SessionId> OnConnected;
        Signal<SessionId> OnDisconnected;
    };
}
```

#### SRS-FND-005: Database System Adapter

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-001.5 |
| **Priority** | P0 |
| **Description** | Integrate database_system for PostgreSQL/MySQL support |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-FND-005.1 | Shall provide connection pool management | Integration test |
| SRS-FND-005.2 | Shall support async query execution | Integration test |
| SRS-FND-005.3 | Shall provide prepared statement support | Unit test |
| SRS-FND-005.4 | Shall support transaction management | Integration test |
| SRS-FND-005.5 | Shall provide query result mapping to game types | Unit test |

**Interface Definition**:

```cpp
namespace cgs::foundation {
    class GameDatabase {
    public:
        // Connection
        GameResult<void> Connect(const DatabaseConfig& config);
        void Disconnect();

        // Query
        GameResult<QueryResult> Query(std::string_view sql);
        GameResult<QueryResult> QueryAsync(std::string_view sql);

        // Prepared statements
        GameResult<PreparedStatement> Prepare(std::string_view sql);
        GameResult<QueryResult> Execute(PreparedStatement& stmt, Args... args);

        // Transaction
        GameResult<Transaction> BeginTransaction();
    };
}
```

#### SRS-FND-006: Monitoring System Adapter

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-001.6 |
| **Priority** | P0 |
| **Description** | Integrate monitoring_system for metrics and tracing |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-FND-006.1 | Shall provide game-specific metrics (CCU, TPS, latency) | Integration test |
| SRS-FND-006.2 | Shall support distributed tracing for cross-service calls | Integration test |
| SRS-FND-006.3 | Shall expose metrics endpoint for Prometheus scraping | Integration test |
| SRS-FND-006.4 | Shall provide health check endpoints | Integration test |

**Interface Definition**:

```cpp
namespace cgs::foundation {
    class GameMetrics {
    public:
        // Counters
        void IncrementCounter(std::string_view name, uint64_t value = 1);

        // Gauges
        void SetGauge(std::string_view name, double value);

        // Histograms
        void RecordHistogram(std::string_view name, double value);

        // Tracing
        TraceSpan StartSpan(std::string_view name);
        void EndSpan(TraceSpan& span);
    };
}
```

#### SRS-FND-007: Container System Adapter

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-001.7 |
| **Priority** | P0 |
| **Description** | Integrate container_system for serialization |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-FND-007.1 | Shall provide binary serialization for network messages | Unit test |
| SRS-FND-007.2 | Shall provide JSON serialization for configuration | Unit test |
| SRS-FND-007.3 | Shall support schema versioning for backward compatibility | Unit test |
| SRS-FND-007.4 | Shall support at least 2M serializations/sec | Performance test |

**Interface Definition**:

```cpp
namespace cgs::foundation {
    class GameSerializer {
    public:
        // Binary
        template<typename T>
        std::vector<uint8_t> SerializeBinary(const T& obj);

        template<typename T>
        GameResult<T> DeserializeBinary(std::span<const uint8_t> data);

        // JSON
        template<typename T>
        std::string SerializeJson(const T& obj);

        template<typename T>
        GameResult<T> DeserializeJson(std::string_view json);
    };
}
```

---

### 3.2 Entity-Component System (ECS)

#### SRS-ECS-001: Component Storage

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-002.1 |
| **Priority** | P0 |
| **Description** | Implement sparse set-based component storage |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-ECS-001.1 | Shall use sparse set for O(1) component access | Unit test |
| SRS-ECS-001.2 | Shall support cache-friendly iteration | Performance test |
| SRS-ECS-001.3 | Shall provide component type registration at compile time | Unit test |
| SRS-ECS-001.4 | Shall support component versioning for change detection | Unit test |

**Interface Definition**:

```cpp
namespace cgs::ecs {
    template<typename T>
    class ComponentStorage {
    public:
        T& Add(Entity entity, T&& component);
        T& Get(Entity entity);
        bool Has(Entity entity) const;
        void Remove(Entity entity);

        // Iteration
        auto begin() -> iterator;
        auto end() -> iterator;
        size_t Size() const;
    };
}
```

#### SRS-ECS-002: Entity Management

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-002.2 |
| **Priority** | P0 |
| **Description** | Support entity creation/destruction at runtime |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-ECS-002.1 | Shall generate unique entity IDs with version counter | Unit test |
| SRS-ECS-002.2 | Shall support deferred entity destruction (end of frame) | Unit test |
| SRS-ECS-002.3 | Shall recycle entity IDs with version increment | Unit test |
| SRS-ECS-002.4 | Shall provide entity validity checking | Unit test |

**Interface Definition**:

```cpp
namespace cgs::ecs {
    struct Entity {
        uint32_t id;
        uint32_t version;

        bool IsValid() const;
        bool operator==(const Entity& other) const;
    };

    class EntityManager {
    public:
        Entity Create();
        void Destroy(Entity entity);
        bool IsAlive(Entity entity) const;
        size_t Count() const;
    };
}
```

#### SRS-ECS-003: System Scheduling

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-002.3 |
| **Priority** | P0 |
| **Description** | Provide system scheduling with dependency management |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-ECS-003.1 | Shall support system execution ordering | Unit test |
| SRS-ECS-003.2 | Shall detect and prevent circular dependencies | Unit test |
| SRS-ECS-003.3 | Shall provide fixed update (physics) and variable update stages | Unit test |
| SRS-ECS-003.4 | Shall support system enable/disable at runtime | Unit test |

**Interface Definition**:

```cpp
namespace cgs::ecs {
    enum class SystemStage { PreUpdate, Update, PostUpdate, FixedUpdate };

    class ISystem {
    public:
        virtual ~ISystem() = default;
        virtual void Update(float deltaTime) = 0;
        virtual SystemStage GetStage() const { return SystemStage::Update; }
    };

    class SystemScheduler {
    public:
        template<typename T>
        void Register();

        void AddDependency(TypeId before, TypeId after);
        void SetEnabled(TypeId system, bool enabled);
        void Execute(float deltaTime);
    };
}
```

#### SRS-ECS-004: Parallel System Execution

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-002.4 |
| **Priority** | P1 |
| **Description** | Enable parallel system execution where safe |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-ECS-004.1 | Shall detect read/write component access patterns | Unit test |
| SRS-ECS-004.2 | Shall automatically parallelize non-conflicting systems | Performance test |
| SRS-ECS-004.3 | Shall provide manual sync points for explicit ordering | Unit test |
| SRS-ECS-004.4 | Shall support thread-local scratch memory | Unit test |

#### SRS-ECS-005: Component Queries

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-002.5 |
| **Priority** | P0 |
| **Description** | Support component queries with filters |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-ECS-005.1 | Shall support multi-component queries | Unit test |
| SRS-ECS-005.2 | Shall support include/exclude filters | Unit test |
| SRS-ECS-005.3 | Shall support optional component access | Unit test |
| SRS-ECS-005.4 | Shall cache query results for repeated access | Performance test |

**Interface Definition**:

```cpp
namespace cgs::ecs {
    template<typename... Includes>
    class Query {
    public:
        Query& Exclude();
        Query& Optional();

        void ForEach(auto&& func);
        size_t Count() const;

        auto begin() -> iterator;
        auto end() -> iterator;
    };

    // Usage
    Query<Position, Velocity> movables;
    movables.Exclude<Static>().ForEach([](Entity e, Position& pos, Velocity& vel) {
        pos.x += vel.x * deltaTime;
        pos.y += vel.y * deltaTime;
    });
}
```

---

### 3.3 Plugin System

#### SRS-PLG-001: Plugin Interface

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-003.1 |
| **Priority** | P0 |
| **Description** | Define plugin interface for game logic modules |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-PLG-001.1 | Shall define standard plugin entry points | Unit test |
| SRS-PLG-001.2 | Shall provide plugin metadata (name, version, dependencies) | Unit test |
| SRS-PLG-001.3 | Shall expose plugin API version for compatibility checking | Unit test |
| SRS-PLG-001.4 | Shall support both static and dynamic linking | Integration test |

**Interface Definition**:

```cpp
namespace cgs::plugin {
    struct PluginInfo {
        std::string name;
        Version version;
        std::vector<std::string> dependencies;
        uint32_t apiVersion;
    };

    class IPlugin {
    public:
        virtual ~IPlugin() = default;
        virtual const PluginInfo& GetInfo() const = 0;
        virtual bool OnLoad(PluginContext& ctx) = 0;
        virtual void OnUnload() = 0;
    };

    // Export macro
    #define CGS_PLUGIN_EXPORT(PluginClass) \
        extern "C" IPlugin* CreatePlugin() { return new PluginClass(); }
}
```

#### SRS-PLG-002: Plugin Lifecycle

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-003.2 |
| **Priority** | P0 |
| **Description** | Support plugin lifecycle management |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-PLG-002.1 | Shall support Load phase (resource acquisition) | Integration test |
| SRS-PLG-002.2 | Shall support Init phase (system registration) | Integration test |
| SRS-PLG-002.3 | Shall support Update phase (per-frame execution) | Integration test |
| SRS-PLG-002.4 | Shall support Shutdown phase (cleanup) | Integration test |

**State Machine**:

```
[Unloaded] --> Load() --> [Loaded] --> Init() --> [Active]
                                                      |
                                                      v
[Unloaded] <-- Unload() <-- [Loaded] <-- Shutdown() --+
```

#### SRS-PLG-003: Plugin Communication

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-003.3 |
| **Priority** | P1 |
| **Description** | Enable plugin-to-plugin communication via events |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-PLG-003.1 | Shall provide type-safe event publishing | Unit test |
| SRS-PLG-003.2 | Shall support synchronous and asynchronous event delivery | Unit test |
| SRS-PLG-003.3 | Shall allow event filtering by type and priority | Unit test |
| SRS-PLG-003.4 | Shall provide event queue for deferred processing | Unit test |

**Interface Definition**:

```cpp
namespace cgs::plugin {
    class EventBus {
    public:
        template<typename E>
        void Subscribe(std::function<void(const E&)> handler);

        template<typename E>
        void Publish(const E& event);

        template<typename E>
        void PublishDeferred(E event);

        void ProcessDeferred();
    };
}
```

#### SRS-PLG-004: Plugin Dependencies

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-003.4 |
| **Priority** | P1 |
| **Description** | Provide plugin dependency resolution |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-PLG-004.1 | Shall resolve plugin load order from dependencies | Unit test |
| SRS-PLG-004.2 | Shall detect circular dependencies and report error | Unit test |
| SRS-PLG-004.3 | Shall support version constraints (>=, <, ~=) | Unit test |
| SRS-PLG-004.4 | Shall fail gracefully if dependencies cannot be satisfied | Unit test |

#### SRS-PLG-005: Hot Reload

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-003.5 |
| **Priority** | P2 |
| **Description** | Support hot-reload for development |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-PLG-005.1 | Shall detect plugin file changes | Integration test |
| SRS-PLG-005.2 | Shall safely unload and reload modified plugins | Integration test |
| SRS-PLG-005.3 | Shall preserve plugin state across reload where possible | Integration test |
| SRS-PLG-005.4 | Shall be disabled in production builds | Build test |

---

### 3.4 Game Logic Layer

#### SRS-GML-001: Object System

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-004.1 |
| **Priority** | P0 |
| **Description** | Port Object/Unit/Player/Creature systems to ECS |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-GML-001.1 | Shall provide Transform component (Position, Rotation, Scale) | Unit test |
| SRS-GML-001.2 | Shall provide Identity component (GUID, Name, Type) | Unit test |
| SRS-GML-001.3 | Shall provide Stats component (Health, Mana, Attributes) | Unit test |
| SRS-GML-001.4 | Shall provide Movement component (Speed, Direction, State) | Unit test |
| SRS-GML-001.5 | Shall implement ObjectUpdateSystem for per-tick processing | Integration test |

**Component Definitions**:

```cpp
namespace cgs::game {
    struct Transform {
        Vector3 position;
        Quaternion rotation;
        Vector3 scale;
    };

    struct Identity {
        GUID guid;
        std::string name;
        ObjectType type;
        uint32_t entry;  // Template ID
    };

    struct Stats {
        int32_t health;
        int32_t maxHealth;
        int32_t mana;
        int32_t maxMana;
        std::array<int32_t, MAX_ATTRIBUTES> attributes;
    };

    struct Movement {
        float speed;
        float baseSpeed;
        Vector3 direction;
        MovementState state;
    };
}
```

#### SRS-GML-002: Combat System

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-004.2 |
| **Priority** | P0 |
| **Description** | Port Combat system (Spell, Aura, Damage) to ECS |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-GML-002.1 | Shall provide SpellCast component with cast state machine | Unit test |
| SRS-GML-002.2 | Shall provide Aura component for buff/debuff tracking | Unit test |
| SRS-GML-002.3 | Shall provide DamageEvent for damage calculation pipeline | Unit test |
| SRS-GML-002.4 | Shall implement CombatSystem for damage/heal processing | Integration test |
| SRS-GML-002.5 | Shall support damage types (Physical, Magic, etc.) | Unit test |

**Component Definitions**:

```cpp
namespace cgs::game {
    struct SpellCast {
        uint32_t spellId;
        Entity target;
        CastState state;
        float castTime;
        float remainingTime;
    };

    struct AuraHolder {
        struct AuraInstance {
            uint32_t auraId;
            Entity caster;
            int32_t stacks;
            float duration;
            float remainingTime;
        };
        std::vector<AuraInstance> auras;
    };

    struct DamageEvent {
        Entity attacker;
        Entity victim;
        DamageType type;
        int32_t baseDamage;
        int32_t finalDamage;
        bool isCritical;
    };
}
```

#### SRS-GML-003: World System

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-004.3 |
| **Priority** | P0 |
| **Description** | Port World system (Map, Zone, Grid) to ECS |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-GML-003.1 | Shall provide Map component for world instance data | Unit test |
| SRS-GML-003.2 | Shall provide Zone component for area-based rules | Unit test |
| SRS-GML-003.3 | Shall implement spatial partitioning (Grid/Quadtree) | Performance test |
| SRS-GML-003.4 | Shall support interest management for network optimization | Integration test |
| SRS-GML-003.5 | Shall support map transitions and teleportation | Integration test |

**Component Definitions**:

```cpp
namespace cgs::game {
    struct MapInstance {
        uint32_t mapId;
        uint32_t instanceId;
        MapType type;
    };

    struct Zone {
        uint32_t zoneId;
        ZoneType type;
        ZoneFlags flags;
    };

    struct GridCell {
        int32_t x, y;
        std::vector<Entity> entities;
    };

    class SpatialIndex {
    public:
        void Insert(Entity entity, const Vector3& position);
        void Update(Entity entity, const Vector3& newPosition);
        void Remove(Entity entity);
        std::vector<Entity> QueryRadius(const Vector3& center, float radius);
        std::vector<Entity> QueryCell(int32_t x, int32_t y);
    };
}
```

#### SRS-GML-004: AI System

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-004.4 |
| **Priority** | P1 |
| **Description** | Port AI system (Brain, BehaviorTree) to ECS |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-GML-004.1 | Shall provide AIBrain component for AI state | Unit test |
| SRS-GML-004.2 | Shall support Behavior Tree execution | Unit test |
| SRS-GML-004.3 | Shall provide common AI tasks (MoveTo, Attack, Patrol) | Integration test |
| SRS-GML-004.4 | Shall support AI update throttling for performance | Performance test |

#### SRS-GML-005: Quest System

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-004.5 |
| **Priority** | P1 |
| **Description** | Port Quest system to ECS |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-GML-005.1 | Shall provide QuestLog component for player quest tracking | Unit test |
| SRS-GML-005.2 | Shall support quest objectives (Kill, Collect, Explore) | Unit test |
| SRS-GML-005.3 | Shall support quest chains and prerequisites | Unit test |
| SRS-GML-005.4 | Shall integrate with event system for objective updates | Integration test |

#### SRS-GML-006: Inventory System

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-004.6 |
| **Priority** | P1 |
| **Description** | Port Inventory system to ECS |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-GML-006.1 | Shall provide Inventory component for item storage | Unit test |
| SRS-GML-006.2 | Shall support item stacking and splitting | Unit test |
| SRS-GML-006.3 | Shall support equipment slots and stat bonuses | Unit test |
| SRS-GML-006.4 | Shall support item durability and modification | Unit test |

---

### 3.5 Microservices

#### SRS-SVC-001: Authentication Server

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-005.1 |
| **Priority** | P0 |
| **Description** | Implement AuthServer for authentication |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-SVC-001.1 | Shall support user registration with email validation | Integration test |
| SRS-SVC-001.2 | Shall support user login with username/password | Integration test |
| SRS-SVC-001.3 | Shall issue JWT access tokens with configurable expiry | Unit test |
| SRS-SVC-001.4 | Shall issue refresh tokens for token renewal | Unit test |
| SRS-SVC-001.5 | Shall support token revocation | Integration test |
| SRS-SVC-001.6 | Shall hash passwords using bcrypt or argon2 | Security test |

**API Endpoints**:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/auth/register` | POST | Register new user |
| `/auth/login` | POST | Authenticate and get tokens |
| `/auth/refresh` | POST | Refresh access token |
| `/auth/logout` | POST | Revoke tokens |
| `/auth/validate` | GET | Validate access token |

#### SRS-SVC-002: Gateway Server

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-005.2 |
| **Priority** | P0 |
| **Description** | Implement GatewayServer for client routing |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-SVC-002.1 | Shall accept client connections (TCP/WebSocket) | Integration test |
| SRS-SVC-002.2 | Shall validate authentication tokens | Integration test |
| SRS-SVC-002.3 | Shall route messages to appropriate game servers | Integration test |
| SRS-SVC-002.4 | Shall support connection migration on server transfer | Integration test |
| SRS-SVC-002.5 | Shall handle rate limiting per client | Integration test |

**Message Flow**:

```
Client <---> Gateway <---> AuthServer
                |
                +--------> GameServer
                +--------> LobbyServer
```

#### SRS-SVC-003: Game Server

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-005.3 |
| **Priority** | P0 |
| **Description** | Implement GameServer for world simulation |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-SVC-003.1 | Shall run ECS world simulation at 20 Hz | Performance test |
| SRS-SVC-003.2 | Shall manage map instances and player sessions | Integration test |
| SRS-SVC-003.3 | Shall execute game logic plugins | Integration test |
| SRS-SVC-003.4 | Shall synchronize world state to clients | Integration test |
| SRS-SVC-003.5 | Shall persist player data to database | Integration test |

#### SRS-SVC-004: Lobby Server

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-005.4 |
| **Priority** | P1 |
| **Description** | Implement LobbyServer for matchmaking |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-SVC-004.1 | Shall manage matchmaking queues | Unit test |
| SRS-SVC-004.2 | Shall support skill-based matchmaking (ELO/MMR) | Unit test |
| SRS-SVC-004.3 | Shall support party/group management | Integration test |
| SRS-SVC-004.4 | Shall allocate game server instances for matches | Integration test |

#### SRS-SVC-005: Database Proxy

| Attribute | Value |
|-----------|-------|
| **PRD Trace** | FR-005.5 |
| **Priority** | P1 |
| **Description** | Implement DBProxy for database connection pooling |

**Functional Requirements**:

| ID | Requirement | Verification |
|----|-------------|--------------|
| SRS-SVC-005.1 | Shall maintain connection pool to database | Integration test |
| SRS-SVC-005.2 | Shall route queries from game servers | Integration test |
| SRS-SVC-005.3 | Shall provide query caching for read-heavy data | Performance test |
| SRS-SVC-005.4 | Shall support read replicas for load distribution | Integration test |

---

## 4. External Interface Requirements

### 4.1 User Interfaces

Not applicable - this is a server-side framework.

### 4.2 Hardware Interfaces

| Interface | Specification |
|-----------|---------------|
| CPU | x86_64 or ARM64 with SIMD support (SSE4.2/AVX/NEON) |
| Memory | Minimum 8GB, 16GB recommended |
| Storage | SSD for database and logging |
| Network | 1Gbps minimum, 10Gbps recommended |

### 4.3 Software Interfaces

| Interface | Protocol | Description |
|-----------|----------|-------------|
| Database | PostgreSQL wire protocol | Primary data storage |
| Metrics | Prometheus exposition format | Metrics endpoint |
| Tracing | OpenTelemetry (OTLP) | Distributed tracing |
| Health | HTTP REST | Health check endpoints |

### 4.4 Communication Interfaces

| Interface | Protocol | Port | Description |
|-----------|----------|------|-------------|
| Client-Gateway | TCP/TLS | 8080 | Game client connections |
| Client-Gateway | WebSocket/TLS | 8081 | Web client connections |
| Service-Service | gRPC | 50051+ | Internal service communication |
| Admin | HTTP REST | 9090 | Administration API |

---

## 5. Non-Functional Requirements

### 5.1 Performance Requirements

| ID | Requirement | Target | PRD Trace | Verification |
|----|-------------|--------|-----------|--------------|
| SRS-NFR-001 | Message throughput | ≥300,000 msg/sec | NFR-001.1 | Load test |
| SRS-NFR-002 | World tick latency | ≤50ms (20 Hz) | NFR-001.2 | Profiling |
| SRS-NFR-003 | Entity update (10K) | ≤5ms | NFR-001.3 | Benchmark |
| SRS-NFR-004 | Database query p99 | ≤50ms | NFR-001.4 | Monitoring |
| SRS-NFR-005 | Memory per 1K players | ≤100MB | NFR-001.5 | Profiling |
| SRS-NFR-006 | Plugin load time | ≤100ms | Section 1.3 | Startup metrics |

### 5.2 Scalability Requirements

| ID | Requirement | Target | PRD Trace | Verification |
|----|-------------|--------|-----------|--------------|
| SRS-NFR-007 | Horizontal scaling | Linear to 100 nodes | NFR-002.1 | Scale test |
| SRS-NFR-008 | CCU per node | ≥1,000 | NFR-002.2 | Load test |
| SRS-NFR-009 | Service discovery | Auto via K8s | NFR-002.3 | Integration test |
| SRS-NFR-010 | Total CCU per cluster | ≥10,000 | Section 1.3 | Load test |

### 5.3 Reliability Requirements

| ID | Requirement | Target | PRD Trace | Verification |
|----|-------------|--------|-----------|--------------|
| SRS-NFR-011 | Uptime | 99.9% | NFR-003.1 | Monitoring |
| SRS-NFR-012 | MTTR | ≤5 minutes | NFR-003.2 | Incident drill |
| SRS-NFR-013 | Data durability | Zero loss on crash | NFR-003.3 | Fault injection |

### 5.4 Security Requirements

| ID | Requirement | Target | PRD Trace | Verification |
|----|-------------|--------|-----------|--------------|
| SRS-NFR-014 | Authentication | JWT + refresh | NFR-004.1 | Security test |
| SRS-NFR-015 | Encryption | TLS 1.3 | NFR-004.2 | Security audit |
| SRS-NFR-016 | Input validation | All client inputs | NFR-004.3 | Penetration test |

### 5.5 Maintainability Requirements

| ID | Requirement | Target | Verification |
|----|-------------|--------|--------------|
| SRS-NFR-017 | Code coverage | ≥80% | CI pipeline |
| SRS-NFR-018 | Documentation | API docs for all public interfaces | Review |
| SRS-NFR-019 | Logging | Structured logs with correlation IDs | Review |

---

## 6. Traceability Matrix

### 6.1 PRD to SRS Traceability

| PRD ID | PRD Description | SRS IDs |
|--------|-----------------|---------|
| FR-001.1 | common_system integration | SRS-FND-001.1, SRS-FND-001.2, SRS-FND-001.3, SRS-FND-001.4 |
| FR-001.2 | thread_system integration | SRS-FND-002.1, SRS-FND-002.2, SRS-FND-002.3, SRS-FND-002.4 |
| FR-001.3 | logger_system integration | SRS-FND-003.1, SRS-FND-003.2, SRS-FND-003.3, SRS-FND-003.4 |
| FR-001.4 | network_system integration | SRS-FND-004.1, SRS-FND-004.2, SRS-FND-004.3, SRS-FND-004.4, SRS-FND-004.5 |
| FR-001.5 | database_system integration | SRS-FND-005.1, SRS-FND-005.2, SRS-FND-005.3, SRS-FND-005.4, SRS-FND-005.5 |
| FR-001.6 | monitoring_system integration | SRS-FND-006.1, SRS-FND-006.2, SRS-FND-006.3, SRS-FND-006.4 |
| FR-001.7 | container_system integration | SRS-FND-007.1, SRS-FND-007.2, SRS-FND-007.3, SRS-FND-007.4 |
| FR-002.1 | Sparse set component storage | SRS-ECS-001.1, SRS-ECS-001.2, SRS-ECS-001.3, SRS-ECS-001.4 |
| FR-002.2 | Entity creation/destruction | SRS-ECS-002.1, SRS-ECS-002.2, SRS-ECS-002.3, SRS-ECS-002.4 |
| FR-002.3 | System scheduling | SRS-ECS-003.1, SRS-ECS-003.2, SRS-ECS-003.3, SRS-ECS-003.4 |
| FR-002.4 | Parallel system execution | SRS-ECS-004.1, SRS-ECS-004.2, SRS-ECS-004.3, SRS-ECS-004.4 |
| FR-002.5 | Component queries | SRS-ECS-005.1, SRS-ECS-005.2, SRS-ECS-005.3, SRS-ECS-005.4 |
| FR-003.1 | Plugin interface | SRS-PLG-001.1, SRS-PLG-001.2, SRS-PLG-001.3, SRS-PLG-001.4 |
| FR-003.2 | Plugin lifecycle | SRS-PLG-002.1, SRS-PLG-002.2, SRS-PLG-002.3, SRS-PLG-002.4 |
| FR-003.3 | Plugin communication | SRS-PLG-003.1, SRS-PLG-003.2, SRS-PLG-003.3, SRS-PLG-003.4 |
| FR-003.4 | Plugin dependencies | SRS-PLG-004.1, SRS-PLG-004.2, SRS-PLG-004.3, SRS-PLG-004.4 |
| FR-003.5 | Hot reload | SRS-PLG-005.1, SRS-PLG-005.2, SRS-PLG-005.3, SRS-PLG-005.4 |
| FR-004.1 | Object/Unit/Player/Creature | SRS-GML-001.1, SRS-GML-001.2, SRS-GML-001.3, SRS-GML-001.4, SRS-GML-001.5 |
| FR-004.2 | Combat system | SRS-GML-002.1, SRS-GML-002.2, SRS-GML-002.3, SRS-GML-002.4, SRS-GML-002.5 |
| FR-004.3 | World system | SRS-GML-003.1, SRS-GML-003.2, SRS-GML-003.3, SRS-GML-003.4, SRS-GML-003.5 |
| FR-004.4 | AI system | SRS-GML-004.1, SRS-GML-004.2, SRS-GML-004.3, SRS-GML-004.4 |
| FR-004.5 | Quest system | SRS-GML-005.1, SRS-GML-005.2, SRS-GML-005.3, SRS-GML-005.4 |
| FR-004.6 | Inventory system | SRS-GML-006.1, SRS-GML-006.2, SRS-GML-006.3, SRS-GML-006.4 |
| FR-005.1 | AuthServer | SRS-SVC-001.1, SRS-SVC-001.2, SRS-SVC-001.3, SRS-SVC-001.4, SRS-SVC-001.5, SRS-SVC-001.6 |
| FR-005.2 | GatewayServer | SRS-SVC-002.1, SRS-SVC-002.2, SRS-SVC-002.3, SRS-SVC-002.4, SRS-SVC-002.5 |
| FR-005.3 | GameServer | SRS-SVC-003.1, SRS-SVC-003.2, SRS-SVC-003.3, SRS-SVC-003.4, SRS-SVC-003.5 |
| FR-005.4 | LobbyServer | SRS-SVC-004.1, SRS-SVC-004.2, SRS-SVC-004.3, SRS-SVC-004.4 |
| FR-005.5 | DBProxy | SRS-SVC-005.1, SRS-SVC-005.2, SRS-SVC-005.3, SRS-SVC-005.4 |
| NFR-001.1 | Message throughput | SRS-NFR-001 |
| NFR-001.2 | World tick latency | SRS-NFR-002 |
| NFR-001.3 | Entity update | SRS-NFR-003 |
| NFR-001.4 | Database latency | SRS-NFR-004 |
| NFR-001.5 | Memory usage | SRS-NFR-005 |
| NFR-002.1 | Horizontal scaling | SRS-NFR-007 |
| NFR-002.2 | CCU per node | SRS-NFR-008 |
| NFR-002.3 | Service discovery | SRS-NFR-009 |
| NFR-003.1 | Uptime | SRS-NFR-011 |
| NFR-003.2 | MTTR | SRS-NFR-012 |
| NFR-003.3 | Data durability | SRS-NFR-013 |
| NFR-004.1 | Authentication | SRS-NFR-014 |
| NFR-004.2 | Encryption | SRS-NFR-015 |
| NFR-004.3 | Input validation | SRS-NFR-016 |

### 6.2 SRS Coverage Summary

| Category | PRD Requirements | SRS Requirements | Coverage |
|----------|------------------|------------------|----------|
| Foundation (FR-001) | 7 | 26 | 100% |
| ECS (FR-002) | 5 | 17 | 100% |
| Plugin (FR-003) | 5 | 17 | 100% |
| Game Logic (FR-004) | 6 | 23 | 100% |
| Services (FR-005) | 5 | 24 | 100% |
| Non-Functional | 12 | 19 | 100% |
| **Total** | **40** | **126** | **100%** |

---

## 7. Appendices

### 7.1 Change History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1.0.0 | 2026-02-03 | - | Initial draft based on PRD v0.1.0.0 |

### 7.2 Open Issues

| ID | Issue | Status | Resolution |
|----|-------|--------|------------|
| OI-001 | Hot reload scope and limitations | Open | Requires further technical analysis |
| OI-002 | Parallel ECS scheduling algorithm | Open | Benchmark different approaches |
| OI-003 | Matchmaking algorithm details | Open | Dependent on game requirements |

### 7.3 Related Documents

- [PRD.md](./PRD.md) - Product Requirements Document
- [ARCHITECTURE.md](./ARCHITECTURE.md) - Technical architecture design
- [reference/ECS_DESIGN.md](./reference/ECS_DESIGN.md) - ECS architecture details
- [reference/PLUGIN_SYSTEM.md](./reference/PLUGIN_SYSTEM.md) - Plugin system design
- [reference/FOUNDATION_ADAPTERS.md](./reference/FOUNDATION_ADAPTERS.md) - Adapter patterns

---

*Document Approval*

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Technical Lead | | | |
| Architect | | | |
| QA Lead | | | |
