# Features

> **English** · [한국어](FEATURES.kr.md)

A complete capability matrix for `common_game_server`. For the architecture
behind these features, see [`ARCHITECTURE.md`](ARCHITECTURE.md). For
performance numbers, see [`BENCHMARKS.md`](BENCHMARKS.md).

## Feature Overview

| Category | Feature | Status |
|---|---|---|
| **Foundation** | 7 kcenon adapter interfaces | Stable |
| | Result<T, E> error handling (no exceptions) | Stable |
| | DI container & event bus | Stable |
| | Structured JSON logging with correlation IDs | Stable |
| **ECS** | Entity manager with sparse-set storage | Stable |
| | Component pool with SoA layout | Stable |
| | System scheduler with DAG dependencies | Stable |
| | Parallel system execution | Stable |
| | Compile-time component query | Stable |
| **Plugins** | Plugin lifecycle interface (`load`/`tick`/`unload`) | Stable |
| | Hot-reload (development only) | Beta |
| | Dependency resolution between plugins | Stable |
| | Plugin event communication via event bus | Stable |
| | Sample MMORPG plugin | Reference |
| **Microservices** | AuthServer (JWT RS256) | Stable |
| | GatewayServer (routing, rate limiting) | Stable |
| | GameServer (world simulation, 20 Hz tick) | Stable |
| | LobbyServer (ELO matchmaking, parties) | Stable |
| | DBProxy (connection pooling, prepared statements) | Stable |
| **Game Logic** | Object system | Stable |
| | Combat system | Stable |
| | World system | Stable |
| | AI (BehaviorTree) | Stable |
| | Quest system | Stable |
| | Inventory system | Stable |
| **Security** | JWT RS256 signing & verification | Stable |
| | TLS 1.3 termination | Stable |
| | Token bucket rate limiting | Stable |
| | SQL parameterization (no string concat) | Stable |
| | Input validation at boundary | Stable |
| **Reliability** | Write-Ahead Log (WAL) + snapshots | Stable |
| | Circuit breaker pattern | Stable |
| | Graceful shutdown | Stable |
| | Chaos testing harness | Stable |
| **Cloud-Native** | Dockerfile per service | Stable |
| | Kubernetes manifests (Deployment, Service, HPA, PDB, StatefulSet) | Stable |
| | Prometheus metrics endpoint | Stable |
| | Grafana dashboards | Stable |
| | Health & readiness probes | Stable |
| **Observability** | Structured logging (JSON) | Stable |
| | Correlation ID propagation | Stable |
| | Metrics: counters, gauges, histograms | Stable |
| | Distributed tracing hooks | Beta |
| **Documentation** | Doxygen API docs | Stable |
| | Bilingual (English / Korean) core docs | Stable |
| | ADR records | Stable |
| | Architecture diagrams | Stable |

**Status legend**:
- **Stable** — Production-ready, API is stable within MINOR versions
- **Beta** — Functional but API may change in next MINOR
- **Reference** — Sample implementation, not for production use

## Foundation Adapters (Detail)

`common_game_server` consumes 7 kcenon foundation systems through thin adapter
interfaces:

| Adapter | Wraps | Purpose |
|---------|-------|---------|
| `IGameLogger` | logger_system | Structured JSON logging, correlation IDs |
| `IGameNetwork` | network_system | TCP/WebSocket server, client routing |
| `IGameDatabase` | database_system | PostgreSQL pool, prepared statements |
| `IGameThreadPool` | thread_system | Async task execution, DAG scheduling |
| `IGameMonitor` | monitoring_system | Metrics, Prometheus exposition |
| `IGameContainer` | container_system | Type-safe state, SIMD serialization |
| `IGameCommon` | common_system | Result<T>, error codes, DI |

All adapters use `cgs::core::Result<T, E>` for error handling — no exceptions
cross adapter boundaries.

## ECS (Detail)

The Entity-Component System is data-oriented:

- **Entities** are 64-bit IDs with version recycling
- **Components** are stored in `SparseSet`-backed pools (cache-friendly SoA)
- **Systems** are scheduled by a DAG with parallel execution where dependencies allow
- **Queries** use C++20 concepts to enforce compile-time component constraints

Performance: 10K entities processed in <5 ms (see [`BENCHMARKS.md`](BENCHMARKS.md)).

## Plugins (Detail)

Plugins implement `cgs::plugin::GamePlugin`:

```cpp
class GamePlugin {
public:
    virtual Result<void, error_code> on_load(PluginContext&) = 0;
    virtual Result<void, error_code> on_tick(float dt) = 0;
    virtual Result<void, error_code> on_unload() = 0;
};
```

Hot-reload (`CGS_HOT_RELOAD=ON`) is **development only** — production builds
disable it for security and stability.

Sample plugin: `src/plugins/mmorpg/` — a complete reference MMORPG game loop
demonstrating ECS systems, foundation adapters, and event communication.

## Microservices (Detail)

| Service | Default Port | Key Capabilities |
|---------|--------------|------------------|
| AuthServer | 8000 | JWT RS256, refresh tokens, session store, rate limit |
| GatewayServer | 8080 (HTTP), 8081 (WebSocket) | Client routing, load balancing, token bucket rate limit |
| GameServer | 9000 | World simulation, game loop @ 20 Hz, map instances |
| LobbyServer | 9100 | ELO matchmaking, party management, region queues |
| DBProxy | 5432 | PostgreSQL connection pool, prepared statements, query routing |

Each service is a standalone binary configurable via YAML
([`guides/CONFIGURATION_GUIDE.md`](guides/CONFIGURATION_GUIDE.md)). All five
services share the `service_runner` template defined in `src/services/shared/`.

## Performance Targets

| Metric | Target | Verified by |
|--------|--------|-------------|
| Concurrent users (CCU) | 10,000+ per cluster | Load test |
| Message throughput | 300,000+ msg/sec | Benchmark |
| World tick rate | 20 Hz (50 ms) | Continuous |
| Entity processing | 10,000 entities < 5 ms | Benchmark |
| Database p99 latency | < 50 ms | Load test |
| Plugin load time | < 100 ms | Benchmark |

See [`BENCHMARKS.md`](BENCHMARKS.md) for measured numbers and methodology.

## Out-of-Scope (Non-Goals)

The following are intentionally **not** part of `common_game_server`:

- **Client SDK** — This is a server framework. Client integration is left to game studios.
- **Asset pipeline** — Game studios bring their own asset toolchains.
- **Voice chat** — Out of scope; integrate with a third-party SFU if needed.
- **Anti-cheat** — Out of scope; integrate with vendor solutions like EAC.
- **Physics simulation** — ECS supports physics components, but no built-in physics engine. Integrate Bullet/PhysX as a plugin.
- **Rendering** — Server-side only. No client-side rendering code.
- **Persistent world tooling** — World editors and content management are studio-side.

## See Also

- [`ARCHITECTURE.md`](ARCHITECTURE.md) — How these features fit together
- [`API_REFERENCE.md`](API_REFERENCE.md) — Detailed API for each feature
- [`ROADMAP.md`](ROADMAP.md) — Planned features
- [`adr/`](adr/) — Why these features were chosen
