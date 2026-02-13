cmake_minimum_required(VERSION 3.20)

# Project definition
project(common_game_server
    VERSION 0.1.0
    DESCRIPTION "Unified Game Server Framework with ECS and Plugin Architecture"
    LANGUAGES CXX
)

# Global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clang-tidy and IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(CGS_BUILD_TESTS "Build unit and integration tests" ON)
option(CGS_BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(CGS_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(CGS_HOT_RELOAD "Enable plugin hot reload (development only)" OFF)
option(CGS_BUILD_SERVICES "Build service executables" OFF)
option(CGS_ENABLE_COVERAGE "Enable code coverage instrumentation (gcov)" OFF)

# Code coverage instrumentation (GCC/Clang only)
if(CGS_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        message(STATUS "Code coverage enabled — adding --coverage flags")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage -fprofile-update=atomic)
        add_link_options(--coverage)
    else()
        message(WARNING "CGS_ENABLE_COVERAGE is only supported with GCC/Clang")
    endif()
endif()

# Compiler warning flags
add_library(cgs_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(cgs_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Woverloaded-virtual
        -Wno-unused-parameter
    )
elseif(MSVC)
    target_compile_options(cgs_warnings INTERFACE
        /W4
        /WX
        /permissive-
        /w14640  # thread-unsafe static member init
        /w14826  # conversion from 'type1' to 'type2' is sign-extended
    )
endif()

# Sanitizer support
if(CGS_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# External dependencies (via Conan or system packages)
find_package(yaml-cpp REQUIRED)

# kcenon ecosystem libraries (via FetchContent)
include(cmake/kcenon_deps.cmake)

# Main library target: cgs_core
add_library(cgs_core INTERFACE)
target_include_directories(cgs_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(cgs_core INTERFACE cgs_warnings yaml-cpp::yaml-cpp)

# Layer targets (libraries built as subsequent issues are implemented)
# Layer 1-2: Foundation adapters
add_subdirectory(src/foundation)

# Layer 3: Services
add_subdirectory(src/services)

# Layer 4: ECS core
add_subdirectory(src/ecs)

# Layer 5: Game logic
add_subdirectory(src/game)

# Layer 6: Plugins
add_subdirectory(src/plugins)

# Testing
if(CGS_BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_subdirectory(tests)
    else()
        message(STATUS "GTest not found - tests will not be built")
    endif()
endif()

# Benchmarks
if(CGS_BUILD_BENCHMARKS)
    find_package(benchmark QUIET)
    if(benchmark_FOUND)
        add_subdirectory(benchmarks)
    else()
        message(STATUS "Google Benchmark not found - benchmarks will not be built")
    endif()
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS cgs_core cgs_warnings
    EXPORT cgs_targets
)
install(DIRECTORY include/cgs
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT cgs_targets
    FILE cgs-config.cmake
    NAMESPACE cgs::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cgs
)
