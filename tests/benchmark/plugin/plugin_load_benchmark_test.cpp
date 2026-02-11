/// @file plugin_load_benchmark_test.cpp
/// @brief Performance benchmark for plugin load and initialization time.
///
/// Measures the MMORPG plugin's full startup sequence:
///   OnLoad  — context binding + component storage registration (18 storages)
///   OnInit  — system registration (6 systems) + scheduler build
///
/// The combined time must meet SRS-NFR-006: plugin load ≤100ms.
///
/// Acceptance criterion: OnLoad + OnInit ≤100ms.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "cgs/plugin/mmorpg_plugin.hpp"

using namespace cgs::plugin;

namespace {

// Benchmark parameters
constexpr int kIterations = 500;
constexpr int kWarmup = 10;
constexpr double kMaxLoadTimeMs = 100.0; // SRS-NFR-006

} // anonymous namespace

// ===========================================================================
// Plugin Load Benchmark
// ===========================================================================

class PluginLoadBenchmark : public ::testing::Test {};

// ===========================================================================
// Full Plugin Startup (OnLoad + OnInit)
// ===========================================================================

TEST_F(PluginLoadBenchmark, FullStartupTime) {
    // Warmup: prime allocators and caches
    for (int i = 0; i < kWarmup; ++i) {
        MMORPGPlugin plugin;
        PluginContext ctx;
        (void)plugin.OnLoad(ctx);
        (void)plugin.OnInit();
        plugin.OnShutdown();
        plugin.OnUnload();
    }

    std::vector<double> totalLatencies;
    std::vector<double> loadLatencies;
    std::vector<double> initLatencies;
    totalLatencies.reserve(static_cast<std::size_t>(kIterations));
    loadLatencies.reserve(static_cast<std::size_t>(kIterations));
    initLatencies.reserve(static_cast<std::size_t>(kIterations));

    for (int iter = 0; iter < kIterations; ++iter) {
        MMORPGPlugin plugin;
        PluginContext ctx;

        // Measure OnLoad (context binding + 18 storage registrations)
        auto loadStart = std::chrono::high_resolution_clock::now();
        bool loadOk = plugin.OnLoad(ctx);
        auto loadEnd = std::chrono::high_resolution_clock::now();
        ASSERT_TRUE(loadOk);

        // Measure OnInit (6 system registrations + scheduler build)
        auto initStart = std::chrono::high_resolution_clock::now();
        bool initOk = plugin.OnInit();
        auto initEnd = std::chrono::high_resolution_clock::now();
        ASSERT_TRUE(initOk);

        double loadMs =
            std::chrono::duration<double, std::milli>(loadEnd - loadStart)
                .count();
        double initMs =
            std::chrono::duration<double, std::milli>(initEnd - initStart)
                .count();

        loadLatencies.push_back(loadMs);
        initLatencies.push_back(initMs);
        totalLatencies.push_back(loadMs + initMs);

        plugin.OnShutdown();
        plugin.OnUnload();
    }

    std::sort(totalLatencies.begin(), totalLatencies.end());
    std::sort(loadLatencies.begin(), loadLatencies.end());
    std::sort(initLatencies.begin(), initLatencies.end());

    double totalMedian = totalLatencies[totalLatencies.size() / 2];
    double totalP99 =
        totalLatencies[static_cast<std::size_t>(
            static_cast<double>(totalLatencies.size()) * 0.99)];
    double totalAvg =
        std::accumulate(totalLatencies.begin(), totalLatencies.end(), 0.0) /
        static_cast<double>(totalLatencies.size());

    double loadMedian = loadLatencies[loadLatencies.size() / 2];
    double initMedian = initLatencies[initLatencies.size() / 2];
    double loadP99 =
        loadLatencies[static_cast<std::size_t>(
            static_cast<double>(loadLatencies.size()) * 0.99)];
    double initP99 =
        initLatencies[static_cast<std::size_t>(
            static_cast<double>(initLatencies.size()) * 0.99)];

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  Plugin Load Benchmark (SRS-NFR-006)             |\n"
              << "|  MMORPGPlugin Full Startup                       |\n"
              << "+-------------------------------------------------+\n"
              << "|  Iterations:    " << std::setw(10) << kIterations
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << "|  OnLoad Median: " << std::setw(10) << std::fixed
              << std::setprecision(4) << loadMedian << " ms               |\n"
              << "|  OnLoad p99:    " << std::setw(10) << loadP99
              << " ms               |\n"
              << "|  OnInit Median: " << std::setw(10) << initMedian
              << " ms               |\n"
              << "|  OnInit p99:    " << std::setw(10) << initP99
              << " ms               |\n"
              << "+-------------------------------------------------+\n"
              << "|  Total Avg:     " << std::setw(10) << totalAvg
              << " ms               |\n"
              << "|  Total Median:  " << std::setw(10) << totalMedian
              << " ms               |\n"
              << "|  Total p99:     " << std::setw(10) << totalP99
              << " ms               |\n"
              << "+-------------------------------------------------+\n"
              << "|  Requirement:   " << std::setw(10) << std::setprecision(1)
              << kMaxLoadTimeMs << " ms (SRS-NFR-006)  |\n"
              << "|  Status:        " << std::setw(10)
              << (totalP99 <= kMaxLoadTimeMs ? "PASS" : "FAIL")
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_LE(totalP99, kMaxLoadTimeMs)
        << "p99 plugin load time " << totalP99
        << " ms exceeds the " << kMaxLoadTimeMs
        << " ms requirement (SRS-NFR-006)";
}

// ===========================================================================
// Plugin Construction Overhead
// ===========================================================================

TEST_F(PluginLoadBenchmark, ConstructionOverhead) {
    // Measure just the constructor (class template initialization)
    constexpr int kConstructIterations = 1000;

    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(kConstructIterations));

    for (int iter = 0; iter < kConstructIterations; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();
        MMORPGPlugin plugin;
        auto end = std::chrono::high_resolution_clock::now();

        double ms =
            std::chrono::duration<double, std::milli>(end - start).count();
        latencies.push_back(ms);

        // Prevent optimizer from eliding the construction
        EXPECT_EQ(plugin.PlayerCount(), 0u);
    }

    std::sort(latencies.begin(), latencies.end());

    double medianMs = latencies[latencies.size() / 2];
    double p99Ms =
        latencies[static_cast<std::size_t>(
            static_cast<double>(latencies.size()) * 0.99)];

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  Plugin Construction Benchmark                   |\n"
              << "|  (Constructor + Class Template Init)             |\n"
              << "+-------------------------------------------------+\n"
              << "|  Iterations:    " << std::setw(10) << kConstructIterations
              << "                    |\n"
              << "|  Median:        " << std::setw(10) << std::fixed
              << std::setprecision(4) << medianMs << " ms               |\n"
              << "|  p99:           " << std::setw(10) << p99Ms
              << " ms               |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;
}

// ===========================================================================
// Repeated Load/Unload Cycle (Hot Reload Scenario)
// ===========================================================================

TEST_F(PluginLoadBenchmark, LoadUnloadCycleStability) {
    constexpr int kCycles = 100;

    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(kCycles));

    for (int cycle = 0; cycle < kCycles; ++cycle) {
        MMORPGPlugin plugin;
        PluginContext ctx;

        auto start = std::chrono::high_resolution_clock::now();

        (void)plugin.OnLoad(ctx);
        (void)plugin.OnInit();

        // Simulate brief usage
        auto map = plugin.CreateMapInstance(1, cgs::game::MapType::OpenWorld);
        (void)plugin.CreateCharacter("TestPlayer", CharacterClass::Warrior,
                                     {0.0f, 0.0f, 0.0f}, map);
        plugin.OnUpdate(0.016f);

        plugin.OnShutdown();
        plugin.OnUnload();

        auto end = std::chrono::high_resolution_clock::now();
        double ms =
            std::chrono::duration<double, std::milli>(end - start).count();
        latencies.push_back(ms);
    }

    std::sort(latencies.begin(), latencies.end());

    double medianMs = latencies[latencies.size() / 2];
    double p99Ms =
        latencies[static_cast<std::size_t>(
            static_cast<double>(latencies.size()) * 0.99)];
    double maxMs = latencies.back();

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  Load/Unload Cycle Stability                     |\n"
              << "|  (Full Lifecycle with Brief Usage)               |\n"
              << "+-------------------------------------------------+\n"
              << "|  Cycles:        " << std::setw(10) << kCycles
              << "                    |\n"
              << "|  Median:        " << std::setw(10) << std::fixed
              << std::setprecision(4) << medianMs << " ms               |\n"
              << "|  p99:           " << std::setw(10) << p99Ms
              << " ms               |\n"
              << "|  Max:           " << std::setw(10) << maxMs
              << " ms               |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_LE(p99Ms, kMaxLoadTimeMs)
        << "p99 load/unload cycle " << p99Ms
        << " ms exceeds the " << kMaxLoadTimeMs << " ms requirement";
}
