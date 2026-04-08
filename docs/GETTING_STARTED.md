# Getting Started

> **Audience**: New users who want to build, run, and explore `common_game_server` in 10 minutes.
> **Time**: ~10 minutes
> **Prerequisites**: Basic familiarity with C++ and the command line.

## What you'll learn

- How to install dependencies and build the project
- How to run the test suite
- How to launch a microservice
- Where to go next in the documentation

## 1. Install prerequisites

You need:

| Tool | Minimum version | Purpose |
|------|----------------|---------|
| C++ compiler | GCC 11+, Clang 14+, MSVC 2022+, Apple Clang 14+ | Build the source |
| CMake | 3.20 | Build orchestration |
| Conan | 2.0 | Dependency resolution |
| Git | 2.30 | Source control |

Optional:

| Tool | Purpose |
|------|---------|
| PostgreSQL 14+ | Database features |
| Docker / Docker Compose | Containerized deployment |
| clang-format 21+ | Local formatting checks |
| Doxygen 1.12.0 | API documentation generation |

### Install Conan

```bash
pip install --user 'conan>=2.0'
conan profile detect --force
```

## 2. Clone and build

```bash
git clone https://github.com/kcenon/common_game_server.git
cd common_game_server

# Install dependencies
conan install . --output-folder=build --build=missing -s build_type=Release

# Configure & build
cmake --preset conan-release
cmake --build --preset conan-release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

The build produces:
- `build/Release/lib/libcgs_core.a` — Header-only umbrella target
- `build/Release/lib/libcgs_*.a` — Layer libraries (foundation, ecs, game, plugins, services)
- `build/Release/bin/*_test` — Test executables (if `CGS_BUILD_TESTS=ON`)
- `build/Release/bin/*_server` — Service executables (if `CGS_BUILD_SERVICES=ON`)

## 3. Run the tests

```bash
ctest --preset conan-release --output-on-failure
```

You should see:

```
Test project /path/to/common_game_server/build/Release
      Start  1: ecs_unit_tests
 1/N  Test  #1: ecs_unit_tests ...................   Passed    0.42 sec
...
100% tests passed, 0 tests failed out of N
```

If a test fails, see [`guides/TROUBLESHOOTING.md`](guides/TROUBLESHOOTING.md).

## 4. Launch a service (optional)

To build the service executables, configure with `-DCGS_BUILD_SERVICES=ON`:

```bash
cmake --preset conan-release -DCGS_BUILD_SERVICES=ON
cmake --build --preset conan-release
```

Then start the auth server:

```bash
./build/Release/bin/auth_server --config config/auth.yaml
```

You should see:

```
[INFO] AuthServer starting on port 8000
[INFO] JWT signing key loaded from config/keys/jwt_private.pem
[INFO] Listening on tcp://0.0.0.0:8000
```

Stop with `Ctrl+C`.

## 5. Explore the codebase

```
common_game_server/
├── include/cgs/        Public headers
│   ├── core/           Core types and utilities
│   ├── ecs/            ECS interfaces
│   ├── foundation/     Foundation adapter interfaces
│   ├── game/           Game logic types
│   ├── plugin/         Plugin interfaces
│   └── service/        Service types
├── src/                Implementation
├── tests/              Unit + integration + benchmark tests
└── docs/               Documentation (you are here)
```

For a complete tour: [`PROJECT_STRUCTURE.md`](PROJECT_STRUCTURE.md).

## 6. Where to go next

| Goal | Document |
|------|----------|
| Understand the architecture | [`ARCHITECTURE.md`](ARCHITECTURE.md) |
| See the full feature list | [`FEATURES.md`](FEATURES.md) |
| Browse the API | [`API_REFERENCE.md`](API_REFERENCE.md) · [Quick reference](API_QUICK_REFERENCE.md) |
| Write a custom plugin | [`guides/PLUGIN_DEVELOPMENT_GUIDE.md`](guides/PLUGIN_DEVELOPMENT_GUIDE.md) |
| Deploy to production | [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md) |
| Configure services | [`guides/CONFIGURATION_GUIDE.md`](guides/CONFIGURATION_GUIDE.md) |
| See benchmarks | [`BENCHMARKS.md`](BENCHMARKS.md) |
| Understand the kcenon ecosystem | [`ECOSYSTEM.md`](ECOSYSTEM.md) |
| Contribute | [`../CONTRIBUTING.md`](../CONTRIBUTING.md) |

## Troubleshooting

### Build fails with "yaml-cpp not found"

Make sure Conan installed dependencies into the same build folder:

```bash
conan install . --output-folder=build --build=missing -s build_type=Release
```

### Tests fail to discover

Ensure `CGS_BUILD_TESTS=ON` (it is on by default in `conan-release` preset):

```bash
cmake --preset conan-release -DCGS_BUILD_TESTS=ON
```

### kcenon dependencies fail to fetch

The kcenon foundation systems are pulled in via FetchContent. If your network
blocks GitHub, see [`guides/BUILD_GUIDE.md`](guides/BUILD_GUIDE.md) for offline
build instructions.

More help: [`guides/TROUBLESHOOTING.md`](guides/TROUBLESHOOTING.md) · [`guides/FAQ.md`](guides/FAQ.md)
