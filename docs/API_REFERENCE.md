# API Reference

> **English** · [한국어](API_REFERENCE.kr.md)

This document provides a comprehensive reference for the public API of
`common_game_server`. For a 1-page cheat sheet, see [`API_QUICK_REFERENCE.md`](API_QUICK_REFERENCE.md).
For auto-generated, code-driven docs, see the Doxygen output.

> **Stability**: Pre-1.0 (v0.1.0). API may change between minor versions.
> See [`../VERSIONING.md`](../VERSIONING.md).

## Namespaces

| Namespace | Purpose | Header |
|-----------|---------|--------|
| `cgs::core` | Result, error codes, common types | `<cgs/core/...>` |
| `cgs::foundation` | Foundation adapter interfaces | `<cgs/foundation/...>` |
| `cgs::ecs` | Entity-Component System | `<cgs/ecs/...>` |
| `cgs::game` | Game logic systems | `<cgs/game/...>` |
| `cgs::plugin` | Plugin interfaces | `<cgs/plugin/...>` |
| `cgs::service` | Service types and configs | `<cgs/service/...>` |

## `cgs::core`

### `Result<T, E>`

Monadic error handling type. Inherits semantics from `kcenon::common::Result<T>`.

```cpp
template<typename T, typename E = error_code>
class Result {
public:
    // Construction
    Result(T value);                       // success
    Result(error<E> err);                  // error

    // Inspection
    bool has_value() const;
    explicit operator bool() const;        // == has_value()
    const T& value() const&;
    T value() &&;
    const E& error() const&;

    // Monadic combinators
    template<typename F> auto and_then(F&& f);
    template<typename F> auto or_else(F&& f);
    template<typename F> auto map(F&& f);
    template<typename F> auto map_err(F&& f);
};

template<typename E = error_code>
using VoidResult = Result<std::monostate, E>;
```

### `error_code` enum

Centralized error code registry. See [`advanced/ERROR_CODES.md`](advanced/ERROR_CODES.md)
(when available) for the full table.

```cpp
enum class error_code : int32_t {
    ok = 0,
    unknown = -1,
    invalid_argument = -2,
    not_found = -3,
    permission_denied = -4,
    resource_exhausted = -5,
    timeout = -6,
    canceled = -7,
    network_error = -100,
    database_error = -200,
    plugin_load_failed = -300,
    // ... see error_codes.hpp
};
```

## `cgs::foundation`

### `IGameLogger`

```cpp
class IGameLogger {
public:
    virtual ~IGameLogger() = default;

    virtual void trace(std::string_view msg) = 0;
    virtual void debug(std::string_view msg) = 0;
    virtual void info(std::string_view msg) = 0;
    virtual void warn(std::string_view msg) = 0;
    virtual void error(std::string_view msg) = 0;

    template<typename... Args>
    void info(fmt::format_string<Args...> fmt, Args&&... args);

    virtual IGameLogger& with_correlation(std::string_view id) = 0;
};
```

### `IGameNetwork`

```cpp
class IGameNetwork {
public:
    virtual ~IGameNetwork() = default;

    virtual Result<void> listen(uint16_t port) = 0;
    virtual Result<void> stop() = 0;
    virtual Result<connection_id> connect(std::string_view host, uint16_t port) = 0;
    virtual Result<size_t> send(connection_id id, std::span<const std::byte> data) = 0;
    virtual void on_message(message_handler handler) = 0;
    virtual void on_disconnect(disconnect_handler handler) = 0;
};
```

### `IGameDatabase`

```cpp
class IGameDatabase {
public:
    virtual ~IGameDatabase() = default;

    virtual Result<query_handle> prepare(std::string_view sql) = 0;
    virtual Result<rowset> execute(query_handle q, parameter_list params = {}) = 0;
    virtual Result<void> begin_transaction() = 0;
    virtual Result<void> commit() = 0;
    virtual Result<void> rollback() = 0;
};
```

### `IGameThreadPool`

```cpp
class IGameThreadPool {
public:
    virtual ~IGameThreadPool() = default;

    template<typename F>
    auto submit(F&& f) -> std::future<std::invoke_result_t<F>>;

    virtual size_t worker_count() const = 0;
    virtual void wait_all() = 0;
};
```

### `IGameMonitor`

```cpp
class IGameMonitor {
public:
    virtual counter_handle counter(std::string_view name) = 0;
    virtual gauge_handle gauge(std::string_view name) = 0;
    virtual histogram_handle histogram(std::string_view name) = 0;
};
```

### `GameFoundation`

Facade combining all foundation adapters.

```cpp
class GameFoundation {
public:
    static GameFoundation create_default();
    static GameFoundation from_config(std::string_view yaml_path);

    IGameLogger& logger();
    IGameNetwork& network();
    IGameDatabase& database();
    IGameThreadPool& thread_pool();
    IGameMonitor& monitor();
};
```

## `cgs::ecs`

### `EntityManager`

```cpp
class EntityManager {
public:
    entity_id create();
    void destroy(entity_id e);
    bool valid(entity_id e) const;

    template<typename Component, typename... Args>
    Component& add(entity_id e, Args&&... args);

    template<typename Component>
    void remove(entity_id e);

    template<typename Component>
    Component* try_get(entity_id e);

    template<typename... Components>
    Query<Components...> query();
};
```

### `Query<Components...>`

```cpp
template<typename... Components>
class Query {
public:
    template<typename F> void each(F&& f);
    template<typename F> void par_each(F&& f);
    size_t count() const;
};
```

### `SystemScheduler`

```cpp
class SystemScheduler {
public:
    template<typename System, typename... Args>
    SystemHandle add(Args&&... args);

    template<typename System>
    SystemHandle add_after(/* ... */);

    void tick(EntityManager& em, float dt);
};
```

## `cgs::plugin`

### `GamePlugin`

```cpp
class GamePlugin {
public:
    virtual ~GamePlugin() = default;
    virtual std::string_view name() const = 0;
    virtual std::string_view version() const = 0;
    virtual std::vector<std::string> dependencies() const { return {}; }

    virtual Result<void> on_load(PluginContext& ctx) = 0;
    virtual Result<void> on_tick(float dt) = 0;
    virtual Result<void> on_unload() = 0;
};

// Required entry point in every plugin shared library
extern "C" GamePlugin* cgs_create_plugin();
```

### `PluginManager`

```cpp
class PluginManager {
public:
    explicit PluginManager(GameFoundation& foundation);

    Result<plugin_handle> load(std::string_view path);
    Result<void> unload(plugin_handle h);
    Result<void> tick_all(float dt);
    std::vector<plugin_info> list() const;
};
```

## `cgs::service`

Each service exposes a configuration struct and a server class:

```cpp
struct AuthServerConfig { /* ... */ };
class AuthServer { /* ... */ };

struct GatewayServerConfig { /* ... */ };
class GatewayServer { /* ... */ };

struct GameServerConfig { /* ... */ };
class GameServer { /* ... */ };

struct LobbyServerConfig { /* ... */ };
class LobbyServer { /* ... */ };

struct DBProxyConfig { /* ... */ };
class DBProxy { /* ... */ };
```

All services follow the same lifecycle pattern:

```cpp
SomeServiceConfig cfg = SomeServiceConfig::from_yaml("config/service.yaml");
SomeService server(cfg, foundation);
auto r = server.start();          // returns Result<void>
// ... runs until stop() is called or signal received
server.stop();
```

## Versioning

Compile-time version macros are available in `<cgs/version.hpp>`:

```cpp
#define CGS_VERSION_MAJOR 0
#define CGS_VERSION_MINOR 1
#define CGS_VERSION_PATCH 0
#define CGS_VERSION_STRING "0.1.0"
```

Runtime version query:

```cpp
namespace cgs {
    constexpr std::string_view version() noexcept;
}
```

## See Also

- [`API_QUICK_REFERENCE.md`](API_QUICK_REFERENCE.md) — 1-page cheat sheet
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — How these APIs fit together
- [`advanced/`](advanced/) — Internal details, performance notes
- Doxygen output — Auto-generated, code-driven reference (run `doxygen Doxyfile`)
