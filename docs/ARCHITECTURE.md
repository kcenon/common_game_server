# Architecture

> **English** · [한국어](ARCHITECTURE.kr.md)

This document describes the high-level architecture of `common_game_server`.
For internal details of specific subsystems, see the [`advanced/`](advanced/)
directory.

## Layered Model

`common_game_server` is organized as 6 layers, with strict downward dependency
flow. Higher layers may depend on lower layers, but never the reverse.

```
+-------------------------------------------------------------------+
|  Layer 6: PLUGIN LAYER                                             |
|  ----------------------------------------------------------------- |
|  MMORPG / Battle Royale / RTS / Custom plugins                     |
|  Hot reload (dev), dependency resolution, event communication      |
+-------------------------------------------------------------------+
|  Layer 5: GAME LOGIC LAYER                                         |
|  ----------------------------------------------------------------- |
|  Object · Combat · World · AI (BehaviorTree) · Quest · Inventory   |
+-------------------------------------------------------------------+
|  Layer 4: CORE ECS LAYER                                           |
|  ----------------------------------------------------------------- |
|  EntityManager · ComponentPool (SparseSet) · SystemScheduler (DAG) |
|  Query (compile-time concepts) · Parallel execution                |
+-------------------------------------------------------------------+
|  Layer 3: SERVICE LAYER                                            |
|  ----------------------------------------------------------------- |
|  AuthServer · GatewayServer · GameServer · LobbyServer · DBProxy   |
|  Shared service_runner template, YAML configuration                |
+-------------------------------------------------------------------+
|  Layer 2: FOUNDATION ADAPTER LAYER                                 |
|  ----------------------------------------------------------------- |
|  IGameLogger · IGameNetwork · IGameDatabase · IGameThreadPool      |
|  IGameMonitor · IGameContainer · IGameCommon                       |
|  Result<T, E> error model, no exceptions across boundaries         |
+-------------------------------------------------------------------+
|  Layer 1: KCENON FOUNDATION SYSTEMS                                |
|  ----------------------------------------------------------------- |
|  common_system (Tier 0) · thread_system · container_system         |
|  logger_system · monitoring_system · database_system               |
|  network_system                                                    |
+-------------------------------------------------------------------+
```

## Layer Responsibilities

### Layer 1 — kcenon Foundation Systems

External dependencies fetched via FetchContent or vcpkg. `common_game_server`
pins to release tags (currently v0.2.0+). See [`../DEPENDENCY_MATRIX.md`](../DEPENDENCY_MATRIX.md).

**Key principle**: `common_game_server` does not modify or fork the foundation
systems. All ecosystem improvements are upstreamed to the respective repos.

### Layer 2 — Foundation Adapter Layer

Thin wrappers translating kcenon APIs into game-server-friendly interfaces.

**Why a wrapper layer?**
1. Game-specific naming (`IGameLogger` vs. `kcenon::logger::ILogger`)
2. Consistent error model (`cgs::core::Result<T, E>` everywhere)
3. Test mocking — adapters can be replaced for unit tests
4. Insulation from upstream API changes

Headers: [`include/cgs/foundation/`](../include/cgs/foundation/)
Implementation: [`src/foundation/`](../src/foundation/)
Detail: [`advanced/FOUNDATION_ADAPTERS.md`](advanced/FOUNDATION_ADAPTERS.md)

### Layer 3 — Service Layer

Five microservices, each a standalone executable:

| Service | Role |
|---------|------|
| AuthServer | Authentication, JWT issuance, session store |
| GatewayServer | Client routing, load balancing, rate limiting |
| GameServer | World simulation, game tick loop |
| LobbyServer | Matchmaking, party management |
| DBProxy | Connection pooling, prepared statements |

All services share the `service_runner` template ([`src/services/shared/`](../src/services/shared/)),
which handles:
- YAML config loading
- Foundation adapter wiring
- Signal handling (SIGINT, SIGTERM)
- Graceful shutdown
- Health/readiness probe endpoints

Implementation: [`src/services/`](../src/services/)
Detail: [`adr/ADR-004-microservice-decomposition.md`](adr/ADR-004-microservice-decomposition.md)

### Layer 4 — Core ECS Layer

Data-oriented Entity-Component System.

**Key types**:
- `EntityManager` — entity creation/destruction, version recycling
- `ComponentPool<T>` — sparse-set storage for component type T
- `SystemScheduler` — DAG-based system ordering with parallel execution
- `Query<Components...>` — compile-time-checked component iteration

**Performance characteristics**:
- Cache-friendly SoA component layout
- O(1) entity lookup, O(1) component add/remove
- 10,000 entities processed in <5 ms (single tick)

Headers: [`include/cgs/ecs/`](../include/cgs/ecs/)
Implementation: [`src/ecs/`](../src/ecs/)
Detail: [`advanced/ECS_DEEP_DIVE.md`](advanced/ECS_DEEP_DIVE.md) ·
[`adr/ADR-002-entity-component-system.md`](adr/ADR-002-entity-component-system.md)

### Layer 5 — Game Logic Layer

Concrete ECS systems implementing canonical game-server features:

| System | Components | Purpose |
|--------|-----------|---------|
| ObjectSystem | Position, Velocity, Rotation | Entity transforms |
| CombatSystem | Health, Attack, Defense | Damage resolution |
| WorldSystem | MapInstance, Region | World state and zones |
| AISystem | BehaviorTree, AIState | NPC decision making |
| QuestSystem | QuestProgress, QuestObjective | Quest tracking |
| InventorySystem | Inventory, ItemSlot | Player inventory |

These are reference implementations — plugins can replace or extend them.

Headers: [`include/cgs/game/`](../include/cgs/game/)
Implementation: [`src/game/`](../src/game/)

### Layer 6 — Plugin Layer

User-authored game logic, dynamically loaded.

**Lifecycle**:
1. `PluginManager::load(path)` — opens shared library, calls `cgs_create_plugin()`
2. `GamePlugin::on_load(ctx)` — plugin initialization
3. `GamePlugin::on_tick(dt)` — called every world tick (50 ms)
4. `GamePlugin::on_unload()` — cleanup before shared library close

**Hot reload** (`CGS_HOT_RELOAD=ON`, dev only):
- File watcher monitors `plugins/` directory
- On change: gracefully unloads old version, loads new
- State migration via plugin-defined `serialize`/`deserialize` hooks

**Sample plugin**: `src/plugins/mmorpg/` — full MMORPG game loop reference.

Headers: [`include/cgs/plugin/`](../include/cgs/plugin/)
Implementation: [`src/plugins/`](../src/plugins/)
Detail: [`adr/ADR-003-plugin-hot-reload.md`](adr/ADR-003-plugin-hot-reload.md)

## Cross-Cutting Concerns

### Error Handling

Every fallible operation returns `cgs::core::Result<T, E>`. Exceptions are
forbidden across layer boundaries. The error type `E` is typically
`cgs::core::error_code` or a domain-specific enum.

Why no exceptions?
- Predictable performance (no stack unwinding cost on hot paths)
- Plugin ABI safety (exceptions don't cross shared library boundaries reliably)
- Consistent with the kcenon ecosystem (`common_system::Result<T>`)

### Logging

All logging flows through `IGameLogger`. Logs are emitted as structured JSON
with mandatory fields: `timestamp`, `level`, `service`, `correlation_id`,
`message`. Correlation IDs are generated at the gateway and propagated through
every service hop and ECS system invocation.

### Metrics

All services expose Prometheus metrics on port 9090 (`/metrics`). Standard
metrics: request count, request duration histograms, error count, active
connections, world tick duration.

### Configuration

Each service loads a YAML config file. Schemas are documented in
[`guides/CONFIGURATION_GUIDE.md`](guides/CONFIGURATION_GUIDE.md). Hot reload
of config is supported via SIGHUP.

## Deployment Topology

```
                    ┌─────────────────┐
                    │  Game Clients   │
                    └────────┬────────┘
                             │ TLS 1.3
                    ┌────────▼────────┐
                    │ GatewayServer   │ (n replicas, HPA)
                    └────────┬────────┘
                             │
                    ┌────────┼────────────────┬────────┐
                    │        │                │        │
            ┌───────▼───┐ ┌──▼────┐ ┌───▼──┐ ┌──▼─────┐
            │ AuthServer│ │ Game  │ │ Lobby│ │DBProxy │
            │           │ │Server │ │Server│ │        │
            └───────────┘ └───┬───┘ └──────┘ └────┬───┘
                              │                    │
                              │              ┌─────▼─────┐
                              │              │PostgreSQL │
                              │              └───────────┘
                              │
                       ┌──────▼──────┐
                       │  Plugins    │
                       │ (MMORPG,    │
                       │  custom)    │
                       └─────────────┘
```

Each service is horizontally scalable via Kubernetes HPA. The DBProxy uses
StatefulSet to maintain connection affinity. PDBs ensure availability during
rolling updates.

Manifests: [`deploy/k8s/`](../deploy/k8s/)
Production guide: [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md)

## Design Principles

1. **Layer separation** — strict downward dependencies, no upward calls
2. **Result<T, E> everywhere** — no exceptions across layer boundaries
3. **Adapter pattern** — wrap third-party APIs into project-owned interfaces
4. **Data-oriented design** — SoA component layout, cache-friendly iteration
5. **Composition over inheritance** — ECS components, not OOP hierarchies
6. **Plugin first** — game-specific logic lives in plugins, not the framework
7. **Observable by default** — logging, metrics, tracing built into every layer
8. **Cloud-native by default** — Kubernetes manifests are first-class

## See Also

- [`FEATURES.md`](FEATURES.md) — what each layer provides
- [`API_REFERENCE.md`](API_REFERENCE.md) — public APIs per layer
- [`PROJECT_STRUCTURE.md`](PROJECT_STRUCTURE.md) — directory layout
- [`adr/`](adr/) — architectural decisions and rationale
- [`advanced/`](advanced/) — deep dives into specific subsystems
