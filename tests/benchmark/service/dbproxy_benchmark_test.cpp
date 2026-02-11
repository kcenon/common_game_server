/// @file dbproxy_benchmark_test.cpp
/// @brief Performance benchmark for DBProxy query cache (SDS-MOD-034).
///
/// Measures p99 query latency for the QueryCache under concurrent load.
/// The cache layer is the critical path for read-heavy workloads
/// (template data, config data) and must meet the SRS-NFR-004 requirement
/// of p99 query latency <= 50ms.
///
/// Acceptance criterion: p99 cache query latency <= 50ms.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "cgs/foundation/game_database.hpp"
#include "cgs/service/dbproxy_types.hpp"
#include "cgs/service/query_cache.hpp"

using namespace cgs::service;
using namespace cgs::foundation;
using namespace std::chrono_literals;

namespace {

// Benchmark parameters
constexpr int kThreadCount = 8;
constexpr int kOpsPerThread = 10000;
constexpr int kTotalOps = kThreadCount * kOpsPerThread;
constexpr int kPreloadEntries = 5000;
constexpr double kMaxP99LatencyMs = 50.0;

/// Create a synthetic QueryResult with the given number of rows.
QueryResult makeResult(int rows) {
    QueryResult result;
    result.reserve(static_cast<std::size_t>(rows));
    for (int i = 0; i < rows; ++i) {
        DbRow row;
        row["id"] = static_cast<std::int64_t>(i);
        row["name"] = std::string("item_template_") + std::to_string(i);
        row["description"] = std::string("A game item used for testing "
                                          "cache performance under load");
        row["level"] = static_cast<std::int64_t>(i % 100);
        row["price"] = static_cast<double>(i * 1.5);
        row["active"] = true;
        result.push_back(std::move(row));
    }
    return result;
}

} // anonymous namespace

// ===========================================================================
// Benchmark Fixture
// ===========================================================================

class DBProxyCacheBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        CacheConfig config;
        config.maxEntries = 20000;
        config.defaultTtl = 300s;
        cache_ = std::make_unique<QueryCache>(config);

        // Preload cache with entries to simulate warm cache state.
        for (int i = 0; i < kPreloadEntries; ++i) {
            auto key = "SELECT * FROM items WHERE template_id = " +
                       std::to_string(i);
            cache_->put(key, makeResult(5));
        }
    }

    std::unique_ptr<QueryCache> cache_;
};

// ===========================================================================
// Cache Read Latency (p99)
// ===========================================================================

TEST_F(DBProxyCacheBenchmark, CacheReadP99Latency) {
    // Each thread records its own latencies to avoid contention on a
    // shared vector. Latencies are merged after all threads complete.
    std::vector<std::vector<double>> threadLatencies(
        static_cast<std::size_t>(kThreadCount));

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(kThreadCount));

    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([this, t, &threadLatencies]() {
            auto& latencies = threadLatencies[static_cast<std::size_t>(t)];
            latencies.reserve(static_cast<std::size_t>(kOpsPerThread));

            for (int i = 0; i < kOpsPerThread; ++i) {
                // Access a random preloaded entry (deterministic per thread).
                auto idx = (t * kOpsPerThread + i) % kPreloadEntries;
                auto key = "SELECT * FROM items WHERE template_id = " +
                           std::to_string(idx);

                auto start = std::chrono::high_resolution_clock::now();
                auto result = cache_->get(key);
                auto end = std::chrono::high_resolution_clock::now();

                double latencyMs =
                    std::chrono::duration<double, std::milli>(end - start)
                        .count();
                latencies.push_back(latencyMs);

                // Verify the cache hit.
                EXPECT_TRUE(result.has_value());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Merge all latencies.
    std::vector<double> allLatencies;
    allLatencies.reserve(static_cast<std::size_t>(kTotalOps));
    for (const auto& tl : threadLatencies) {
        allLatencies.insert(allLatencies.end(), tl.begin(), tl.end());
    }

    // Sort for percentile calculation.
    std::sort(allLatencies.begin(), allLatencies.end());

    // Calculate statistics.
    double minLatency = allLatencies.front();
    double maxLatency = allLatencies.back();
    double medianLatency = allLatencies[allLatencies.size() / 2];
    double p95Latency =
        allLatencies[static_cast<std::size_t>(
            static_cast<double>(allLatencies.size()) * 0.95)];
    double p99Latency =
        allLatencies[static_cast<std::size_t>(
            static_cast<double>(allLatencies.size()) * 0.99)];
    double avgLatency =
        std::accumulate(allLatencies.begin(), allLatencies.end(), 0.0) /
        static_cast<double>(allLatencies.size());

    double throughput =
        static_cast<double>(kTotalOps) /
        (std::accumulate(allLatencies.begin(), allLatencies.end(), 0.0) /
         1000.0);

    // Report results.
    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  DBProxy Cache Read Benchmark Results            |\n"
              << "+-------------------------------------------------+\n"
              << "|  Threads:       " << std::setw(10) << kThreadCount
              << "                    |\n"
              << "|  Total Ops:     " << std::setw(10) << kTotalOps
              << "                    |\n"
              << "|  Preloaded:     " << std::setw(10) << kPreloadEntries
              << " entries             |\n"
              << "+-------------------------------------------------+\n"
              << "|  Min Latency:   " << std::setw(10) << std::fixed
              << std::setprecision(4) << minLatency << " ms               |\n"
              << "|  Avg Latency:   " << std::setw(10) << avgLatency
              << " ms               |\n"
              << "|  Median:        " << std::setw(10) << medianLatency
              << " ms               |\n"
              << "|  p95 Latency:   " << std::setw(10) << p95Latency
              << " ms               |\n"
              << "|  p99 Latency:   " << std::setw(10) << p99Latency
              << " ms               |\n"
              << "|  Max Latency:   " << std::setw(10) << maxLatency
              << " ms               |\n"
              << "+-------------------------------------------------+\n"
              << "|  Throughput:    " << std::setw(10) << std::setprecision(0)
              << throughput << " ops/sec            |\n"
              << "|  Hit Rate:      " << std::setw(10) << std::setprecision(4)
              << (cache_->hitRate() * 100.0) << " %                 |\n"
              << "|  Requirement:   " << std::setw(10) << std::setprecision(1)
              << kMaxP99LatencyMs << " ms (p99)          |\n"
              << "|  Status:        " << std::setw(10)
              << (p99Latency <= kMaxP99LatencyMs ? "PASS" : "FAIL")
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_LE(p99Latency, kMaxP99LatencyMs)
        << "p99 cache read latency " << p99Latency
        << " ms exceeds the " << kMaxP99LatencyMs << " ms requirement";
}

// ===========================================================================
// Cache Write Latency (p99)
// ===========================================================================

TEST_F(DBProxyCacheBenchmark, CacheWriteP99Latency) {
    std::vector<std::vector<double>> threadLatencies(
        static_cast<std::size_t>(kThreadCount));

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(kThreadCount));

    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([this, t, &threadLatencies]() {
            auto& latencies = threadLatencies[static_cast<std::size_t>(t)];
            latencies.reserve(static_cast<std::size_t>(kOpsPerThread));

            for (int i = 0; i < kOpsPerThread; ++i) {
                auto key = "SELECT * FROM items WHERE id = " +
                           std::to_string(t * kOpsPerThread + i +
                                          kPreloadEntries);
                auto result = makeResult(3);

                auto start = std::chrono::high_resolution_clock::now();
                cache_->put(key, result);
                auto end = std::chrono::high_resolution_clock::now();

                double latencyMs =
                    std::chrono::duration<double, std::milli>(end - start)
                        .count();
                latencies.push_back(latencyMs);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Merge and sort.
    std::vector<double> allLatencies;
    allLatencies.reserve(static_cast<std::size_t>(kTotalOps));
    for (const auto& tl : threadLatencies) {
        allLatencies.insert(allLatencies.end(), tl.begin(), tl.end());
    }
    std::sort(allLatencies.begin(), allLatencies.end());

    double p99Latency =
        allLatencies[static_cast<std::size_t>(
            static_cast<double>(allLatencies.size()) * 0.99)];
    double avgLatency =
        std::accumulate(allLatencies.begin(), allLatencies.end(), 0.0) /
        static_cast<double>(allLatencies.size());

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  DBProxy Cache Write Benchmark Results           |\n"
              << "+-------------------------------------------------+\n"
              << "|  Avg Latency:   " << std::setw(10) << std::fixed
              << std::setprecision(4) << avgLatency << " ms               |\n"
              << "|  p99 Latency:   " << std::setw(10) << p99Latency
              << " ms               |\n"
              << "|  Requirement:   " << std::setw(10) << std::setprecision(1)
              << kMaxP99LatencyMs << " ms (p99)          |\n"
              << "|  Status:        " << std::setw(10)
              << (p99Latency <= kMaxP99LatencyMs ? "PASS" : "FAIL")
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_LE(p99Latency, kMaxP99LatencyMs)
        << "p99 cache write latency " << p99Latency
        << " ms exceeds the " << kMaxP99LatencyMs << " ms requirement";
}

// ===========================================================================
// Mixed Read/Write Under Contention
// ===========================================================================

TEST_F(DBProxyCacheBenchmark, MixedReadWriteP99Latency) {
    // 80% reads, 20% writes — typical read-heavy workload.
    std::vector<std::vector<double>> threadLatencies(
        static_cast<std::size_t>(kThreadCount));

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(kThreadCount));

    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([this, t, &threadLatencies]() {
            auto& latencies = threadLatencies[static_cast<std::size_t>(t)];
            latencies.reserve(static_cast<std::size_t>(kOpsPerThread));

            for (int i = 0; i < kOpsPerThread; ++i) {
                bool isRead = (i % 5 != 0); // 80% reads.

                auto start = std::chrono::high_resolution_clock::now();

                if (isRead) {
                    auto idx = (t * kOpsPerThread + i) % kPreloadEntries;
                    auto key = "SELECT * FROM items WHERE template_id = " +
                               std::to_string(idx);
                    (void)cache_->get(key);
                } else {
                    auto key = "SELECT * FROM writes WHERE id = " +
                               std::to_string(t * kOpsPerThread + i);
                    cache_->put(key, makeResult(2));
                }

                auto end = std::chrono::high_resolution_clock::now();

                double latencyMs =
                    std::chrono::duration<double, std::milli>(end - start)
                        .count();
                latencies.push_back(latencyMs);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Merge and sort.
    std::vector<double> allLatencies;
    allLatencies.reserve(static_cast<std::size_t>(kTotalOps));
    for (const auto& tl : threadLatencies) {
        allLatencies.insert(allLatencies.end(), tl.begin(), tl.end());
    }
    std::sort(allLatencies.begin(), allLatencies.end());

    double p99Latency =
        allLatencies[static_cast<std::size_t>(
            static_cast<double>(allLatencies.size()) * 0.99)];
    double avgLatency =
        std::accumulate(allLatencies.begin(), allLatencies.end(), 0.0) /
        static_cast<double>(allLatencies.size());

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  DBProxy Mixed R/W Benchmark Results             |\n"
              << "+-------------------------------------------------+\n"
              << "|  Mix:           " << std::setw(10) << "80/20"
              << " read/write          |\n"
              << "|  Avg Latency:   " << std::setw(10) << std::fixed
              << std::setprecision(4) << avgLatency << " ms               |\n"
              << "|  p99 Latency:   " << std::setw(10) << p99Latency
              << " ms               |\n"
              << "|  Requirement:   " << std::setw(10) << std::setprecision(1)
              << kMaxP99LatencyMs << " ms (p99)          |\n"
              << "|  Status:        " << std::setw(10)
              << (p99Latency <= kMaxP99LatencyMs ? "PASS" : "FAIL")
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_LE(p99Latency, kMaxP99LatencyMs)
        << "p99 mixed R/W latency " << p99Latency
        << " ms exceeds the " << kMaxP99LatencyMs << " ms requirement";
}
