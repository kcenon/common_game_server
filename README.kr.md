# Common Game Server

[![CI](https://github.com/kcenon/common_game_server/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/common_game_server/actions/workflows/ci.yml)
[![Code Coverage](https://github.com/kcenon/common_game_server/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/common_game_server/actions/workflows/coverage.yml)
[![API Docs](https://github.com/kcenon/common_game_server/actions/workflows/docs.yml/badge.svg)](https://github.com/kcenon/common_game_server/actions/workflows/docs.yml)
[![License](https://img.shields.io/badge/license-BSD%203--Clause-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

> English: [README.md](README.md)

여러 게임 서버 구현체에서 검증된 패턴을 통합한 프로덕션 준비 C++20 게임 서버
프레임워크입니다. [kcenon 생태계](docs/ECOSYSTEM.md)의 **Tier 3 애플리케이션
계층**으로 설계되었습니다.

## 주요 기능

- **Entity-Component System (ECS)** — 데이터 지향 설계, 병렬 실행, 컴포넌트 쿼리
- **플러그인 아키텍처** — 핫 리로드 가능한 플러그인, 의존성 해결, 이벤트 통신
- **마이크로서비스** — 5개의 수평 확장 가능한 서비스 (Auth, Gateway, Game, Lobby, DBProxy)
- **7개의 Foundation 어댑터** — 로깅, 네트워킹, 데이터베이스, 스레딩, 모니터링, 직렬화, 공통 유틸리티
- **게임 로직 시스템** — Object, Combat, World, AI (BehaviorTree), Quest, Inventory
- **보안** — JWT RS256 서명, TLS 1.3, 입력 검증, SQL 매개변수화
- **신뢰성** — WAL + 스냅샷, 회로 차단기, 우아한 종료, 카오스 테스트
- **클라우드 네이티브** — Kubernetes 지원 (HPA, StatefulSet, PDB, Prometheus/Grafana)
- **구조화된 로깅** — 상관 ID를 포함한 JSON 출력
- **Doxygen API 문서** — 소스 주석에서 자동 생성

전체 기능 매트릭스: [`docs/FEATURES.kr.md`](docs/FEATURES.kr.md)

## 아키텍처

```
+-------------------------------------------------------------------+
|                      COMMON GAME SERVER                            |
+-------------------------------------------------------------------+
|  Layer 6: 플러그인 계층 (MMORPG, 배틀로얄, RTS, 커스텀)             |
|           핫 리로드, 의존성 해결, 이벤트 통신                       |
+-------------------------------------------------------------------+
|  Layer 5: 게임 로직 계층                                            |
|           Object, Combat, World, AI, Quest, Inventory              |
+-------------------------------------------------------------------+
|  Layer 4: 코어 ECS 계층                                             |
|           Entity, Component, System Scheduler, Query, 병렬 실행     |
+-------------------------------------------------------------------+
|  Layer 3: 서비스 계층                                               |
|           Auth, Gateway, Game, Lobby, DBProxy                      |
+-------------------------------------------------------------------+
|  Layer 2: FOUNDATION 어댑터 계층                                   |
|           Result<T,E> 패턴, 예외 사용 안 함                         |
+-------------------------------------------------------------------+
|  Layer 1: 7개의 KCENON FOUNDATION 시스템                            |
|           common, thread, logger, network, database, container,    |
|           monitoring                                               |
+-------------------------------------------------------------------+
```

상세 아키텍처: [`docs/ARCHITECTURE.kr.md`](docs/ARCHITECTURE.kr.md)

## 생태계 위치

`common_game_server`는 kcenon 생태계의 **Tier 3 애플리케이션 계층**이며,
kcenon foundation 스택 전체를 사용합니다.

```
Tier 0  common_system        ◀── foundation 인터페이스
Tier 1  thread_system, container_system
Tier 2  logger_system, monitoring_system, database_system, network_system
Tier 3  common_game_server   ◀── 본 프로젝트
```

전체 생태계 맵: [`docs/ECOSYSTEM.md`](docs/ECOSYSTEM.md) ·
의존성 매트릭스: [`DEPENDENCY_MATRIX.md`](DEPENDENCY_MATRIX.md)

## 성능 목표

| 지표 | 목표 |
|------|------|
| 동시 접속자 | 클러스터당 10,000+ |
| 메시지 처리량 | 300K+ msg/sec |
| 월드 틱 레이트 | 20 Hz (50ms) |
| 엔티티 처리 | 10K 엔티티를 5ms 미만에 |
| 데이터베이스 지연 | <50ms p99 |
| 플러그인 로드 시간 | <100ms |

상세 벤치마크: [`docs/BENCHMARKS.kr.md`](docs/BENCHMARKS.kr.md)

## 기술 스택

| 컴포넌트 | 기술 |
|---------|------|
| 언어 | C++20 |
| 빌드 시스템 | CMake 3.20+ (presets 사용) |
| 패키지 매니저 | Conan 2 (또는 vcpkg) |
| 데이터베이스 | PostgreSQL 14+ |
| 컨테이너 | Docker + Docker Compose |
| 오케스트레이션 | Kubernetes (HPA, StatefulSet, PDB) |
| 모니터링 | Prometheus + Grafana |
| 코드 스타일 | clang-format 21 (Google 기반) |
| 문서화 | Doxygen 1.12.0 |

## 시작하기

### 사전 요구 사항

- C++20 컴파일러 (GCC 11+, Clang 14+, MSVC 2022+, Apple Clang 14+)
- CMake 3.20 이상
- Conan 2 패키지 매니저
- PostgreSQL 14+ (데이터베이스 기능용)
- Docker (선택 사항, 컨테이너 배포용)

### 소스에서 빌드

```bash
git clone https://github.com/kcenon/common_game_server.git
cd common_game_server

# Conan으로 의존성 설치
conan install . --output-folder=build --build=missing -s build_type=Release

# CMake 프리셋으로 구성 및 빌드
cmake --preset conan-release
cmake --build --preset conan-release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# 테스트 실행
ctest --preset conan-release --output-on-failure
```

단계별 튜토리얼: [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) ·
전체 빌드 가이드: [`docs/guides/BUILD_GUIDE.md`](docs/guides/BUILD_GUIDE.md)

### 서비스 실행

```bash
./build/bin/auth_server --config config/auth.yaml
./build/bin/gateway_server --config config/gateway.yaml
./build/bin/game_server --config config/game.yaml
```

### Docker 배포

```bash
cd deploy
docker compose up --build
```

### Kubernetes 배포

```bash
kubectl apply -f deploy/k8s/base/
```

프로덕션 배포: [`docs/guides/DEPLOYMENT_GUIDE.md`](docs/guides/DEPLOYMENT_GUIDE.md)

## 마이크로서비스

| 서비스 | 설명 | 기본 포트 |
|--------|------|----------|
| AuthServer | JWT RS256 인증, 세션 관리, 속도 제한 | 8000 |
| GatewayServer | 클라이언트 라우팅, 로드 밸런싱, 토큰 버킷 속도 제한 | 8080/8081 |
| GameServer | 월드 시뮬레이션, 게임 루프, 맵 인스턴스 관리 | 9000 |
| LobbyServer | 매치메이킹 (ELO), 파티 관리, 지역 기반 큐 | 9100 |
| DBProxy | 연결 풀링, 준비된 명령문, SQL 인젝션 방지 | 5432 |

## 문서

| 카테고리 | 문서 |
|---|---|
| 색인 | [`docs/README.md`](docs/README.md) |
| 빠른 시작 | [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) |
| 기능 | [`docs/FEATURES.kr.md`](docs/FEATURES.kr.md) |
| 아키텍처 | [`docs/ARCHITECTURE.kr.md`](docs/ARCHITECTURE.kr.md) |
| API 레퍼런스 | [`docs/API_REFERENCE.kr.md`](docs/API_REFERENCE.kr.md) · [빠른 참조](docs/API_QUICK_REFERENCE.md) |
| 벤치마크 | [`docs/BENCHMARKS.kr.md`](docs/BENCHMARKS.kr.md) |
| 프로덕션 품질 | [`docs/PRODUCTION_QUALITY.kr.md`](docs/PRODUCTION_QUALITY.kr.md) |
| 프로젝트 구조 | [`docs/PROJECT_STRUCTURE.kr.md`](docs/PROJECT_STRUCTURE.kr.md) |
| 로드맵 | [`docs/ROADMAP.md`](docs/ROADMAP.md) |
| 변경 이력 | [`CHANGELOG.md`](CHANGELOG.md) |
| 생태계 | [`docs/ECOSYSTEM.md`](docs/ECOSYSTEM.md) |
| ADR 기록 | [`docs/adr/`](docs/adr/) |

### How-to 가이드
[빌드](docs/guides/BUILD_GUIDE.md) ·
[배포](docs/guides/DEPLOYMENT_GUIDE.md) ·
[설정](docs/guides/CONFIGURATION_GUIDE.md) ·
[테스트](docs/guides/TESTING_GUIDE.md) ·
[플러그인 개발](docs/guides/PLUGIN_DEVELOPMENT_GUIDE.md) ·
[문제 해결](docs/guides/TROUBLESHOOTING.md) ·
[FAQ](docs/guides/FAQ.md)

### 심화 주제
[ECS 심층 분석](docs/advanced/ECS_DEEP_DIVE.md) ·
[Foundation 어댑터](docs/advanced/FOUNDATION_ADAPTERS.md) ·
[프로토콜 명세](docs/advanced/PROTOCOL_SPECIFICATION.md) ·
[데이터베이스 스키마](docs/advanced/DATABASE_SCHEMA.md)

## CI/CD 파이프라인

본 프로젝트는 모든 푸시와 풀 리퀘스트에 대해 6개의 자동화된 워크플로를 실행합니다:

| 워크플로 | 설명 | 트리거 |
|---------|------|--------|
| **CI** | Lint (clang-format) → 빌드 & 테스트 (3개 구성) | push, PR |
| **Code Coverage** | lcov 커버리지 보고서 | push, PR |
| **API Docs** | Doxygen 문서 생성 | push, PR |
| **Benchmarks** | 성능 벤치마크 스위트 | 수동 |
| **Load Test** | CCU 검증 스크립트 | 수동 |
| **Chaos Tests** | 장애 주입 및 복원력 테스트 | 수동 |

CI 가이드: [`docs/contributing/CI_CD_GUIDE.md`](docs/contributing/CI_CD_GUIDE.md)

## 기여하기

기여를 환영합니다! 개발 워크플로, 코딩 표준, PR 프로세스에 대한 자세한 내용은
[`CONTRIBUTING.md`](CONTRIBUTING.md)를 참조하세요.

- **행동 강령** — [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md)
- **보안 정책** — [`SECURITY.md`](SECURITY.md)
- **버전 정책** — [`VERSIONING.md`](VERSIONING.md)
- **코딩 표준** — [`docs/contributing/CODING_STANDARDS.md`](docs/contributing/CODING_STANDARDS.md)

### 코드 스타일

본 프로젝트는 clang-format 21+ (Google 기반, 4-space 들여쓰기, 100열 제한)을
사용합니다. CI는 모든 PR에서 포맷팅을 강제합니다. 로컬에서 확인하려면:

```bash
find include/cgs src -name '*.hpp' -o -name '*.cpp' | xargs clang-format --dry-run --Werror
```

### Git Blame

`git blame`에서 대량 포맷팅 커밋을 건너뛰려면:

```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

## 라이선스

본 프로젝트는 BSD 3-Clause 라이선스로 배포됩니다 — 자세한 내용은
[LICENSE](LICENSE) 파일을 참조하세요. 서드파티 의존성 라이선스:
[LICENSE-THIRD-PARTY](LICENSE-THIRD-PARTY) 및 [NOTICES](NOTICES).
