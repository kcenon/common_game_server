#!/usr/bin/env bash
# generate_report.sh — Generate a markdown performance report from load test results.
#
# Reads results from tests/load/results/ and produces a markdown report
# with scaling charts (ASCII) and SRS requirement validation tables.
#
# Usage:
#   ./tests/load/generate_report.sh
#   ./tests/load/generate_report.sh --output /path/to/report.md

set -euo pipefail

RESULTS_DIR="${RESULTS_DIR:-tests/load/results}"
OUTPUT="${1:---output}"

if [[ "$OUTPUT" == "--output" ]]; then
    shift 2>/dev/null || true
    OUTPUT="${1:-$RESULTS_DIR/performance_report.md}"
fi

# ---------------------------------------------------------------------------
# Generate report
# ---------------------------------------------------------------------------

cat > "$OUTPUT" << 'HEADER'
# CGS Performance & Scalability Report

## Overview

This report summarizes the load testing and CCU (Concurrent Connected Users)
validation results for the Common Game Server (CGS) platform.

### SRS Requirements Under Test

| SRS ID | Requirement | Target | Status |
|--------|-------------|--------|--------|
| SRS-NFR-001 | Message throughput | >= 300,000 msg/sec | See [Benchmark CI] |
| SRS-NFR-002 | World tick latency | <= 50ms (20 Hz) | See [CCU Benchmark] |
| SRS-NFR-003 | Entity update (10K) | <= 5ms | See [Benchmark CI] |
| SRS-NFR-004 | Database query p99 | <= 50ms | See [Benchmark CI] |
| SRS-NFR-005 | Memory per 1K players | <= 100MB | See [CCU Benchmark] |
| SRS-NFR-006 | Plugin load time | <= 100ms | See [Benchmark CI] |
| SRS-NFR-007 | Horizontal scaling | Linear to 100 nodes | See [Scaling Test] |
| SRS-NFR-008 | CCU per node | >= 1,000 | See [CCU Benchmark] |
| SRS-NFR-009 | Service discovery | Auto via K8s | Validated in K8s deploy |
| SRS-NFR-010 | Total CCU per cluster | >= 10,000 | See [Scaling Test] |

HEADER

# Add CCU benchmark results if available
if compgen -G "$RESULTS_DIR/*ccu*" > /dev/null 2>&1; then
    cat >> "$OUTPUT" << 'SECTION'
## CCU Benchmark Results (Single Node)

The C++ CCU benchmark directly exercises GameServer with 1,000 simulated
players, measuring join throughput, tick latency under load, and leave
throughput.

### Test Matrix

| Test | Players | Instances | Metric | Threshold |
|------|---------|-----------|--------|-----------|
| JoinThroughput | 1,000 | 10 | Join time < 5s | SRS-NFR-008 |
| TickLatency | 1,000 | 10 | P95 tick < 50ms | SRS-NFR-002 |
| LeaveThroughput | 1,000 | 10 | All removed cleanly | - |
| ScalingLinearity | 100-1000 | 1-10 | < 1.5x linear | SRS-NFR-007 |

SECTION
fi

# Add scaling results if CSV exists
if [[ -f "$RESULTS_DIR/scaling_report.csv" ]]; then
    cat >> "$OUTPUT" << 'SECTION'
## Kubernetes Scaling Results

### Scaling Steps

SECTION

    # Convert CSV to markdown table
    echo "| Timestamp | Replicas | VUs Max | HTTP Requests | P95 Latency (ms) | Success Rate |" >> "$OUTPUT"
    echo "|-----------|----------|---------|---------------|-------------------|--------------|" >> "$OUTPUT"
    tail -n +2 "$RESULTS_DIR/scaling_report.csv" | while IFS=',' read -r ts step gw_rep game_rep scenario vus reqs p95 success; do
        echo "| $ts | $step | $vus | $reqs | $p95 | $success |" >> "$OUTPUT"
    done

    cat >> "$OUTPUT" << 'SECTION'

### Scaling Analysis

The table above shows how service performance changes as the number of
replicas increases from 1 to 10. Key observations:

- **Linear scaling**: Throughput should increase roughly proportionally
  with replica count.
- **Latency stability**: P95 latency should remain under threshold
  regardless of scale.
- **Success rate**: Should remain above 95% at all scale levels.

SECTION
fi

# Add k6 results summaries
for summary_file in "$RESULTS_DIR"/*_summary.json; do
    [[ -f "$summary_file" ]] || continue
    service_name=$(basename "$summary_file" | sed 's/_summary\.json//')

    echo "## k6 Results: $service_name" >> "$OUTPUT"
    echo "" >> "$OUTPUT"
    echo '```json' >> "$OUTPUT"
    cat "$summary_file" >> "$OUTPUT"
    echo "" >> "$OUTPUT"
    echo '```' >> "$OUTPUT"
    echo "" >> "$OUTPUT"
done

# Footer
cat >> "$OUTPUT" << FOOTER

## How to Run

### C++ CCU Benchmark (local)

\`\`\`bash
cmake --build --preset conan-release
ctest --preset conan-release -R "GameCCU" --output-on-failure --verbose
\`\`\`

### k6 Load Test (requires running services)

\`\`\`bash
# Start services
cd deploy && docker compose up -d

# Run smoke test
k6 run tests/load/auth_load.js

# Run 1K CCU test
k6 run -e SCENARIO=ccu_1k tests/load/gateway_load.js
\`\`\`

### Kubernetes Scaling Validation

\`\`\`bash
# Deploy to K8s cluster
kubectl apply -k deploy/k8s/base/

# Run scaling test
./tests/load/scaling_validation.sh

# Smoke test only
./tests/load/scaling_validation.sh --smoke
\`\`\`

---

*Generated: $(date -u '+%Y-%m-%d %H:%M:%S UTC')*
*Commit: $(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')*
FOOTER

echo "Report generated: $OUTPUT"
