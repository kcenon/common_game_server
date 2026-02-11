/// @file network_throughput_test.cpp
/// @brief Throughput benchmark for GameNetworkManager (SDS-MOD-004).
///
/// Measures message dispatch throughput using one-way send over TCP loopback.
/// The server-side handler counts received messages (avoids TCP coalescing
/// issues that affect echo-based counting on the client side).
/// Acceptance criterion: >= 305,000 msg/sec.

#include <gtest/gtest.h>

#include "cgs/foundation/game_network_manager.hpp"
#include "cgs/foundation/network_adapter.hpp"

// kcenon client facades
#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/interfaces/connection_observer.h>
#include <kcenon/network/interfaces/i_protocol_client.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

using namespace cgs::foundation;
using namespace kcenon::network;

namespace {

constexpr uint16_t kBenchmarkPort = 19010;
constexpr uint16_t kBenchOpcode = 0x01;
constexpr auto kConnectTimeout = std::chrono::seconds(5);
constexpr auto kStartupDelay = std::chrono::milliseconds(200);

// Number of messages for benchmark
constexpr std::size_t kWarmupCount = 1000;
constexpr std::size_t kBenchmarkCount = 100000;

// Minimum acceptable throughput (msg/sec)
constexpr double kMinThroughput = 305000.0;

} // anonymous namespace

// ===========================================================================
// Benchmark Fixture
// ===========================================================================

class NetworkThroughputTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr_ = std::make_unique<GameNetworkManager>();
    }

    void TearDown() override {
        if (mgr_) {
            mgr_->stopAll();
        }
    }

    std::unique_ptr<GameNetworkManager> mgr_;
};

// ===========================================================================
// TCP Throughput Benchmark (server-side counting)
// ===========================================================================

TEST_F(NetworkThroughputTest, TCPMessageThroughput) {
    // Server-side counter: counts messages dispatched by the handler.
    // This avoids TCP stream coalescing issues (multiple serialized
    // messages delivered in a single on_receive callback).
    std::atomic<std::size_t> serverRecvCount{0};
    std::mutex countMutex;
    std::condition_variable countCV;

    mgr_->registerHandler(kBenchOpcode,
        [&](SessionId /*sid*/, const NetworkMessage& /*msg*/) {
            auto prev = serverRecvCount.fetch_add(1, std::memory_order_relaxed);
            // Notify at strategic points to reduce CV overhead
            auto current = prev + 1;
            if (current == kWarmupCount ||
                current == kWarmupCount + kBenchmarkCount) {
                std::lock_guard<std::mutex> lock(countMutex);
                countCV.notify_all();
            }
        });

    auto listenResult = mgr_->listen(kBenchmarkPort, Protocol::TCP);
    ASSERT_TRUE(listenResult.hasValue())
        << "Failed to listen on port " << kBenchmarkPort;

    std::this_thread::sleep_for(kStartupDelay);

    // Create client — TCP facade auto-starts the connection
    facade::tcp_facade tcp;
    facade::tcp_facade::client_config clientCfg{};
    clientCfg.host = "127.0.0.1";
    clientCfg.port = kBenchmarkPort;
    clientCfg.client_id = "bench-client";
    auto client = tcp.create_client(clientCfg);

    // Attach observer (connection already started by facade)
    std::promise<void> connectedPromise;
    auto connectedFuture = connectedPromise.get_future();
    std::once_flag connectedOnce;

    auto adapter = std::make_shared<interfaces::callback_adapter>();
    adapter->on_connected([&]() {
        std::call_once(connectedOnce, [&] {
            connectedPromise.set_value();
        });
    }).on_receive([](std::span<const uint8_t> /*data*/) {
    }).on_disconnected([](std::optional<std::string_view> /*reason*/) {
    }).on_error([](std::error_code /*ec*/) {
    });

    client->set_observer(adapter);

    // Wait for connection (handle race if already connected)
    if (!client->is_connected()) {
        ASSERT_EQ(connectedFuture.wait_for(kConnectTimeout), std::future_status::ready)
            << "Connection timed out";
    }

    // Build a small payload (~64 bytes total wire size)
    NetworkMessage msg;
    msg.opcode = kBenchOpcode;
    msg.payload.resize(58, 0xAB);  // 6 header + 58 payload = 64 bytes total
    auto wireData = msg.serialize();

    // ── Warmup phase ─────────────────────────────────────────────────────────
    for (std::size_t i = 0; i < kWarmupCount; ++i) {
        auto copy = wireData;
        (void)client->send(std::move(copy));
    }

    // Wait for server to process all warmup messages
    {
        std::unique_lock<std::mutex> lock(countMutex);
        auto ok = countCV.wait_for(lock, std::chrono::seconds(10),
            [&] { return serverRecvCount.load(std::memory_order_relaxed) >= kWarmupCount; });
        ASSERT_TRUE(ok) << "Warmup timed out. Server received "
                        << serverRecvCount.load() << "/" << kWarmupCount;
    }

    // ── Benchmark phase ──────────────────────────────────────────────────────
    // Reset the counter for accurate measurement
    serverRecvCount.store(kWarmupCount, std::memory_order_relaxed);
    const std::size_t targetCount = kWarmupCount + kBenchmarkCount;

    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < kBenchmarkCount; ++i) {
        auto copy = wireData;
        (void)client->send(std::move(copy));
    }

    // Wait for server to process all benchmark messages
    {
        std::unique_lock<std::mutex> lock(countMutex);
        auto ok = countCV.wait_for(lock, std::chrono::seconds(60),
            [&] { return serverRecvCount.load(std::memory_order_relaxed) >= targetCount; });
        ASSERT_TRUE(ok) << "Benchmark timed out. Server received "
                        << serverRecvCount.load() - kWarmupCount << "/" << kBenchmarkCount;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start);

    // Throughput: messages dispatched per second (send + server-side dispatch)
    double throughput = static_cast<double>(kBenchmarkCount) / elapsed.count();

    // Report results
    std::cout << "\n"
              << "┌─────────────────────────────────────────┐\n"
              << "│  Network Throughput Benchmark Results    │\n"
              << "├─────────────────────────────────────────┤\n"
              << "│  Messages:     " << std::setw(10) << kBenchmarkCount << " dispatched   │\n"
              << "│  Payload:      " << std::setw(10) << wireData.size() << " bytes        │\n"
              << "│  Elapsed:      " << std::setw(10) << std::fixed << std::setprecision(3)
              << elapsed.count() << " sec          │\n"
              << "│  Throughput:   " << std::setw(10) << std::fixed << std::setprecision(0)
              << throughput << " msg/sec      │\n"
              << "│  Requirement:  " << std::setw(10) << std::fixed << std::setprecision(0)
              << kMinThroughput << " msg/sec      │\n"
              << "│  Status:       " << std::setw(10)
              << (throughput >= kMinThroughput ? "PASS" : "FAIL")
              << "               │\n"
              << "└─────────────────────────────────────────┘\n"
              << std::endl;

    EXPECT_GE(throughput, kMinThroughput)
        << "Throughput " << throughput << " msg/sec is below the required "
        << kMinThroughput << " msg/sec";

    (void)client->stop();
}
