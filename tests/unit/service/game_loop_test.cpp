/// @file game_loop_test.cpp
/// @brief Unit tests for GameLoop and MapInstanceManager.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "cgs/service/game_loop.hpp"
#include "cgs/service/map_instance_manager.hpp"

using namespace cgs::service;
using namespace std::chrono_literals;

// ============================================================================
// GameLoop Tests
// ============================================================================

class GameLoopTest : public ::testing::Test {
protected:
    GameLoop loop_{20}; // 20 Hz
};

TEST_F(GameLoopTest, DefaultTickRate) {
    EXPECT_EQ(loop_.tickRate(), 20u);
}

TEST_F(GameLoopTest, TargetFrameTimeForTwentyHz) {
    EXPECT_EQ(loop_.targetFrameTime(), 50000us);
}

TEST_F(GameLoopTest, CustomTickRate) {
    GameLoop custom(60);
    EXPECT_EQ(custom.tickRate(), 60u);
    // 1'000'000 / 60 = 16666 us
    EXPECT_EQ(custom.targetFrameTime().count(), 16666);
}

TEST_F(GameLoopTest, ZeroTickRateDefaultsToTwenty) {
    GameLoop zeroRate(0);
    EXPECT_EQ(zeroRate.tickRate(), 20u);
}

TEST_F(GameLoopTest, InitialTickCountIsZero) {
    EXPECT_EQ(loop_.tickCount(), 0u);
}

TEST_F(GameLoopTest, NotRunningByDefault) {
    EXPECT_FALSE(loop_.isRunning());
}

TEST_F(GameLoopTest, ManualTickIncreasesCount) {
    (void)loop_.tick();
    EXPECT_EQ(loop_.tickCount(), 1u);

    (void)loop_.tick();
    EXPECT_EQ(loop_.tickCount(), 2u);
}

TEST_F(GameLoopTest, ManualTickReturnsMetrics) {
    auto metrics = loop_.tick();

    EXPECT_EQ(metrics.tickNumber, 0u);
    EXPECT_GE(metrics.updateTime.count(), 0);
    EXPECT_GE(metrics.frameTime.count(), 0);
    EXPECT_GE(metrics.budgetUtilization, 0.0f);
}

TEST_F(GameLoopTest, TickCallbackIsInvoked) {
    int callCount = 0;
    float receivedDt = 0.0f;

    loop_.setTickCallback([&](float dt) {
        ++callCount;
        receivedDt = dt;
    });

    (void)loop_.tick();

    EXPECT_EQ(callCount, 1);
    EXPECT_NEAR(receivedDt, 0.05f, 0.001f); // 20Hz → 50ms → 0.05s
}

TEST_F(GameLoopTest, MetricsCallbackNotCalledOnManualTick) {
    // Metrics callback is only invoked from the thread loop,
    // not from manual tick().
    int metricsCount = 0;
    loop_.setMetricsCallback([&](const TickMetrics&) {
        ++metricsCount;
    });

    (void)loop_.tick();
    EXPECT_EQ(metricsCount, 0);
}

TEST_F(GameLoopTest, StartAndStopLifecycle) {
    EXPECT_TRUE(loop_.start());
    EXPECT_TRUE(loop_.isRunning());

    // Let it run for a few ticks.
    std::this_thread::sleep_for(120ms);

    loop_.stop();
    EXPECT_FALSE(loop_.isRunning());
    EXPECT_GT(loop_.tickCount(), 0u);
}

TEST_F(GameLoopTest, DoubleStartReturnsFalse) {
    EXPECT_TRUE(loop_.start());
    EXPECT_FALSE(loop_.start()); // Already running.
    loop_.stop();
}

TEST_F(GameLoopTest, StopWhenNotRunningIsSafe) {
    loop_.stop(); // No-op, should not crash.
}

TEST_F(GameLoopTest, ThreadedTickCallbackInvocation) {
    std::atomic<int> callCount{0};

    loop_.setTickCallback([&](float) {
        callCount.fetch_add(1);
    });

    EXPECT_TRUE(loop_.start());
    std::this_thread::sleep_for(250ms);
    loop_.stop();

    // At 20Hz, ~250ms should yield at least 2 ticks (generous for CI).
    EXPECT_GE(callCount.load(), 2);
}

TEST_F(GameLoopTest, ThreadedMetricsCallbackInvocation) {
    std::atomic<int> metricsCount{0};
    TickMetrics lastMetrics;

    loop_.setMetricsCallback([&](const TickMetrics& m) {
        metricsCount.fetch_add(1);
        lastMetrics = m;
    });

    EXPECT_TRUE(loop_.start());
    std::this_thread::sleep_for(250ms);
    loop_.stop();

    EXPECT_GE(metricsCount.load(), 2);
}

TEST_F(GameLoopTest, OverrunDetection) {
    loop_.setTickCallback([&](float) {
        // Simulate a slow tick that exceeds the 50ms budget.
        std::this_thread::sleep_for(60ms);
    });

    auto metrics = loop_.tick();
    EXPECT_TRUE(metrics.overrun);
    EXPECT_GT(metrics.budgetUtilization, 1.0f);
}

TEST_F(GameLoopTest, NormalTickIsNotOverrun) {
    loop_.setTickCallback([&](float) {
        // Very fast tick.
    });

    auto metrics = loop_.tick();
    EXPECT_FALSE(metrics.overrun);
    EXPECT_LT(metrics.budgetUtilization, 1.0f);
}

TEST_F(GameLoopTest, LastMetricsReflectsLatestTick) {
    loop_.setTickCallback([&](float) {});

    (void)loop_.tick();
    (void)loop_.tick();
    (void)loop_.tick();

    // Manual tick doesn't update lastMetrics_ (only thread loop does),
    // so we test via the thread loop.
    EXPECT_TRUE(loop_.start());
    std::this_thread::sleep_for(80ms);
    loop_.stop();

    auto metrics = loop_.lastMetrics();
    EXPECT_GT(metrics.tickNumber, 0u);
}

TEST_F(GameLoopTest, DestructorStopsRunningLoop) {
    auto* loop = new GameLoop(20);
    loop->setTickCallback([](float) {});
    EXPECT_TRUE(loop->start());
    EXPECT_TRUE(loop->isRunning());
    delete loop; // Should call stop() in destructor.
}

// ============================================================================
// MapInstanceManager Tests
// ============================================================================

class MapInstanceManagerTest : public ::testing::Test {
protected:
    MapInstanceManager mgr_{100}; // Max 100 instances.
};

TEST_F(MapInstanceManagerTest, InitialCountIsZero) {
    EXPECT_EQ(mgr_.instanceCount(), 0u);
}

TEST_F(MapInstanceManagerTest, CreateInstance) {
    auto result = mgr_.createInstance(1);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value(), 1u); // First instance gets ID 1.

    EXPECT_EQ(mgr_.instanceCount(), 1u);
}

TEST_F(MapInstanceManagerTest, CreateMultipleInstances) {
    auto r1 = mgr_.createInstance(1);
    auto r2 = mgr_.createInstance(1);
    auto r3 = mgr_.createInstance(2);

    ASSERT_TRUE(r1.hasValue());
    ASSERT_TRUE(r2.hasValue());
    ASSERT_TRUE(r3.hasValue());

    EXPECT_NE(r1.value(), r2.value());
    EXPECT_NE(r2.value(), r3.value());
    EXPECT_EQ(mgr_.instanceCount(), 3u);
}

TEST_F(MapInstanceManagerTest, CreateInstanceRespectMaxLimit) {
    MapInstanceManager small(2);
    auto r1 = small.createInstance(1);
    auto r2 = small.createInstance(1);
    auto r3 = small.createInstance(1);

    EXPECT_TRUE(r1.hasValue());
    EXPECT_TRUE(r2.hasValue());
    EXPECT_TRUE(r3.hasError());
}

TEST_F(MapInstanceManagerTest, GetInstanceReturnsInfo) {
    auto result = mgr_.createInstance(42, cgs::game::MapType::Dungeon, 50);
    ASSERT_TRUE(result.hasValue());

    auto info = mgr_.getInstance(result.value());
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->mapId, 42u);
    EXPECT_EQ(info->type, cgs::game::MapType::Dungeon);
    EXPECT_EQ(info->maxPlayers, 50u);
    EXPECT_EQ(info->state, InstanceState::Active);
    EXPECT_EQ(info->playerCount, 0u);
}

TEST_F(MapInstanceManagerTest, GetNonexistentInstanceReturnsNullopt) {
    auto info = mgr_.getInstance(999);
    EXPECT_FALSE(info.has_value());
}

TEST_F(MapInstanceManagerTest, DestroyEmptyInstance) {
    auto result = mgr_.createInstance(1);
    ASSERT_TRUE(result.hasValue());

    auto destroyResult = mgr_.destroyInstance(result.value());
    EXPECT_TRUE(destroyResult.hasValue());
    EXPECT_EQ(mgr_.instanceCount(), 0u);
}

TEST_F(MapInstanceManagerTest, DestroyNonexistentInstanceFails) {
    auto result = mgr_.destroyInstance(999);
    EXPECT_TRUE(result.hasError());
}

TEST_F(MapInstanceManagerTest, DestroyInstanceWithPlayersFails) {
    auto result = mgr_.createInstance(1);
    ASSERT_TRUE(result.hasValue());

    EXPECT_TRUE(mgr_.addPlayer(result.value()));

    auto destroyResult = mgr_.destroyInstance(result.value());
    EXPECT_TRUE(destroyResult.hasError());
}

TEST_F(MapInstanceManagerTest, AddAndRemovePlayer) {
    auto result = mgr_.createInstance(1, cgs::game::MapType::OpenWorld, 10);
    ASSERT_TRUE(result.hasValue());
    auto id = result.value();

    EXPECT_TRUE(mgr_.addPlayer(id));
    EXPECT_TRUE(mgr_.addPlayer(id));

    auto info = mgr_.getInstance(id);
    EXPECT_EQ(info->playerCount, 2u);

    EXPECT_TRUE(mgr_.removePlayer(id));
    info = mgr_.getInstance(id);
    EXPECT_EQ(info->playerCount, 1u);
}

TEST_F(MapInstanceManagerTest, AddPlayerToNonexistentInstanceFails) {
    EXPECT_FALSE(mgr_.addPlayer(999));
}

TEST_F(MapInstanceManagerTest, RemovePlayerFromEmptyInstanceFails) {
    auto result = mgr_.createInstance(1);
    ASSERT_TRUE(result.hasValue());

    EXPECT_FALSE(mgr_.removePlayer(result.value()));
}

TEST_F(MapInstanceManagerTest, AddPlayerRespectMaxPlayers) {
    auto result = mgr_.createInstance(1, cgs::game::MapType::OpenWorld, 2);
    ASSERT_TRUE(result.hasValue());
    auto id = result.value();

    EXPECT_TRUE(mgr_.addPlayer(id));
    EXPECT_TRUE(mgr_.addPlayer(id));
    EXPECT_FALSE(mgr_.addPlayer(id)); // Full.
}

TEST_F(MapInstanceManagerTest, StateTransitionActiveToDraining) {
    auto result = mgr_.createInstance(1);
    ASSERT_TRUE(result.hasValue());
    auto id = result.value();

    EXPECT_TRUE(mgr_.setInstanceState(id, InstanceState::Draining));

    auto info = mgr_.getInstance(id);
    EXPECT_EQ(info->state, InstanceState::Draining);
}

TEST_F(MapInstanceManagerTest, StateTransitionDrainingToShuttingDown) {
    auto result = mgr_.createInstance(1);
    ASSERT_TRUE(result.hasValue());
    auto id = result.value();

    EXPECT_TRUE(mgr_.setInstanceState(id, InstanceState::Draining));
    EXPECT_TRUE(mgr_.setInstanceState(id, InstanceState::ShuttingDown));

    auto info = mgr_.getInstance(id);
    EXPECT_EQ(info->state, InstanceState::ShuttingDown);
}

TEST_F(MapInstanceManagerTest, InvalidStateTransitionActiveToShuttingDown) {
    auto result = mgr_.createInstance(1);
    ASSERT_TRUE(result.hasValue());

    // Cannot skip Draining.
    EXPECT_FALSE(mgr_.setInstanceState(result.value(), InstanceState::ShuttingDown));
}

TEST_F(MapInstanceManagerTest, CannotTransitionBackToActive) {
    auto result = mgr_.createInstance(1);
    ASSERT_TRUE(result.hasValue());
    auto id = result.value();

    EXPECT_TRUE(mgr_.setInstanceState(id, InstanceState::Draining));
    EXPECT_FALSE(mgr_.setInstanceState(id, InstanceState::Active));
}

TEST_F(MapInstanceManagerTest, CannotAddPlayerToDrainingInstance) {
    auto result = mgr_.createInstance(1);
    ASSERT_TRUE(result.hasValue());
    auto id = result.value();

    EXPECT_TRUE(mgr_.setInstanceState(id, InstanceState::Draining));
    EXPECT_FALSE(mgr_.addPlayer(id));
}

TEST_F(MapInstanceManagerTest, CanRemovePlayerFromDrainingInstance) {
    auto result = mgr_.createInstance(1);
    ASSERT_TRUE(result.hasValue());
    auto id = result.value();

    EXPECT_TRUE(mgr_.addPlayer(id));
    EXPECT_TRUE(mgr_.setInstanceState(id, InstanceState::Draining));
    EXPECT_TRUE(mgr_.removePlayer(id));
}

TEST_F(MapInstanceManagerTest, GetInstancesByMap) {
    (void)mgr_.createInstance(1);
    (void)mgr_.createInstance(1);
    (void)mgr_.createInstance(2);

    auto map1Instances = mgr_.getInstancesByMap(1);
    EXPECT_EQ(map1Instances.size(), 2u);

    auto map2Instances = mgr_.getInstancesByMap(2);
    EXPECT_EQ(map2Instances.size(), 1u);

    auto map3Instances = mgr_.getInstancesByMap(3);
    EXPECT_TRUE(map3Instances.empty());
}

TEST_F(MapInstanceManagerTest, GetInstancesByState) {
    auto r1 = mgr_.createInstance(1);
    auto r2 = mgr_.createInstance(1);
    (void)mgr_.createInstance(2);

    ASSERT_TRUE(r1.hasValue());
    ASSERT_TRUE(r2.hasValue());

    EXPECT_TRUE(mgr_.setInstanceState(r1.value(), InstanceState::Draining));

    auto active = mgr_.getInstancesByState(InstanceState::Active);
    EXPECT_EQ(active.size(), 2u);

    auto draining = mgr_.getInstancesByState(InstanceState::Draining);
    EXPECT_EQ(draining.size(), 1u);
}

TEST_F(MapInstanceManagerTest, InstanceCountByState) {
    auto r1 = mgr_.createInstance(1);
    (void)mgr_.createInstance(1);

    ASSERT_TRUE(r1.hasValue());
    EXPECT_TRUE(mgr_.setInstanceState(r1.value(), InstanceState::Draining));

    EXPECT_EQ(mgr_.instanceCount(InstanceState::Active), 1u);
    EXPECT_EQ(mgr_.instanceCount(InstanceState::Draining), 1u);
    EXPECT_EQ(mgr_.instanceCount(InstanceState::ShuttingDown), 0u);
}

TEST_F(MapInstanceManagerTest, FindEmptyInstances) {
    auto r1 = mgr_.createInstance(1);
    auto r2 = mgr_.createInstance(1);

    ASSERT_TRUE(r1.hasValue());
    ASSERT_TRUE(r2.hasValue());

    EXPECT_TRUE(mgr_.addPlayer(r1.value()));

    auto empty = mgr_.findEmptyInstances();
    ASSERT_EQ(empty.size(), 1u);
    EXPECT_EQ(empty[0], r2.value());
}

TEST_F(MapInstanceManagerTest, FindAvailableInstances) {
    auto r1 = mgr_.createInstance(1, cgs::game::MapType::OpenWorld, 2);
    auto r2 = mgr_.createInstance(1, cgs::game::MapType::OpenWorld, 2);
    auto r3 = mgr_.createInstance(2, cgs::game::MapType::OpenWorld, 2);

    ASSERT_TRUE(r1.hasValue());
    ASSERT_TRUE(r2.hasValue());
    ASSERT_TRUE(r3.hasValue());

    // Fill up r1.
    EXPECT_TRUE(mgr_.addPlayer(r1.value()));
    EXPECT_TRUE(mgr_.addPlayer(r1.value()));

    // Drain r2.
    EXPECT_TRUE(mgr_.setInstanceState(r2.value(), InstanceState::Draining));

    // Map 1: no available instances (r1 full, r2 draining).
    auto available1 = mgr_.findAvailableInstances(1);
    EXPECT_TRUE(available1.empty());

    // Map 2: r3 is available.
    auto available2 = mgr_.findAvailableInstances(2);
    ASSERT_EQ(available2.size(), 1u);
    EXPECT_EQ(available2[0], r3.value());
}

TEST_F(MapInstanceManagerTest, InstanceCreatedAtTimestamp) {
    auto before = std::chrono::steady_clock::now();
    auto result = mgr_.createInstance(1);
    auto after = std::chrono::steady_clock::now();

    ASSERT_TRUE(result.hasValue());
    auto info = mgr_.getInstance(result.value());
    ASSERT_TRUE(info.has_value());

    EXPECT_GE(info->createdAt, before);
    EXPECT_LE(info->createdAt, after);
}

TEST_F(MapInstanceManagerTest, StateTransitionOnNonexistentInstanceFails) {
    EXPECT_FALSE(mgr_.setInstanceState(999, InstanceState::Draining));
}

TEST_F(MapInstanceManagerTest, MapTypePreserved) {
    auto r1 = mgr_.createInstance(1, cgs::game::MapType::OpenWorld);
    auto r2 = mgr_.createInstance(2, cgs::game::MapType::Dungeon);
    auto r3 = mgr_.createInstance(3, cgs::game::MapType::Battleground);

    ASSERT_TRUE(r1.hasValue());
    ASSERT_TRUE(r2.hasValue());
    ASSERT_TRUE(r3.hasValue());

    EXPECT_EQ(mgr_.getInstance(r1.value())->type, cgs::game::MapType::OpenWorld);
    EXPECT_EQ(mgr_.getInstance(r2.value())->type, cgs::game::MapType::Dungeon);
    EXPECT_EQ(mgr_.getInstance(r3.value())->type, cgs::game::MapType::Battleground);
}

TEST_F(MapInstanceManagerTest, DestroyAfterDrainingAndEmpty) {
    auto result = mgr_.createInstance(1);
    ASSERT_TRUE(result.hasValue());
    auto id = result.value();

    EXPECT_TRUE(mgr_.addPlayer(id));
    EXPECT_TRUE(mgr_.setInstanceState(id, InstanceState::Draining));
    EXPECT_TRUE(mgr_.removePlayer(id));
    EXPECT_TRUE(mgr_.setInstanceState(id, InstanceState::ShuttingDown));

    auto destroyResult = mgr_.destroyInstance(id);
    EXPECT_TRUE(destroyResult.hasValue());
    EXPECT_EQ(mgr_.instanceCount(), 0u);
}
