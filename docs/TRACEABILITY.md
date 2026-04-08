# Traceability

This document maps relationships between requirements, design, code, tests,
and historical SDLC documents. Use it to navigate from a feature to its
implementation, tests, and origin documents.

## Documentation Cross-Reference

| Topic | Active Document | Legacy Source | Code |
|-------|----------------|---------------|------|
| Project vision | [`FEATURES.md`](FEATURES.md), [`README.md`](../README.md) | [`archive/sdlc/PRD.md`](archive/sdlc/PRD.md) | — |
| Functional requirements | [`FEATURES.md`](FEATURES.md), [`API_REFERENCE.md`](API_REFERENCE.md) | [`archive/sdlc/SRS.md`](archive/sdlc/SRS.md) | `include/cgs/`, `src/` |
| System design | [`ARCHITECTURE.md`](ARCHITECTURE.md), [`advanced/`](advanced/) | [`archive/sdlc/SDS.md`](archive/sdlc/SDS.md) | `src/` |
| Multi-project integration | [`ECOSYSTEM.md`](ECOSYSTEM.md), [`adr/ADR-001-unified-game-server-architecture.md`](adr/ADR-001-unified-game-server-architecture.md) | [`archive/sdlc/INTEGRATION_STRATEGY.md`](archive/sdlc/INTEGRATION_STRATEGY.md) | `cmake/kcenon_deps.cmake` |
| Performance metrics | [`BENCHMARKS.md`](BENCHMARKS.md), [`performance/BENCHMARKS_METHODOLOGY.md`](performance/BENCHMARKS_METHODOLOGY.md) | [`archive/sdlc/PERFORMANCE_REPORT.md`](archive/sdlc/PERFORMANCE_REPORT.md) | `tests/benchmark/` |
| Deployment | [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md) | — | `deploy/` |
| Configuration | [`guides/CONFIGURATION_GUIDE.md`](guides/CONFIGURATION_GUIDE.md) | — | `config/`, `src/services/shared/config.cpp` |

## Feature → Implementation → Test Map

| Feature | Header | Implementation | Tests |
|---------|--------|---------------|-------|
| Result<T, E> | `include/cgs/core/result.hpp` | (header-only) | `tests/unit/core/result_test.cpp` |
| EntityManager | `include/cgs/ecs/entity_manager.hpp` | `src/ecs/entity_manager.cpp` | `tests/unit/ecs/entity_manager_test.cpp` |
| ComponentPool | `include/cgs/ecs/component_pool.hpp` | (header-only template) | `tests/unit/ecs/component_pool_test.cpp` |
| SystemScheduler | `include/cgs/ecs/system_scheduler.hpp` | `src/ecs/system_scheduler.cpp` | `tests/unit/ecs/system_scheduler_test.cpp` |
| IGameLogger | `include/cgs/foundation/game_logger.hpp` | `src/foundation/logger_adapter.cpp` | `tests/unit/foundation/logger_adapter_test.cpp` |
| IGameNetwork | `include/cgs/foundation/game_network.hpp` | `src/foundation/network_adapter.cpp` | `tests/unit/foundation/network_adapter_test.cpp` |
| IGameDatabase | `include/cgs/foundation/game_database.hpp` | `src/foundation/database_adapter.cpp` | `tests/integration/foundation/database_test.cpp` |
| GamePlugin | `include/cgs/plugin/game_plugin.hpp` | (interface) | `tests/unit/plugin/plugin_interface_test.cpp` |
| PluginManager | `include/cgs/plugin/plugin_manager.hpp` | `src/plugins/manager/plugin_manager.cpp` | `tests/integration/plugin/plugin_manager_test.cpp` |
| AuthServer | `include/cgs/service/auth_server.hpp` | `src/services/auth/` | `tests/integration/services/auth_server_test.cpp` |
| GatewayServer | `include/cgs/service/gateway_server.hpp` | `src/services/gateway/` | `tests/integration/services/gateway_server_test.cpp` |
| GameServer | `include/cgs/service/game_server.hpp` | `src/services/game/` | `tests/integration/services/game_server_test.cpp` |
| LobbyServer | `include/cgs/service/lobby_server.hpp` | `src/services/lobby/` | `tests/integration/services/lobby_server_test.cpp` |
| DBProxy | `include/cgs/service/dbproxy.hpp` | `src/services/dbproxy/` | `tests/integration/services/dbproxy_test.cpp` |

> **Note**: Some test paths above are reference paths. The exact test file
> names may differ — see [`tests/`](../tests/) for the canonical layout.

## ADR → Affected Components

| ADR | Affected Components | Documents |
|-----|--------------------|-----------|
| [`ADR-001`](adr/ADR-001-unified-game-server-architecture.md) | Whole framework | [`ECOSYSTEM.md`](ECOSYSTEM.md), [`ARCHITECTURE.md`](ARCHITECTURE.md) |
| [`ADR-002`](adr/ADR-002-entity-component-system.md) | `cgs::ecs` | [`advanced/ECS_DEEP_DIVE.md`](advanced/ECS_DEEP_DIVE.md) |
| [`ADR-003`](adr/ADR-003-plugin-hot-reload.md) | `cgs::plugin` | [`guides/PLUGIN_DEVELOPMENT_GUIDE.md`](guides/PLUGIN_DEVELOPMENT_GUIDE.md) |
| [`ADR-004`](adr/ADR-004-microservice-decomposition.md) | `cgs::service`, `deploy/` | [`ARCHITECTURE.md#layer-3`](ARCHITECTURE.md), [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md) |

## Legacy Document Migration Map

How content from `docs/archive/sdlc/` was migrated to the active documentation:

| Legacy Section | Active Location |
|----------------|----------------|
| `PRD.md` § Vision | [`README.md`](../README.md) introduction, [`FEATURES.md`](FEATURES.md) intro |
| `PRD.md` § Functional requirements | [`FEATURES.md`](FEATURES.md) feature matrix |
| `PRD.md` § Non-functional requirements | [`PRODUCTION_QUALITY.md`](PRODUCTION_QUALITY.md) SLOs |
| `PRD.md` § Milestones | [`ROADMAP.md`](ROADMAP.md) |
| `SRS.md` § Functional reqs | [`FEATURES.md`](FEATURES.md) detailed sections |
| `SRS.md` § Interface definitions | [`API_REFERENCE.md`](API_REFERENCE.md) |
| `SRS.md` § Non-functional reqs | [`PRODUCTION_QUALITY.md`](PRODUCTION_QUALITY.md) |
| `SDS.md` § Module catalog | [`PROJECT_STRUCTURE.md`](PROJECT_STRUCTURE.md), [`ARCHITECTURE.md`](ARCHITECTURE.md) |
| `SDS.md` § Component design | [`advanced/`](advanced/) deep-dives |
| `SDS.md` § API specifications | [`API_REFERENCE.md`](API_REFERENCE.md) |
| `SDS.md` § Deployment architecture | [`ARCHITECTURE.md#deployment-topology`](ARCHITECTURE.md), [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md) |
| `INTEGRATION_STRATEGY.md` § Project merger | [`adr/ADR-001-unified-game-server-architecture.md`](adr/ADR-001-unified-game-server-architecture.md) |
| `INTEGRATION_STRATEGY.md` § Migration plan | [`guides/INTEGRATION_GUIDE.md`](guides/INTEGRATION_GUIDE.md) |
| `INDEX.md` | [`README.md`](README.md) (this docs index) |
| `PERFORMANCE_REPORT.md` § Methodology | [`performance/BENCHMARKS_METHODOLOGY.md`](performance/BENCHMARKS_METHODOLOGY.md) |
| `PERFORMANCE_REPORT.md` § Results | [`BENCHMARKS.md`](BENCHMARKS.md) |

## See Also

- [`README.md`](README.md) — Documentation index
- [`archive/README.md`](archive/README.md) — Archive overview
- [`adr/`](adr/) — Architecture Decision Records
