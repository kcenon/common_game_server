# kcenon_deps.cmake — FetchContent configuration for kcenon ecosystem libraries
#
# Dependency chain:
#   common_game_server
#     +-- network_system (TCP/UDP/WebSocket, ASIO-based async I/O)
#     |     +-- thread_system (thread pool, job scheduling)
#     |     +-- common_system (header-only: Result<T>, error codes, utilities)
#     +-- thread_system
#           +-- common_system
#
# All kcenon libraries follow the same integration pattern via FetchContent.
# Pin to 'main' branch initially; switch to commit SHA for reproducible builds.

include(FetchContent)

set(FETCHCONTENT_QUIET ON)

# ── common_system ─────────────────────────────────────────────────────────
# Header-only foundation: Result<T> pattern, error handling, utilities.
# Must be fetched before thread_system (transitive dependency).
FetchContent_Declare(
    common_system
    GIT_REPOSITORY https://github.com/kcenon/common_system.git
    GIT_TAG        main
)

# ── thread_system ─────────────────────────────────────────────────────────
# Thread pool with adaptive queuing, lock-free structures, priority routing.
FetchContent_Declare(
    thread_system
    GIT_REPOSITORY https://github.com/kcenon/thread_system.git
    GIT_TAG        main
)

# ── network_system ───────────────────────────────────────────────────────
# Async TCP/UDP/WebSocket networking with ASIO-based non-blocking I/O.
# Requires: common_system (Tier 0), thread_system (Tier 1)
# External: standalone ASIO, OpenSSL 3.x (for TLS/WebSocket)
FetchContent_Declare(
    network_system
    GIT_REPOSITORY https://github.com/kcenon/network_system.git
    GIT_TAG        main
)

# Disable dependency tests/samples to speed up configure
set(BUILD_TESTING_SAVED "${BUILD_TESTING}")
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

# Populate common_system first so we can set COMMON_SYSTEM_INCLUDE_DIR
# before thread_system configures (it requires this variable).
FetchContent_MakeAvailable(common_system)

set(COMMON_SYSTEM_INCLUDE_DIR "${common_system_SOURCE_DIR}/include"
    CACHE PATH "Path to common_system include directory" FORCE)

FetchContent_MakeAvailable(thread_system)

# network_system options: disable tests/samples/benchmarks for subproject
set(BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_VERIFY_BUILD OFF CACHE BOOL "" FORCE)
set(NETWORK_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(NETWORK_BUILD_INTEGRATION_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(network_system)

# Restore BUILD_TESTING for our own tests
set(BUILD_TESTING "${BUILD_TESTING_SAVED}" CACHE BOOL "" FORCE)

# Mark kcenon include directories as SYSTEM to suppress third-party warnings.
# Our strict -Werror flags should not apply to external headers.
function(_cgs_mark_system_includes target)
    if(NOT TARGET ${target})
        return()
    endif()
    get_target_property(_dirs ${target} INTERFACE_INCLUDE_DIRECTORIES)
    if(_dirs)
        set_property(TARGET ${target} APPEND PROPERTY
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${_dirs})
    endif()
endfunction()

# Only apply to real (non-alias) targets
_cgs_mark_system_includes(common_system)
_cgs_mark_system_includes(thread_base)
_cgs_mark_system_includes(thread_core)
_cgs_mark_system_includes(NetworkSystem)
