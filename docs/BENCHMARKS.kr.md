# 벤치마크

> [English](BENCHMARKS.md) · **한국어**

본 문서는 `common_game_server`의 성능 특성을 추적합니다. 상세 방법론은
[`performance/BENCHMARKS_METHODOLOGY.md`](performance/BENCHMARKS_METHODOLOGY.md)
를 참조하세요.

## 성능 목표 vs 측정

| 지표 | 목표 | 측정값 | 상태 |
|------|------|--------|------|
| 동시 접속자 (CCU) | 클러스터당 10,000+ | TBD | 부하 테스트로 검증 |
| 메시지 처리량 | 300,000+ msg/sec | TBD | 벤치마크로 검증 |
| 월드 틱 레이트 | 20 Hz (50 ms 예산) | 10K 엔티티 미만에서 유지 | 연속 |
| 엔티티 처리 | 10,000 엔티티 <5 ms | TBD | 벤치마크 |
| 데이터베이스 p99 지연 | <50 ms | TBD | 부하 테스트 |
| 플러그인 로드 시간 | <100 ms | TBD | 벤치마크 |

> **참고**: TBD = 다음 CI 벤치마크 실행으로 채워질 값. 숫자는 `benchmarks.yml`
> 워크플로에 의해 자동으로 업데이트됩니다.

## 벤치마크 스위트

벤치마크 스위트는 [`tests/benchmark/`](../tests/benchmark/) 아래에 위치하며
Google Benchmark 1.9.1을 사용합니다.

### ECS 벤치마크

| 벤치마크 | 측정 항목 |
|---------|----------|
| `ECS_CreateDestroy` | 엔티티 생성/소멸 처리량 |
| `ECS_AddRemoveComponent` | 기존 엔티티에 컴포넌트 추가/제거 |
| `ECS_Query2Components` | 2-컴포넌트 쿼리 반복 |
| `ECS_Query5Components` | 5-컴포넌트 쿼리 반복 |
| `ECS_ParallelTick` | 10K 엔티티에서 병렬 시스템 실행 |

### Foundation 벤치마크

| 벤치마크 | 측정 항목 |
|---------|----------|
| `Logger_StructuredJson` | JSON 로그 인코딩 처리량 |
| `Network_TcpEcho` | TCP 에코 서버 처리량 |
| `Database_PreparedQuery` | 준비된 명령문 실행 지연 |
| `ThreadPool_TaskSubmit` | 작업 제출 오버헤드 |

### 서비스 벤치마크

| 벤치마크 | 측정 항목 |
|---------|----------|
| `Auth_JwtIssue` | 초당 JWT 발급 |
| `Auth_JwtVerify` | 초당 JWT 검증 |
| `Gateway_Routing` | 요청 라우팅 오버헤드 |
| `Lobby_EloMatch` | ELO 매치메이킹 지연 |

## 로컬에서 벤치마크 실행

```bash
# 벤치마크 활성화로 구성
cmake --preset conan-release -DCGS_BUILD_BENCHMARKS=ON
cmake --build --preset conan-release

# 모든 벤치마크 실행
./build/Release/bin/cgs_benchmarks

# 특정 벤치마크만 실행
./build/Release/bin/cgs_benchmarks --benchmark_filter=ECS_

# JSON으로 출력
./build/Release/bin/cgs_benchmarks --benchmark_format=json > results.json
```

## 하드웨어 기준

(채워졌을 때) 보고된 수치는 다음 CI 하드웨어에서 측정됩니다:

| 환경 | 사양 |
|------|------|
| GitHub Actions Linux | ubuntu-24.04, 4 vCPU, 16 GB RAM |
| GitHub Actions macOS | macos-14, M1, 8 GB RAM |

프로덕션 하드웨어 사이징은 [`PRODUCTION_QUALITY.kr.md`](PRODUCTION_QUALITY.kr.md)를 참조하세요.

## 연속 추적

벤치마크 결과는 `benchmarks.yml` 워크플로를 통해 시간 경과에 따라 추적됩니다.
어떤 벤치마크에서도 10%를 초과하는 성능 회귀는 CI 실패를 트리거합니다.

수동으로 벤치마크 실행을 트리거하려면:

```bash
gh workflow run benchmarks.yml -R kcenon/common_game_server
```

## 재현성 노트

- 모든 벤치마크는 해당되는 경우 고정된 random seed 사용
- 엔티티 수는 별도 명시가 없으면 정확히 10,000
- 틱 레이트 측정은 1,000 틱에 걸쳐 평균
- 지연 백분위수는 1,000,000 샘플 사용
- 모든 측정은 `Release` 빌드 + `-O3 -march=native`로 수행

## 참조

- [`performance/BENCHMARKS_METHODOLOGY.md`](performance/BENCHMARKS_METHODOLOGY.md) — 측정 방법
- [`PRODUCTION_QUALITY.kr.md`](PRODUCTION_QUALITY.kr.md) — SLA 목표
- [`FEATURES.kr.md`](FEATURES.kr.md) — 기능 성능 특성
- [`ARCHITECTURE.kr.md`](ARCHITECTURE.kr.md) — 아키텍처가 이러한 목표를 어떻게 지원하는지
