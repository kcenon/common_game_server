# API Quick Reference

A 1-page cheat sheet of the most-used `cgs::*` types and functions. For full
details, see [`API_REFERENCE.md`](API_REFERENCE.md).

## Headers

```cpp
#include <cgs/cgs.hpp>          // Umbrella header (all submodules)
#include <cgs/version.hpp>      // Version macros (CGS_VERSION_MAJOR, ...)

// Or import individual modules:
#include <cgs/core/result.hpp>
#include <cgs/foundation/game_logger.hpp>
#include <cgs/foundation/game_foundation.hpp>
#include <cgs/ecs/entity_manager.hpp>
#include <cgs/ecs/system_scheduler.hpp>
#include <cgs/game/combat_system.hpp>
#include <cgs/plugin/game_plugin.hpp>
#include <cgs/plugin/plugin_manager.hpp>
#include <cgs/service/auth_server.hpp>
```

## Result and Error Handling

```cpp
using cgs::core::Result;

Result<int, error_code> divide(int a, int b) {
    if (b == 0) return cgs::core::error{error_code::div_by_zero, "b == 0"};
    return a / b;
}

auto r = divide(10, 0);
if (r) { use(r.value()); } else { log(r.error()); }

// Monadic composition
divide(10, 2)
    .and_then([](int x) { return divide(x, 1); })
    .map([](int x) { return x * 2; })
    .or_else([](auto e) { return Result<int, error_code>{42}; });
```

## Foundation Adapters

```cpp
using namespace cgs::foundation;

GameFoundation foundation = GameFoundation::create_default();

foundation.logger().info("server started");
foundation.network().listen(8080);
foundation.database().query("SELECT 1");
foundation.thread_pool().submit([] { do_work(); });
foundation.monitor().counter("requests_total").increment();
```

## Entity-Component System

```cpp
using namespace cgs::ecs;

EntityManager em;
auto entity = em.create();

em.add<Position>(entity, {10.0f, 20.0f});
em.add<Velocity>(entity, {1.0f, 0.0f});

// Query: iterate all entities with Position + Velocity
em.query<Position, Velocity>().each([](auto& pos, const auto& vel) {
    pos.x += vel.dx;
    pos.y += vel.dy;
});

// System scheduler: parallel execution
SystemScheduler sched;
sched.add<MovementSystem>();
sched.add<CombatSystem>().depends_on<MovementSystem>();
sched.tick(em, 0.05f);  // 50 ms (20 Hz)
```

## Plugins

```cpp
using namespace cgs::plugin;

class MyPlugin : public GamePlugin {
public:
    Result<void, error_code> on_load(PluginContext& ctx) override { ... }
    Result<void, error_code> on_tick(float dt) override { ... }
    Result<void, error_code> on_unload() override { ... }
};

PluginManager pm;
auto r = pm.load("plugins/libmy_plugin.so");
pm.tick(0.05f);  // forwards to all plugins
```

## Microservices

```cpp
using namespace cgs::service;

AuthServerConfig cfg;
cfg.port = 8000;
cfg.jwt_private_key_path = "config/keys/jwt_private.pem";

AuthServer server(cfg, foundation);
server.start();
// ... runs until SIGINT
server.stop();
```

## Common Patterns

### Logging with correlation ID

```cpp
auto correlation_id = ctx.correlation_id();
foundation.logger()
    .with_correlation(correlation_id)
    .info("processing request {}", request.id);
```

### Metric increment

```cpp
foundation.monitor()
    .counter("auth_login_total")
    .with_tag("result", "success")
    .increment();
```

### Database query with prepared statement

```cpp
auto result = co_await foundation.database()
    .prepare("SELECT id, name FROM users WHERE id = $1")
    .bind(user_id)
    .execute();
```

## Build Macros

```cpp
#if CGS_VERSION_MAJOR >= 1
    // 1.x+ behavior
#endif

#if defined(CGS_HOT_RELOAD)
    // Plugin hot reload enabled
#endif
```

## See Also

- Full API: [`API_REFERENCE.md`](API_REFERENCE.md)
- Architecture: [`ARCHITECTURE.md`](ARCHITECTURE.md)
- Tutorial: [`GETTING_STARTED.md`](GETTING_STARTED.md)
- Plugin guide: [`guides/PLUGIN_DEVELOPMENT_GUIDE.md`](guides/PLUGIN_DEVELOPMENT_GUIDE.md)
