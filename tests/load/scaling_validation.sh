#!/usr/bin/env bash
# scaling_validation.sh — Kubernetes scaling validation for CGS.
#
# Deploys the CGS stack to a K8s cluster, scales services, runs k6 load
# tests at each scale step, and collects results for a scaling report.
#
# Prerequisites:
#   - kubectl configured with target cluster
#   - k6 installed (https://k6.io/docs/getting-started/installation/)
#   - Docker images built and pushed to registry
#
# Usage:
#   ./tests/load/scaling_validation.sh                     # Full test
#   ./tests/load/scaling_validation.sh --smoke             # Quick smoke test
#   ./tests/load/scaling_validation.sh --max-replicas 5    # Limit scale
#
# Environment variables:
#   CGS_NAMESPACE     Kubernetes namespace (default: cgs-system)
#   CGS_REGISTRY      Container registry prefix (default: cgs)
#   K6_BINARY         Path to k6 binary (default: k6)
#   RESULTS_DIR       Output directory (default: tests/load/results)

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

NAMESPACE="${CGS_NAMESPACE:-cgs-system}"
REGISTRY="${CGS_REGISTRY:-cgs}"
K6="${K6_BINARY:-k6}"
RESULTS_DIR="${RESULTS_DIR:-tests/load/results}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

SMOKE_MODE=false
MAX_REPLICAS=10

while [[ $# -gt 0 ]]; do
    case "$1" in
        --smoke)
            SMOKE_MODE=true
            MAX_REPLICAS=2
            shift
            ;;
        --max-replicas)
            MAX_REPLICAS="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

mkdir -p "$RESULTS_DIR"

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

log() {
    echo "[$(date -u '+%Y-%m-%d %H:%M:%S UTC')] $*"
}

wait_for_ready() {
    local deployment="$1"
    local timeout="${2:-120}"
    log "Waiting for $deployment to be ready (timeout: ${timeout}s)..."
    kubectl rollout status "deployment/$deployment" \
        -n "$NAMESPACE" --timeout="${timeout}s" 2>/dev/null || true
}

wait_for_statefulset_ready() {
    local sts="$1"
    local replicas="$2"
    local timeout="${3:-180}"
    log "Waiting for StatefulSet $sts ($replicas replicas)..."
    local elapsed=0
    while [[ $elapsed -lt $timeout ]]; do
        local ready
        ready=$(kubectl get sts "$sts" -n "$NAMESPACE" \
            -o jsonpath='{.status.readyReplicas}' 2>/dev/null || echo "0")
        if [[ "$ready" -ge "$replicas" ]]; then
            log "StatefulSet $sts: $ready/$replicas ready"
            return 0
        fi
        sleep 5
        elapsed=$((elapsed + 5))
    done
    log "WARNING: StatefulSet $sts timed out ($elapsed/$timeout sec)"
    return 1
}

get_gateway_url() {
    # Try NodePort first, then port-forward
    local port
    port=$(kubectl get svc gateway -n "$NAMESPACE" \
        -o jsonpath='{.spec.ports[?(@.name=="tcp")].nodePort}' 2>/dev/null || echo "")
    if [[ -n "$port" ]]; then
        echo "http://$(kubectl get nodes -o jsonpath='{.items[0].status.addresses[0].address}'):$port"
    else
        echo "http://localhost:8080"
    fi
}

# ---------------------------------------------------------------------------
# Step 1: Verify cluster connectivity
# ---------------------------------------------------------------------------

log "=== CGS Scaling Validation ==="
log "Namespace: $NAMESPACE"
log "Max replicas: $MAX_REPLICAS"
log "Smoke mode: $SMOKE_MODE"

if ! kubectl cluster-info &>/dev/null; then
    echo "ERROR: Cannot connect to Kubernetes cluster."
    echo "Configure kubectl or set KUBECONFIG."
    exit 1
fi

log "Cluster connected: $(kubectl cluster-info 2>/dev/null | head -1)"

# ---------------------------------------------------------------------------
# Step 2: Deploy base manifests
# ---------------------------------------------------------------------------

log "Deploying CGS base manifests..."
kubectl apply -k deploy/k8s/base/ || {
    log "ERROR: Failed to apply K8s manifests"
    exit 1
}

# Wait for all base deployments
for svc in auth gateway lobby dbproxy; do
    wait_for_ready "$svc"
done
wait_for_statefulset_ready "game" 2

log "All services running at base replica count."

# ---------------------------------------------------------------------------
# Step 3: Scaling steps
# ---------------------------------------------------------------------------

SCALE_STEPS=(1 2 5)
if [[ $MAX_REPLICAS -ge 10 ]]; then
    SCALE_STEPS=(1 2 5 10)
fi
if $SMOKE_MODE; then
    SCALE_STEPS=(1 2)
fi

REPORT_FILE="$RESULTS_DIR/scaling_report.csv"
echo "timestamp,step,gateway_replicas,game_replicas,scenario,vus_max,http_reqs,p95_latency_ms,success_rate" > "$REPORT_FILE"

for step in "${SCALE_STEPS[@]}"; do
    log "--- Scale step: $step replicas ---"

    # Scale stateless services
    kubectl scale deployment/gateway --replicas="$step" -n "$NAMESPACE"
    kubectl scale deployment/auth --replicas="$step" -n "$NAMESPACE"
    kubectl scale deployment/lobby --replicas="$step" -n "$NAMESPACE"

    # Scale StatefulSet (game server)
    kubectl scale statefulset/game --replicas="$step" -n "$NAMESPACE"

    # Wait for all to be ready
    for svc in auth gateway lobby; do
        wait_for_ready "$svc" 180
    done
    wait_for_statefulset_ready "game" "$step" 180

    # Brief settle period
    sleep 10

    # Run k6 load test
    local scenario="load"
    if $SMOKE_MODE; then
        scenario="smoke"
    fi

    local gateway_url
    gateway_url=$(get_gateway_url)

    log "Running k6 (scenario=$scenario) against $gateway_url..."
    local k6_output="$RESULTS_DIR/k6_step_${step}.json"

    "$K6" run \
        -e "SCENARIO=$scenario" \
        -e "GATEWAY_HTTP=$gateway_url" \
        -e "AUTH_URL=${gateway_url%:*}:9001" \
        --out "json=$k6_output" \
        "$SCRIPT_DIR/gateway_load.js" \
        2>&1 | tee "$RESULTS_DIR/k6_step_${step}.log" || true

    # Extract metrics from k6 output
    local p95_latency="N/A"
    local success_rate="N/A"
    local http_reqs="N/A"
    local vus_max="N/A"

    if [[ -f "$RESULTS_DIR/gateway_summary.json" ]]; then
        p95_latency=$(python3 -c "
import json, sys
with open('$RESULTS_DIR/gateway_summary.json') as f:
    d = json.load(f)
print(d.get('metrics', {}).get('connection_latency_p95', 'N/A'))
" 2>/dev/null || echo "N/A")

        success_rate=$(python3 -c "
import json
with open('$RESULTS_DIR/gateway_summary.json') as f:
    d = json.load(f)
print(d.get('metrics', {}).get('connection_success_rate', 'N/A'))
" 2>/dev/null || echo "N/A")

        http_reqs=$(python3 -c "
import json
with open('$RESULTS_DIR/gateway_summary.json') as f:
    d = json.load(f)
print(d.get('metrics', {}).get('http_reqs', 'N/A'))
" 2>/dev/null || echo "N/A")

        vus_max=$(python3 -c "
import json
with open('$RESULTS_DIR/gateway_summary.json') as f:
    d = json.load(f)
print(d.get('metrics', {}).get('vus_max', 'N/A'))
" 2>/dev/null || echo "N/A")
    fi

    echo "$(date -u '+%Y-%m-%dT%H:%M:%SZ'),$step,$step,$step,$scenario,$vus_max,$http_reqs,$p95_latency,$success_rate" >> "$REPORT_FILE"

    log "Step $step complete: vus=$vus_max, reqs=$http_reqs, p95=${p95_latency}ms, success=$success_rate"
done

# ---------------------------------------------------------------------------
# Step 4: Generate summary
# ---------------------------------------------------------------------------

log "=== Scaling Validation Complete ==="
log ""
log "Results:"
log "  CSV report:  $REPORT_FILE"
log "  k6 outputs:  $RESULTS_DIR/k6_step_*.json"
log "  Logs:        $RESULTS_DIR/k6_step_*.log"
log ""

# Print CSV as table
echo ""
echo "=== Scaling Results ==="
column -t -s',' "$REPORT_FILE" 2>/dev/null || cat "$REPORT_FILE"

# ---------------------------------------------------------------------------
# Step 5: Scale back to base
# ---------------------------------------------------------------------------

log "Scaling back to base replicas..."
kubectl scale deployment/gateway --replicas=2 -n "$NAMESPACE"
kubectl scale deployment/auth --replicas=2 -n "$NAMESPACE"
kubectl scale deployment/lobby --replicas=2 -n "$NAMESPACE"
kubectl scale statefulset/game --replicas=2 -n "$NAMESPACE"

log "Done."
