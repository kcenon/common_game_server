# Common Game Server

[![CI](https://github.com/kcenon/common_game_server/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/common_game_server/actions/workflows/ci.yml)
[![Code Coverage](https://github.com/kcenon/common_game_server/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/common_game_server/actions/workflows/coverage.yml)
[![API Docs](https://github.com/kcenon/common_game_server/actions/workflows/docs.yml/badge.svg)](https://github.com/kcenon/common_game_server/actions/workflows/docs.yml)

A unified, production-ready game server framework built with C++20, combining proven patterns from multiple game server implementations.

## Key Features

- **Entity-Component System (ECS)**: Data-oriented design with parallel execution and component queries
- **Plugin Architecture**: Hot-reloadable plugins with dependency resolution and event communication
- **Microservices**: 5 horizontally scalable services (Auth, Gateway, Game, Lobby, DBProxy)
- **7 Foundation Adapters**: Logging, networking, database, threading, monitoring, serialization, and common utilities
- **Game Logic Systems**: Object, Combat, World, AI (BehaviorTree), Quest, and Inventory systems
- **Security**: JWT RS256 signing, TLS 1.3, input validation, SQL parameterization
- **Reliability**: WAL + snapshots, circuit breaker, graceful shutdown, chaos testing
- **Cloud-Native**: Kubernetes-ready with HPA, StatefulSet, PDB, Prometheus/Grafana integration
- **Structured Logging**: JSON log output with correlation IDs
- **Doxygen API Documentation**: Auto-generated from source comments

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
|  Layer 1: 7 FOUNDATION SYSTEMS                                      |
|           common, thread, logger, network, database, container,    |
|           monitoring                                               |
+-------------------------------------------------------------------+
```

## Performance Targets

| Metric | Target |
|--------|--------|
| Concurrent Users | 10,000+ per cluster |
| Message Throughput | 300K+ msg/sec |
| World Tick Rate | 20 Hz (50ms) |
| Entity Processing | 10K entities @ <5ms |
| Database Latency | <50ms p99 |
| Plugin Load Time | <100ms |

## Technology Stack

| Component | Technology |
|-----------|------------|
| Language | C++20 |
| Build System | CMake 3.20+ with presets |
| Package Manager | Conan 2 |
| Database | PostgreSQL 14+ |
| Container | Docker + Docker Compose |
| Orchestration | Kubernetes (HPA, StatefulSet, PDB) |
| Monitoring | Prometheus + Grafana |
| Code Style | clang-format 21 (Google-based) |
| Documentation | Doxygen |

## Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.20 or higher
- Conan 2 package manager
- PostgreSQL 14+ (for database features)
- Docker (optional, for containerized deployment)

## Getting Started

### Build from Source

```bash
# Clone the repository
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

### Debug Build (Linux)

```bash
conan install . --output-folder=build --build=missing -s build_type=Debug
cmake --preset conan-debug
cmake --build --preset conan-debug -j$(nproc)
```

### Running Services

```bash
# Start the auth service
./build/bin/auth_server --config config/auth.yaml

# Start the gateway service
./build/bin/gateway_server --config config/gateway.yaml

# Start the game server
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

```
lint (clang-format 21.1.8)
  └─> build & test (ubuntu-24.04 Debug)
  └─> build & test (ubuntu-24.04 Release)
  └─> build & test (macos-14 Release)
coverage (lcov)
docs (doxygen)
```

## Project Structure

```
common_game_server/
├── include/cgs/              # Public headers
│   ├── core/                 # Core types and utilities
│   ├── ecs/                  # ECS (entity, component, scheduler, query)
│   ├── foundation/           # Foundation adapter interfaces
│   ├── game/                 # Game logic (combat, world, AI, quest, inventory)
│   ├── plugin/               # Plugin interfaces and hot reload
│   └── service/              # Service types (auth, gateway, lobby, etc.)
├── src/                      # Implementation
│   ├── ecs/                  # ECS implementation
│   ├── foundation/           # 8 foundation adapters
│   ├── game/                 # 6 game logic systems
│   ├── plugins/              # Plugin manager, hot reload, MMORPG plugin
│   └── services/             # 5 microservices + shared runner
├── tests/                    # Test suites
│   ├── unit/                 # Unit tests
│   ├── integration/          # Integration tests
│   ├── benchmark/            # Performance benchmarks
│   ├── load/                 # Load testing scripts
│   └── chaos/                # Chaos/fault injection tests
├── deploy/                   # Deployment configuration
│   ├── docker/               # Dockerfiles for each service
│   ├── k8s/                  # Kubernetes manifests (HPA, PDB, StatefulSet)
│   ├── monitoring/           # Prometheus + Grafana dashboards
│   ├── config/               # Service configuration files
│   └── docker-compose.yml    # Local full-stack development
├── docs/                     # Documentation
│   ├── reference/            # Reference documentation
│   ├── PRD.md                # Product Requirements Document
│   ├── SRS.md                # Software Requirements Specification
│   ├── SDS.md                # Software Design Specification
│   ├── ARCHITECTURE.md       # Technical architecture design
│   └── PERFORMANCE_REPORT.md # Benchmark results
├── .clang-format             # Code style (Google-based, 4-space indent)
├── .clang-tidy               # Static analysis checks
├── Doxyfile                  # API documentation configuration
└── conanfile.py              # Conan dependency definition
```

## Microservices

| Service | Description | Default Port |
|---------|-------------|--------------|
| AuthServer | JWT RS256 authentication, session management, rate limiting | 8000 |
| GatewayServer | Client routing, load balancing, token bucket rate limiting | 8080/8081 |
| GameServer | World simulation, game loop, map instance management | 9000 |
| LobbyServer | Matchmaking (ELO), party management, region-based queues | 9100 |
| DBProxy | Connection pooling, prepared statements, SQL injection prevention | 5432 |

## Documentation

| Document | Description |
|----------|-------------|
| [PRD](docs/PRD.md) | Product Requirements Document |
| [SRS](docs/SRS.md) | Software Requirements Specification (126 requirements) |
| [SDS](docs/SDS.md) | Software Design Specification (29 modules) |
| [Architecture](docs/ARCHITECTURE.md) | Technical architecture design |
| [Performance Report](docs/PERFORMANCE_REPORT.md) | Benchmark results and analysis |
| [Integration Strategy](docs/INTEGRATION_STRATEGY.md) | Integration approach |
| [Roadmap](docs/ROADMAP.md) | Implementation timeline |

### Reference Documentation

| Document | Description |
|----------|-------------|
| [ECS Design](docs/reference/ECS_DESIGN.md) | Entity-Component System details |
| [Plugin System](docs/reference/PLUGIN_SYSTEM.md) | Plugin architecture and hot reload |
| [Foundation Adapters](docs/reference/FOUNDATION_ADAPTERS.md) | Adapter patterns |
| [Protocol Design](docs/reference/PROTOCOL_DESIGN.md) | Network protocol specification |
| [Database Schema](docs/reference/DATABASE_SCHEMA.md) | Database design |
| [Coding Standards](docs/reference/CODING_STANDARDS.md) | Code style guidelines |
| [Testing Strategy](docs/reference/TESTING_STRATEGY.md) | Testing approach |
| [Deployment Guide](docs/reference/DEPLOYMENT_GUIDE.md) | Production deployment |
| [Configuration Guide](docs/reference/CONFIGURATION_GUIDE.md) | Configuration options |

## Milestones

| Milestone | Description | Status |
|-----------|-------------|--------|
| M1: Foundation | Project bootstrap + 7 foundation adapters | Done |
| M2: Core ECS | Component storage, entity management, scheduling, queries, parallel execution | Done |
| M3: Game Logic | Object, Combat, World, AI, Quest, Inventory systems | Done |
| M4: Services | Auth, Gateway, Game, Lobby, DBProxy microservices | Done |
| M5: Plugin | Event communication, hot reload, MMORPG plugin | Done |
| M6: Production | Performance, scalability (K8s), reliability, security, code quality | Done |

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feat/issue-N-description`)
3. Format your code (`clang-format -i <files>` using version 21+)
4. Commit your changes following [Conventional Commits](https://www.conventionalcommits.org/)
5. Push to the branch (`git push origin feat/issue-N-description`)
6. Open a Pull Request

### Code Style

This project uses clang-format (Google-based style, 4-space indent, 100-column limit). CI enforces formatting on every pull request. To check locally:

```bash
find include/cgs src -name '*.hpp' -o -name '*.cpp' | xargs clang-format --dry-run --Werror
```

### Git Blame

To skip the mass formatting commit in `git blame`:

```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

## License

This project is licensed under the BSD 3-Clause License - see the [LICENSE](LICENSE) file for details.

