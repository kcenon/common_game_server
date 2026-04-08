# Production Quality

> **English** · [한국어](PRODUCTION_QUALITY.kr.md)

This document tracks the production-readiness checklist for `common_game_server`.

## Quality Gates

| Gate | Target | Status |
|------|--------|--------|
| Build matrix (Linux GCC, Linux Clang, macOS) | All passing | Passing |
| Unit test pass rate | 100% | Passing |
| Integration test pass rate | 100% | Passing |
| Code coverage | ≥ 40% (project), ≥ 60% (patch) | See [`BENCHMARKS.md`](BENCHMARKS.md) |
| clang-format compliance | 0 violations | Enforced by CI |
| clang-tidy compliance | 0 errors | Enforced by CI |
| AddressSanitizer | 0 leaks, 0 errors | Validated nightly |
| ThreadSanitizer | 0 races | Validated nightly |
| UndefinedBehaviorSanitizer | 0 UB | Validated nightly |
| Doxygen warning count | 0 | Enforced (`WARN_NO_PARAMDOC = YES`) |
| CVE scan (dependencies) | 0 critical, 0 high | Manual gate |
| SBOM generation | Complete | See [`SOUP.md`](SOUP.md) |

## Service-Level Objectives (SLOs)

| Service | Metric | Target |
|---------|--------|--------|
| AuthServer | p99 latency | < 50 ms |
| AuthServer | Availability | 99.9% |
| AuthServer | JWT issuance throughput | 5,000 / sec |
| GatewayServer | p99 latency | < 20 ms |
| GatewayServer | Availability | 99.95% |
| GatewayServer | Concurrent connections | 10,000 |
| GameServer | World tick budget | < 50 ms (20 Hz) |
| GameServer | Tick miss rate | < 0.1% |
| GameServer | Entity processing | 10K entities < 5 ms |
| LobbyServer | Match creation latency | < 5 sec |
| LobbyServer | ELO calculation | < 1 ms |
| DBProxy | p99 query latency | < 50 ms |
| DBProxy | Connection pool exhaustion | < 0.01% |

## Reliability Patterns

| Pattern | Where Applied | Verified by |
|---------|--------------|-------------|
| Circuit breaker | All inter-service calls | Chaos test |
| Retry with exponential backoff | Database, network calls | Chaos test |
| Graceful shutdown | All services (SIGTERM handling) | Integration test |
| Write-Ahead Log + snapshots | Game state persistence | Integration test |
| Connection pooling | DBProxy | Load test |
| Rate limiting | Gateway (token bucket) | Load test |
| Bulkhead isolation | Per-service thread pools | Manual review |
| Timeout enforcement | All blocking I/O | Code review |

## Security Posture

| Control | Implementation |
|---------|---------------|
| Transport encryption | TLS 1.3 (gateway termination) |
| Authentication | JWT RS256 (asymmetric) |
| Authorization | Service-to-service tokens |
| Input validation | At every boundary |
| SQL injection prevention | Prepared statements (no string concat) |
| Secrets management | Kubernetes Secrets (no env vars in containers) |
| Rate limiting | Token bucket at gateway |
| Audit logging | Authentication events to dedicated log |

Vulnerability reporting: [`../SECURITY.md`](../SECURITY.md)

## Operational Readiness

| Capability | Status |
|------------|--------|
| Health probe endpoints | All services expose `/health`, `/ready` |
| Prometheus metrics | All services expose `/metrics` on port 9090 |
| Structured logging | JSON output, correlation IDs |
| Distributed tracing | OpenTelemetry hooks (Beta) |
| Grafana dashboards | Provided in [`deploy/monitoring/`](../deploy/monitoring/) |
| Alert rules | Provided in [`deploy/monitoring/alerts/`](../deploy/monitoring/) |
| Runbooks | [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md) |
| Disaster recovery | Documented in deployment guide |

## Testing Coverage

| Test Type | Where | Frequency |
|-----------|-------|-----------|
| Unit | `tests/unit/` | Every commit |
| Integration | `tests/integration/` | Every commit |
| Benchmark | `tests/benchmark/` | Manual / scheduled |
| Load | `tests/load/` | Manual |
| Chaos / Fault injection | `tests/chaos/` | Manual |

Detailed strategy: [`guides/TESTING_GUIDE.md`](guides/TESTING_GUIDE.md)

## Hardware Sizing (Reference)

For 10,000 CCU production deployment:

| Service | Replicas | CPU per replica | Memory per replica |
|---------|----------|----------------|--------------------|
| GatewayServer | 4 | 2 cores | 4 GB |
| AuthServer | 2 | 1 core | 2 GB |
| GameServer | 4 | 4 cores | 8 GB |
| LobbyServer | 2 | 1 core | 2 GB |
| DBProxy | 2 | 2 cores | 4 GB |
| PostgreSQL | 1 (primary) + 1 (replica) | 8 cores | 32 GB |

Total: ~36 cores, ~120 GB RAM (excluding monitoring stack).

## Known Limitations

- Pre-1.0 (v0.1.0) — API may change between minor versions
- Plugin hot reload is **development-only** — production builds disable it
- Single database backend (PostgreSQL) — multi-backend support planned post-1.0
- Windows is best-effort — no CI yet
- 10K CCU validated; higher CCU requires profiling

See [`COMPATIBILITY.md`](COMPATIBILITY.md) for the full constraint matrix.

## See Also

- [`BENCHMARKS.md`](BENCHMARKS.md) — Measured performance
- [`COMPATIBILITY.md`](COMPATIBILITY.md) — Platform support matrix
- [`guides/DEPLOYMENT_GUIDE.md`](guides/DEPLOYMENT_GUIDE.md) — Production deployment
- [`SOUP.md`](SOUP.md) — Third-party Bill of Materials
