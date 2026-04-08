# Benchmarks Methodology

This document describes how benchmarks are designed, executed, and reported
for `common_game_server`. For the latest measured numbers, see
[`../BENCHMARKS.md`](../BENCHMARKS.md).

## Tooling

- **Framework**: Google Benchmark 1.9.1
- **Build configuration**: Release with `-O3 -march=native`
- **Coverage**: Disabled during benchmark runs (instrumentation skews timings)
- **Sanitizers**: Disabled during benchmark runs (overhead invalidates results)

## Hardware Reference

Benchmarks reported in CI run on the following hardware:

| Environment | Spec |
|-------------|------|
| GitHub Actions Linux | ubuntu-24.04, 4 vCPU (Intel Xeon), 16 GB RAM |
| GitHub Actions macOS | macos-14, Apple M1, 8 GB RAM |

For local runs, results vary with CPU model, frequency scaling, thermal
throttling, and background load. Disable CPU frequency scaling for
reproducible local results:

```bash
# Linux: pin CPU to performance governor
sudo cpupower frequency-set -g performance

# macOS: ensure no thermal throttling — let the laptop cool down
```

## Reproducibility Practices

To make benchmarks reproducible:

1. **Fixed random seeds** — Any benchmark using random data sets a fixed seed
2. **Warm-up iterations** — Google Benchmark auto-detects warm-up; we accept its defaults
3. **Single-threaded by default** — Multi-threaded benchmarks are explicitly opt-in via `--benchmark_threads=N`
4. **Iteration count** — Google Benchmark auto-tunes iterations to reach statistical stability
5. **Background processes** — Benchmarks should be the only meaningful CPU consumer
6. **Pinned dependencies** — Benchmark dependencies are pinned in `conanfile.py`

## Benchmark Categories

### Microbenchmarks

Measure a single operation in isolation. Examples:
- `Result_Construct`
- `Logger_Format`
- `EntityManager_Create`

These should run in microseconds and use Google Benchmark's auto-iteration.

### Subsystem benchmarks

Measure a coordinated subsystem under realistic load. Examples:
- `ECS_ParallelTick` (10K entities, 5 systems)
- `Network_TcpEcho` (100 connections, 1KB messages)
- `Database_PreparedQuery` (1000 queries against in-memory PostgreSQL)

These run in milliseconds and may use fixed iteration counts.

### End-to-end benchmarks

Measure full request paths through multiple subsystems. Examples:
- `Auth_FullLogin` (HTTP → JWT issue → DB write → response)
- `Game_FullTick` (input → world tick → output broadcast)

These run in tens of milliseconds and use small iteration counts (10-100).

## Reading Benchmark Output

Google Benchmark output looks like:

```
Running ./cgs_benchmarks
Run on (4 X 2400 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x4)
  L1 Instruction 32 KiB (x4)
  L2 Unified 256 KiB (x4)
  L3 Unified 6144 KiB (x1)
Load Average: 0.42, 0.30, 0.25
-----------------------------------------------------
Benchmark                Time             CPU   Iterations
-----------------------------------------------------
ECS_CreateDestroy      125 ns          124 ns      5645284
ECS_Query2Components  4.85 ms         4.83 ms          145
```

Key fields:
- **Time** — wall-clock time per iteration (real time, including I/O wait)
- **CPU** — CPU time per iteration (excludes I/O wait)
- **Iterations** — how many times the benchmark ran (auto-tuned)

## Comparing Runs

To compare two benchmark runs:

```bash
# Run baseline
./build/Release/bin/cgs_benchmarks --benchmark_format=json > baseline.json

# Apply changes, then run again
./build/Release/bin/cgs_benchmarks --benchmark_format=json > experiment.json

# Compare with Google Benchmark's compare.py
python3 third_party/benchmark/tools/compare.py benchmarks baseline.json experiment.json
```

CI does this automatically and fails the build on >10% regression.

## Statistical Notes

- Google Benchmark reports the **median** by default. The mean is sensitive to outliers.
- Variance below 5% is considered stable; above 5%, look for noise sources.
- Don't compare benchmark numbers across different hardware.
- When in doubt, run the benchmark 3 times and use the lowest run.

## Anti-Patterns to Avoid

### Premature micro-optimization

If a function takes 10 ns and is called 1000 times per tick, it costs 10 µs.
Speeding it up by 50% saves 5 µs per tick — irrelevant if the entire tick
budget is 50 ms (50000 µs).

**Rule**: Optimize the slowest 5% of operations first.

### Benchmarking the compiler

Some benchmarks are dominated by inlining decisions. Use `benchmark::DoNotOptimize`
and `benchmark::ClobberMemory` to prevent the compiler from eliding work.

### Cold caches

Many benchmarks run with hot caches and don't reflect real production
behavior. For cache-sensitive code, use the `--benchmark_min_time` flag to
force longer runs.

## See Also

- [`../BENCHMARKS.md`](../BENCHMARKS.md) — Latest measured numbers
- [`../PRODUCTION_QUALITY.md`](../PRODUCTION_QUALITY.md) — SLO targets
- Google Benchmark documentation: https://github.com/google/benchmark
- "What every programmer should know about memory" — Ulrich Drepper
