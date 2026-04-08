# Ecosystem Integration

## Position in kcenon Ecosystem

**common_game_server** is the **Tier 3 application layer** of the kcenon
ecosystem. It is a complete game server framework built on top of the entire
kcenon foundation stack and exposes higher-level abstractions (ECS, plugin
manager, microservices) to game studios.

```
Tier 0   common_system          ◀── foundation interfaces
            │
Tier 1   thread_system          container_system
            │                       │
Tier 2   logger_system          monitoring_system          database_system          network_system
            │                       │                          │                         │
            └───────────────────────┴──────────────────────────┴─────────────────────────┘
                                                  │
Tier 3   common_game_server     ◀── this project
```

`common_game_server` is an **application leaf** — no other kcenon system
depends on it. Game projects built on top of it are out-of-tree consumers
(game studios, sample MMORPG plugins, etc.).

## Dependencies

| System | How It Is Used |
|--------|---------------|
| common_system | `Result<T, E>` error handling, DI container, event bus, error code registry, core interfaces |
| thread_system | Foundation thread pool adapter (`IGameThreadPool`), DAG scheduler |
| logger_system | Foundation logger adapter (`IGameLogger`), structured JSON output, correlation IDs |
| network_system | Foundation network adapter (`IGameNetwork`), TCP server, WebSocket |
| database_system | Foundation database adapter (`IGameDatabase`), PostgreSQL pool, prepared statements |
| monitoring_system | Foundation monitor adapter (`IGameMonitor`), metrics, Prometheus exposition |
| container_system | Type-safe state containers, SIMD serialization |

Pinned versions: see [`../DEPENDENCY_MATRIX.md`](../DEPENDENCY_MATRIX.md).

## Dependent Systems

| System | Relationship |
|--------|-------------|
| (none) | `common_game_server` is an application leaf — no kcenon system depends on it |

## All Ecosystem Systems

| System | Tier | Description | Docs |
|--------|------|-------------|------|
| common_system | 0 | Foundation interfaces, patterns, utilities | [Docs](https://kcenon.github.io/common_system/) |
| thread_system | 1 | High-performance thread pool, DAG scheduling | [Docs](https://kcenon.github.io/thread_system/) |
| container_system | 1 | Type-safe containers, SIMD serialization | [Docs](https://kcenon.github.io/container_system/) |
| logger_system | 2 | Async logging, decorators, OpenTelemetry | [Docs](https://kcenon.github.io/logger_system/) |
| monitoring_system | 2 | Metrics, tracing, alerts, plugins | [Docs](https://kcenon.github.io/monitoring_system/) |
| database_system | 2 | Multi-backend DB (PostgreSQL, SQLite, MongoDB, Redis) | [Docs](https://kcenon.github.io/database_system/) |
| network_system | 2 | TCP/UDP/WebSocket/HTTP2/QUIC/gRPC networking | [Docs](https://kcenon.github.io/network_system/) |
| pacs_system | 3 | DICOM medical imaging | [Docs](https://kcenon.github.io/pacs_system/) |
| **common_game_server** | **3** | **Game server framework with ECS, plugins, microservices** | [Docs](https://kcenon.github.io/common_game_server/) |

## Why Use the kcenon Ecosystem?

`common_game_server` benefits from the kcenon ecosystem in several ways:

1. **Consistent error handling** — `Result<T, E>` flows through every layer; no exceptions
2. **Production-tested foundations** — Each kcenon system is independently
   benchmarked and stress-tested before being adopted as a dependency
3. **Unified observability** — Logging, metrics, and tracing share correlation IDs
4. **Pluggable interfaces** — Foundation adapters can be swapped (e.g., a mock
   logger for tests, a structured JSON logger for production)
5. **Build system consistency** — Same CMake presets, same vcpkg/Conan layout
6. **Decoupled releases** — Each foundation system versions independently;
   `common_game_server` pins to release tags for reproducibility

## Compatibility Matrix

| common_game_server | common_system | thread_system | logger_system | network_system | database_system | monitoring_system | container_system |
|---|---|---|---|---|---|---|---|
| 0.1.0 (current) | ≥ v0.2.0 | ≥ v0.2.0 | ≥ v0.2.0 | ≥ v0.2.0 | ≥ v0.2.0 | ≥ v0.2.0 | ≥ v0.2.0 |

See [`../DEPENDENCY_MATRIX.md`](../DEPENDENCY_MATRIX.md) for the maintained version table.

## Ecosystem Overview

For the complete dependency graph and system selection guide, see the
[Ecosystem Overview](https://kcenon.github.io/common_system/) maintained in
`common_system` (Tier 0).
