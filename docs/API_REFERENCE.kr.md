# API 레퍼런스

> [English](API_REFERENCE.md) · **한국어**

본 문서는 `common_game_server`의 공개 API에 대한 종합 레퍼런스를 제공합니다.
1페이지 치트시트는 [`API_QUICK_REFERENCE.md`](API_QUICK_REFERENCE.md)를,
자동 생성되는 코드 기반 문서는 Doxygen 출력을 참조하세요.

> **안정성**: Pre-1.0 (v0.1.0). API는 minor 버전 사이에 변경될 수 있습니다.
> [`../VERSIONING.md`](../VERSIONING.md) 참조.

## 네임스페이스

| 네임스페이스 | 목적 | 헤더 |
|------------|------|------|
| `cgs::core` | Result, error codes, 공통 타입 | `<cgs/core/...>` |
| `cgs::foundation` | Foundation 어댑터 인터페이스 | `<cgs/foundation/...>` |
| `cgs::ecs` | Entity-Component System | `<cgs/ecs/...>` |
| `cgs::game` | 게임 로직 시스템 | `<cgs/game/...>` |
| `cgs::plugin` | 플러그인 인터페이스 | `<cgs/plugin/...>` |
| `cgs::service` | 서비스 타입 및 설정 | `<cgs/service/...>` |

## `cgs::core::Result<T, E>`

Monadic 오류 처리 타입. `kcenon::common::Result<T>`에서 의미를 상속합니다.

```cpp
template<typename T, typename E = error_code>
class Result {
public:
    Result(T value);                       // 성공
    Result(error<E> err);                  // 오류

    bool has_value() const;
    explicit operator bool() const;
    const T& value() const&;
    const E& error() const&;

    template<typename F> auto and_then(F&& f);
    template<typename F> auto or_else(F&& f);
    template<typename F> auto map(F&& f);
};
```

전체 API 표면은 영문 [`API_REFERENCE.md`](API_REFERENCE.md)와 동일합니다.
번역 노력 절약을 위해 본 문서는 핵심 타입과 사용 예시만 한국어로 제공하고,
시그니처 자체는 영문판을 정본으로 합니다.

## `cgs::foundation` 핵심 어댑터

```cpp
// 게임 서버 친화적 로거 인터페이스
class IGameLogger {
public:
    virtual void info(std::string_view msg) = 0;
    virtual IGameLogger& with_correlation(std::string_view id) = 0;
};

// 모든 어댑터를 결합한 facade
class GameFoundation {
public:
    static GameFoundation create_default();
    IGameLogger& logger();
    IGameNetwork& network();
    IGameDatabase& database();
    IGameThreadPool& thread_pool();
    IGameMonitor& monitor();
};
```

## `cgs::ecs` 사용 예시

```cpp
EntityManager em;
auto e = em.create();
em.add<Position>(e, {10.0f, 20.0f});
em.add<Velocity>(e, {1.0f, 0.0f});

em.query<Position, Velocity>().each([](auto& p, const auto& v) {
    p.x += v.dx;
    p.y += v.dy;
});

SystemScheduler sched;
sched.add<MovementSystem>();
sched.tick(em, 0.05f);  // 50 ms (20 Hz)
```

## `cgs::plugin` 사용 예시

```cpp
class MyPlugin : public GamePlugin {
public:
    Result<void> on_load(PluginContext& ctx) override { return {}; }
    Result<void> on_tick(float dt) override { return {}; }
    Result<void> on_unload() override { return {}; }
};

extern "C" GamePlugin* cgs_create_plugin() { return new MyPlugin(); }
```

## 버전 정보

```cpp
#include <cgs/version.hpp>

#if CGS_VERSION_MAJOR >= 1
    // 1.x+ 동작
#endif
```

## 참조

- [`API_QUICK_REFERENCE.md`](API_QUICK_REFERENCE.md) — 1페이지 치트시트
- [`API_REFERENCE.md`](API_REFERENCE.md) — 영문 정본 (모든 시그니처 포함)
- [`ARCHITECTURE.kr.md`](ARCHITECTURE.kr.md) — 이러한 API가 어떻게 결합되는지
- Doxygen 출력 — 자동 생성, 코드 기반 레퍼런스 (`doxygen Doxyfile` 실행)
