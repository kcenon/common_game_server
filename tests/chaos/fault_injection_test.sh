#!/usr/bin/env bash
# fault_injection_test.sh — Kill a CGS service pod and verify recovery within MTTR SLO.
#
# SRS-NFR-012: MTTR <= 5 minutes
#
# Prerequisites:
#   - kubectl configured with access to the target cluster
#   - CGS services deployed in cgs-system namespace
#   - curl available
#
# Usage:
#   ./tests/chaos/fault_injection_test.sh [service_name]
#   ./tests/chaos/fault_injection_test.sh game
#   ./tests/chaos/fault_injection_test.sh          # Tests all services

set -euo pipefail

NAMESPACE="cgs-system"
MTTR_LIMIT_SECONDS=300  # 5 minutes
POLL_INTERVAL=5
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[FAIL]${NC} $*"; }

# ── Helpers ──────────────────────────────────────────────────────────────

check_prerequisites() {
    if ! command -v kubectl &>/dev/null; then
        log_error "kubectl is not installed"
        exit 1
    fi

    if ! kubectl get namespace "$NAMESPACE" &>/dev/null; then
        log_error "Namespace $NAMESPACE does not exist"
        exit 1
    fi
}

get_service_health_port() {
    local service="$1"
    case "$service" in
        auth)    echo 9101 ;;
        gateway) echo 9100 ;;
        game)    echo 9110 ;;
        lobby)   echo 9102 ;;
        dbproxy) echo 9103 ;;
        *) log_error "Unknown service: $service"; exit 1 ;;
    esac
}

wait_for_pod_ready() {
    local service="$1"
    local timeout="$2"
    local start_time
    start_time=$(date +%s)

    while true; do
        local elapsed=$(( $(date +%s) - start_time ))
        if (( elapsed >= timeout )); then
            return 1
        fi

        local ready_count
        ready_count=$(kubectl get pods -n "$NAMESPACE" \
            -l "app.kubernetes.io/name=$service" \
            --field-selector=status.phase=Running \
            -o jsonpath='{range .items[*]}{.status.conditions[?(@.type=="Ready")].status}{"\n"}{end}' 2>/dev/null \
            | grep -c "True" || true)

        if (( ready_count > 0 )); then
            return 0
        fi

        sleep "$POLL_INTERVAL"
    done
}

# ── Test: Kill Service Pod ───────────────────────────────────────────────

test_service_recovery() {
    local service="$1"
    local health_port
    health_port=$(get_service_health_port "$service")

    log_info "=== Testing fault injection for service: $service ==="

    # 1. Verify service is healthy before test.
    log_info "Verifying $service is healthy..."
    local pod_name
    pod_name=$(kubectl get pods -n "$NAMESPACE" \
        -l "app.kubernetes.io/name=$service" \
        --field-selector=status.phase=Running \
        -o jsonpath='{.items[0].metadata.name}' 2>/dev/null)

    if [[ -z "$pod_name" ]]; then
        log_error "No running pod found for $service"
        return 1
    fi
    log_info "Target pod: $pod_name"

    # 2. Kill the pod (simulate crash).
    log_info "Killing pod $pod_name..."
    local kill_time
    kill_time=$(date +%s)
    kubectl delete pod "$pod_name" -n "$NAMESPACE" --grace-period=0 --force 2>/dev/null || true

    # 3. Wait for recovery.
    log_info "Waiting for $service to recover (MTTR limit: ${MTTR_LIMIT_SECONDS}s)..."

    if wait_for_pod_ready "$service" "$MTTR_LIMIT_SECONDS"; then
        local recovery_time=$(( $(date +%s) - kill_time ))
        log_info "$service recovered in ${recovery_time}s (limit: ${MTTR_LIMIT_SECONDS}s)"

        if (( recovery_time <= MTTR_LIMIT_SECONDS )); then
            log_info "PASS: $service MTTR = ${recovery_time}s <= ${MTTR_LIMIT_SECONDS}s"
            return 0
        else
            log_error "FAIL: $service MTTR = ${recovery_time}s > ${MTTR_LIMIT_SECONDS}s"
            return 1
        fi
    else
        log_error "FAIL: $service did not recover within ${MTTR_LIMIT_SECONDS}s"
        return 1
    fi
}

# ── Main ─────────────────────────────────────────────────────────────────

main() {
    check_prerequisites

    local services=("$@")
    if (( ${#services[@]} == 0 )); then
        services=(auth gateway game lobby dbproxy)
    fi

    local passed=0
    local failed=0
    local results=()

    for service in "${services[@]}"; do
        if test_service_recovery "$service"; then
            (( passed++ ))
            results+=("PASS: $service")
        else
            (( failed++ ))
            results+=("FAIL: $service")
        fi
        echo ""
    done

    # Summary
    echo "========================================"
    echo "Fault Injection Test Summary"
    echo "========================================"
    for r in "${results[@]}"; do
        echo "  $r"
    done
    echo "----------------------------------------"
    echo "Passed: $passed  Failed: $failed  Total: $(( passed + failed ))"
    echo "========================================"

    if (( failed > 0 )); then
        exit 1
    fi
}

main "$@"
