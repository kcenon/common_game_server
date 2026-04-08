# 아키텍처

> [English](ARCHITECTURE.md) · **한국어**

본 문서는 `common_game_server`의 고수준 아키텍처를 설명합니다. 특정 서브시스템의
내부 세부 사항은 [`advanced/`](advanced/) 디렉토리를 참조하세요.

## 계층 모델

`common_game_server`는 6개 계층으로 구성되며, 엄격한 하향식 의존 흐름을 갖습니다.
상위 계층은 하위 계층에 의존할 수 있지만, 그 반대는 결코 없습니다.

```
+-------------------------------------------------------------------+
|  Layer 6: 플러그인 계층                                             |
|  ----------------------------------------------------------------- |
|  MMORPG / 배틀로얄 / RTS / 커스텀 플러그인                          |
|  핫 리로드 (개발), 의존성 해결, 이벤트 통신                         |
+-------------------------------------------------------------------+
|  Layer 5: 게임 로직 계층                                            |
|  ----------------------------------------------------------------- |
|  Object · Combat · World · AI (BehaviorTree) · Quest · Inventory   |
+-------------------------------------------------------------------+
|  Layer 4: 코어 ECS 계층                                             |
|  ----------------------------------------------------------------- |
|  EntityManager · ComponentPool (SparseSet) · SystemScheduler (DAG) |
|  Query (컴파일 타임 concepts) · 병렬 실행                           |
+-------------------------------------------------------------------+
|  Layer 3: 서비스 계층                                               |
|  ----------------------------------------------------------------- |
|  AuthServer · GatewayServer · GameServer · LobbyServer · DBProxy   |
|  공유 service_runner 템플릿, YAML 설정                              |
+-------------------------------------------------------------------+
|  Layer 2: FOUNDATION 어댑터 계층                                   |
|  ----------------------------------------------------------------- |
|  IGameLogger · IGameNetwork · IGameDatabase · IGameThreadPool      |
|  IGameMonitor · IGameContainer · IGameCommon                       |
|  Result<T, E> 오류 모델, 경계를 넘는 예외 없음                      |
+-------------------------------------------------------------------+
|  Layer 1: KCENON FOUNDATION 시스템                                  |
|  ----------------------------------------------------------------- |
|  common_system (Tier 0) · thread_system · container_system         |
|  logger_system · monitoring_system · database_system               |
|  network_system                                                    |
+-------------------------------------------------------------------+
```

## 계층별 책임

### Layer 1 — kcenon Foundation 시스템

FetchContent 또는 vcpkg로 가져오는 외부 의존성. `common_game_server`는 릴리스
태그(현재 v0.2.0+)에 고정합니다. [`../DEPENDENCY_MATRIX.md`](../DEPENDENCY_MATRIX.md) 참조.

**핵심 원칙**: `common_game_server`는 foundation 시스템을 수정하거나 fork하지
않습니다. 모든 생태계 개선 사항은 해당 저장소로 upstream 합니다.

### Layer 2 — Foundation 어댑터 계층

kcenon API를 게임 서버 친화적인 인터페이스로 번역하는 얇은 래퍼.

**왜 래퍼 계층이 필요한가?**
1. 게임 특화 명명 (`IGameLogger` vs `kcenon::logger::ILogger`)
2. 일관된 오류 모델 (`cgs::core::Result<T, E>` 전역 사용)
3. 테스트 모킹 — 어댑터를 단위 테스트용으로 교체 가능
4. 상위 API 변경으로부터의 절연

헤더: [`include/cgs/foundation/`](../include/cgs/foundation/)
구현: [`src/foundation/`](../src/foundation/)
상세: [`advanced/FOUNDATION_ADAPTERS.md`](advanced/FOUNDATION_ADAPTERS.md)

### Layer 3 — 서비스 계층

5개의 마이크로서비스, 각각 독립형 실행 파일:

| 서비스 | 역할 |
|--------|------|
| AuthServer | 인증, JWT 발급, 세션 저장소 |
| GatewayServer | 클라이언트 라우팅, 로드 밸런싱, 속도 제한 |
| GameServer | 월드 시뮬레이션, 게임 틱 루프 |
| LobbyServer | 매치메이킹, 파티 관리 |
| DBProxy | 연결 풀링, 준비된 명령문 |

모든 서비스는 `service_runner` 템플릿([`src/services/shared/`](../src/services/shared/))을
공유하며, 다음을 처리합니다:
- YAML 설정 로딩
- Foundation 어댑터 와이어링
- 시그널 처리 (SIGINT, SIGTERM)
- 우아한 종료
- Health/readiness probe 엔드포인트

구현: [`src/services/`](../src/services/)
상세: [`adr/ADR-004-microservice-decomposition.md`](adr/ADR-004-microservice-decomposition.md)

### Layer 4 — 코어 ECS 계층

데이터 지향 Entity-Component System.

**핵심 타입**:
- `EntityManager` — 엔티티 생성/소멸, 버전 재활용
- `ComponentPool<T>` — 컴포넌트 타입 T를 위한 sparse-set 저장소
- `SystemScheduler` — 병렬 실행을 갖는 DAG 기반 시스템 순서 결정
- `Query<Components...>` — 컴파일 타임 검증 컴포넌트 반복

**성능 특성**:
- 캐시 친화적 SoA 컴포넌트 레이아웃
- O(1) 엔티티 조회, O(1) 컴포넌트 추가/제거
- 10,000 엔티티를 5 ms 미만에 처리 (단일 틱)

헤더: [`include/cgs/ecs/`](../include/cgs/ecs/)
구현: [`src/ecs/`](../src/ecs/)
상세: [`advanced/ECS_DEEP_DIVE.md`](advanced/ECS_DEEP_DIVE.md) ·
[`adr/ADR-002-entity-component-system.md`](adr/ADR-002-entity-component-system.md)

### Layer 5 — 게임 로직 계층

표준적인 게임 서버 기능을 구현하는 구체적 ECS 시스템:

| 시스템 | 컴포넌트 | 목적 |
|--------|---------|------|
| ObjectSystem | Position, Velocity, Rotation | 엔티티 변환 |
| CombatSystem | Health, Attack, Defense | 데미지 해결 |
| WorldSystem | MapInstance, Region | 월드 상태 및 영역 |
| AISystem | BehaviorTree, AIState | NPC 의사 결정 |
| QuestSystem | QuestProgress, QuestObjective | 퀘스트 추적 |
| InventorySystem | Inventory, ItemSlot | 플레이어 인벤토리 |

이는 레퍼런스 구현입니다 — 플러그인이 이를 대체하거나 확장할 수 있습니다.

헤더: [`include/cgs/game/`](../include/cgs/game/)
구현: [`src/game/`](../src/game/)

### Layer 6 — 플러그인 계층

사용자가 작성한 게임 로직, 동적으로 로드됨.

**라이프사이클**:
1. `PluginManager::load(path)` — 공유 라이브러리 열기, `cgs_create_plugin()` 호출
2. `GamePlugin::on_load(ctx)` — 플러그인 초기화
3. `GamePlugin::on_tick(dt)` — 매 월드 틱(50 ms)마다 호출
4. `GamePlugin::on_unload()` — 공유 라이브러리 종료 전 정리

**핫 리로드** (`CGS_HOT_RELOAD=ON`, 개발 전용):
- 파일 워처가 `plugins/` 디렉토리 모니터링
- 변경 시: 이전 버전을 우아하게 unload, 새 버전 로드
- 플러그인이 정의한 `serialize`/`deserialize` 훅을 통한 상태 마이그레이션

**샘플 플러그인**: `src/plugins/mmorpg/` — 완전한 MMORPG 게임 루프 레퍼런스.

헤더: [`include/cgs/plugin/`](../include/cgs/plugin/)
구현: [`src/plugins/`](../src/plugins/)
상세: [`adr/ADR-003-plugin-hot-reload.md`](adr/ADR-003-plugin-hot-reload.md)

## 횡단 관심사

### 오류 처리

모든 실패 가능한 작업은 `cgs::core::Result<T, E>`를 반환합니다. 계층 경계를 넘는
예외는 금지됩니다. 오류 타입 `E`는 일반적으로 `cgs::core::error_code` 또는 도메인
특화 enum입니다.

왜 예외를 사용하지 않는가?
- 예측 가능한 성능 (핫 패스에서 스택 언와인딩 비용 없음)
- 플러그인 ABI 안전성 (예외는 공유 라이브러리 경계를 안정적으로 넘지 못함)
- kcenon 생태계와의 일관성 (`common_system::Result<T>`)

### 로깅

모든 로깅은 `IGameLogger`를 통해 흐릅니다. 로그는 필수 필드 `timestamp`,
`level`, `service`, `correlation_id`, `message`와 함께 구조화된 JSON으로 발행됩니다.
상관 ID는 게이트웨이에서 생성되어 모든 서비스 hop과 ECS 시스템 호출을 통해 전파됩니다.

### 메트릭

모든 서비스는 포트 9090에서 Prometheus 메트릭을 노출합니다 (`/metrics`). 표준
메트릭: 요청 수, 요청 지속 시간 히스토그램, 오류 수, 활성 연결, 월드 틱 지속 시간.

### 설정

각 서비스는 YAML 설정 파일을 로드합니다. 스키마는
[`guides/CONFIGURATION_GUIDE.md`](guides/CONFIGURATION_GUIDE.md)에 문서화되어 있습니다.
설정 핫 리로드는 SIGHUP을 통해 지원됩니다.

## 배포 토폴로지

```
                    ┌─────────────────┐
                    │  게임 클라이언트│
                    └────────┬────────┘
                             │ TLS 1.3
                    ┌────────▼────────┐
                    │ GatewayServer   │ (n 복제본, HPA)
                    └────────┬────────┘
                             │
                    ┌────────┼────────────────┬────────┐
                    │        │                │        │
            ┌───────▼───┐ ┌──▼────┐ ┌───▼──┐ ┌──▼─────┐
            │ AuthServer│ │ Game  │ │ Lobby│ │DBProxy │
            │           │ │Server │ │Server│ │        │
            └───────────┘ └───┬───┘ └──────┘ └────┬───┘
                              │                    │
                              │              ┌─────▼─────┐
                              │              │PostgreSQL │
                              │              └───────────┘
                              │
                       ┌──────▼──────┐
                       │  플러그인   │
                       │ (MMORPG,    │
                       │  커스텀)    │
                       └─────────────┘
```

각 서비스는 Kubernetes HPA를 통해 수평 확장 가능합니다. DBProxy는 연결 친화성을
유지하기 위해 StatefulSet을 사용합니다. PDB는 롤링 업데이트 중 가용성을 보장합니다.

매니페스트: [`deploy/k8s/`](../deploy/k8s/)
프로덕션 가이드: [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md)

## 설계 원칙

1. **계층 분리** — 엄격한 하향식 의존성, 상향 호출 없음
2. **Result<T, E> 전역 사용** — 계층 경계를 넘는 예외 없음
3. **어댑터 패턴** — 서드파티 API를 프로젝트 소유 인터페이스로 래핑
4. **데이터 지향 설계** — SoA 컴포넌트 레이아웃, 캐시 친화적 반복
5. **상속보다 합성** — ECS 컴포넌트, OOP 계층 구조 아님
6. **플러그인 우선** — 게임 특화 로직은 프레임워크가 아닌 플러그인에 위치
7. **기본 관측 가능** — 로깅, 메트릭, 트레이싱이 모든 계층에 내장
8. **기본 클라우드 네이티브** — Kubernetes 매니페스트가 일등 시민

## 참조

- [`FEATURES.kr.md`](FEATURES.kr.md) — 각 계층이 제공하는 것
- [`API_REFERENCE.kr.md`](API_REFERENCE.kr.md) — 계층별 공개 API
- [`PROJECT_STRUCTURE.kr.md`](PROJECT_STRUCTURE.kr.md) — 디렉토리 레이아웃
- [`adr/`](adr/) — 아키텍처 결정과 근거
- [`advanced/`](advanced/) — 특정 서브시스템 심층 분석
