#include <gtest/gtest.h>

#include "cgs/ecs/entity.hpp"
#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/types.hpp"
#include "cgs/service/game_server.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

using namespace cgs::service;
using cgs::foundation::ErrorCode;
using cgs::foundation::PlayerId;

// =============================================================================
// Integration fixture: full GameServer lifecycle
// =============================================================================

class GameServerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        GameServerConfig config;
        config.tickRate = 20;
        config.maxInstances = 100;
        config.spatialCellSize = 32.0f;
        config.aiTickInterval = 0.1f;
        server_ = std::make_unique<GameServer>(std::move(config));
    }

    static PlayerId pid(uint64_t id) { return PlayerId(id); }

    std::unique_ptr<GameServer> server_;
};

// =============================================================================
// Multi-player world simulation
// =============================================================================

TEST_F(GameServerIntegrationTest, MultiPlayerWorldSimulation) {
    constexpr int kPlayers = 20;
    constexpr int kTicks = 50;

    // Create two map instances.
    auto inst1 = server_->createInstance(1, 50);
    auto inst2 = server_->createInstance(2, 50);
    ASSERT_TRUE(inst1.hasValue());
    ASSERT_TRUE(inst2.hasValue());

    // Add players split across two instances.
    for (int i = 1; i <= kPlayers; ++i) {
        uint32_t target = (i <= kPlayers / 2) ? inst1.value() : inst2.value();
        auto result = server_->addPlayer(pid(static_cast<uint64_t>(i)), target);
        ASSERT_TRUE(result.hasValue())
            << "Failed to add player " << i;
    }

    auto s1 = server_->stats();
    EXPECT_EQ(s1.playerCount, static_cast<std::size_t>(kPlayers));
    EXPECT_EQ(s1.activeInstances, 2u);

    // Run simulation ticks.
    for (int t = 0; t < kTicks; ++t) {
        auto result = server_->tick();
        ASSERT_TRUE(result.hasValue())
            << "Tick " << t << " failed";
    }

    // All players should still exist.
    for (int i = 1; i <= kPlayers; ++i) {
        auto session = server_->getPlayerSession(
            pid(static_cast<uint64_t>(i)));
        EXPECT_TRUE(session.has_value())
            << "Player " << i << " missing after simulation";
    }

    // Remove all players.
    for (int i = 1; i <= kPlayers; ++i) {
        auto result = server_->removePlayer(pid(static_cast<uint64_t>(i)));
        EXPECT_TRUE(result.hasValue())
            << "Failed to remove player " << i;
    }

    auto s2 = server_->stats();
    EXPECT_EQ(s2.playerCount, 0u);
    EXPECT_EQ(s2.playersJoined, static_cast<uint64_t>(kPlayers));
    EXPECT_EQ(s2.playersLeft, static_cast<uint64_t>(kPlayers));
}

// =============================================================================
// Player transfer chain across instances
// =============================================================================

TEST_F(GameServerIntegrationTest, PlayerTransferChain) {
    auto inst1 = server_->createInstance(1);
    auto inst2 = server_->createInstance(2);
    auto inst3 = server_->createInstance(3);
    ASSERT_TRUE(inst1.hasValue());
    ASSERT_TRUE(inst2.hasValue());
    ASSERT_TRUE(inst3.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), inst1.value()).hasValue());

    // Transfer 1 -> 2 -> 3 -> 1 (round trip).
    ASSERT_TRUE(server_->transferPlayer(pid(1), inst2.value()).hasValue());
    EXPECT_EQ(server_->getPlayerSession(pid(1))->instanceId, inst2.value());

    // Tick between transfers to ensure systems handle membership changes.
    ASSERT_TRUE(server_->tick().hasValue());

    ASSERT_TRUE(server_->transferPlayer(pid(1), inst3.value()).hasValue());
    EXPECT_EQ(server_->getPlayerSession(pid(1))->instanceId, inst3.value());

    ASSERT_TRUE(server_->tick().hasValue());

    ASSERT_TRUE(server_->transferPlayer(pid(1), inst1.value()).hasValue());
    EXPECT_EQ(server_->getPlayerSession(pid(1))->instanceId, inst1.value());

    // Entity should be consistent after round-trip transfers.
    ASSERT_TRUE(server_->tick().hasValue());
}

// =============================================================================
// Instance lifecycle: create, populate, drain, destroy
// =============================================================================

TEST_F(GameServerIntegrationTest, InstanceLifecycle) {
    auto instId = server_->createInstance(1, 5);
    ASSERT_TRUE(instId.hasValue());

    // Fill up the instance.
    for (int i = 1; i <= 5; ++i) {
        ASSERT_TRUE(server_->addPlayer(
            pid(static_cast<uint64_t>(i)), instId.value()).hasValue());
    }

    // Instance should be full.
    auto result = server_->addPlayer(pid(6), instId.value());
    EXPECT_TRUE(result.hasError());

    // Run some ticks.
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(server_->tick().hasValue());
    }

    // Remove all players.
    for (int i = 1; i <= 5; ++i) {
        ASSERT_TRUE(server_->removePlayer(
            pid(static_cast<uint64_t>(i))).hasValue());
    }

    // Now we can destroy the empty instance.
    auto destroy = server_->destroyInstance(instId.value());
    EXPECT_TRUE(destroy.hasValue());

    EXPECT_EQ(server_->stats().activeInstances, 0u);
}

// =============================================================================
// Benchmark: tick throughput with players
// =============================================================================

TEST_F(GameServerIntegrationTest, TickThroughputBenchmark) {
    constexpr int kPlayers = 50;
    constexpr int kTicks = 200;

    auto instId = server_->createInstance(1, 100);
    ASSERT_TRUE(instId.hasValue());

    for (int i = 1; i <= kPlayers; ++i) {
        ASSERT_TRUE(server_->addPlayer(
            pid(static_cast<uint64_t>(i)), instId.value()).hasValue());
    }

    auto start = std::chrono::steady_clock::now();

    for (int t = 0; t < kTicks; ++t) {
        auto result = server_->tick();
        ASSERT_TRUE(result.hasValue());
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

    auto s = server_->stats();
    EXPECT_EQ(s.playerCount, static_cast<std::size_t>(kPlayers));

    if (ms.count() > 0) {
        auto ticksPerSec = (kTicks * 1000) / ms.count();
        auto avgMs = static_cast<double>(ms.count()) / kTicks;
        std::cout << "[BENCHMARK] " << kTicks << " ticks with "
                  << kPlayers << " players in " << ms.count()
                  << "ms (" << ticksPerSec << " ticks/sec, "
                  << avgMs << " ms/tick avg)" << std::endl;
    }
}
