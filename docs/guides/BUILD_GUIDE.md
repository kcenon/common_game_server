# Build Guide

This guide walks through building `common_game_server` from source on Linux,
macOS, and Windows.

## Prerequisites

| Tool | Minimum version | Linux install | macOS install |
|------|----------------|---------------|---------------|
| C++ compiler | GCC 11+, Clang 14+, Apple Clang 14+ | `sudo apt install g++` | `xcode-select --install` |
| CMake | 3.20 | `sudo apt install cmake` | `brew install cmake` |
| Conan | 2.0 | `pip install 'conan>=2.0'` | `pip install 'conan>=2.0'` |
| Git | 2.30 | `sudo apt install git` | `brew install git` |
| Doxygen (optional) | 1.12.0 | `sudo apt install doxygen` | `brew install doxygen` |

## Quick Build

```bash
git clone https://github.com/kcenon/common_game_server.git
cd common_game_server

# Install Conan dependencies
conan install . --output-folder=build --build=missing -s build_type=Release

# Configure with CMake preset
cmake --preset conan-release

# Build
cmake --build --preset conan-release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# Test
ctest --preset conan-release --output-on-failure
```

## Build Configurations

The project provides standard CMake presets in [`../../CMakePresets.json`](../../CMakePresets.json).

| Preset | Build type | Tests | Sanitizers | Coverage |
|--------|-----------|-------|------------|----------|
| `default` | Release | On | Off | Off |
| `debug` | Debug | On | Off | On |
| `release` | Release | Off | Off | Off |
| `asan` | Debug | On | ASan + UBSan | Off |
| `tsan` | Debug | On | ThreadSanitizer | Off |
| `ci` | RelWithDebInfo | On | Off | Off |
| `conan-release` | Release | On | Off | Off |
| `conan-debug` | Debug | On | Off | On |

Use a preset with `--preset <name>`:

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

## Build Options

CMake options that customize the build:

| Option | Default | Effect |
|--------|---------|--------|
| `CGS_BUILD_TESTS` | ON | Build unit and integration tests |
| `CGS_BUILD_BENCHMARKS` | OFF | Build performance benchmarks |
| `CGS_BUILD_SERVICES` | OFF | Build service executables |
| `CGS_HOT_RELOAD` | OFF | Enable plugin hot reload (development only) |
| `CGS_ENABLE_SANITIZERS` | OFF | Enable ASan + UBSan |
| `CGS_ENABLE_COVERAGE` | OFF | Enable gcov instrumentation |

Combine options with the preset:

```bash
cmake --preset conan-release \
    -DCGS_BUILD_BENCHMARKS=ON \
    -DCGS_BUILD_SERVICES=ON
```

## Conan vs vcpkg vs FetchContent

`common_game_server` supports three dependency-resolution paths:

### Conan (recommended)

The `conanfile.py` declares Conan dependencies. The `conan-release` and
`conan-debug` presets wire Conan into CMake automatically.

```bash
conan install . --output-folder=build --build=missing -s build_type=Release
cmake --preset conan-release
```

### FetchContent (no package manager)

If you don't have Conan or vcpkg, the build will use CMake's `FetchContent`
to clone kcenon dependencies from GitHub. This is slower (compiles everything
from source) but requires no external tooling.

```bash
cmake --preset default
cmake --build build
```

### vcpkg (advanced)

vcpkg manifest mode is supported but requires manual setup. See the kcenon
ecosystem `VCPKG_DEPLOYMENT.md` (in `common_system`) for details.

## Building Without Network Access

If your environment blocks GitHub or other dependency hosts:

1. Pre-download the kcenon repos to local paths
2. Set `FETCHCONTENT_SOURCE_DIR_*` env vars to those paths
3. Use Conan from a local mirror (configure via `~/.conan2/remotes.json`)

```bash
export FETCHCONTENT_SOURCE_DIR_COMMON_SYSTEM=/path/to/local/common_system
export FETCHCONTENT_SOURCE_DIR_THREAD_SYSTEM=/path/to/local/thread_system
# ... etc for each kcenon dependency
cmake --preset default
```

## Generating Doxygen Documentation

```bash
doxygen Doxyfile
open documents/html/index.html  # macOS
xdg-open documents/html/index.html  # Linux
```

The output uses the kcenon ecosystem doxygen-awesome theme. Doxygen 1.12.0
is required for theme compatibility — older versions may render incorrectly.

## Cross-Platform Notes

### Linux

- Tested on Ubuntu 22.04 and 24.04
- GCC 11+ or Clang 14+
- Install dependencies: `sudo apt install build-essential cmake python3-pip git`

### macOS

- Tested on macOS 14+ (Apple Silicon)
- Apple Clang 14+ (ships with Xcode 14)
- Install dependencies: `brew install cmake python git`

### Windows

- Tested on Windows 11 with MSVC 2022
- Use the `vcpkg` preset (requires `VCPKG_ROOT` env var)
- No CI yet; report issues on GitHub
- Hot reload uses `LoadLibrary`/`FreeLibrary` instead of `dlopen`/`dlclose`

## Common Build Issues

### "yaml-cpp not found"

Run `conan install` before `cmake --preset`. See [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md).

### "kcenon::common_system not found"

Either run with the `conan-release` preset (which uses Conan), or use the
`default` preset (which uses FetchContent to clone from GitHub).

### "Compiler doesn't support C++20"

Upgrade to GCC 11+, Clang 14+, or MSVC 2022+. The project uses C++20 concepts
and other features that older compilers don't support.

## Verifying the Build

```bash
# Run all tests
ctest --preset conan-release --output-on-failure

# Run a specific test
./build/Release/bin/ecs_unit_tests --gtest_filter='EntityManagerTest.*'

# Run benchmarks
./build/Release/bin/cgs_benchmarks --benchmark_filter=ECS_

# Check Doxygen builds clean
doxygen -x Doxyfile > /dev/null
```

## Installing

```bash
sudo cmake --install build/Release --prefix /usr/local
```

This installs:
- Headers to `/usr/local/include/cgs/`
- Libraries to `/usr/local/lib/`
- CMake config to `/usr/local/lib/cmake/cgs/`

Downstream projects can then use `find_package(cgs REQUIRED)`.

## See Also

- [`../GETTING_STARTED.md`](../GETTING_STARTED.md) — 5-10 min tutorial
- [`CONFIGURATION_GUIDE.md`](CONFIGURATION_GUIDE.md) — Service configuration
- [`DEPLOYMENT_GUIDE.md`](DEPLOYMENT_GUIDE.md) — Production deployment
- [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md) — Common build issues
