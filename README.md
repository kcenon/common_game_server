# Common Game Server

[![CI](https://github.com/kcenon/common_game_server/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/common_game_server/actions/workflows/ci.yml)
[![Code Coverage](https://github.com/kcenon/common_game_server/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/common_game_server/actions/workflows/coverage.yml)
[![API Docs](https://github.com/kcenon/common_game_server/actions/workflows/docs.yml/badge.svg)](https://github.com/kcenon/common_game_server/actions/workflows/docs.yml)
[![License](https://img.shields.io/badge/license-BSD%203--Clause-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

> 한국어 문서: [README.kr.md](README.kr.md)

A unified, production-ready C++20 game server framework combining proven
patterns from multiple game server implementations. Built as the **Tier 3
application layer** of the [kcenon ecosystem](docs/ECOSYSTEM.md).

## Key Features

- **Entity-Component System (ECS)** — Data-oriented design with parallel execution and component queries
- **Plugin Architecture** — Hot-reloadable plugins with dependency resolution and event communication
- **Microservices** — 5 horizontally scalable services (Auth, Gateway, Game, Lobby, DBProxy)
- **7 Foundation Adapters** — Logging, networking, database, threading, monitoring, serialization, common utilities
- **Game Logic Systems** — Object, Combat, World, AI (BehaviorTree), Quest, Inventory
- **Security** — JWT RS256 signing, TLS 1.3, input validation, SQL parameterization
- **Reliability** — WAL + snapshots, circuit breaker, graceful shutdown, chaos testing
- **Cloud-Native** — Kubernetes-ready with HPA, StatefulSet, PDB, Prometheus/Grafana
- **Structured Logging** — JSON output with correlation IDs
- **Doxygen API Docs** — Auto-generated from source comments

See [`docs/FEATURES.md`](docs/FEATURES.md) for the complete feature matrix.

## Architecture

```
+-------------------------------------------------------------------+
|                      COMMON GAME SERVER                            |
+-------------------------------------------------------------------+
|  Layer 6: PLUGIN LAYER (MMORPG, Battle Royale, RTS, Custom)       |
|           Hot reload, dependency resolution, event communication   |
+-------------------------------------------------------------------+
|  Layer 5: GAME LOGIC LAYER                                         |
|           Object, Combat, World, AI, Quest, Inventory              |
+-------------------------------------------------------------------+
|  Layer 4: CORE ECS LAYER                                           |
|           Entity, Component, System Scheduler, Query, Parallel Exec|
+-------------------------------------------------------------------+
|  Layer 3: SERVICE LAYER                                            |
|           Auth, Gateway, Game, Lobby, DBProxy                      |
+-------------------------------------------------------------------+
|  Layer 2: FOUNDATION ADAPTER LAYER                                 |
|           Result<T,E> pattern, no exceptions                       |
+-------------------------------------------------------------------+
|  Layer 1: 7 KCENON FOUNDATION SYSTEMS                              |
|           common, thread, logger, network, database, container,    |
|           monitoring                                               |
+-------------------------------------------------------------------+
```

Detailed architecture: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)

## Ecosystem Position

`common_game_server` is the **Tier 3 application layer** of the kcenon
ecosystem and consumes the entire kcenon foundation stack.

```
Tier 0  common_system        ◀── foundation interfaces
Tier 1  thread_system, container_system
Tier 2  logger_system, monitoring_system, database_system, network_system
Tier 3  common_game_server   ◀── this project
```

Full ecosystem map: [`docs/ECOSYSTEM.md`](docs/ECOSYSTEM.md) ·
Dependencies: [`DEPENDENCY_MATRIX.md`](DEPENDENCY_MATRIX.md)

## Performance Targets

| Metric | Target |
|--------|--------|
| Concurrent Users | 10,000+ per cluster |
| Message Throughput | 300K+ msg/sec |
| World Tick Rate | 20 Hz (50ms) |
| Entity Processing | 10K entities @ <5ms |
| Database Latency | <50ms p99 |
| Plugin Load Time | <100ms |

Detailed benchmarks: [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md)

## Technology Stack

| Component | Technology |
|-----------|------------|
| Language | C++20 |
| Build System | CMake 3.20+ with presets |
| Package Manager | Conan 2 (or vcpkg) |
| Database | PostgreSQL 14+ |
| Container | Docker + Docker Compose |
| Orchestration | Kubernetes (HPA, StatefulSet, PDB) |
| Monitoring | Prometheus + Grafana |
| Code Style | clang-format 21 (Google-based) |
| Documentation | Doxygen 1.12.0 |

## Getting Started

### Prerequisites

- C++20 compiler (GCC 11+, Clang 14+, MSVC 2022+, Apple Clang 14+)
- CMake 3.20 or higher
- Conan 2 package manager
- PostgreSQL 14+ (for database features)
- Docker (optional, for containerized deployment)

### Build from Source

```bash
git clone https://github.com/kcenon/common_game_server.git
cd common_game_server

# Install dependencies via Conan
conan install . --output-folder=build --build=missing -s build_type=Release

# Configure and build using CMake presets
cmake --preset conan-release
cmake --build --preset conan-release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# Run tests
ctest --preset conan-release --output-on-failure
```

Step-by-step tutorial: [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) ·
Full build guide: [`docs/guides/BUILD_GUIDE.md`](docs/guides/BUILD_GUIDE.md)

### Run a Service

```bash
./build/bin/auth_server --config config/auth.yaml
./build/bin/gateway_server --config config/gateway.yaml
./build/bin/game_server --config config/game.yaml
```

### Docker Deployment

```bash
cd deploy
docker compose up --build
```

### Kubernetes Deployment

```bash
kubectl apply -f deploy/k8s/base/
```

Production deployment: [`docs/guides/DEPLOYMENT_GUIDE.md`](docs/guides/DEPLOYMENT_GUIDE.md)

## Microservices

| Service | Description | Default Port |
|---------|-------------|--------------|
| AuthServer | JWT RS256 authentication, session management, rate limiting | 8000 |
| GatewayServer | Client routing, load balancing, token bucket rate limiting | 8080/8081 |
| GameServer | World simulation, game loop, map instance management | 9000 |
| LobbyServer | Matchmaking (ELO), party management, region-based queues | 9100 |
| DBProxy | Connection pooling, prepared statements, SQL injection prevention | 5432 |

## Documentation

| Category | Document |
|---|---|
| Index | [`docs/README.md`](docs/README.md) |
| Quick start | [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) |
| Features | [`docs/FEATURES.md`](docs/FEATURES.md) |
| Architecture | [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) |
| API reference | [`docs/API_REFERENCE.md`](docs/API_REFERENCE.md) · [Quick reference](docs/API_QUICK_REFERENCE.md) |
| Benchmarks | [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md) |
| Production quality | [`docs/PRODUCTION_QUALITY.md`](docs/PRODUCTION_QUALITY.md) |
| Project structure | [`docs/PROJECT_STRUCTURE.md`](docs/PROJECT_STRUCTURE.md) |
| Roadmap | [`docs/ROADMAP.md`](docs/ROADMAP.md) |
| Changelog | [`CHANGELOG.md`](CHANGELOG.md) |
| Ecosystem | [`docs/ECOSYSTEM.md`](docs/ECOSYSTEM.md) |
| ADR records | [`docs/adr/`](docs/adr/) |

### How-to guides
[Build](docs/guides/BUILD_GUIDE.md) ·
[Deployment](docs/guides/DEPLOYMENT_GUIDE.md) ·
[Configuration](docs/guides/CONFIGURATION_GUIDE.md) ·
[Testing](docs/guides/TESTING_GUIDE.md) ·
[Plugin development](docs/guides/PLUGIN_DEVELOPMENT_GUIDE.md) ·
[Troubleshooting](docs/guides/TROUBLESHOOTING.md) ·
[FAQ](docs/guides/FAQ.md)

### Advanced topics
[ECS deep dive](docs/advanced/ECS_DEEP_DIVE.md) ·
[Foundation adapters](docs/advanced/FOUNDATION_ADAPTERS.md) ·
[Protocol specification](docs/advanced/PROTOCOL_SPECIFICATION.md) ·
[Database schema](docs/advanced/DATABASE_SCHEMA.md)

## CI/CD Pipeline

The project runs 6 automated workflows on every push and pull request:

| Workflow | Description | Trigger |
|----------|-------------|---------|
| **CI** | Lint (clang-format) → Build & Test (3 configs) | push, PR |
| **Code Coverage** | lcov coverage report | push, PR |
| **API Docs** | Doxygen documentation generation | push, PR |
| **Benchmarks** | Performance benchmark suite | manual |
| **Load Test** | CCU validation scripts | manual |
| **Chaos Tests** | Fault injection & resilience testing | manual |

CI guide: [`docs/contributing/CI_CD_GUIDE.md`](docs/contributing/CI_CD_GUIDE.md)

## Contributing

We welcome contributions! See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the
development workflow, coding standards, and PR process.

- **Code of Conduct** — [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md)
- **Security policy** — [`SECURITY.md`](SECURITY.md)
- **Versioning** — [`VERSIONING.md`](VERSIONING.md)
- **Coding standards** — [`docs/contributing/CODING_STANDARDS.md`](docs/contributing/CODING_STANDARDS.md)

### Code Style

This project uses clang-format 21+ (Google-based, 4-space indent, 100-column
limit). CI enforces formatting on every PR. To check locally:

```bash
find include/cgs src -name '*.hpp' -o -name '*.cpp' | xargs clang-format --dry-run --Werror
```

### Git Blame

To skip the mass formatting commit in `git blame`:

```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

## License

This project is licensed under the BSD 3-Clause License — see [LICENSE](LICENSE)
for details. Third-party dependency licenses: [LICENSE-THIRD-PARTY](LICENSE-THIRD-PARTY)
and [NOTICES](NOTICES).
