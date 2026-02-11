/// @file memory_profiling_benchmark_test.cpp
/// @brief Memory profiling benchmark for concurrent player entities.
///
/// Measures memory usage per 1K concurrent player entities with all MMORPG
/// components attached.  Reports per-component breakdown and total memory.
///
/// SRS-NFR-005: Memory per 1K concurrent players ≤100MB.
///
/// Memory measurement approach:
///   - Theoretical: sizeof(T) × entity count for each component type
///   - Practical: Platform-specific RSS measurement (macOS mach_task_info)
///   - Both methods are reported for comprehensive analysis.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

#include "cgs/plugin/mmorpg_plugin.hpp"

using namespace cgs::plugin;
using namespace cgs::game;

namespace {

// Benchmark parameters
constexpr int kPlayerCount = 1000;
constexpr double kMaxMemoryMB = 100.0; // SRS-NFR-005

/// Get current process resident set size (RSS) in bytes.
/// Returns 0 if not available on the platform.
std::size_t getCurrentRSS() {
#ifdef __APPLE__
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    kern_return_t kr = task_info(
        mach_task_self(), MACH_TASK_BASIC_INFO,
        reinterpret_cast<task_info_t>(&info), &count);
    if (kr == KERN_SUCCESS) {
        return info.resident_size;
    }
#endif
    return 0;
}

/// Helper to format bytes as human-readable string.
std::string formatBytes(std::size_t bytes) {
    if (bytes >= 1024ULL * 1024ULL) {
        double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << mb << " MB";
        return oss.str();
    }
    if (bytes >= 1024ULL) {
        double kb = static_cast<double>(bytes) / 1024.0;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << kb << " KB";
        return oss.str();
    }
    return std::to_string(bytes) + " B";
}

} // anonymous namespace

// ===========================================================================
// Memory Profiling Fixture
// ===========================================================================

class MemoryProfilingBenchmark : public ::testing::Test {};

// ===========================================================================
// Per-Component Memory Breakdown
// ===========================================================================

TEST_F(MemoryProfilingBenchmark, ComponentMemoryBreakdown) {
    // Calculate theoretical memory per component type
    struct ComponentInfo {
        const char* name;
        std::size_t sizePerEntity;
        int count;
        std::size_t total() const {
            return sizePerEntity * static_cast<std::size_t>(count);
        }
    };

    // Player entities have these components:
    // Identity, Transform, Stats, Movement, SpellCast, AuraHolder,
    // ThreatList, MapMembership, VisibilityRange, QuestLog,
    // Inventory, Equipment
    std::vector<ComponentInfo> playerComponents = {
        {"Transform",       sizeof(Transform),       kPlayerCount},
        {"Identity",        sizeof(Identity),        kPlayerCount},
        {"Stats",           sizeof(Stats),           kPlayerCount},
        {"Movement",        sizeof(Movement),        kPlayerCount},
        {"SpellCast",       sizeof(SpellCast),       kPlayerCount},
        {"AuraHolder",      sizeof(AuraHolder),      kPlayerCount},
        {"ThreatList",      sizeof(ThreatList),      kPlayerCount},
        {"MapMembership",   sizeof(MapMembership),   kPlayerCount},
        {"VisibilityRange", sizeof(VisibilityRange), kPlayerCount},
        {"QuestLog",        sizeof(QuestLog),        kPlayerCount},
        {"Inventory",       sizeof(Inventory),       kPlayerCount},
        {"Equipment",       sizeof(Equipment),       kPlayerCount},
    };

    // Sparse-set overhead: sparse array (uint32_t per potential entity slot)
    // + entities array (uint32_t per stored entity)
    // + versions array (uint32_t per stored entity)
    std::size_t sparseOverheadPerStorage =
        static_cast<std::size_t>(kPlayerCount) * sizeof(uint32_t) * 3;
    std::size_t totalSparseOverhead =
        sparseOverheadPerStorage * playerComponents.size();

    std::size_t totalComponentMemory = 0;
    for (const auto& c : playerComponents) {
        totalComponentMemory += c.total();
    }

    // CharacterData per player (stored in unordered_map)
    std::size_t characterDataSize = sizeof(CharacterData);
    std::size_t totalCharacterData =
        characterDataSize * static_cast<std::size_t>(kPlayerCount);

    // Entity manager overhead (versions_, alive_, freeList_)
    std::size_t entityManagerOverhead =
        static_cast<std::size_t>(kPlayerCount) *
        (sizeof(uint8_t) + sizeof(bool));

    std::size_t totalTheoretical =
        totalComponentMemory + totalSparseOverhead +
        totalCharacterData + entityManagerOverhead;
    double totalMB =
        static_cast<double>(totalTheoretical) / (1024.0 * 1024.0);

    // Report per-component breakdown
    std::cout << "\n"
              << "+---------------------------------------------------+\n"
              << "|  Memory Profiling: Per-Component Breakdown         |\n"
              << "|  (SRS-NFR-005: ≤100MB per 1K players)             |\n"
              << "+---------------------------------------------------+\n"
              << "|  Players:        " << std::setw(10) << kPlayerCount
              << "                      |\n"
              << "+---------------------------------------------------+\n";

    for (const auto& c : playerComponents) {
        std::cout << "|  " << std::left << std::setw(16) << c.name
                  << std::right
                  << "  sizeof=" << std::setw(5) << c.sizePerEntity
                  << "  total=" << std::setw(10)
                  << formatBytes(c.total()) << "  |\n";
    }

    std::cout << "+---------------------------------------------------+\n"
              << "|  Component Data:  " << std::setw(10)
              << formatBytes(totalComponentMemory)
              << "                      |\n"
              << "|  Sparse Overhead: " << std::setw(10)
              << formatBytes(totalSparseOverhead)
              << "                      |\n"
              << "|  CharacterData:   " << std::setw(10)
              << formatBytes(totalCharacterData)
              << "                      |\n"
              << "|  EntityManager:   " << std::setw(10)
              << formatBytes(entityManagerOverhead)
              << "                      |\n"
              << "+---------------------------------------------------+\n"
              << "|  Total (theory):  " << std::setw(10)
              << formatBytes(totalTheoretical)
              << "                      |\n"
              << "|  Requirement:     " << std::setw(10) << std::fixed
              << std::setprecision(1) << kMaxMemoryMB
              << " MB" << "                    |\n"
              << "|  Status:          " << std::setw(10)
              << (totalMB <= kMaxMemoryMB ? "PASS" : "FAIL")
              << "                      |\n"
              << "+---------------------------------------------------+\n"
              << std::endl;

    EXPECT_LE(totalMB, kMaxMemoryMB)
        << "Theoretical memory " << totalMB
        << " MB exceeds " << kMaxMemoryMB
        << " MB requirement (SRS-NFR-005)";
}

// ===========================================================================
// Practical RSS Measurement with 1K Players
// ===========================================================================

TEST_F(MemoryProfilingBenchmark, PracticalRSSMeasurement) {
    // Measure RSS before and after creating 1K player entities
    std::size_t rssBefore = getCurrentRSS();

    MMORPGPlugin plugin;
    PluginContext ctx;
    ASSERT_TRUE(plugin.OnLoad(ctx));
    ASSERT_TRUE(plugin.OnInit());

    auto map = plugin.CreateMapInstance(1, MapType::OpenWorld);

    for (int i = 0; i < kPlayerCount; ++i) {
        Vector3 pos{static_cast<float>(i % 100),
                    0.0f,
                    static_cast<float>(i / 100)};
        auto cls = static_cast<CharacterClass>(
            static_cast<std::size_t>(i) % kCharacterClassCount);
        (void)plugin.CreateCharacter(
            "Player" + std::to_string(i), cls, pos, map);
    }

    EXPECT_EQ(plugin.PlayerCount(), static_cast<std::size_t>(kPlayerCount));

    std::size_t rssAfter = getCurrentRSS();

    // RSS delta (approximate — other allocations may occur)
    std::size_t rssDelta = 0;
    if (rssAfter > rssBefore) {
        rssDelta = rssAfter - rssBefore;
    }
    double rssDeltaMB =
        static_cast<double>(rssDelta) / (1024.0 * 1024.0);

    std::cout << "\n"
              << "+---------------------------------------------------+\n"
              << "|  Memory Profiling: RSS Measurement                 |\n"
              << "|  (SRS-NFR-005: ≤100MB per 1K players)             |\n"
              << "+---------------------------------------------------+\n"
              << "|  Players:        " << std::setw(10) << kPlayerCount
              << "                      |\n"
              << "|  RSS Before:     " << std::setw(10)
              << formatBytes(rssBefore) << "                      |\n"
              << "|  RSS After:      " << std::setw(10)
              << formatBytes(rssAfter) << "                      |\n"
              << "|  RSS Delta:      " << std::setw(10)
              << formatBytes(rssDelta) << "                      |\n"
              << "+---------------------------------------------------+\n"
              << "|  Requirement:    " << std::setw(10) << std::fixed
              << std::setprecision(1) << kMaxMemoryMB
              << " MB" << "                    |\n"
              << "|  Status:         " << std::setw(10)
              << (rssDeltaMB <= kMaxMemoryMB ? "PASS" : "FAIL")
              << "                      |\n"
              << "+---------------------------------------------------+\n"
              << std::endl;

    // RSS delta should be well within the 100MB budget
    if (rssDelta > 0) {
        EXPECT_LE(rssDeltaMB, kMaxMemoryMB)
            << "RSS delta " << rssDeltaMB
            << " MB exceeds " << kMaxMemoryMB
            << " MB requirement (SRS-NFR-005)";
    }

    plugin.OnShutdown();
    plugin.OnUnload();
}

// ===========================================================================
// Memory Scaling Test (100, 500, 1000 players)
// ===========================================================================

TEST_F(MemoryProfilingBenchmark, MemoryScalingLinear) {
    struct ScalePoint {
        int playerCount;
        std::size_t rssBefore;
        std::size_t rssAfter;
        std::size_t delta;
    };

    std::vector<int> scaleLevels = {100, 500, 1000};
    std::vector<ScalePoint> points;

    for (int count : scaleLevels) {
        std::size_t before = getCurrentRSS();

        {
            MMORPGPlugin plugin;
            PluginContext ctx;
            ASSERT_TRUE(plugin.OnLoad(ctx));
            ASSERT_TRUE(plugin.OnInit());

            auto map = plugin.CreateMapInstance(1, MapType::OpenWorld);
            for (int i = 0; i < count; ++i) {
                Vector3 pos{static_cast<float>(i % 100), 0.0f,
                            static_cast<float>(i / 100)};
                auto cls = static_cast<CharacterClass>(
                    static_cast<std::size_t>(i) % kCharacterClassCount);
                (void)plugin.CreateCharacter(
                    "P" + std::to_string(i), cls, pos, map);
            }

            std::size_t after = getCurrentRSS();
            std::size_t delta = (after > before) ? (after - before) : 0;
            points.push_back({count, before, after, delta});

            plugin.OnShutdown();
            plugin.OnUnload();
        }
    }

    std::cout << "\n"
              << "+---------------------------------------------------+\n"
              << "|  Memory Scaling Analysis                           |\n"
              << "+---------------------------------------------------+\n";

    for (const auto& p : points) {
        double perPlayerKB =
            (p.delta > 0 && p.playerCount > 0)
            ? static_cast<double>(p.delta) /
              (static_cast<double>(p.playerCount) * 1024.0)
            : 0.0;
        std::cout << "|  " << std::setw(5) << p.playerCount
                  << " players: delta=" << std::setw(10)
                  << formatBytes(p.delta)
                  << "  per-player=" << std::setw(8) << std::fixed
                  << std::setprecision(2) << perPlayerKB << " KB |\n";
    }

    // Extrapolate 1K player memory from measured data
    if (!points.empty() && points.back().delta > 0) {
        double extrapolatedMB =
            static_cast<double>(points.back().delta) / (1024.0 * 1024.0);
        std::cout << "+---------------------------------------------------+\n"
                  << "|  1K Extrapolated: " << std::setw(10) << std::fixed
                  << std::setprecision(2) << extrapolatedMB
                  << " MB" << "                    |\n";
    }

    std::cout << "|  Requirement:     " << std::setw(10) << std::fixed
              << std::setprecision(1) << kMaxMemoryMB
              << " MB" << "                    |\n"
              << "+---------------------------------------------------+\n"
              << std::endl;
}

// ===========================================================================
// Entity Destruction Memory Reclamation
// ===========================================================================

TEST_F(MemoryProfilingBenchmark, DestructionMemoryReclamation) {
    MMORPGPlugin plugin;
    PluginContext ctx;
    ASSERT_TRUE(plugin.OnLoad(ctx));
    ASSERT_TRUE(plugin.OnInit());

    auto map = plugin.CreateMapInstance(1, MapType::OpenWorld);

    // Create 1K players
    std::vector<cgs::ecs::Entity> entities;
    entities.reserve(static_cast<std::size_t>(kPlayerCount));

    for (int i = 0; i < kPlayerCount; ++i) {
        Vector3 pos{static_cast<float>(i), 0.0f, 0.0f};
        auto cls = static_cast<CharacterClass>(
            static_cast<std::size_t>(i) % kCharacterClassCount);
        entities.push_back(plugin.CreateCharacter(
            "P" + std::to_string(i), cls, pos, map));
    }

    EXPECT_EQ(plugin.PlayerCount(), static_cast<std::size_t>(kPlayerCount));
    std::size_t rssWithPlayers = getCurrentRSS();

    // Destroy all players
    for (auto e : entities) {
        plugin.RemoveCharacter(e);
    }

    EXPECT_EQ(plugin.PlayerCount(), 0u);
    std::size_t rssAfterDestroy = getCurrentRSS();

    // Verify entity manager reclaimed the slots
    EXPECT_EQ(plugin.GetEntityManager().Count(), 1u); // Only map entity left

    std::cout << "\n"
              << "+---------------------------------------------------+\n"
              << "|  Memory Reclamation After Entity Destruction       |\n"
              << "+---------------------------------------------------+\n"
              << "|  RSS With Players:   " << std::setw(12)
              << formatBytes(rssWithPlayers) << "                |\n"
              << "|  RSS After Destroy:  " << std::setw(12)
              << formatBytes(rssAfterDestroy) << "                |\n"
              << "+---------------------------------------------------+\n"
              << std::endl;

    plugin.OnShutdown();
    plugin.OnUnload();
}
