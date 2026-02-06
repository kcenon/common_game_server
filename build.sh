#!/usr/bin/env bash
#
# build.sh — Common Game Server build driver
#
# Usage:
#   ./build.sh [command] [options]
#
# Commands:
#   configure   Run CMake configure step only
#   build       Build the project (configure first if needed)
#   test        Build and run tests
#   clean       Remove build directory
#   rebuild     Clean + build
#   install     Build and install to prefix
#   all         Build + test (default)
#
# Options:
#   -t, --type TYPE        Build type: Debug|Release|RelWithDebInfo (default: Debug)
#   -j, --jobs N           Parallel build jobs (default: nproc)
#   -s, --sanitizers       Enable address + UB sanitizers
#   -b, --benchmarks       Enable benchmark targets
#       --no-tests         Disable test targets
#       --dir DIR          Build directory (default: build/<type>)
#       --prefix DIR       Install prefix (default: /usr/local)
#   -v, --verbose          Verbose build output
#   -h, --help             Show this help message

set -euo pipefail

# ── Project root (script location) ──────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}"

# ── Defaults ────────────────────────────────────────────────────────────────
COMMAND="all"
BUILD_TYPE="Debug"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
SANITIZERS="OFF"
BENCHMARKS="OFF"
TESTS="ON"
BUILD_DIR=""
INSTALL_PREFIX="/usr/local"
VERBOSE=""

# ── Colors (only if stdout is a terminal) ───────────────────────────────────
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[0;33m'
    CYAN='\033[0;36m'
    BOLD='\033[1m'
    RESET='\033[0m'
else
    RED='' GREEN='' YELLOW='' CYAN='' BOLD='' RESET=''
fi

# ── Helpers ─────────────────────────────────────────────────────────────────
info()  { echo -e "${CYAN}[INFO]${RESET}  $*"; }
ok()    { echo -e "${GREEN}[OK]${RESET}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
err()   { echo -e "${RED}[ERROR]${RESET} $*" >&2; }
die()   { err "$@"; exit 1; }

usage() {
    sed -n '2,/^$/s/^# \{0,1\}//p' "$0"
    exit 0
}

# ── Parse arguments ─────────────────────────────────────────────────────────
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            configure|build|test|clean|rebuild|install|all)
                COMMAND="$1"; shift ;;
            -t|--type)
                BUILD_TYPE="$2"; shift 2 ;;
            -j|--jobs)
                JOBS="$2"; shift 2 ;;
            -s|--sanitizers)
                SANITIZERS="ON"; shift ;;
            -b|--benchmarks)
                BENCHMARKS="ON"; shift ;;
            --no-tests)
                TESTS="OFF"; shift ;;
            --dir)
                BUILD_DIR="$2"; shift 2 ;;
            --prefix)
                INSTALL_PREFIX="$2"; shift 2 ;;
            -v|--verbose)
                VERBOSE="--verbose"; shift ;;
            -h|--help)
                usage ;;
            *)
                die "Unknown argument: $1  (use --help for usage)" ;;
        esac
    done

    # Default build directory based on type
    if [[ -z "${BUILD_DIR}" ]]; then
        local type_lower
        type_lower="$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"
        BUILD_DIR="${PROJECT_ROOT}/build/${type_lower}"
    fi
}

# ── Prerequisites check ────────────────────────────────────────────────────
check_prerequisites() {
    local missing=()
    command -v cmake  >/dev/null 2>&1 || missing+=("cmake")
    command -v make   >/dev/null 2>&1 || command -v ninja >/dev/null 2>&1 || missing+=("make or ninja")

    if [[ ${#missing[@]} -gt 0 ]]; then
        die "Missing required tools: ${missing[*]}"
    fi

    # Verify CMake version >= 3.20
    local cmake_ver
    cmake_ver="$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+')"
    local major minor
    major="${cmake_ver%%.*}"
    minor="${cmake_ver##*.}"
    if [[ "${major}" -lt 3 ]] || { [[ "${major}" -eq 3 ]] && [[ "${minor}" -lt 20 ]]; }; then
        die "CMake >= 3.20 required (found ${cmake_ver})"
    fi
}

# ── Commands ────────────────────────────────────────────────────────────────
do_configure() {
    info "Configuring: ${BUILD_TYPE}  dir=${BUILD_DIR}"

    local cmake_args=(
        -B "${BUILD_DIR}"
        -S "${PROJECT_ROOT}"
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
        -DCGS_BUILD_TESTS="${TESTS}"
        -DCGS_BUILD_BENCHMARKS="${BENCHMARKS}"
        -DCGS_ENABLE_SANITIZERS="${SANITIZERS}"
    )

    # Prefer Ninja if available
    if command -v ninja >/dev/null 2>&1; then
        cmake_args+=(-G Ninja)
    fi

    cmake "${cmake_args[@]}"
    ok "Configure complete"
}

do_build() {
    # Auto-configure if build dir doesn't exist
    if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
        do_configure
    fi

    info "Building: ${BUILD_TYPE}  jobs=${JOBS}"
    cmake --build "${BUILD_DIR}" --parallel "${JOBS}" ${VERBOSE}
    ok "Build complete"
}

do_test() {
    if [[ "${TESTS}" == "OFF" ]]; then
        warn "Tests are disabled (--no-tests). Skipping."
        return 0
    fi

    do_build

    info "Running tests..."
    ctest --test-dir "${BUILD_DIR}" --output-on-failure --parallel "${JOBS}"
    ok "All tests passed"
}

do_clean() {
    if [[ -d "${BUILD_DIR}" ]]; then
        info "Removing ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
        ok "Clean complete"
    else
        info "Nothing to clean (${BUILD_DIR} does not exist)"
    fi
}

do_rebuild() {
    do_clean
    do_build
}

do_install() {
    do_build

    info "Installing to ${INSTALL_PREFIX}"
    cmake --install "${BUILD_DIR}" --prefix "${INSTALL_PREFIX}"
    ok "Install complete"
}

do_all() {
    do_test
}

# ── Main ────────────────────────────────────────────────────────────────────
main() {
    parse_args "$@"
    check_prerequisites

    echo -e "${BOLD}common_game_server${RESET} build driver"
    echo "  command : ${COMMAND}"
    echo "  type    : ${BUILD_TYPE}"
    echo "  dir     : ${BUILD_DIR}"
    echo "  jobs    : ${JOBS}"
    echo "  tests   : ${TESTS}"
    echo "  sanitize: ${SANITIZERS}"
    echo ""

    case "${COMMAND}" in
        configure) do_configure ;;
        build)     do_build ;;
        test)      do_test ;;
        clean)     do_clean ;;
        rebuild)   do_rebuild ;;
        install)   do_install ;;
        all)       do_all ;;
        *)         die "Unknown command: ${COMMAND}" ;;
    esac
}

main "$@"
