#include <gtest/gtest.h>

#include "cgs/ecs/entity.hpp"
#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/types.hpp"
#include "cgs/service/game_server.hpp"

#include <optional>
#include <vector>

using namespace cgs::service;
using cgs::foundation::ErrorCode;
using cgs::foundation::PlayerId;

// =============================================================================
// Test fixture
// =============================================================================

class GameServerTest : public ::testing::Test {
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
// Lifecycle tests
// =============================================================================

TEST_F(GameServerTest, DefaultNotRunning) {
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(GameServerTest, ConfigAccessible) {
    EXPECT_EQ(server_->config().tickRate, 20u);
    EXPECT_EQ(server_->config().maxInstances, 100u);
}

TEST_F(GameServerTest, ManualTickSucceeds) {
    auto result = server_->tick();
    ASSERT_TRUE(result.hasValue());
}

TEST_F(GameServerTest, MultipleManualTicks) {
    for (int i = 0; i < 5; ++i) {
        auto result = server_->tick();
        ASSERT_TRUE(result.hasValue());
    }
}

// =============================================================================
// Instance management tests
// =============================================================================

TEST_F(GameServerTest, CreateInstance) {
    auto result = server_->createInstance(1, 50);
    ASSERT_TRUE(result.hasValue());
    EXPECT_GT(result.value(), 0u);
}

TEST_F(GameServerTest, CreateMultipleInstances) {
    auto r1 = server_->createInstance(1);
    auto r2 = server_->createInstance(1);
    auto r3 = server_->createInstance(2);

    ASSERT_TRUE(r1.hasValue());
    ASSERT_TRUE(r2.hasValue());
    ASSERT_TRUE(r3.hasValue());

    EXPECT_NE(r1.value(), r2.value());
    EXPECT_NE(r1.value(), r3.value());
}

TEST_F(GameServerTest, DestroyEmptyInstance) {
    auto create = server_->createInstance(1);
    ASSERT_TRUE(create.hasValue());

    auto destroy = server_->destroyInstance(create.value());
    EXPECT_TRUE(destroy.hasValue());
}

TEST_F(GameServerTest, DestroyNonexistentInstance) {
    auto result = server_->destroyInstance(99999);
    EXPECT_TRUE(result.hasError());
}

TEST_F(GameServerTest, AvailableInstancesForMap) {
    auto r1 = server_->createInstance(1, 10);
    auto r2 = server_->createInstance(1, 10);
    auto r3 = server_->createInstance(2, 10);

    ASSERT_TRUE(r1.hasValue());
    ASSERT_TRUE(r2.hasValue());
    ASSERT_TRUE(r3.hasValue());

    auto avail = server_->availableInstances(1);
    EXPECT_EQ(avail.size(), 2u);

    auto avail2 = server_->availableInstances(2);
    EXPECT_EQ(avail2.size(), 1u);
}

// =============================================================================
// Player lifecycle tests
// =============================================================================

TEST_F(GameServerTest, AddPlayer) {
    auto instId = server_->createInstance(1);
    ASSERT_TRUE(instId.hasValue());

    auto result = server_->addPlayer(pid(1), instId.value());
    ASSERT_TRUE(result.hasValue());
    EXPECT_TRUE(result.value().isValid());
}

TEST_F(GameServerTest, AddPlayerDuplicate) {
    auto instId = server_->createInstance(1);
    ASSERT_TRUE(instId.hasValue());

    auto r1 = server_->addPlayer(pid(1), instId.value());
    ASSERT_TRUE(r1.hasValue());

    auto r2 = server_->addPlayer(pid(1), instId.value());
    EXPECT_TRUE(r2.hasError());
    EXPECT_EQ(r2.error().code(), ErrorCode::PlayerAlreadyInWorld);
}

TEST_F(GameServerTest, AddPlayerToNonexistentInstance) {
    auto result = server_->addPlayer(pid(1), 99999);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::MapInstanceNotFound);
}

TEST_F(GameServerTest, AddPlayerToFullInstance) {
    auto instId = server_->createInstance(1, 2);
    ASSERT_TRUE(instId.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), instId.value()).hasValue());
    ASSERT_TRUE(server_->addPlayer(pid(2), instId.value()).hasValue());

    auto result = server_->addPlayer(pid(3), instId.value());
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InstanceFull);
}

TEST_F(GameServerTest, RemovePlayer) {
    auto instId = server_->createInstance(1);
    ASSERT_TRUE(instId.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), instId.value()).hasValue());

    auto result = server_->removePlayer(pid(1));
    EXPECT_TRUE(result.hasValue());
}

TEST_F(GameServerTest, RemoveNonexistentPlayer) {
    auto result = server_->removePlayer(pid(99));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PlayerNotInWorld);
}

TEST_F(GameServerTest, GetPlayerSession) {
    auto instId = server_->createInstance(1);
    ASSERT_TRUE(instId.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), instId.value()).hasValue());

    auto session = server_->getPlayerSession(pid(1));
    ASSERT_TRUE(session.has_value());
    EXPECT_EQ(session->playerId, pid(1));
    EXPECT_EQ(session->instanceId, instId.value());
    EXPECT_TRUE(session->entity.isValid());
}

TEST_F(GameServerTest, GetPlayerSessionNotFound) {
    auto session = server_->getPlayerSession(pid(99));
    EXPECT_FALSE(session.has_value());
}

// =============================================================================
// Player transfer tests
// =============================================================================

TEST_F(GameServerTest, TransferPlayerBetweenInstances) {
    auto inst1 = server_->createInstance(1);
    auto inst2 = server_->createInstance(2);
    ASSERT_TRUE(inst1.hasValue());
    ASSERT_TRUE(inst2.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), inst1.value()).hasValue());

    auto result = server_->transferPlayer(pid(1), inst2.value());
    EXPECT_TRUE(result.hasValue());

    auto session = server_->getPlayerSession(pid(1));
    ASSERT_TRUE(session.has_value());
    EXPECT_EQ(session->instanceId, inst2.value());
}

TEST_F(GameServerTest, TransferPlayerToSameInstance) {
    auto instId = server_->createInstance(1);
    ASSERT_TRUE(instId.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), instId.value()).hasValue());

    auto result = server_->transferPlayer(pid(1), instId.value());
    EXPECT_TRUE(result.hasValue());
}

TEST_F(GameServerTest, TransferPlayerToFullInstance) {
    auto inst1 = server_->createInstance(1, 10);
    auto inst2 = server_->createInstance(2, 1);
    ASSERT_TRUE(inst1.hasValue());
    ASSERT_TRUE(inst2.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), inst1.value()).hasValue());
    ASSERT_TRUE(server_->addPlayer(pid(2), inst2.value()).hasValue());

    auto result = server_->transferPlayer(pid(1), inst2.value());
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InstanceFull);

    // Player should remain in original instance.
    auto session = server_->getPlayerSession(pid(1));
    ASSERT_TRUE(session.has_value());
    EXPECT_EQ(session->instanceId, inst1.value());
}

TEST_F(GameServerTest, TransferNonexistentPlayer) {
    auto instId = server_->createInstance(1);
    ASSERT_TRUE(instId.hasValue());

    auto result = server_->transferPlayer(pid(99), instId.value());
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PlayerNotInWorld);
}

// =============================================================================
// Statistics tests
// =============================================================================

TEST_F(GameServerTest, StatsInitial) {
    auto s = server_->stats();
    EXPECT_EQ(s.entityCount, 0u);
    EXPECT_EQ(s.playerCount, 0u);
    EXPECT_EQ(s.activeInstances, 0u);
    EXPECT_EQ(s.playersJoined, 0u);
    EXPECT_EQ(s.playersLeft, 0u);
}

TEST_F(GameServerTest, StatsAfterPlayerLifecycle) {
    auto instId = server_->createInstance(1);
    ASSERT_TRUE(instId.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), instId.value()).hasValue());
    ASSERT_TRUE(server_->addPlayer(pid(2), instId.value()).hasValue());

    auto s1 = server_->stats();
    EXPECT_EQ(s1.playerCount, 2u);
    EXPECT_EQ(s1.activeInstances, 1u);
    EXPECT_EQ(s1.playersJoined, 2u);
    // entityCount includes map entity + 2 player entities.
    EXPECT_GE(s1.entityCount, 2u);

    ASSERT_TRUE(server_->removePlayer(pid(1)).hasValue());

    auto s2 = server_->stats();
    EXPECT_EQ(s2.playerCount, 1u);
    EXPECT_EQ(s2.playersJoined, 2u);
    EXPECT_EQ(s2.playersLeft, 1u);
}

// =============================================================================
// Tick with players (smoke test)
// =============================================================================

TEST_F(GameServerTest, TickWithPlayersDoesNotCrash) {
    auto instId = server_->createInstance(1);
    ASSERT_TRUE(instId.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), instId.value()).hasValue());
    ASSERT_TRUE(server_->addPlayer(pid(2), instId.value()).hasValue());

    // Run several ticks to exercise all 6 game systems.
    for (int i = 0; i < 10; ++i) {
        auto result = server_->tick();
        ASSERT_TRUE(result.hasValue());
    }

    // Players should still be alive after ticks.
    auto s1 = server_->getPlayerSession(pid(1));
    auto s2 = server_->getPlayerSession(pid(2));
    EXPECT_TRUE(s1.has_value());
    EXPECT_TRUE(s2.has_value());
}

TEST_F(GameServerTest, TickAfterPlayerRemoval) {
    auto instId = server_->createInstance(1);
    ASSERT_TRUE(instId.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), instId.value()).hasValue());
    ASSERT_TRUE(server_->tick().hasValue());

    ASSERT_TRUE(server_->removePlayer(pid(1)).hasValue());
    // Ticking after removal should not crash.
    ASSERT_TRUE(server_->tick().hasValue());
}

// =============================================================================
// Destroy instance with players should fail
// =============================================================================

TEST_F(GameServerTest, DestroyInstanceWithPlayersFailes) {
    auto instId = server_->createInstance(1);
    ASSERT_TRUE(instId.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), instId.value()).hasValue());

    auto result = server_->destroyInstance(instId.value());
    EXPECT_TRUE(result.hasError());
}

// =============================================================================
// Reuse instance slot after player removal
// =============================================================================

TEST_F(GameServerTest, PlayerSlotReusedAfterRemoval) {
    auto instId = server_->createInstance(1, 1);
    ASSERT_TRUE(instId.hasValue());

    ASSERT_TRUE(server_->addPlayer(pid(1), instId.value()).hasValue());
    ASSERT_TRUE(server_->removePlayer(pid(1)).hasValue());

    // The slot should be freed; a new player can join.
    auto result = server_->addPlayer(pid(2), instId.value());
    EXPECT_TRUE(result.hasValue());
}
