# Common Game Server

A unified, production-ready game server framework built with C++20, combining proven patterns from multiple game server implementations.

## Overview

Common Game Server unifies the best aspects of four existing projects into a cohesive, high-performance framework:

- **common_game_server_system (CGSS)**: Standardized specifications and 7 foundation systems
- **game_server**: Battle-tested MMORPG implementation patterns
- **unified_game_server (UGS)**: Modern microservices and ECS architecture
- **game_server_system**: Integration target (foundation layer)

## Key Features

- **Entity-Component System (ECS)**: Data-oriented design for optimal CPU cache utilization
- **Plugin Architecture**: Support multiple game genres without modifying core code
- **Microservices**: Horizontally scalable service-based deployment
- **7 Foundation Systems**: Logging, networking, database, threading, monitoring, serialization, and common utilities
- **Cloud-Native**: Kubernetes-ready with Prometheus/Grafana integration

## Architecture

```
+-------------------------------------------------------------------+
|                      COMMON GAME SERVER                            |
+-------------------------------------------------------------------+
|  Layer 6: PLUGIN LAYER (MMORPG, Battle Royale, RTS, Custom)       |
+-------------------------------------------------------------------+
|  Layer 5: GAME LOGIC LAYER (Object, Combat, World, AI Systems)    |
+-------------------------------------------------------------------+
|  Layer 4: CORE ECS LAYER (Entity, Component, System, World)       |
+-------------------------------------------------------------------+
|  Layer 3: SERVICE LAYER (Auth, Gateway, Game, Lobby, DBProxy)     |
+-------------------------------------------------------------------+
|  Layer 2: FOUNDATION ADAPTER LAYER                                 |
+-------------------------------------------------------------------+
|  Layer 1: 7 FOUNDATION SYSTEMS                                     |
|  (common, thread, logger, network, database, container, monitoring)|
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
| Build System | CMake 3.20+ |
| Package Manager | Conan |
| Database | PostgreSQL 14+ |
| Container | Docker |
| Orchestration | Kubernetes |
| Monitoring | Prometheus + Grafana |

## Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.20 or higher
- Conan package manager
- PostgreSQL 14+
- Docker (optional, for containerized deployment)

## Getting Started

### Build from Source

```bash
# Clone the repository
git clone https://github.com/your-org/common_game_server.git
cd common_game_server

# Install dependencies via Conan
conan install . --build=missing

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run tests
ctest --test-dir build
```

### Running the Server

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
# Build Docker images
docker compose build

# Start all services
docker compose up -d
```

## Documentation

| Document | Description |
|----------|-------------|
| [PRD](docs/PRD.md) | Product Requirements Document |
| [SRS](docs/SRS.md) | Software Requirements Specification |
| [SDS](docs/SDS.md) | Software Design Specification |
| [Architecture](docs/ARCHITECTURE.md) | Technical architecture design |
| [Integration Strategy](docs/INTEGRATION_STRATEGY.md) | Integration approach |
| [Roadmap](docs/ROADMAP.md) | Implementation timeline |

### Reference Documentation

| Document | Description |
|----------|-------------|
| [ECS Design](docs/reference/ECS_DESIGN.md) | Entity-Component System details |
| [Plugin System](docs/reference/PLUGIN_SYSTEM.md) | Plugin architecture |
| [Foundation Adapters](docs/reference/FOUNDATION_ADAPTERS.md) | Adapter patterns |
| [Protocol Design](docs/reference/PROTOCOL_DESIGN.md) | Network protocol specification |
| [Database Schema](docs/reference/DATABASE_SCHEMA.md) | Database design |
| [Coding Standards](docs/reference/CODING_STANDARDS.md) | Code style guidelines |
| [Testing Strategy](docs/reference/TESTING_STRATEGY.md) | Testing approach |
| [Deployment Guide](docs/reference/DEPLOYMENT_GUIDE.md) | Production deployment |
| [Configuration Guide](docs/reference/CONFIGURATION_GUIDE.md) | Configuration options |

## Project Structure

```
common_game_server/
+-- docs/                    # Documentation
|   +-- reference/           # Reference documentation
+-- src/                     # Source code
|   +-- foundation/          # Foundation adapters
|   +-- ecs/                 # Entity-Component System
|   +-- services/            # Microservices
|   +-- game/                # Game logic systems
|   +-- plugins/             # Game plugins
+-- include/                 # Public headers
+-- tests/                   # Test suites
+-- config/                  # Configuration files
+-- deploy/                  # Deployment manifests
+-- scripts/                 # Build and utility scripts
```

## Microservices

| Service | Description | Default Port |
|---------|-------------|--------------|
| AuthServer | Authentication and session management | 8000 |
| GatewayServer | Client routing and load balancing | 8080/8081 |
| GameServer | World simulation and game logic | 9000 |
| LobbyServer | Matchmaking and party management | 9100 |
| DBProxy | Database connection pooling | 5432 |

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes following [Conventional Commits](https://www.conventionalcommits.org/)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Milestones

| Milestone | Duration | Status |
|-----------|----------|--------|
| M1: Foundation | 4 weeks | Planned |
| M2: Core ECS | 4 weeks | Planned |
| M3: Game Logic | 6 weeks | Planned |
| M4: Services | 4 weeks | Planned |
| M5: MMORPG Plugin | 4 weeks | Planned |
| M6: Production | 4 weeks | Planned |

## License

This project is licensed under the BSD 3-Clause License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

This project builds upon proven patterns from:

- common_game_server_system (CGSS)
- game_server
- unified_game_server (UGS)
- game_server_system
