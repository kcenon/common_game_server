# Dependency Matrix

This document tracks the version compatibility matrix between
`common_game_server` and its upstream and downstream dependencies in the
kcenon ecosystem.

> **Audience**: Maintainers, integrators, and CI authors.

## Position in kcenon Ecosystem

`common_game_server` is a **Tier 3 application** that consumes the entire
kcenon foundation stack. It does not have any downstream dependents in the
ecosystem (i.e., no other kcenon system depends on `common_game_server`).

```
Tier 0  common_system            (foundation interfaces)
Tier 1  thread_system            (executor)
        container_system         (containers, serialization)
Tier 2  logger_system            (logging)
        monitoring_system        (metrics)
        database_system          (DB abstraction)
        network_system           (TCP/UDP/WebSocket)
─────────────────────────────────────────────────────────
Tier 3  common_game_server  ◀── (this project)
```

## Upstream Dependencies (kcenon ecosystem)

These are required runtime dependencies, fetched via
`cmake/kcenon_deps.cmake` and pinned to release tags.

| Library | Minimum Version | Used For |
|---------|----------------|----------|
| common_system | v0.2.0 | `Result<T, E>`, `IExecutor`, `IJob`, `ILogger`, `IDatabase` interfaces, DI container, event bus, error codes |
| thread_system | v0.2.0 | Foundation thread pool adapter (`IGameThreadPool`), DAG scheduler |
| logger_system | v0.2.0 | Foundation logger adapter (`IGameLogger`), structured JSON output, correlation IDs |
| network_system | v0.2.0 | Foundation network adapter (`IGameNetwork`), TCP server, WebSocket support |
| database_system | v0.2.0 | Foundation DB adapter (`IGameDatabase`), PostgreSQL connection pool, prepared statements |
| monitoring_system | v0.2.0 | Foundation monitor adapter (`IGameMonitor`), metrics collection, Prometheus exposition |
| container_system | v0.2.0 | Type-safe containers, SIMD serialization for game state |

> **Pin convention**: `common_game_server` always references release tags
> (`vX.Y.Z`), never `main`. This ensures reproducible builds and clear
> compatibility tracking.

## Downstream Consumers

`common_game_server` has **no downstream consumers** in the kcenon ecosystem.
It is an application leaf — game projects built on top of it are typically
out-of-tree consumers (game studios, sample MMORPG plugins, etc.).

If you build a game on top of `common_game_server`, please file a PR to add
yourself to this section.

## Third-Party Dependencies

See [`LICENSE-THIRD-PARTY`](LICENSE-THIRD-PARTY) for the full list and
[`docs/SOUP.md`](docs/SOUP.md) for the structured Bill of Materials.

| Package | Version | Source |
|---------|---------|--------|
| yaml-cpp | 0.8.0 | Conan / system |
| googletest | 1.15.0 | Conan (test only) |
| google-benchmark | 1.9.1 | Conan (benchmark only) |

## Compatibility Notes

### C++ Standard

`common_game_server` requires **C++20**. The minimum tested compiler versions are:

- GCC 11+
- Clang 14+
- Apple Clang 14+
- MSVC 2022+

This matches the kcenon ecosystem baseline.

### Platform Support

| Platform | Support Level |
|----------|---------------|
| Linux (Ubuntu 22.04+, Debian 12+) | First-class (CI tested) |
| macOS 14+ | First-class (CI tested) |
| Windows (MSVC 2022+) | Best-effort (no CI yet) |

### CMake

Minimum CMake version: **3.20**.

The project provides standard CMake presets via [`CMakePresets.json`](CMakePresets.json):
`default`, `debug`, `release`, `asan`, `tsan`, `ci`, `conan-release`, `conan-debug`.

## Version Update Policy

When upgrading any kcenon dependency:

1. Update `cmake/kcenon_deps.cmake` with the new tag
2. Update this matrix table with the new minimum version
3. Add an entry to [`CHANGELOG.md`](CHANGELOG.md) under `[Unreleased]`
4. Run the full CI matrix to confirm no regressions
5. If the upstream change is breaking, evaluate whether `common_game_server`
   needs a MAJOR bump per [`VERSIONING.md`](VERSIONING.md)

---

*Last updated: 2026-04-08 — synced with kcenon ecosystem v0.2.0 release*
