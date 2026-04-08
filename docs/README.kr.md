# 문서

> **프로젝트**: common_game_server — [kcenon 생태계](ECOSYSTEM.md)의 Tier 3 애플리케이션
> **버전**: [`../VERSION`](../VERSION) 참조
> [English](README.md) · **한국어**

본 색인은 모든 활성 문서를 나열합니다. 역사적 SDLC 문서는 참조용으로
[`archive/sdlc/`](archive/sdlc/)에 보존되어 있습니다.

## 핵심 문서

| 문서 | 설명 |
|---|---|
| [`GETTING_STARTED.md`](GETTING_STARTED.md) | 신규 사용자용 5-10분 튜토리얼 |
| [`FEATURES.md`](FEATURES.md) · [한](FEATURES.kr.md) | 전체 기능 매트릭스 |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) · [한](ARCHITECTURE.kr.md) | 시스템 설계, 계층 모델, 컴포넌트 |
| [`API_REFERENCE.md`](API_REFERENCE.md) · [한](API_REFERENCE.kr.md) | 공개 API 레퍼런스 |
| [`API_QUICK_REFERENCE.md`](API_QUICK_REFERENCE.md) | 1페이지 치트시트 |
| [`BENCHMARKS.md`](BENCHMARKS.md) · [한](BENCHMARKS.kr.md) | 성능 메트릭과 분석 |
| [`PROJECT_STRUCTURE.md`](PROJECT_STRUCTURE.md) · [한](PROJECT_STRUCTURE.kr.md) | 디렉토리 레이아웃 설명 |
| [`PRODUCTION_QUALITY.md`](PRODUCTION_QUALITY.md) · [한](PRODUCTION_QUALITY.kr.md) | SLA, 준비도 체크리스트 |
| [`ROADMAP.md`](ROADMAP.md) | 구현 마일스톤 |
| [`CHANGELOG.md`](CHANGELOG.md) · [한](CHANGELOG.kr.md) | 릴리스 이력 (루트 [`../CHANGELOG.md`](../CHANGELOG.md) 미러) |

## 생태계 및 컴플라이언스

| 문서 | 설명 |
|---|---|
| [`ECOSYSTEM.md`](ECOSYSTEM.md) | kcenon 생태계 위치 (Tier 3) |
| [`COMPATIBILITY.md`](COMPATIBILITY.md) | C++ 표준, 컴파일러, 플랫폼 지원 |
| [`DEPRECATION.md`](DEPRECATION.md) | 사용 중단된 API와 제거 일정 |
| [`TRACEABILITY.md`](TRACEABILITY.md) | 교차 문서 참조 맵 |
| [`SOUP.md`](SOUP.md) | Software of Unknown Pedigree (서드파티 BOM) |

## How-to 가이드

| 가이드 | 설명 |
|---|---|
| [`guides/BUILD_GUIDE.md`](guides/BUILD_GUIDE.md) | Conan + CMake 빌드 |
| [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md) | 프로덕션 배포 (Docker, Kubernetes) |
| [`guides/CONFIGURATION_GUIDE.md`](guides/CONFIGURATION_GUIDE.md) | YAML 설정, env vars, 핫 리로드 |
| [`guides/TESTING_GUIDE.md`](guides/TESTING_GUIDE.md) | 단위, 통합, 부하, 카오스 테스트 |
| [`guides/PLUGIN_DEVELOPMENT_GUIDE.md`](guides/PLUGIN_DEVELOPMENT_GUIDE.md) | 커스텀 게임 플러그인 작성 |
| [`guides/INTEGRATION_GUIDE.md`](guides/INTEGRATION_GUIDE.md) | 하위 프로젝트와 통합 |
| [`guides/TROUBLESHOOTING.md`](guides/TROUBLESHOOTING.md) | 일반적인 문제와 해결책 |
| [`guides/FAQ.md`](guides/FAQ.md) | 자주 묻는 질문 |

## 튜토리얼 (Doxygen 페이지)

실행 가능한 튜토리얼은 `docs/tutorial_*.dox` 아래에 있으며 퍼블리시된
Doxygen 사이트로 렌더링됩니다. 각 튜토리얼은 `cmake --preset debug`로
빌드할 수 있는 [`../examples/`](../examples/) 동반 프로그램을 갖고 있습니다.

**프레임워크 기초**

| 튜토리얼 | 주제 |
|---|---|
| `tutorial_ecs.dox` | Entity-Component System 사용법 |
| `tutorial_plugin.dox` | 플러그인 작성 스켈레톤 |
| `tutorial_service.dox` | 마이크로서비스 실행 및 설정 |

**핵심 패턴**

| 튜토리얼 | 주제 |
|---|---|
| `tutorial_result_errors.dox` | `Result<T>`, `GameError`, 분류된 에러 코드 |
| `tutorial_foundation_adapters.dox` | 파운데이션 어댑터와 `ServiceLocator` |

**게임 로직**

| 튜토리얼 | 주제 |
|---|---|
| `tutorial_game_objects.dox` | `Transform`, `Identity`, `Stats`, `Movement` |
| `tutorial_spatial_world.dox` | `SpatialIndex`, 존, 관심 관리 |
| `tutorial_combat.dox` | 전투 시스템, 데미지 타입, 오라, 스펠 캐스팅 |
| `tutorial_ai_behavior.dox` | AI 행동 트리와 블랙보드 |
| `tutorial_inventory.dox` | 인벤토리와 장비 슬롯 |
| `tutorial_quest.dox` | 퀘스트 상태 머신과 목표 |

**영속성과 네트워킹**

| 튜토리얼 | 주제 |
|---|---|
| `tutorial_database.dox` | `GameDatabase`, 프리페어드 스테이트먼트, 트랜잭션 |
| `tutorial_networking.dox` | `NetworkMessage`, 시그널, TLS |

동반 실행 프로그램은 [`../examples/README.md`](../examples/README.md)를
참조하세요.

## 심화 주제

| 문서 | 설명 |
|---|---|
| [`advanced/ECS_DEEP_DIVE.md`](advanced/ECS_DEEP_DIVE.md) | Entity-Component System 내부 |
| [`advanced/FOUNDATION_ADAPTERS.md`](advanced/FOUNDATION_ADAPTERS.md) | 어댑터 패턴 레퍼런스 |
| [`advanced/PROTOCOL_SPECIFICATION.md`](advanced/PROTOCOL_SPECIFICATION.md) | 네트워크 프로토콜 명세 |
| [`advanced/DATABASE_SCHEMA.md`](advanced/DATABASE_SCHEMA.md) | 데이터베이스 설계 |

## 아키텍처 결정 기록

| ADR | 제목 |
|---|---|
| [`adr/ADR-001-unified-game-server-architecture.md`](adr/ADR-001-unified-game-server-architecture.md) | 4개의 레거시 프로젝트를 통합한 이유 |
| [`adr/ADR-002-entity-component-system.md`](adr/ADR-002-entity-component-system.md) | OOP 대신 ECS |
| [`adr/ADR-003-plugin-hot-reload.md`](adr/ADR-003-plugin-hot-reload.md) | 플러그인 핫 리로드 모델 |
| [`adr/ADR-004-microservice-decomposition.md`](adr/ADR-004-microservice-decomposition.md) | 5개의 서비스 분해 |

## 기여하기

| 문서 | 설명 |
|---|---|
| [`contributing/CODING_STANDARDS.md`](contributing/CODING_STANDARDS.md) | C++ 스타일 가이드라인 |
| [`contributing/DOCUMENTATION_GUIDELINES.md`](contributing/DOCUMENTATION_GUIDELINES.md) | 문서 작성 방법 |
| [`contributing/CI_CD_GUIDE.md`](contributing/CI_CD_GUIDE.md) | CI/CD 워크플로 가이드 |
| [`contributing/CHANGELOG_TEMPLATE.md`](contributing/CHANGELOG_TEMPLATE.md) | 변경 이력 항목 템플릿 |
| [`contributing/templates/`](contributing/templates/) | 마크다운 템플릿 |

## 성능

| 문서 | 설명 |
|---|---|
| [`performance/BENCHMARKS_METHODOLOGY.md`](performance/BENCHMARKS_METHODOLOGY.md) | 벤치마크 측정 방법 |

## 아카이브

| 문서 | 설명 |
|---|---|
| [`archive/README.md`](archive/README.md) | 아카이브 개요 |
| [`archive/sdlc/`](archive/sdlc/) | 레거시 SDLC 문서 (PRD, SRS, SDS 등) |

---

*프로젝트 개요와 빠른 시작은 [`../README.kr.md`](../README.kr.md)를 참조하세요.*
