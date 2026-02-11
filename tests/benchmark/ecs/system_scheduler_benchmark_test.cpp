/// @file system_scheduler_benchmark_test.cpp
/// @brief Performance benchmark for SystemScheduler and full world tick.
///
/// Measures staged system execution with 6 game systems operating on 10K
/// entities.  This directly validates the SRS-NFR-002 requirement: world
/// tick latency ≤50ms (20 Hz server tick rate).
///
/// The benchmark uses MMORPGPlugin as the integration point since it owns
/// all ECS infrastructure and registers all 6 game systems.
///
/// Acceptance criteria:
///   - SRS-NFR-002: Full world tick ≤50ms
///   - SRS-NFR-003: 10K entity update within a single tick ≤5ms

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "cgs/game/ai_system.hpp"
#include "cgs/game/combat_system.hpp"
#include "cgs/game/inventory_system.hpp"
#include "cgs/game/object_system.hpp"
#include "cgs/game/quest_system.hpp"
#include "cgs/game/world_system.hpp"
#include "cgs/plugin/mmorpg_plugin.hpp"

using namespace cgs::plugin;

namespace {

// Benchmark parameters
constexpr int kPlayerCount = 1000;
constexpr int kCreatureCount = 9000;
constexpr int kTotalEntities = kPlayerCount + kCreatureCount;
constexpr int kTickIterations = 200;
constexpr int kWarmupTicks = 20;
constexpr float kDeltaTime = 0.05f; // 20 Hz tick (50ms budget)
constexpr double kMaxWorldTickMs = 50.0; // SRS-NFR-002

} // anonymous namespace

// ===========================================================================
// Benchmark Fixture
// ===========================================================================

class SystemSchedulerBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        PluginContext ctx;
        ASSERT_TRUE(plugin_.OnLoad(ctx));
        ASSERT_TRUE(plugin_.OnInit());

        // Create a world map
        mapEntity_ = plugin_.CreateMapInstance(1, cgs::game::MapType::OpenWorld);

        // Spawn players and creatures to reach 10K entities
        for (int i = 0; i < kPlayerCount; ++i) {
            cgs::game::Vector3 pos{static_cast<float>(i % 100),
                                    0.0f,
                                    static_cast<float>(i / 100)};
            auto cls = static_cast<CharacterClass>(
                static_cast<std::size_t>(i) % kCharacterClassCount);
            (void)plugin_.CreateCharacter(
                "Player" + std::to_string(i), cls, pos, mapEntity_);
        }

        for (int i = 0; i < kCreatureCount; ++i) {
            cgs::game::Vector3 pos{static_cast<float>(i % 200),
                                    0.0f,
                                    static_cast<float>(i / 200)};
            (void)plugin_.SpawnCreature(
                static_cast<uint32_t>(i + 1),
                "Creature" + std::to_string(i),
                pos, mapEntity_);
        }
    }

    void TearDown() override {
        plugin_.OnShutdown();
        plugin_.OnUnload();
    }

    MMORPGPlugin plugin_;
    cgs::ecs::Entity mapEntity_;
};

// ===========================================================================
// Full World Tick Latency (SRS-NFR-002)
// ===========================================================================

TEST_F(SystemSchedulerBenchmark, WorldTickLatency) {
    // Warmup: let caches, branch predictors, and allocators stabilize
    for (int i = 0; i < kWarmupTicks; ++i) {
        plugin_.OnUpdate(kDeltaTime);
    }

    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(kTickIterations));

    for (int iter = 0; iter < kTickIterations; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();
        plugin_.OnUpdate(kDeltaTime);
        auto end = std::chrono::high_resolution_clock::now();

        double ms =
            std::chrono::duration<double, std::milli>(end - start).count();
        latencies.push_back(ms);
    }

    std::sort(latencies.begin(), latencies.end());

    double minMs = latencies.front();
    double maxMs = latencies.back();
    double medianMs = latencies[latencies.size() / 2];
    double avgMs =
        std::accumulate(latencies.begin(), latencies.end(), 0.0) /
        static_cast<double>(latencies.size());
    double p95Ms =
        latencies[static_cast<std::size_t>(
            static_cast<double>(latencies.size()) * 0.95)];
    double p99Ms =
        latencies[static_cast<std::size_t>(
            static_cast<double>(latencies.size()) * 0.99)];

    double tickRate = 1000.0 / medianMs;

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  World Tick Benchmark (SRS-NFR-002)              |\n"
              << "|  6 Systems × " << std::setw(5) << kTotalEntities
              << " Entities                    |\n"
              << "+-------------------------------------------------+\n"
              << "|  Players:       " << std::setw(10) << kPlayerCount
              << "                    |\n"
              << "|  Creatures:     " << std::setw(10) << kCreatureCount
              << "                    |\n"
              << "|  Tick Count:    " << std::setw(10) << kTickIterations
              << "                    |\n"
              << "|  Delta Time:    " << std::setw(10) << std::fixed
              << std::setprecision(3) << kDeltaTime << " sec              |\n"
              << "+-------------------------------------------------+\n"
              << "|  Min Latency:   " << std::setw(10) << std::setprecision(4)
              << minMs << " ms               |\n"
              << "|  Avg Latency:   " << std::setw(10) << avgMs
              << " ms               |\n"
              << "|  Median:        " << std::setw(10) << medianMs
              << " ms               |\n"
              << "|  p95 Latency:   " << std::setw(10) << p95Ms
              << " ms               |\n"
              << "|  p99 Latency:   " << std::setw(10) << p99Ms
              << " ms               |\n"
              << "|  Max Latency:   " << std::setw(10) << maxMs
              << " ms               |\n"
              << "+-------------------------------------------------+\n"
              << "|  Tick Rate:     " << std::setw(10) << std::setprecision(1)
              << tickRate << " Hz                |\n"
              << "|  Requirement:   " << std::setw(10) << kMaxWorldTickMs
              << " ms (≥20 Hz)       |\n"
              << "|  Status:        " << std::setw(10)
              << (p99Ms <= kMaxWorldTickMs ? "PASS" : "FAIL")
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_LE(p99Ms, kMaxWorldTickMs)
        << "p99 world tick latency " << p99Ms
        << " ms exceeds the " << kMaxWorldTickMs
        << " ms requirement (SRS-NFR-002) with " << kTotalEntities
        << " entities";
}

// ===========================================================================
// Scheduler Build Time
// ===========================================================================

TEST_F(SystemSchedulerBenchmark, SchedulerBuildTime) {
    // Measure how long it takes to build the execution plan
    constexpr int kBuildIterations = 1000;

    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(kBuildIterations));

    for (int iter = 0; iter < kBuildIterations; ++iter) {
        // Create a fresh scheduler and register systems
        cgs::ecs::SystemScheduler scheduler;
        cgs::ecs::ComponentStorage<cgs::game::Transform> transforms;
        cgs::ecs::ComponentStorage<cgs::game::Movement> movements;
        cgs::ecs::ComponentStorage<cgs::game::MapMembership> mapMemberships;
        cgs::ecs::ComponentStorage<cgs::game::MapInstance> mapInstances;
        cgs::ecs::ComponentStorage<cgs::game::VisibilityRange> visibilityRanges;
        cgs::ecs::ComponentStorage<cgs::game::Zone> zones;
        cgs::ecs::ComponentStorage<cgs::game::SpellCast> spellCasts;
        cgs::ecs::ComponentStorage<cgs::game::AuraHolder> auraHolders;
        cgs::ecs::ComponentStorage<cgs::game::DamageEvent> damageEvents;
        cgs::ecs::ComponentStorage<cgs::game::Stats> stats;
        cgs::ecs::ComponentStorage<cgs::game::ThreatList> threatLists;
        cgs::ecs::ComponentStorage<cgs::game::AIBrain> aiBrains;
        cgs::ecs::ComponentStorage<cgs::game::QuestLog> questLogs;
        cgs::ecs::ComponentStorage<cgs::game::QuestEvent> questEvents;
        cgs::ecs::ComponentStorage<cgs::game::Inventory> inventories;
        cgs::ecs::ComponentStorage<cgs::game::Equipment> equipment;
        cgs::ecs::ComponentStorage<cgs::game::DurabilityEvent> durabilityEvents;

        scheduler.Register<cgs::game::WorldSystem>(
            transforms, mapMemberships, mapInstances,
            visibilityRanges, zones);
        scheduler.Register<cgs::game::ObjectUpdateSystem>(
            transforms, movements);
        scheduler.Register<cgs::game::CombatSystem>(
            spellCasts, auraHolders, damageEvents, stats, threatLists);
        scheduler.Register<cgs::game::AISystem>(
            aiBrains, transforms, movements, stats, threatLists);
        scheduler.Register<cgs::game::QuestSystem>(
            questLogs, questEvents);
        scheduler.Register<cgs::game::InventorySystem>(
            inventories, equipment, durabilityEvents);

        auto start = std::chrono::high_resolution_clock::now();
        bool ok = scheduler.Build();
        auto end = std::chrono::high_resolution_clock::now();

        ASSERT_TRUE(ok);
        double ms =
            std::chrono::duration<double, std::milli>(end - start).count();
        latencies.push_back(ms);
    }

    std::sort(latencies.begin(), latencies.end());

    double medianMs = latencies[latencies.size() / 2];
    double p99Ms =
        latencies[static_cast<std::size_t>(
            static_cast<double>(latencies.size()) * 0.99)];
    double avgMs =
        std::accumulate(latencies.begin(), latencies.end(), 0.0) /
        static_cast<double>(latencies.size());

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  Scheduler Build Benchmark                       |\n"
              << "|  (6 Systems, Topological Sort)                   |\n"
              << "+-------------------------------------------------+\n"
              << "|  Iterations:    " << std::setw(10) << kBuildIterations
              << "                    |\n"
              << "|  Avg:           " << std::setw(10) << std::fixed
              << std::setprecision(4) << avgMs << " ms               |\n"
              << "|  Median:        " << std::setw(10) << medianMs
              << " ms               |\n"
              << "|  p99:           " << std::setw(10) << p99Ms
              << " ms               |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;
}

// ===========================================================================
// Sustained Tick Rate Stability
// ===========================================================================

TEST_F(SystemSchedulerBenchmark, SustainedTickRateStability) {
    // Run 1000 ticks and check that no tick exceeds the budget
    constexpr int kSustainedTicks = 1000;
    constexpr int kSustainedWarmup = 50;

    // Warmup
    for (int i = 0; i < kSustainedWarmup; ++i) {
        plugin_.OnUpdate(kDeltaTime);
    }

    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(kSustainedTicks));
    int overBudget = 0;

    for (int iter = 0; iter < kSustainedTicks; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();
        plugin_.OnUpdate(kDeltaTime);
        auto end = std::chrono::high_resolution_clock::now();

        double ms =
            std::chrono::duration<double, std::milli>(end - start).count();
        latencies.push_back(ms);

        if (ms > kMaxWorldTickMs) {
            ++overBudget;
        }
    }

    std::sort(latencies.begin(), latencies.end());

    double medianMs = latencies[latencies.size() / 2];
    double p99Ms =
        latencies[static_cast<std::size_t>(
            static_cast<double>(latencies.size()) * 0.99)];
    double maxMs = latencies.back();
    double overBudgetPct =
        static_cast<double>(overBudget) /
        static_cast<double>(kSustainedTicks) * 100.0;

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  Sustained Tick Rate Stability                   |\n"
              << "|  (1000 Consecutive Ticks)                        |\n"
              << "+-------------------------------------------------+\n"
              << "|  Total Ticks:   " << std::setw(10) << kSustainedTicks
              << "                    |\n"
              << "|  Median:        " << std::setw(10) << std::fixed
              << std::setprecision(4) << medianMs << " ms               |\n"
              << "|  p99:           " << std::setw(10) << p99Ms
              << " ms               |\n"
              << "|  Max:           " << std::setw(10) << maxMs
              << " ms               |\n"
              << "|  Over Budget:   " << std::setw(10) << std::setprecision(1)
              << overBudgetPct << " %                 |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    // Allow up to 1% of ticks to be over budget (jitter tolerance)
    EXPECT_LE(overBudgetPct, 1.0)
        << overBudget << " out of " << kSustainedTicks
        << " ticks exceeded the " << kMaxWorldTickMs << " ms budget";
}
