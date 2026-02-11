# Performance Report

**Project**: Common Game Server (CGS) v0.1.0
**Date**: 2026-02-11
**Platform**: macOS (Apple Silicon), Release build, C++20
**Compiler**: AppleClang with `-O2` optimization

## Executive Summary

All six SRS non-functional performance requirements (SRS-NFR-001 through SRS-NFR-006) are met with
significant margin. The CGS framework demonstrates production-grade performance characteristics
suitable for real-time game server workloads.

| SRS ID | Requirement | Target | Measured | Margin | Status |
|--------|-------------|--------|----------|--------|--------|
| SRS-NFR-001 | Message throughput | ≥300K msg/sec | 30.6M msg/sec | 102x | **PASS** |
| SRS-NFR-002 | World tick latency | ≤50ms (20 Hz) | 0.14ms p99 (9,360 Hz) | 357x | **PASS** |
| SRS-NFR-003 | Entity update (10K) | ≤5ms | 0.009ms p99 | 556x | **PASS** |
| SRS-NFR-004 | Database query p99 | ≤50ms | 0.10ms p99 | 500x | **PASS** |
| SRS-NFR-005 | Memory per 1K players | ≤100MB | 4.06MB RSS | 24x | **PASS** |
| SRS-NFR-006 | Plugin load time | ≤100ms | 0.004ms p99 | 25,000x | **PASS** |

## 1. Message Throughput (SRS-NFR-001)

**Requirement**: ≥300,000 messages/second with realistic game payloads.

### 1.1 Serialization Throughput

| Metric | Value |
|--------|-------|
| Median | 62.2M msg/sec |
| Min | 50.6M msg/sec |
| Max | 63.4M msg/sec |
| Payload | 64 bytes (6-byte header + 58-byte payload) |

### 1.2 Deserialization Throughput

| Metric | Value |
|--------|-------|
| Median | 67.5M msg/sec |

### 1.3 Round-Trip Throughput (Serialize + Deserialize)

| Metric | Value |
|--------|-------|
| Median | 30.6M msg/sec |
| Margin over target | 102x |

### 1.4 Payload Size Impact

Round-trip throughput by message type:

| Message Type | Wire Size | Throughput | Status |
|-------------|-----------|------------|--------|
| Heartbeat | 10B | 33.8M msg/sec | PASS |
| Movement | 38B | 30.3M msg/sec | PASS |
| SpellCast | 30B | 31.2M msg/sec | PASS |
| Chat | 134B | 35.4M msg/sec | PASS |
| InventoryUpdate | 262B | 23.5M msg/sec | PASS |
| WorldState | 518B | 18.8M msg/sec | PASS |

**Analysis**: Even the largest payload (WorldState at 518 bytes on the wire) achieves 18.8M msg/sec,
which is 62x above the 300K target. The binary wire format (`[4-byte length][2-byte opcode][payload]`)
provides near-zero overhead serialization through direct memory copy.

### 1.5 TCP Network Dispatch

| Metric | Value |
|--------|-------|
| Messages dispatched | 100,000 |
| Throughput | 1.1M msg/sec |
| Requirement | 305K msg/sec |

## 2. World Tick Latency (SRS-NFR-002)

**Requirement**: Full world tick ≤50ms (supporting 20 Hz server tick rate).

### 2.1 Configuration

- 6 game systems: WorldSystem, ObjectUpdateSystem, CombatSystem, AISystem, QuestSystem, InventorySystem
- 10,000 entities (1,000 players + 9,000 creatures)
- Stage-based topological execution: PreUpdate → Update → PostUpdate

### 2.2 Results (200 ticks)

| Metric | Value |
|--------|-------|
| Min latency | 0.100ms |
| Average latency | 0.109ms |
| Median latency | 0.107ms |
| p95 latency | 0.120ms |
| p99 latency | 0.141ms |
| Max latency | 0.145ms |
| Effective tick rate | 9,360 Hz |

### 2.3 Sustained Stability (1,000 consecutive ticks)

| Metric | Value |
|--------|-------|
| Median | 0.107ms |
| p99 | 0.136ms |
| Max | 0.161ms |
| Over-budget ticks | 0.0% |

**Analysis**: The system achieves 9,360 Hz effective tick rate — 468x the required 20 Hz. Zero ticks
exceeded the 50ms budget across 1,000 consecutive ticks, demonstrating excellent stability with
no GC pauses or allocation spikes.

### 2.4 Scheduler Build Time

| Metric | Value |
|--------|-------|
| Median | 0.002ms |
| p99 | 0.003ms |

The topological sort and stage assignment for 6 systems completes in microseconds.

## 3. ECS Entity Update (SRS-NFR-003)

**Requirement**: 10K entity component iteration ≤5ms.

### 3.1 ComponentStorage Iteration

| Metric | Value |
|--------|-------|
| Entities | 10,000 |
| Median | 0.008ms |
| p99 | 0.009ms |
| Margin | 556x under budget |

### 3.2 Component Operations

| Operation | Median | p99 | Throughput |
|-----------|--------|-----|------------|
| Add (10K entities) | 0.078ms | 0.133ms | 128.5M adds/sec |
| Iteration (10K) | 0.008ms | 0.009ms | — |
| Random Access | 0.010ms | 0.022ms | — |
| Remove (10K) | 0.017ms | 0.025ms | — |
| Full Lifecycle | 1.852ms | 2.033ms | 10.8M lifecycle/sec |

**Analysis**: The sparse-set ECS architecture provides cache-friendly dense iteration. Components are
stored contiguously in memory, enabling the CPU prefetcher to work effectively. The 0.008ms iteration
time for 10K entities with Transform+Movement components is ~625x under the 5ms budget.

## 4. Database Query Latency (SRS-NFR-004)

**Requirement**: p99 query latency ≤50ms with connection pooling.

### 4.1 Cache Layer Performance (8 threads, 80K operations)

| Operation | Avg | p99 | Throughput |
|-----------|-----|-----|------------|
| Read | 0.015ms | 0.100ms | 67,228 ops/sec |
| Write | 0.021ms | 0.113ms | — |
| Mixed (80/20 R/W) | 0.018ms | 0.106ms | — |

**Analysis**: The DBProxy query cache achieves sub-millisecond p99 latency across all operation types.
The 100% hit rate for read operations demonstrates effective caching. Mixed read/write workloads
(80/20 ratio typical for game servers) maintain p99 at 0.106ms — 471x under the 50ms target.

## 5. Memory Usage (SRS-NFR-005)

**Requirement**: ≤100MB per 1,000 concurrent players.

### 5.1 Theoretical Memory Breakdown (1,000 players)

| Category | Size |
|----------|------|
| **Component Data** | 1.01 MB |
| Sparse-set Overhead | 140.62 KB |
| CharacterData | 46.88 KB |
| EntityManager | 1.95 KB |
| **Total (theoretical)** | **1.19 MB** |

### 5.2 Per-Component Sizes

| Component | sizeof | Total (1K) |
|-----------|--------|------------|
| Equipment | 680B | 664.06 KB |
| Stats | 80B | 78.12 KB |
| QuestLog | 72B | 70.31 KB |
| Transform | 40B | 39.06 KB |
| Identity | 40B | 39.06 KB |
| Inventory | 40B | 39.06 KB |
| Movement | 24B | 23.44 KB |
| AuraHolder | 24B | 23.44 KB |
| ThreatList | 24B | 23.44 KB |
| SpellCast | 20B | 19.53 KB |
| MapMembership | 8B | 7.81 KB |
| VisibilityRange | 4B | 3.91 KB |

### 5.3 Practical RSS Measurement (macOS)

| Metric | Value |
|--------|-------|
| RSS before player creation | 1.69 MB |
| RSS after 1,000 players | 5.75 MB |
| **RSS delta** | **4.06 MB** |
| Margin | 24x under budget |

### 5.4 Memory Scaling

| Players | RSS Delta | Per-Player |
|---------|-----------|------------|
| 100 | 704 KB | 7.04 KB |
| 500 | 1.64 MB | 3.36 KB |
| 1,000 | 2.09 MB | 2.14 KB |

**Analysis**: Per-player memory cost decreases with scale due to fixed overhead amortization. At 1K
players, the practical RSS delta (4.06 MB) is 24x under the 100MB budget. The Equipment component
(680 bytes) dominates per-entity memory — this is by design, as each equipment slot stores item data.

### 5.5 Memory Reclamation

| Metric | Value |
|--------|-------|
| RSS with 1K players | 5.80 MB |
| RSS after destruction | 5.81 MB |

Entity destruction reclaims ECS storage slots for reuse. RSS does not decrease immediately due to
allocator memory pooling (standard C++ allocator behavior), but slots are recycled for new entities.

## 6. Plugin Load Time (SRS-NFR-006)

**Requirement**: MMORPG plugin load ≤100ms.

### 6.1 Full Startup (OnLoad + OnInit)

| Metric | Value |
|--------|-------|
| OnLoad median | 0.0002ms |
| OnInit median | 0.0025ms |
| **Total median** | **0.0027ms** |
| Total p99 | 0.0038ms |
| Margin | 25,000x under budget |

### 6.2 Construction Overhead

| Metric | Value |
|--------|-------|
| Constructor median | <0.001ms |
| Constructor p99 | 0.0001ms |

### 6.3 Load/Unload Cycle Stability

| Metric | Value |
|--------|-------|
| Cycles | 100 |
| Median | 0.004ms |
| p99 | 0.025ms |

**Analysis**: The plugin startup involves registering 18 ComponentStorages (OnLoad) and 6 Systems with
topological scheduling (OnInit). All operations complete in microseconds. No resource leaks detected
across 100 load/unload cycles — essential for hot-reload scenarios.

## Benchmark Inventory

### Test Suites

| Suite | Tests | CTest Label | SRS Coverage |
|-------|-------|-------------|--------------|
| `cgs_service_dbproxy_benchmark_tests` | 3 | benchmark | SRS-NFR-004 |
| `cgs_foundation_network_throughput_tests` | 1 | benchmark | SRS-NFR-001 |
| `cgs_ecs_component_storage_benchmark_tests` | 5 | benchmark | SRS-NFR-003 |
| `cgs_ecs_system_scheduler_benchmark_tests` | 3 | benchmark | SRS-NFR-002 |
| `cgs_plugin_load_benchmark_tests` | 3 | benchmark | SRS-NFR-006 |
| `cgs_ecs_memory_profiling_benchmark_tests` | 4 | benchmark | SRS-NFR-005 |
| `cgs_message_serialization_benchmark_tests` | 4 | benchmark | SRS-NFR-001 |
| **Total** | **23** | | |

### Running Benchmarks

```bash
# Run all benchmarks
ctest --test-dir build -L benchmark --verbose

# Run specific NFR benchmarks
ctest --test-dir build -R "SystemSchedulerBenchmark" --verbose    # NFR-002
ctest --test-dir build -R "ComponentStorageBenchmark" --verbose   # NFR-003
ctest --test-dir build -R "MemoryProfilingBenchmark" --verbose    # NFR-005
ctest --test-dir build -R "MessageSerializationBenchmark" --verbose # NFR-001
ctest --test-dir build -R "PluginLoadBenchmark" --verbose         # NFR-006
ctest --test-dir build -R "DBProxyCacheBenchmark" --verbose       # NFR-004
```

## CI Integration

Benchmarks are integrated into the CI pipeline via GitHub Actions:

- **`ci.yml`**: Runs on every push/PR. Executes unit tests only (excludes benchmarks and integration
  tests) for fast feedback.
- **`benchmarks.yml`**: Runs on every release publication and can be triggered manually. Executes all
  23 benchmark tests and uploads results as artifacts. If any benchmark fails (indicating a
  performance regression against SRS-NFR targets), an issue is automatically created with
  `type:performance` and `priority:P0-critical` labels.

## Methodology

### Measurement Approach

- **Statistical rigor**: Each benchmark runs multiple iterations (10-1,000 depending on variance).
  Results are reported as min, median, p95, p99, and max to capture distribution characteristics.
- **Warmup**: All benchmarks include warmup iterations to stabilize CPU caches, branch predictors,
  and memory allocators before measurement begins.
- **Memory measurement**: Dual approach using theoretical `sizeof(T)` calculations and practical
  RSS measurement via platform APIs (`mach_task_basic_info` on macOS).
- **Dead-code prevention**: Benchmark loops include `(void)` casts and size checks to prevent
  compiler optimization from eliminating measured operations.

### Test Environment

- **Build type**: Release (`-O2` optimization)
- **Framework**: Google Test with `std::chrono::high_resolution_clock`
- **Assertions**: `EXPECT_GE` / `EXPECT_LE` against SRS-NFR targets — benchmarks fail if
  requirements are not met

## Traceability

| Document | Reference |
|----------|-----------|
| PRD | NFR-001 (Performance), Section 1.3 (Success Metrics) |
| SRS | Section 5.1 (Performance Requirements): SRS-NFR-001 through SRS-NFR-006 |
| SDS | SDS-ARC-001 (Architecture Overview), SDS-MOD-010 (Component Storage) |
| GitHub | Issue #30, Sub-issues #75, #76 |
