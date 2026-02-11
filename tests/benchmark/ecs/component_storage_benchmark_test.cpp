/// @file component_storage_benchmark_test.cpp
/// @brief Performance benchmark for ComponentStorage<T> sparse-set operations.
///
/// Measures Add, Get, iteration, and Remove throughput for 10K+ entities.
/// The sparse-set is the foundation of all ECS data access; its performance
/// directly determines entity update latency (SRS-NFR-003: 10K entities ≤5ms).
///
/// Acceptance criterion: 10K entity component iteration ≤5ms.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/game/components.hpp"

using namespace cgs::ecs;
using namespace cgs::game;

namespace {

// Benchmark parameters
constexpr int kEntityCount = 10000;
constexpr int kIterations = 100;
constexpr double kMaxIterationMs = 5.0; // SRS-NFR-003: ≤5ms for 10K entities

} // anonymous namespace

// ===========================================================================
// Benchmark Fixture
// ===========================================================================

class ComponentStorageBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        entities_.reserve(static_cast<std::size_t>(kEntityCount));
        for (int i = 0; i < kEntityCount; ++i) {
            entities_.push_back(manager_.Create());
        }
    }

    EntityManager manager_;
    std::vector<Entity> entities_;
};

// ===========================================================================
// Add Throughput
// ===========================================================================

TEST_F(ComponentStorageBenchmark, AddThroughput) {
    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(kIterations));

    for (int iter = 0; iter < kIterations; ++iter) {
        ComponentStorage<Transform> storage;

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < kEntityCount; ++i) {
            storage.Add(entities_[static_cast<std::size_t>(i)],
                        Transform{{static_cast<float>(i), 0.0f, 0.0f}, {}, {}});
        }

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
    double p99Ms =
        latencies[static_cast<std::size_t>(
            static_cast<double>(latencies.size()) * 0.99)];

    double opsPerSec = static_cast<double>(kEntityCount) / (medianMs / 1000.0);

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  ComponentStorage Add Benchmark                  |\n"
              << "+-------------------------------------------------+\n"
              << "|  Entities:      " << std::setw(10) << kEntityCount
              << "                    |\n"
              << "|  Iterations:    " << std::setw(10) << kIterations
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << "|  Min:           " << std::setw(10) << std::fixed
              << std::setprecision(4) << minMs << " ms               |\n"
              << "|  Avg:           " << std::setw(10) << avgMs
              << " ms               |\n"
              << "|  Median:        " << std::setw(10) << medianMs
              << " ms               |\n"
              << "|  p99:           " << std::setw(10) << p99Ms
              << " ms               |\n"
              << "|  Max:           " << std::setw(10) << maxMs
              << " ms               |\n"
              << "+-------------------------------------------------+\n"
              << "|  Throughput:    " << std::setw(10) << std::setprecision(0)
              << opsPerSec << " adds/sec           |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_LE(medianMs, kMaxIterationMs)
        << "Median add time " << medianMs << " ms exceeds " << kMaxIterationMs
        << " ms for " << kEntityCount << " entities";
}

// ===========================================================================
// Iteration Throughput (SRS-NFR-003 Critical Path)
// ===========================================================================

TEST_F(ComponentStorageBenchmark, IterationThroughput10K) {
    ComponentStorage<Transform> transforms;
    ComponentStorage<Movement> movements;

    for (int i = 0; i < kEntityCount; ++i) {
        auto e = entities_[static_cast<std::size_t>(i)];
        transforms.Add(e, Transform{{static_cast<float>(i), 0.0f, 0.0f}, {}, {}});
        Movement mov;
        mov.speed = 5.0f;
        mov.baseSpeed = 5.0f;
        mov.direction = {1.0f, 0.0f, 0.0f};
        movements.Add(e, std::move(mov));
    }

    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(kIterations));

    for (int iter = 0; iter < kIterations; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();

        // Simulate ObjectUpdateSystem: iterate all transforms and apply movement
        for (std::size_t i = 0; i < transforms.Size(); ++i) {
            auto entityId = transforms.EntityAt(i);
            Entity entity(entityId, 0); // version unused for Has() check via id
            // Direct dense array access for cache-friendly iteration
            auto& transform = *(transforms.begin() + static_cast<std::ptrdiff_t>(i));
            auto& movement = *(movements.begin() + static_cast<std::ptrdiff_t>(i));

            float dt = 0.016f; // 60 FPS delta
            transform.position.x += movement.direction.x * movement.speed * dt;
            transform.position.y += movement.direction.y * movement.speed * dt;
            transform.position.z += movement.direction.z * movement.speed * dt;
        }

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
    double p99Ms =
        latencies[static_cast<std::size_t>(
            static_cast<double>(latencies.size()) * 0.99)];

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  ComponentStorage Iteration Benchmark            |\n"
              << "|  (SRS-NFR-003: 10K entity update ≤5ms)          |\n"
              << "+-------------------------------------------------+\n"
              << "|  Entities:      " << std::setw(10) << kEntityCount
              << "                    |\n"
              << "|  Iterations:    " << std::setw(10) << kIterations
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << "|  Min:           " << std::setw(10) << std::fixed
              << std::setprecision(4) << minMs << " ms               |\n"
              << "|  Avg:           " << std::setw(10) << avgMs
              << " ms               |\n"
              << "|  Median:        " << std::setw(10) << medianMs
              << " ms               |\n"
              << "|  p99:           " << std::setw(10) << p99Ms
              << " ms               |\n"
              << "|  Max:           " << std::setw(10) << maxMs
              << " ms               |\n"
              << "+-------------------------------------------------+\n"
              << "|  Requirement:   " << std::setw(10) << std::setprecision(1)
              << kMaxIterationMs << " ms (SRS-NFR-003)  |\n"
              << "|  Status:        " << std::setw(10)
              << (p99Ms <= kMaxIterationMs ? "PASS" : "FAIL")
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_LE(p99Ms, kMaxIterationMs)
        << "p99 iteration time " << p99Ms << " ms exceeds the "
        << kMaxIterationMs << " ms requirement (SRS-NFR-003) for "
        << kEntityCount << " entities";
}

// ===========================================================================
// Random Access (Get) Throughput
// ===========================================================================

TEST_F(ComponentStorageBenchmark, RandomAccessThroughput) {
    ComponentStorage<Stats> stats;

    for (int i = 0; i < kEntityCount; ++i) {
        Stats s;
        s.health = 100;
        s.maxHealth = 100;
        s.mana = 50;
        s.maxMana = 50;
        stats.Add(entities_[static_cast<std::size_t>(i)], std::move(s));
    }

    // Create a shuffled access order to test random access pattern
    std::vector<std::size_t> accessOrder(static_cast<std::size_t>(kEntityCount));
    std::iota(accessOrder.begin(), accessOrder.end(), 0);
    // Deterministic shuffle using simple index swapping
    for (std::size_t i = accessOrder.size() - 1; i > 0; --i) {
        std::size_t j = (i * 2654435761ULL) % (i + 1);
        std::swap(accessOrder[i], accessOrder[j]);
    }

    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(kIterations));

    for (int iter = 0; iter < kIterations; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < kEntityCount; ++i) {
            auto idx = accessOrder[static_cast<std::size_t>(i)];
            auto& s = stats.Get(entities_[idx]);
            s.SetHealth(s.health - 1);
        }

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

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  ComponentStorage Random Access Benchmark        |\n"
              << "+-------------------------------------------------+\n"
              << "|  Entities:      " << std::setw(10) << kEntityCount
              << "                    |\n"
              << "|  Median:        " << std::setw(10) << std::fixed
              << std::setprecision(4) << medianMs << " ms               |\n"
              << "|  p99:           " << std::setw(10) << p99Ms
              << " ms               |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_LE(p99Ms, kMaxIterationMs)
        << "p99 random access time " << p99Ms << " ms exceeds " << kMaxIterationMs
        << " ms for " << kEntityCount << " entities";
}

// ===========================================================================
// Remove Throughput
// ===========================================================================

TEST_F(ComponentStorageBenchmark, RemoveThroughput) {
    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(kIterations));

    for (int iter = 0; iter < kIterations; ++iter) {
        ComponentStorage<Transform> storage;
        for (int i = 0; i < kEntityCount; ++i) {
            storage.Add(entities_[static_cast<std::size_t>(i)],
                        Transform{{static_cast<float>(i), 0.0f, 0.0f}, {}, {}});
        }

        auto start = std::chrono::high_resolution_clock::now();

        // Remove in reverse order (worst case for swap-with-last strategy)
        for (int i = kEntityCount - 1; i >= 0; --i) {
            storage.Remove(entities_[static_cast<std::size_t>(i)]);
        }

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

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  ComponentStorage Remove Benchmark               |\n"
              << "+-------------------------------------------------+\n"
              << "|  Entities:      " << std::setw(10) << kEntityCount
              << "                    |\n"
              << "|  Median:        " << std::setw(10) << std::fixed
              << std::setprecision(4) << medianMs << " ms               |\n"
              << "|  p99:           " << std::setw(10) << p99Ms
              << " ms               |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_LE(p99Ms, kMaxIterationMs)
        << "p99 remove time " << p99Ms << " ms exceeds " << kMaxIterationMs
        << " ms for " << kEntityCount << " entities";
}

// ===========================================================================
// Entity Create/Destroy Lifecycle Throughput
// ===========================================================================

TEST_F(ComponentStorageBenchmark, EntityLifecycleThroughput) {
    ComponentStorage<Transform> transforms;
    ComponentStorage<Stats> stats;
    manager_.RegisterStorage(&transforms);
    manager_.RegisterStorage(&stats);

    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(kIterations));

    for (int iter = 0; iter < kIterations; ++iter) {
        std::vector<Entity> created;
        created.reserve(static_cast<std::size_t>(kEntityCount));

        auto start = std::chrono::high_resolution_clock::now();

        // Create entities with components
        for (int i = 0; i < kEntityCount; ++i) {
            auto e = manager_.Create();
            transforms.Add(e, Transform{{static_cast<float>(i), 0.0f, 0.0f}, {}, {}});
            Stats s;
            s.health = 100;
            s.maxHealth = 100;
            stats.Add(e, std::move(s));
            created.push_back(e);
        }

        // Destroy all (triggers component cleanup)
        for (auto e : created) {
            manager_.Destroy(e);
        }

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
    double opsPerSec =
        static_cast<double>(kEntityCount * 2) / (medianMs / 1000.0);

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  Entity Lifecycle Benchmark                      |\n"
              << "|  (Create + 2 Components + Destroy)               |\n"
              << "+-------------------------------------------------+\n"
              << "|  Entities:      " << std::setw(10) << kEntityCount
              << "                    |\n"
              << "|  Median:        " << std::setw(10) << std::fixed
              << std::setprecision(4) << medianMs << " ms               |\n"
              << "|  p99:           " << std::setw(10) << p99Ms
              << " ms               |\n"
              << "|  Throughput:    " << std::setw(10) << std::setprecision(0)
              << opsPerSec << " lifecycle/sec      |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;
}
