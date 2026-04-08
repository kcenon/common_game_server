# 기능

> [English](FEATURES.md) · **한국어**

`common_game_server`의 전체 기능 매트릭스입니다. 아키텍처는 [`ARCHITECTURE.kr.md`](ARCHITECTURE.kr.md),
성능 수치는 [`BENCHMARKS.kr.md`](BENCHMARKS.kr.md)를 참조하세요.

## 기능 개요

| 카테고리 | 기능 | 상태 |
|---|---|---|
| **Foundation** | 7개의 kcenon 어댑터 인터페이스 | Stable |
| | Result<T, E> 오류 처리 (예외 없음) | Stable |
| | DI 컨테이너 및 이벤트 버스 | Stable |
| | 상관 ID를 포함한 구조화된 JSON 로깅 | Stable |
| **ECS** | sparse-set 저장소를 사용하는 엔티티 매니저 | Stable |
| | SoA 레이아웃의 컴포넌트 풀 | Stable |
| | DAG 의존성을 갖는 시스템 스케줄러 | Stable |
| | 병렬 시스템 실행 | Stable |
| | 컴파일 타임 컴포넌트 쿼리 | Stable |
| **플러그인** | 플러그인 라이프사이클 인터페이스 (`load`/`tick`/`unload`) | Stable |
| | 핫 리로드 (개발용) | Beta |
| | 플러그인 간 의존성 해결 | Stable |
| | 이벤트 버스를 통한 플러그인 통신 | Stable |
| | MMORPG 샘플 플러그인 | Reference |
| **마이크로서비스** | AuthServer (JWT RS256) | Stable |
| | GatewayServer (라우팅, 속도 제한) | Stable |
| | GameServer (월드 시뮬레이션, 20 Hz 틱) | Stable |
| | LobbyServer (ELO 매치메이킹, 파티) | Stable |
| | DBProxy (연결 풀링, 준비된 명령문) | Stable |
| **게임 로직** | Object 시스템 | Stable |
| | Combat 시스템 | Stable |
| | World 시스템 | Stable |
| | AI (BehaviorTree) | Stable |
| | Quest 시스템 | Stable |
| | Inventory 시스템 | Stable |
| **보안** | JWT RS256 서명 및 검증 | Stable |
| | TLS 1.3 종단 처리 | Stable |
| | Token bucket 속도 제한 | Stable |
| | SQL 매개변수화 (문자열 연결 금지) | Stable |
| | 경계에서의 입력 검증 | Stable |
| **신뢰성** | Write-Ahead Log (WAL) + 스냅샷 | Stable |
| | 회로 차단기 패턴 | Stable |
| | 우아한 종료 | Stable |
| | 카오스 테스트 하네스 | Stable |
| **클라우드 네이티브** | 서비스별 Dockerfile | Stable |
| | Kubernetes 매니페스트 (Deployment, Service, HPA, PDB, StatefulSet) | Stable |
| | Prometheus 메트릭 엔드포인트 | Stable |
| | Grafana 대시보드 | Stable |
| | Health 및 readiness probe | Stable |
| **관측성** | 구조화된 로깅 (JSON) | Stable |
| | 상관 ID 전파 | Stable |
| | 메트릭: counters, gauges, histograms | Stable |
| | 분산 트레이싱 훅 | Beta |
| **문서화** | Doxygen API 문서 | Stable |
| | 이중 언어 (영어 / 한국어) 핵심 문서 | Stable |
| | ADR 기록 | Stable |
| | 아키텍처 다이어그램 | Stable |

**상태 범례**:
- **Stable** — 프로덕션 준비됨, MINOR 버전 내에서 API 안정
- **Beta** — 동작하지만 다음 MINOR에서 API 변경 가능
- **Reference** — 샘플 구현, 프로덕션 사용 금지

## Foundation 어댑터 (상세)

`common_game_server`는 7개의 kcenon foundation 시스템을 얇은 어댑터
인터페이스로 사용합니다:

| 어댑터 | 래핑 대상 | 목적 |
|--------|---------|------|
| `IGameLogger` | logger_system | 구조화된 JSON 로깅, 상관 ID |
| `IGameNetwork` | network_system | TCP/WebSocket 서버, 클라이언트 라우팅 |
| `IGameDatabase` | database_system | PostgreSQL 풀, 준비된 명령문 |
| `IGameThreadPool` | thread_system | 비동기 작업 실행, DAG 스케줄링 |
| `IGameMonitor` | monitoring_system | 메트릭, Prometheus 노출 |
| `IGameContainer` | container_system | 타입 안전 상태, SIMD 직렬화 |
| `IGameCommon` | common_system | Result<T>, 오류 코드, DI |

모든 어댑터는 오류 처리에 `cgs::core::Result<T, E>`를 사용합니다 — 예외는
어댑터 경계를 넘지 않습니다.

## ECS (상세)

Entity-Component System은 데이터 지향적입니다:

- **엔티티**는 버전 재활용을 갖는 64-bit ID
- **컴포넌트**는 `SparseSet` 기반 풀에 저장 (캐시 친화적 SoA)
- **시스템**은 DAG로 스케줄링되며 의존성이 허용하는 곳에서 병렬 실행
- **쿼리**는 컴파일 타임 컴포넌트 제약을 적용하기 위해 C++20 concepts 사용

성능: 10K 엔티티를 5 ms 미만에 처리 ([`BENCHMARKS.kr.md`](BENCHMARKS.kr.md) 참조).

## 플러그인 (상세)

플러그인은 `cgs::plugin::GamePlugin`을 구현합니다:

```cpp
class GamePlugin {
public:
    virtual Result<void, error_code> on_load(PluginContext&) = 0;
    virtual Result<void, error_code> on_tick(float dt) = 0;
    virtual Result<void, error_code> on_unload() = 0;
};
```

핫 리로드 (`CGS_HOT_RELOAD=ON`)는 **개발 전용**입니다 — 보안과 안정성을 위해
프로덕션 빌드에서는 비활성화됩니다.

샘플 플러그인: `src/plugins/mmorpg/` — ECS 시스템, foundation 어댑터, 이벤트
통신을 보여주는 완전한 MMORPG 게임 루프 레퍼런스.

## 마이크로서비스 (상세)

| 서비스 | 기본 포트 | 핵심 기능 |
|--------|----------|----------|
| AuthServer | 8000 | JWT RS256, refresh 토큰, 세션 저장, 속도 제한 |
| GatewayServer | 8080 (HTTP), 8081 (WebSocket) | 클라이언트 라우팅, 로드 밸런싱, token bucket 속도 제한 |
| GameServer | 9000 | 월드 시뮬레이션, 게임 루프 @ 20 Hz, 맵 인스턴스 |
| LobbyServer | 9100 | ELO 매치메이킹, 파티 관리, 지역 큐 |
| DBProxy | 5432 | PostgreSQL 연결 풀, 준비된 명령문, 쿼리 라우팅 |

각 서비스는 YAML로 설정 가능한 독립형 바이너리입니다
([`guides/CONFIGURATION_GUIDE.md`](guides/CONFIGURATION_GUIDE.md)). 5개 서비스
모두 `src/services/shared/`에 정의된 `service_runner` 템플릿을 공유합니다.

## 성능 목표

| 지표 | 목표 | 검증 방법 |
|------|------|----------|
| 동시 접속자 (CCU) | 클러스터당 10,000+ | 부하 테스트 |
| 메시지 처리량 | 300,000+ msg/sec | 벤치마크 |
| 월드 틱 레이트 | 20 Hz (50 ms) | 연속 |
| 엔티티 처리 | 10,000 엔티티 < 5 ms | 벤치마크 |
| 데이터베이스 p99 지연 | < 50 ms | 부하 테스트 |
| 플러그인 로드 시간 | < 100 ms | 벤치마크 |

측정 수치와 방법론은 [`BENCHMARKS.kr.md`](BENCHMARKS.kr.md)를 참조하세요.

## 범위 외 (Non-Goals)

다음은 의도적으로 `common_game_server`의 일부가 **아닙니다**:

- **클라이언트 SDK** — 본 프로젝트는 서버 프레임워크입니다. 클라이언트 통합은 게임 스튜디오의 몫입니다.
- **에셋 파이프라인** — 게임 스튜디오는 자신만의 에셋 도구체인을 사용합니다.
- **음성 채팅** — 범위 외; 필요한 경우 서드파티 SFU와 통합하세요.
- **안티치트** — 범위 외; EAC와 같은 벤더 솔루션과 통합하세요.
- **물리 시뮬레이션** — ECS는 물리 컴포넌트를 지원하지만 내장 물리 엔진은 없습니다. Bullet/PhysX를 플러그인으로 통합하세요.
- **렌더링** — 서버 전용입니다. 클라이언트 측 렌더링 코드는 없습니다.
- **영속 월드 도구** — 월드 편집기와 콘텐츠 관리는 스튜디오 측입니다.

## 참조

- [`ARCHITECTURE.kr.md`](ARCHITECTURE.kr.md) — 이러한 기능들이 어떻게 결합되는지
- [`API_REFERENCE.kr.md`](API_REFERENCE.kr.md) — 각 기능의 상세 API
- [`ROADMAP.md`](ROADMAP.md) — 계획된 기능
- [`adr/`](adr/) — 이러한 기능을 선택한 이유
