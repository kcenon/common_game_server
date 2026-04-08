# Integration Guide

This guide explains how to integrate `common_game_server` into a downstream
project — typically a game studio building a multiplayer game on top of the
framework.

## Integration Modes

There are two ways to use `common_game_server` in your project:

### Mode 1: Use as a framework (recommended)

Build your game as a **plugin** that the framework loads. Your code lives in
a separate repository or directory; the framework binary stays unmodified.

**Best for**: Most game studios. Clean separation, easy upgrades.

### Mode 2: Use as a library

Link `common_game_server` as a library and call its APIs from your own
executable. Your code coexists with the framework in the same process.

**Best for**: Heavy customization where the plugin model is too constraining.

## Mode 1: Plugin Integration

### Step 1: Author your plugin

Create a shared library that exports `cgs_create_plugin()`:

```cpp
// my_game_plugin.cpp
#include <cgs/plugin/game_plugin.hpp>
#include <cgs/plugin/plugin_context.hpp>

using namespace cgs::plugin;
using cgs::core::Result;

class MyGamePlugin : public GamePlugin {
public:
    std::string_view name() const override { return "MyGame"; }
    std::string_view version() const override { return "1.0.0"; }

    Result<void> on_load(PluginContext& ctx) override {
        // Initialize your game systems
        return {};
    }

    Result<void> on_tick(float dt) override {
        // Run your game logic each tick
        return {};
    }

    Result<void> on_unload() override {
        // Cleanup
        return {};
    }
};

extern "C" GamePlugin* cgs_create_plugin() {
    return new MyGamePlugin();
}
```

See [`PLUGIN_DEVELOPMENT_GUIDE.md`](PLUGIN_DEVELOPMENT_GUIDE.md) for the full
plugin authoring tutorial.

### Step 2: Build your plugin against `common_game_server`

```cmake
# Your project's CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(my_game CXX)

find_package(cgs REQUIRED)  # Or FetchContent / Conan

add_library(my_game_plugin SHARED my_game_plugin.cpp)
target_link_libraries(my_game_plugin PRIVATE cgs::cgs_core)
```

### Step 3: Run the GameServer with your plugin

Drop your `libmy_game_plugin.so` (or `.dll`/`.dylib`) into the GameServer's
`plugins/` directory and start the server:

```bash
./game_server --config config/game.yaml --plugin-dir ./plugins
```

The framework discovers and loads plugins automatically at startup.

## Mode 2: Library Integration

### Step 1: Add `common_game_server` as a dependency

#### Via Conan

Add to your `conanfile.txt` or `conanfile.py`:

```python
def requirements(self):
    self.requires("common_game_server/0.1.0")
```

#### Via CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    common_game_server
    GIT_REPOSITORY https://github.com/kcenon/common_game_server.git
    GIT_TAG v0.1.0  # always pin to a release tag
)
FetchContent_MakeAvailable(common_game_server)
```

#### Via system install

After `sudo cmake --install build/...`:

```cmake
find_package(cgs REQUIRED)
target_link_libraries(my_game PRIVATE cgs::cgs_core)
```

### Step 2: Use the framework APIs

```cpp
#include <cgs/cgs.hpp>

int main() {
    using namespace cgs::foundation;
    using namespace cgs::ecs;

    auto foundation = GameFoundation::create_default();
    EntityManager em;

    // ... build your game ...

    return 0;
}
```

### Step 3: Build your project

```bash
cmake -B build
cmake --build build
```

## Pinning Versions

Always pin to a release tag, not `main`:

```cmake
# Good
GIT_TAG v0.1.0

# Bad — non-reproducible builds
GIT_TAG main
```

For Conan, pin in `conanfile.txt`:

```
[requires]
common_game_server/0.1.0
```

## Compatibility Matrix

When you upgrade `common_game_server`, also verify the kcenon foundation
versions are compatible. See [`../../DEPENDENCY_MATRIX.md`](../../DEPENDENCY_MATRIX.md)
and [`../ECOSYSTEM.md`](../ECOSYSTEM.md).

## Migrating from Legacy Game Servers

If you're migrating from one of the four legacy game server projects that
were merged into `common_game_server`, see:

- [`../adr/ADR-001-unified-game-server-architecture.md`](../adr/ADR-001-unified-game-server-architecture.md) — Why the merge happened
- [`../archive/sdlc/INTEGRATION_STRATEGY.md`](../archive/sdlc/INTEGRATION_STRATEGY.md) — Original migration playbook (historical reference)

## Configuration

Each microservice reads a YAML config. See [`CONFIGURATION_GUIDE.md`](CONFIGURATION_GUIDE.md)
for the schema and examples.

## Deployment

For production deployment topologies, see [`DEPLOYMENT_GUIDE.md`](DEPLOYMENT_GUIDE.md).

## See Also

- [`../ECOSYSTEM.md`](../ECOSYSTEM.md) — Position in the kcenon ecosystem
- [`PLUGIN_DEVELOPMENT_GUIDE.md`](PLUGIN_DEVELOPMENT_GUIDE.md) — Plugin authoring
- [`BUILD_GUIDE.md`](BUILD_GUIDE.md) — Building from source
- [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md) — Contributing changes back upstream
