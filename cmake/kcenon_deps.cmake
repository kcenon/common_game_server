# kcenon_deps.cmake — FetchContent configuration for kcenon ecosystem libraries
#
# Dependency chain:
#   common_game_server
#     +-- network_system (TCP/UDP/WebSocket, ASIO-based async I/O)
#     |     +-- thread_system (thread pool, job scheduling)
#     |     +-- common_system (header-only: Result<T>, error codes, utilities)
#     +-- database_system (PostgreSQL/MySQL/SQLite DAL)
#     |     +-- common_system
#     +-- thread_system
#           +-- common_system
#
# All kcenon libraries follow the same integration pattern via FetchContent.
# Pin to 'main' branch initially; switch to commit SHA for reproducible builds.

include(FetchContent)

set(FETCHCONTENT_QUIET ON)

# ── Declare all dependencies upfront ──────────────────────────────────────

FetchContent_Declare(
    common_system
    GIT_REPOSITORY https://github.com/kcenon/common_system.git
    GIT_TAG        main
)

FetchContent_Declare(
    thread_system
    GIT_REPOSITORY https://github.com/kcenon/thread_system.git
    GIT_TAG        main
)

FetchContent_Declare(
    network_system
    GIT_REPOSITORY https://github.com/kcenon/network_system.git
    GIT_TAG        main
)

FetchContent_Declare(
    database_system
    GIT_REPOSITORY https://github.com/kcenon/database_system.git
    GIT_TAG        main
)

# ── Populate dependencies in tier order ───────────────────────────────────
# Disable dependency tests/samples to speed up configure.
set(BUILD_TESTING_SAVED "${BUILD_TESTING}")
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

message(STATUS "")
message(STATUS "========================================")
message(STATUS " CGS dependency resolution (4 packages)")
message(STATUS "========================================")

# ── [1/4] common_system (Tier 0) ──────────────────────────────────────────
message(STATUS "")
message(STATUS "[1/4] common_system: fetching...")
FetchContent_MakeAvailable(common_system)
set(COMMON_SYSTEM_INCLUDE_DIR "${common_system_SOURCE_DIR}/include"
    CACHE PATH "Path to common_system include directory" FORCE)
message(STATUS "[1/4] common_system: ready (${common_system_SOURCE_DIR})")

# ── [2/4] thread_system (Tier 1) ──────────────────────────────────────────
message(STATUS "")
message(STATUS "[2/4] thread_system: fetching...")
FetchContent_MakeAvailable(thread_system)
set(THREAD_SYSTEM_INCLUDE_DIR "${thread_system_SOURCE_DIR}/include"
    CACHE PATH "Path to thread_system include directory" FORCE)

# Propagate thread_system's C++20 feature-detection definitions (USE_STD_JTHREAD,
# USE_STD_FORMAT, HAS_STD_LATCH, …) to CGS targets that include thread_system
# headers.  thread_system sets these via add_definitions() which is directory-scoped
# and does NOT propagate to consumer directories.  Without this, CGS translation
# units see a different class layout (e.g. lifecycle_controller uses
# std::atomic<bool> instead of std::optional<std::stop_source>), causing an ODR
# violation and heap corruption on GCC / glibc.  (Fixes #80)
#
# NOTE: We store these in a cache variable and apply them per-target (not via
# global add_definitions()) to avoid leaking definitions into other FetchContent
# dependencies like network_system, which has conditional code paths gated on
# THREAD_HAS_COMMON_EXECUTOR that assume APIs not present in thread_pool.
get_directory_property(_thread_system_defs
    DIRECTORY "${thread_system_SOURCE_DIR}"
    COMPILE_DEFINITIONS)
set(CGS_THREAD_SYSTEM_DEFS "${_thread_system_defs}"
    CACHE INTERNAL "thread_system compile definitions for CGS targets")

message(STATUS "[2/4] thread_system: ready (${thread_system_SOURCE_DIR})")

# ── [3/4] network_system (Tier 4) ─────────────────────────────────────────
# Pre-set options before configure. network_system uses BUILD_TESTS (not
# BUILD_TESTING), and enables many optional subsystems by default.
message(STATUS "")
message(STATUS "[3/4] network_system: setting options...")
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_VERIFY_BUILD OFF CACHE BOOL "" FORCE)
set(BUILD_MESSAGING_BRIDGE OFF CACHE BOOL "" FORCE)
set(BUILD_WITH_CONTAINER_SYSTEM OFF CACHE BOOL "" FORCE)
set(BUILD_WITH_LOGGER_SYSTEM OFF CACHE BOOL "" FORCE)
set(BUILD_WITH_MONITORING_SYSTEM OFF CACHE BOOL "" FORCE)
set(NETWORK_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(NETWORK_BUILD_INTEGRATION_TESTS OFF CACHE BOOL "" FORCE)
set(NETWORK_ENABLE_GRPC_OFFICIAL OFF CACHE BOOL "" FORCE)
set(NETWORK_BUILD_MODULES OFF CACHE BOOL "" FORCE)
set(ENABLE_COVERAGE OFF CACHE BOOL "" FORCE)
message(STATUS "[3/4] network_system: fetching and configuring (may take ~20s)...")
FetchContent_MakeAvailable(network_system)
message(STATUS "[3/4] network_system: ready (${network_system_SOURCE_DIR})")

# ── [4/4] database_system (Tier 3) ───────────────────────────────────────
# Disable optional backends and features to minimize build footprint.
message(STATUS "")
message(STATUS "[4/4] database_system: setting options...")
set(USE_UNIT_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_DATABASE_SAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_DATABASE_SYSTEM_AS_SUBMODULE ON CACHE BOOL "" FORCE)
set(BUILD_WITH_COMMON_SYSTEM ON CACHE BOOL "" FORCE)
set(USE_THREAD_SYSTEM OFF CACHE BOOL "" FORCE)
set(USE_CONTAINER_SYSTEM OFF CACHE BOOL "" FORCE)
set(USE_MONGODB OFF CACHE BOOL "" FORCE)
set(USE_REDIS OFF CACHE BOOL "" FORCE)
set(USE_MYSQL OFF CACHE BOOL "" FORCE)
set(BUILD_INTEGRATED_DATABASE OFF CACHE BOOL "" FORCE)
set(DATABASE_BUILD_MODULES OFF CACHE BOOL "" FORCE)
message(STATUS "[4/4] database_system: fetching and configuring...")
FetchContent_MakeAvailable(database_system)
set(DATABASE_SYSTEM_INCLUDE_DIR "${database_system_SOURCE_DIR}"
    CACHE PATH "Path to database_system root directory" FORCE)
message(STATUS "[4/4] database_system: ready (${database_system_SOURCE_DIR})")

# ── Restore and finalize ──────────────────────────────────────────────────
set(BUILD_TESTING "${BUILD_TESTING_SAVED}" CACHE BOOL "" FORCE)

message(STATUS "")
message(STATUS "========================================")
message(STATUS " CGS dependencies resolved:")
message(STATUS "   common_system    (Tier 0, header-only)")
message(STATUS "   thread_system    (Tier 1, thread pool)")
message(STATUS "   network_system   (Tier 4, TCP/UDP/WS)")
message(STATUS "   database_system  (Tier 3, DAL)")
message(STATUS "========================================")
message(STATUS "")

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

_cgs_mark_system_includes(common_system)
_cgs_mark_system_includes(thread_base)
_cgs_mark_system_includes(thread_core)
_cgs_mark_system_includes(NetworkSystem)
_cgs_mark_system_includes(database)
