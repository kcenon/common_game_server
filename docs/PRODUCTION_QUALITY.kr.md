# 프로덕션 품질

> [English](PRODUCTION_QUALITY.md) · **한국어**

본 문서는 `common_game_server`의 프로덕션 준비 체크리스트를 추적합니다.

## 품질 게이트

| 게이트 | 목표 | 상태 |
|--------|------|------|
| 빌드 매트릭스 (Linux GCC, Linux Clang, macOS) | 모두 통과 | Passing |
| 단위 테스트 통과율 | 100% | Passing |
| 통합 테스트 통과율 | 100% | Passing |
| 코드 커버리지 | ≥ 40% (project), ≥ 60% (patch) | [`BENCHMARKS.kr.md`](BENCHMARKS.kr.md) 참조 |
| clang-format 준수 | 0 위반 | CI 강제 |
| clang-tidy 준수 | 0 오류 | CI 강제 |
| AddressSanitizer | 0 leak, 0 error | 야간 검증 |
| ThreadSanitizer | 0 race | 야간 검증 |
| UndefinedBehaviorSanitizer | 0 UB | 야간 검증 |
| Doxygen 경고 수 | 0 | 강제 (`WARN_NO_PARAMDOC = YES`) |
| CVE 스캔 (의존성) | 0 critical, 0 high | 수동 게이트 |
| SBOM 생성 | 완료 | [`SOUP.md`](SOUP.md) 참조 |

## 서비스 수준 목표 (SLO)

| 서비스 | 지표 | 목표 |
|--------|------|------|
| AuthServer | p99 지연 | < 50 ms |
| AuthServer | 가용성 | 99.9% |
| AuthServer | JWT 발급 처리량 | 5,000 / sec |
| GatewayServer | p99 지연 | < 20 ms |
| GatewayServer | 가용성 | 99.95% |
| GatewayServer | 동시 연결 | 10,000 |
| GameServer | 월드 틱 예산 | < 50 ms (20 Hz) |
| GameServer | 틱 미스율 | < 0.1% |
| GameServer | 엔티티 처리 | 10K 엔티티 < 5 ms |
| LobbyServer | 매치 생성 지연 | < 5 sec |
| LobbyServer | ELO 계산 | < 1 ms |
| DBProxy | p99 쿼리 지연 | < 50 ms |
| DBProxy | 연결 풀 고갈 | < 0.01% |

## 신뢰성 패턴

| 패턴 | 적용 위치 | 검증 방법 |
|------|---------|----------|
| 회로 차단기 | 모든 서비스 간 호출 | 카오스 테스트 |
| 지수 백오프 재시도 | 데이터베이스, 네트워크 호출 | 카오스 테스트 |
| 우아한 종료 | 모든 서비스 (SIGTERM 처리) | 통합 테스트 |
| Write-Ahead Log + 스냅샷 | 게임 상태 영속화 | 통합 테스트 |
| 연결 풀링 | DBProxy | 부하 테스트 |
| 속도 제한 | Gateway (token bucket) | 부하 테스트 |
| Bulkhead 격리 | 서비스별 스레드 풀 | 수동 리뷰 |
| 타임아웃 강제 | 모든 블로킹 I/O | 코드 리뷰 |

## 보안 자세

| 통제 | 구현 |
|------|------|
| 전송 암호화 | TLS 1.3 (gateway 종단) |
| 인증 | JWT RS256 (비대칭) |
| 인가 | 서비스 간 토큰 |
| 입력 검증 | 모든 경계에서 |
| SQL 인젝션 방지 | 준비된 명령문 (문자열 연결 금지) |
| 시크릿 관리 | Kubernetes Secrets (컨테이너에 env var 사용 안 함) |
| 속도 제한 | Gateway의 token bucket |
| 감사 로깅 | 인증 이벤트를 전용 로그로 |

취약점 보고: [`../SECURITY.md`](../SECURITY.md)

## 운영 준비도

| 기능 | 상태 |
|------|------|
| Health probe 엔드포인트 | 모든 서비스가 `/health`, `/ready` 노출 |
| Prometheus 메트릭 | 모든 서비스가 포트 9090에서 `/metrics` 노출 |
| 구조화된 로깅 | JSON 출력, 상관 ID |
| 분산 트레이싱 | OpenTelemetry 훅 (Beta) |
| Grafana 대시보드 | [`deploy/monitoring/`](../deploy/monitoring/)에 제공 |
| 알림 규칙 | [`deploy/monitoring/alerts/`](../deploy/monitoring/)에 제공 |
| 런북 | [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md) |
| 재해 복구 | 배포 가이드에 문서화 |

## 테스트 커버리지

| 테스트 유형 | 위치 | 주기 |
|------------|------|------|
| 단위 | `tests/unit/` | 매 커밋 |
| 통합 | `tests/integration/` | 매 커밋 |
| 벤치마크 | `tests/benchmark/` | 수동 / 예약 |
| 부하 | `tests/load/` | 수동 |
| 카오스 / 장애 주입 | `tests/chaos/` | 수동 |

상세 전략: [`guides/TESTING_GUIDE.md`](guides/TESTING_GUIDE.md)

## 하드웨어 사이징 (참조)

10,000 CCU 프로덕션 배포의 경우:

| 서비스 | 복제본 | 복제본당 CPU | 복제본당 메모리 |
|--------|--------|------------|--------------|
| GatewayServer | 4 | 2 cores | 4 GB |
| AuthServer | 2 | 1 core | 2 GB |
| GameServer | 4 | 4 cores | 8 GB |
| LobbyServer | 2 | 1 core | 2 GB |
| DBProxy | 2 | 2 cores | 4 GB |
| PostgreSQL | 1 (primary) + 1 (replica) | 8 cores | 32 GB |

총: ~36 cores, ~120 GB RAM (모니터링 스택 제외).

## 알려진 제한 사항

- Pre-1.0 (v0.1.0) — API는 minor 버전 사이에 변경될 수 있음
- 플러그인 핫 리로드는 **개발 전용** — 프로덕션 빌드는 비활성화
- 단일 데이터베이스 백엔드 (PostgreSQL) — 다중 백엔드 지원은 1.0 이후 계획
- Windows는 best-effort — 아직 CI 없음
- 10K CCU 검증됨; 더 높은 CCU는 프로파일링 필요

전체 제약 매트릭스는 [`COMPATIBILITY.md`](COMPATIBILITY.md) 참조.

## 참조

- [`BENCHMARKS.kr.md`](BENCHMARKS.kr.md) — 측정된 성능
- [`COMPATIBILITY.md`](COMPATIBILITY.md) — 플랫폼 지원 매트릭스
- [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md) — 프로덕션 배포
- [`SOUP.md`](SOUP.md) — 서드파티 BOM
