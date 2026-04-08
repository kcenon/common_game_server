# common_game_server

## Overview

Unified C++20 game server framework that integrates the entire kcenon ecosystem.
Tier 3 application layer combining an Entity-Component System (ECS), a hot-reloadable
plugin architecture, and 5 horizontally scalable microservices (Auth, Gateway,
Game, Lobby, DBProxy). Built on top of the 7 kcenon foundation systems via thin
adapter interfaces.

## Architecture

```
include/cgs/
  core/        - Core types and utilities
  ecs/         - Entity-Component System (entity, component, scheduler, query)
  foundation/  - Foundation adapter interfaces (IGameLogger, IGameNetwork,
                 IGameDatabase, IGameThreadPool, IGameMonitor, ...)
  game/        - Game logic systems (Object, Combat, World, AI, Quest, Inventory)
  plugin/      - Plugin interfaces and hot-reload manager
  service/     - Microservice types (Auth, Gateway, Game, Lobby, DBProxy)
  cgs.hpp      - Umbrella header
  version.hpp  - Compile-time version macros
```

Layer model (bottom to top):
1. **Foundation systems** — common, thread, logger, network, database, container, monitoring
2. **Foundation adapters** — `Result<T, E>` based wrappers, no exceptions
3. **Service layer** — Auth, Gateway, Game, Lobby, DBProxy microservices
4. **Core ECS** — Entity manager, component pool, system scheduler, query, parallel execution
5. **Game logic** — Object, Combat, World, AI (BehaviorTree), Quest, Inventory
6. **Plugin layer** — MMORPG, Battle Royale, RTS, custom plugins with hot reload

Key abstractions:
- `Result<T, E>` — Inherited from `kcenon::common::Result<T>`, no exceptions
- `IGameLogger` / `IGameNetwork` / `IGameDatabase` / `IGameThreadPool` / `IGameMonitor` — Foundation adapter interfaces
- `GameFoundation` — Facade combining all foundation adapters
- `EntityManager` / `ComponentPool` / `SystemScheduler` — ECS core
- `GamePlugin` — Plugin lifecycle interface (`load`, `unload`, `tick`)
- `PluginManager` — Hot-reload-aware plugin loader

## Build & Test

```bash
# Conan-based build (recommended)
conan install . --output-folder=build --build=missing -s build_type=Release
cmake --preset conan-release
cmake --build --preset conan-release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
ctest --preset conan-release --output-on-failure

# Direct CMake (with kcenon FetchContent)
cmake --preset default
cmake --build build
```

Key CMake options:
- `CGS_BUILD_TESTS` (ON) — Unit and integration tests (Google Test 1.15.0)
- `CGS_BUILD_BENCHMARKS` (OFF) — Performance benchmarks (Google Benchmark 1.9.1)
- `CGS_BUILD_SERVICES` (OFF) — Service executables
- `CGS_HOT_RELOAD` (OFF) — Plugin hot reload (development only)
- `CGS_ENABLE_SANITIZERS` (OFF) — ASan + UBSan
- `CGS_ENABLE_COVERAGE` (OFF) — gcov instrumentation

Presets: `default`, `debug`, `release`, `asan`, `tsan`, `ci`, `conan-release`, `conan-debug`

CI: GitHub Actions — `ci.yml` (lint + 3-config build & test), `coverage.yml` (lcov),
`docs.yml` (Doxygen), `benchmarks.yml` (manual), `load-test.yml` (manual),
`chaos-tests.yml` (manual).

## Key Patterns

- **Result<T, E>** — Monadic error handling inherited from `common_system`; no exceptions
- **Foundation adapter pattern** — Game-server-friendly facades over kcenon Tier 0-2 systems
- **ECS data-oriented design** — `SparseSet` component storage, parallel system execution, query API
- **Plugin hot reload** — `dlopen`/`LoadLibrary` based, dependency resolution, event communication
- **Microservice composition** — 5 services share `service_runner` template; each binary configurable via YAML
- **JWT RS256 authentication** — Asymmetric keys, refresh tokens, rate limiting at gateway
- **Structured logging** — JSON output, correlation IDs flow through services

## Ecosystem Position

**Tier 3** — Application layer. `common_game_server` consumes the entire kcenon
foundation stack and exposes a complete game server framework.

| Upstream | Used As |
|----------|---------|
| common_system | `Result<T>`, interfaces, DI, event bus |
| thread_system | Foundation thread pool adapter |
| logger_system | Foundation logger adapter |
| network_system | Foundation network adapter |
| database_system | Foundation DB adapter |
| monitoring_system | Foundation monitor adapter |
| container_system | Type-safe state containers, serialization |

| Downstream | Used By |
|------------|---------|
| (none) | `common_game_server` is an application leaf — game studios consume it as a framework, not as a library dependency |

## Dependencies

**Runtime**: yaml-cpp 0.8.0; the 7 kcenon foundation systems (pinned to v0.2.0+)
**Optional**: PostgreSQL 14+ for database features; Docker / Kubernetes for deployment
**Dev/test**: Google Test 1.15.0, Google Benchmark 1.9.1
**Tooling**: clang-format 21.1.8 (CI-enforced), clang-tidy, Doxygen 1.12.0

## Known Constraints

- C++20 required (GCC 11+, Clang 14+, MSVC 2022+, Apple Clang 14+)
- Pre-1.0 (v0.1.0): API and on-the-wire protocol may change between minor versions
- Plugin hot reload is **development-only** — production builds disable `CGS_HOT_RELOAD`
- World tick rate fixed at 20 Hz (50 ms); higher rates require profile-driven optimization
- Concurrent users target ≤ 10,000 per cluster (horizontal scaling via Kubernetes HPA)
- Database backend: PostgreSQL only in v0.1.0 (multi-backend planned post-1.0)
- Platform: Linux and macOS first-class; Windows best-effort (no CI yet)
