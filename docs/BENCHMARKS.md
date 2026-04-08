# Benchmarks

> **English** · [한국어](BENCHMARKS.kr.md)

This document tracks performance characteristics of `common_game_server`. For
detailed methodology, see [`performance/BENCHMARKS_METHODOLOGY.md`](performance/BENCHMARKS_METHODOLOGY.md).

## Performance Targets vs. Measured

| Metric | Target | Measured | Status |
|--------|--------|----------|--------|
| Concurrent users (CCU) | 10,000+ per cluster | TBD | Validated by load test |
| Message throughput | 300,000+ msg/sec | TBD | Validated by benchmark |
| World tick rate | 20 Hz (50 ms budget) | Holds at <10K entities | Continuous |
| Entity processing | 10,000 entities <5 ms | TBD | Benchmark |
| Database p99 latency | <50 ms | TBD | Load test |
| Plugin load time | <100 ms | TBD | Benchmark |

> **Note**: TBD = to be filled in by the next CI benchmark run. Numbers will
> be updated automatically by the `benchmarks.yml` workflow.

## Benchmark Suite

The benchmark suite lives under [`tests/benchmark/`](../tests/benchmark/) and
uses Google Benchmark 1.9.1.

### ECS Benchmarks

| Benchmark | What it measures |
|-----------|------------------|
| `ECS_CreateDestroy` | Entity create/destroy throughput |
| `ECS_AddRemoveComponent` | Component add/remove on existing entity |
| `ECS_Query2Components` | Iteration over 2-component query |
| `ECS_Query5Components` | Iteration over 5-component query |
| `ECS_ParallelTick` | Parallel system execution at 10K entities |

### Foundation Benchmarks

| Benchmark | What it measures |
|-----------|------------------|
| `Logger_StructuredJson` | JSON log encoding throughput |
| `Network_TcpEcho` | TCP echo server throughput |
| `Database_PreparedQuery` | Prepared statement execution latency |
| `ThreadPool_TaskSubmit` | Task submission overhead |

### Service Benchmarks

| Benchmark | What it measures |
|-----------|------------------|
| `Auth_JwtIssue` | JWT issuance per second |
| `Auth_JwtVerify` | JWT verification per second |
| `Gateway_Routing` | Request routing overhead |
| `Lobby_EloMatch` | ELO matchmaking latency |

## Running Benchmarks Locally

```bash
# Configure with benchmarks enabled
cmake --preset conan-release -DCGS_BUILD_BENCHMARKS=ON
cmake --build --preset conan-release

# Run all benchmarks
./build/Release/bin/cgs_benchmarks

# Run a specific benchmark
./build/Release/bin/cgs_benchmarks --benchmark_filter=ECS_

# Output as JSON
./build/Release/bin/cgs_benchmarks --benchmark_format=json > results.json
```

## Hardware Reference

Reported numbers (when filled in) are measured on the following CI hardware:

| Environment | Spec |
|-------------|------|
| GitHub Actions Linux | ubuntu-24.04, 4 vCPU, 16 GB RAM |
| GitHub Actions macOS | macos-14, M1, 8 GB RAM |

For production hardware sizing, see [`PRODUCTION_QUALITY.md`](PRODUCTION_QUALITY.md).

## Continuous Tracking

Benchmark results are tracked over time via the `benchmarks.yml` workflow.
Regressions exceeding 10% on any benchmark trigger a CI failure.

To trigger a benchmark run manually:

```bash
gh workflow run benchmarks.yml -R kcenon/common_game_server
```

## Reproducibility Notes

- All benchmarks use a fixed random seed where applicable
- Entity counts are exactly 10,000 unless otherwise noted
- Tick rate measurements are averaged over 1,000 ticks
- Latency percentiles use 1,000,000 samples
- All measurements are taken in `Release` build with `-O3 -march=native`

## See Also

- [`performance/BENCHMARKS_METHODOLOGY.md`](performance/BENCHMARKS_METHODOLOGY.md) — How measurements are taken
- [`PRODUCTION_QUALITY.md`](PRODUCTION_QUALITY.md) — SLA targets
- [`FEATURES.md`](FEATURES.md) — Feature performance characteristics
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — Why the architecture supports these targets
