# Roadmap

This document tracks the implementation milestones for `common_game_server`.
For released features, see [`../CHANGELOG.md`](../CHANGELOG.md). For active
development, see [GitHub Issues](https://github.com/kcenon/common_game_server/issues).

## Current Status

| Version | Status | Released |
|---------|--------|----------|
| 0.1.0 | Released | 2026-02-03 |
| Unreleased | In progress | — |

## Milestones

### M1: Foundation Integration — Done

**Goal**: Establish project bootstrap and adapter layer.

- [x] Project bootstrap (CMake, Conan, CI)
- [x] 7 foundation adapter interfaces (`IGameLogger`, `IGameNetwork`, `IGameDatabase`, `IGameThreadPool`, `IGameMonitor`, `IGameContainer`, `IGameCommon`)
- [x] `Result<T, E>` integration with `kcenon::common`
- [x] Adapter implementations wrapping kcenon foundation systems
- [x] `GameFoundation` facade
- [x] Mock adapters for testing
- [x] Unit test coverage > 80%

### M2: Core ECS — Done

**Goal**: Data-oriented entity-component-system core.

- [x] `EntityManager` with version recycling
- [x] `ComponentPool<T>` with `SparseSet` storage
- [x] `Query<Components...>` with C++20 concepts
- [x] `SystemScheduler` with DAG dependencies
- [x] Parallel system execution
- [x] Benchmark: 10K entities < 5 ms

### M3: Game Logic — Done

**Goal**: Reference game logic systems on top of the ECS.

- [x] ObjectSystem (transforms)
- [x] CombatSystem
- [x] WorldSystem
- [x] AISystem (BehaviorTree)
- [x] QuestSystem
- [x] InventorySystem

### M4: Microservices — Done

**Goal**: 5 horizontally scalable services.

- [x] AuthServer (JWT RS256, sessions, rate limiting)
- [x] GatewayServer (routing, load balancing)
- [x] GameServer (world simulation, 20 Hz tick)
- [x] LobbyServer (ELO matchmaking, parties)
- [x] DBProxy (connection pool, prepared statements)
- [x] Shared `service_runner` template
- [x] YAML configuration

### M5: Plugin System — Done

**Goal**: Hot-reloadable plugins with dependency resolution.

- [x] `GamePlugin` interface
- [x] `PluginManager` with dependency resolution
- [x] Hot reload (development only)
- [x] Plugin event communication via event bus
- [x] Sample MMORPG plugin

### M6: Production Readiness — Done

**Goal**: Cloud-native deployment, security, observability.

- [x] Kubernetes manifests (HPA, StatefulSet, PDB)
- [x] Prometheus metrics + Grafana dashboards
- [x] TLS 1.3 termination
- [x] Chaos testing harness
- [x] CVE scanning
- [x] CI/CD pipeline (lint, build, test, coverage, docs, benchmarks)

## Future Milestones

### M7: Documentation Alignment (In Progress)

**Goal**: Align documentation with the kcenon ecosystem template.

- [x] Archive legacy SDLC docs to `docs/archive/sdlc/`
- [x] Reorganize `docs/reference/` into `guides/`, `advanced/`, `contributing/`
- [x] Create root governance files (CLAUDE.md, CHANGELOG.md, etc.)
- [x] Bilingual core docs (EN + KR)
- [ ] ADR records
- [ ] Doxygen integration with shared theme
- [ ] CI verification of doc structure

Tracking: [#122](https://github.com/kcenon/common_game_server/issues/122)

### M8: Stability Polish (Planned)

**Goal**: Reach v1.0.0 quality bar.

- [ ] API audit and stabilization
- [ ] Increase code coverage to 60%+
- [ ] All CI workflows green on every PR
- [ ] Reproducible benchmarks across CI runs
- [ ] First v1.0.0 release candidate

### M9: Multi-Backend Database (Planned)

**Goal**: Support backends beyond PostgreSQL.

- [ ] SQLite backend (for testing and small deployments)
- [ ] Redis backend (for caching layer)
- [ ] MongoDB backend (for document data)

Depends on: kcenon `database_system` multi-backend support.

### M10: Distributed Tracing (Planned)

**Goal**: First-class distributed tracing across services.

- [ ] OpenTelemetry SDK integration
- [ ] Trace context propagation through gateway
- [ ] Span instrumentation in all services
- [ ] Jaeger/Tempo collector configuration

## Non-Goals

These are intentionally out of scope. See [`FEATURES.md#out-of-scope-non-goals`](FEATURES.md#out-of-scope-non-goals).

## How to Suggest a Feature

1. Open an [issue](https://github.com/kcenon/common_game_server/issues/new) with the `enhancement` label
2. Describe the use case and expected behavior
3. Reference any related ADRs in [`adr/`](adr/)
4. The maintainers will triage and (if accepted) add it to the roadmap above

## See Also

- [`FEATURES.md`](FEATURES.md) — Currently shipping features
- [`../CHANGELOG.md`](../CHANGELOG.md) — Release history
- [`../VERSIONING.md`](../VERSIONING.md) — Versioning policy
- [`adr/`](adr/) — Architecture Decision Records
