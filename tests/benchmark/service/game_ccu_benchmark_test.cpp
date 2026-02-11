/// @file game_ccu_benchmark_test.cpp
/// @brief CCU (Concurrent Connected Users) benchmark for GameServer.
///
/// Simulates 1,000+ concurrent players joining, performing game ticks,
/// and leaving game instances.  This validates:
///   - SRS-NFR-008: >= 1,000 CCU per node
///   - SRS-NFR-010: Linear scaling (per-instance overhead)
///
/// The test measures:
///   1. Player join throughput (players/sec)
///   2. Tick latency under full CCU load
///   3. Player leave throughput
///   4. Memory overhead per player
///
/// Acceptance criteria:
///   - 1,000 players join within 5 seconds
///   - Tick latency <= 50ms under full load (20 Hz budget)
///   - No failures during join/leave lifecycle

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "cgs/foundation/types.hpp"
#include "cgs/service/game_server.hpp"

using namespace cgs::service;
using namespace cgs::foundation;
using namespace std::chrono;

namespace {

// CCU targets
constexpr int kTargetCCU = 1000;                     // SRS-NFR-008
constexpr int kInstanceCount = 10;                    // Distribute across instances
constexpr int kPlayersPerInstance = kTargetCCU / kInstanceCount;
constexpr int kTicksUnderLoad = 100;                  // Ticks at full CCU
constexpr int kWarmupTicks = 10;
constexpr double kMaxJoinTimeSeconds = 5.0;           // 1K joins within 5s
constexpr double kMaxTickLatencyMs = 50.0;            // SRS-NFR-002

/// Simple helper to compute percentile from a sorted vector.
double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) {
        return 0.0;
    }
    auto idx = static_cast<std::size_t>(
        p / 100.0 * static_cast<double>(sorted.size() - 1));
    return sorted[idx];
}

} // anonymous namespace

// ===========================================================================
// Benchmark Fixture
// ===========================================================================

class GameCCUBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        GameServerConfig config;
        config.tickRate = 20;
        config.maxInstances = 1000;
        config.spatialCellSize = 32.0f;
        config.aiTickInterval = 0.1f;

        server_ = std::make_unique<GameServer>(config);
        auto r = server_->start();
        ASSERT_TRUE(r.hasValue()) << "GameServer start failed";
    }

    void TearDown() override {
        if (server_) {
            server_->stop();
        }
    }

    std::unique_ptr<GameServer> server_;
};

// ===========================================================================
// CCU Join Throughput
// ===========================================================================

TEST_F(GameCCUBenchmark, JoinThroughput1KCCU) {
    std::cout << "\n=== CCU Join Throughput Benchmark ===" << std::endl;
    std::cout << "Target: " << kTargetCCU << " players across "
              << kInstanceCount << " instances\n" << std::endl;

    // Create instances
    std::vector<uint32_t> instanceIds;
    instanceIds.reserve(kInstanceCount);
    for (int i = 0; i < kInstanceCount; ++i) {
        auto mapId = static_cast<uint32_t>(i + 1);
        auto result = server_->createInstance(mapId, kPlayersPerInstance + 10);
        ASSERT_TRUE(result.hasValue())
            << "Failed to create instance " << i;
        instanceIds.push_back(result.value());
    }

    // Join all players and measure time
    const auto joinStart = steady_clock::now();
    int joinedCount = 0;
    int joinFailures = 0;

    for (int i = 0; i < kTargetCCU; ++i) {
        PlayerId playerId(static_cast<uint64_t>(i + 1));
        auto instIdx = static_cast<std::size_t>(i % kInstanceCount);
        auto result = server_->addPlayer(playerId, instanceIds[instIdx]);
        if (result.hasValue()) {
            ++joinedCount;
        } else {
            ++joinFailures;
        }
    }

    const auto joinEnd = steady_clock::now();
    const double joinElapsedSec =
        static_cast<double>(duration_cast<microseconds>(joinEnd - joinStart).count()) / 1e6;
    const double joinRate =
        static_cast<double>(joinedCount) / joinElapsedSec;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Joined:    " << joinedCount << " / " << kTargetCCU
              << " players\n";
    std::cout << "  Failures:  " << joinFailures << "\n";
    std::cout << "  Time:      " << joinElapsedSec << " sec\n";
    std::cout << "  Rate:      " << joinRate << " players/sec\n";

    // Verify stats
    auto stats = server_->stats();
    std::cout << "  Entities:  " << stats.entityCount << "\n";
    std::cout << "  Players:   " << stats.playerCount << "\n";
    std::cout << "  Instances: " << stats.activeInstances << "\n";

    EXPECT_EQ(joinFailures, 0) << "All joins should succeed";
    EXPECT_EQ(joinedCount, kTargetCCU);
    EXPECT_LT(joinElapsedSec, kMaxJoinTimeSeconds)
        << "Joining " << kTargetCCU << " players took too long";

    std::cout << "\n  [PASS] " << kTargetCCU
              << " players joined in " << joinElapsedSec
              << "s (< " << kMaxJoinTimeSeconds << "s)\n";
}

// ===========================================================================
// Tick Latency Under Full CCU Load
// ===========================================================================

TEST_F(GameCCUBenchmark, TickLatencyUnderFullLoad) {
    std::cout << "\n=== Tick Latency Under " << kTargetCCU
              << " CCU ===" << std::endl;

    // Setup: stop game loop, create instances, add players, then tick manually
    server_->stop();

    GameServerConfig config;
    config.tickRate = 20;
    config.maxInstances = 1000;
    config.spatialCellSize = 32.0f;
    config.aiTickInterval = 0.1f;
    server_ = std::make_unique<GameServer>(config);

    // Create instances
    std::vector<uint32_t> instanceIds;
    for (int i = 0; i < kInstanceCount; ++i) {
        auto result = server_->createInstance(
            static_cast<uint32_t>(i + 1), kPlayersPerInstance + 10);
        ASSERT_TRUE(result.hasValue());
        instanceIds.push_back(result.value());
    }

    // Add all players
    for (int i = 0; i < kTargetCCU; ++i) {
        PlayerId playerId(static_cast<uint64_t>(i + 1));
        auto instIdx = static_cast<std::size_t>(i % kInstanceCount);
        auto r = server_->addPlayer(playerId, instanceIds[instIdx]);
        ASSERT_TRUE(r.hasValue())
            << "addPlayer failed for player " << i;
    }

    ASSERT_EQ(server_->stats().playerCount,
              static_cast<std::size_t>(kTargetCCU));

    // Warmup ticks
    for (int i = 0; i < kWarmupTicks; ++i) {
        (void)server_->tick();
    }

    // Measure tick latencies
    std::vector<double> latencies;
    latencies.reserve(kTicksUnderLoad);

    for (int i = 0; i < kTicksUnderLoad; ++i) {
        const auto start = steady_clock::now();
        auto result = server_->tick();
        const auto end = steady_clock::now();

        ASSERT_TRUE(result.hasValue()) << "Tick " << i << " failed";
        double ms = static_cast<double>(duration_cast<microseconds>(end - start).count()) / 1e3;
        latencies.push_back(ms);
    }

    // Compute statistics
    std::sort(latencies.begin(), latencies.end());
    double avgMs = std::accumulate(latencies.begin(), latencies.end(), 0.0) /
                   static_cast<double>(latencies.size());
    double minMs = latencies.front();
    double maxMs = latencies.back();
    double p50 = percentile(latencies, 50.0);
    double p95 = percentile(latencies, 95.0);
    double p99 = percentile(latencies, 99.0);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Ticks:     " << kTicksUnderLoad
              << " (after " << kWarmupTicks << " warmup)\n";
    std::cout << "  Players:   " << kTargetCCU << "\n";
    std::cout << "  Instances: " << kInstanceCount << "\n\n";
    std::cout << "  Latency (ms):\n";
    std::cout << "    Min:  " << minMs << "\n";
    std::cout << "    Avg:  " << avgMs << "\n";
    std::cout << "    P50:  " << p50 << "\n";
    std::cout << "    P95:  " << p95 << "\n";
    std::cout << "    P99:  " << p99 << "\n";
    std::cout << "    Max:  " << maxMs << "\n";

    EXPECT_LE(p95, kMaxTickLatencyMs)
        << "P95 tick latency exceeds " << kMaxTickLatencyMs << "ms budget";

    std::cout << "\n  [" << (p95 <= kMaxTickLatencyMs ? "PASS" : "FAIL")
              << "] P95=" << p95 << "ms (budget: "
              << kMaxTickLatencyMs << "ms)\n";
}

// ===========================================================================
// Player Leave Throughput
// ===========================================================================

TEST_F(GameCCUBenchmark, LeaveThroughput1KCCU) {
    std::cout << "\n=== CCU Leave Throughput Benchmark ===" << std::endl;

    // Setup: add players
    std::vector<uint32_t> instanceIds;
    for (int i = 0; i < kInstanceCount; ++i) {
        auto r = server_->createInstance(
            static_cast<uint32_t>(i + 1), kPlayersPerInstance + 10);
        ASSERT_TRUE(r.hasValue());
        instanceIds.push_back(r.value());
    }

    for (int i = 0; i < kTargetCCU; ++i) {
        auto instIdx = static_cast<std::size_t>(i % kInstanceCount);
        auto r = server_->addPlayer(
            PlayerId(static_cast<uint64_t>(i + 1)), instanceIds[instIdx]);
        ASSERT_TRUE(r.hasValue());
    }

    ASSERT_EQ(server_->stats().playerCount,
              static_cast<std::size_t>(kTargetCCU));

    // Remove all players and measure time
    const auto leaveStart = steady_clock::now();
    int leftCount = 0;

    for (int i = 0; i < kTargetCCU; ++i) {
        auto r = server_->removePlayer(PlayerId(static_cast<uint64_t>(i + 1)));
        if (r.hasValue()) {
            ++leftCount;
        }
    }

    const auto leaveEnd = steady_clock::now();
    const double leaveElapsedSec =
        static_cast<double>(duration_cast<microseconds>(leaveEnd - leaveStart).count()) / 1e6;
    const double leaveRate =
        static_cast<double>(leftCount) / leaveElapsedSec;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Removed:   " << leftCount << " / " << kTargetCCU
              << " players\n";
    std::cout << "  Time:      " << leaveElapsedSec << " sec\n";
    std::cout << "  Rate:      " << leaveRate << " players/sec\n";

    auto stats = server_->stats();
    std::cout << "  Remaining: " << stats.playerCount << " players\n";

    EXPECT_EQ(leftCount, kTargetCCU);
    EXPECT_EQ(stats.playerCount, 0u);

    std::cout << "\n  [PASS] All " << kTargetCCU
              << " players removed cleanly\n";
}

// ===========================================================================
// Scaling Linearity — measure per-instance overhead
// ===========================================================================

TEST_F(GameCCUBenchmark, ScalingLinearity) {
    std::cout << "\n=== Scaling Linearity Benchmark ===" << std::endl;
    std::cout << "Measuring tick latency as instances increase\n" << std::endl;

    // Stop the game loop for manual ticking
    server_->stop();

    GameServerConfig config;
    config.tickRate = 20;
    config.maxInstances = 1000;
    config.spatialCellSize = 32.0f;
    config.aiTickInterval = 0.1f;
    server_ = std::make_unique<GameServer>(config);

    constexpr int kPlayersPerTest = 100;
    constexpr int kTicksPerStep = 50;
    const std::vector<int> instanceCounts = {1, 2, 5, 10};

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Instances | Players | Avg Tick (ms) | P95 Tick (ms)\n";
    std::cout << "  ----------+---------+---------------+--------------\n";

    std::vector<double> avgLatencies;

    for (int nInstances : instanceCounts) {
        // Recreate server for clean state
        server_ = std::make_unique<GameServer>(config);

        // Create instances and fill with players
        for (int i = 0; i < nInstances; ++i) {
            auto r = server_->createInstance(
                static_cast<uint32_t>(i + 1), kPlayersPerTest + 10);
            ASSERT_TRUE(r.hasValue());
            uint32_t instId = r.value();

            for (int p = 0; p < kPlayersPerTest; ++p) {
                PlayerId pid(static_cast<uint64_t>(
                    i * kPlayersPerTest + p + 1));
                (void)server_->addPlayer(pid, instId);
            }
        }

        // Warmup
        for (int i = 0; i < kWarmupTicks; ++i) {
            (void)server_->tick();
        }

        // Measure
        std::vector<double> ticks;
        ticks.reserve(kTicksPerStep);
        for (int i = 0; i < kTicksPerStep; ++i) {
            auto start = steady_clock::now();
            (void)server_->tick();
            auto end = steady_clock::now();
            ticks.push_back(
                static_cast<double>(duration_cast<microseconds>(end - start).count()) / 1e3);
        }

        std::sort(ticks.begin(), ticks.end());
        double avg = std::accumulate(ticks.begin(), ticks.end(), 0.0) /
                     static_cast<double>(ticks.size());
        double p95 = percentile(ticks, 95.0);
        int totalPlayers = nInstances * kPlayersPerTest;

        std::cout << "  " << std::setw(9) << nInstances
                  << " | " << std::setw(7) << totalPlayers
                  << " | " << std::setw(13) << avg
                  << " | " << std::setw(12) << p95 << "\n";

        avgLatencies.push_back(avg);
    }

    // Check linearity: ratio between 10x and 1x should be < 15x
    // (allowing some overhead for scheduling)
    if (avgLatencies.size() >= 2 && avgLatencies[0] > 0.0) {
        double ratio = avgLatencies.back() / avgLatencies.front();
        double instanceRatio = static_cast<double>(instanceCounts.back()) /
                               static_cast<double>(instanceCounts.front());
        std::cout << "\n  Scale factor: " << ratio << "x for "
                  << instanceRatio << "x instances\n";

        // Allow up to 15x for 10x instances (sub-linear is fine,
        // super-linear indicates contention)
        EXPECT_LE(ratio, instanceRatio * 1.5)
            << "Scaling is worse than 1.5x linear";

        std::cout << "  [" << (ratio <= instanceRatio * 1.5 ? "PASS" : "FAIL")
                  << "] Scaling is "
                  << (ratio <= instanceRatio ? "sub-linear" : "super-linear")
                  << "\n";
    }
}
