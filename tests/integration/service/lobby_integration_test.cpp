/// @file lobby_integration_test.cpp
/// @brief Integration tests for LobbyServer: full matchmaking flow,
///        party queuing, and throughput benchmark.

#include <gtest/gtest.h>

#include <vector>

#include "cgs/service/lobby_server.hpp"
#include "cgs/service/lobby_types.hpp"

using namespace cgs::service;

// ============================================================================
// Integration Tests
// ============================================================================

class LobbyIntegrationTest : public ::testing::Test {
protected:
    LobbyConfig config_;
    std::unique_ptr<LobbyServer> lobby_;

    void SetUp() override {
        config_.queueConfig.minPlayersPerMatch = 2;
        config_.queueConfig.maxPlayersPerMatch = 2;
        config_.queueConfig.initialRatingTolerance = 100;
        config_.queueConfig.maxQueueSize = 10000;
        config_.maxPartySize = 5;

        lobby_ = std::make_unique<LobbyServer>(config_);
        (void)lobby_->start();
    }

    PlayerRating makeRating(int32_t mmr) {
        PlayerRating r;
        r.mmr = mmr;
        return r;
    }
};

TEST_F(LobbyIntegrationTest, FullSoloMatchmakingFlow) {
    // 20 players enqueue with close ratings.
    for (uint64_t i = 1; i <= 20; ++i) {
        auto mmr = static_cast<int32_t>(1500 + i * 5);
        auto result = lobby_->enqueuePlayer(i, makeRating(mmr));
        ASSERT_TRUE(result.hasValue()) << "Failed to enqueue player " << i;
    }

    auto s1 = lobby_->stats();
    EXPECT_EQ(s1.queuedPlayers, 20u);

    auto matches = lobby_->processMatchmaking();

    // All 20 players should form 10 matches.
    EXPECT_EQ(matches.size(), 10u);

    auto s2 = lobby_->stats();
    EXPECT_EQ(s2.queuedPlayers, 0u);
    EXPECT_EQ(s2.matchesFormed, 10u);

    // Each match should have 2 players.
    for (const auto& match : matches) {
        EXPECT_EQ(match.players.size(), 2u);
        EXPECT_GT(match.matchQuality, 0.0f);
        EXPECT_GT(match.matchId, 0u);
    }
}

TEST_F(LobbyIntegrationTest, PartyQueueAndMatch) {
    // Create two parties of 1 player each and queue them.
    auto p1 = lobby_->createParty(1, "Alice", makeRating(1500));
    auto p2 = lobby_->createParty(2, "Bob", makeRating(1520));
    ASSERT_TRUE(p1.hasValue());
    ASSERT_TRUE(p2.hasValue());

    (void)lobby_->enqueueParty(p1.value());
    (void)lobby_->enqueueParty(p2.value());

    auto matches = lobby_->processMatchmaking();
    EXPECT_EQ(matches.size(), 1u);

    // Verify party members were matched.
    bool hasAlice = false, hasBob = false;
    for (const auto& player : matches[0].players) {
        if (player.playerId == 1) hasAlice = true;
        if (player.playerId == 2) hasBob = true;
    }
    EXPECT_TRUE(hasAlice);
    EXPECT_TRUE(hasBob);

    auto s = lobby_->stats();
    EXPECT_EQ(s.matchesFormed, 1u);
    EXPECT_EQ(s.partiesCreated, 2u);
}

TEST_F(LobbyIntegrationTest, PartyWithMultipleMembersQueue) {
    // Create a 3-member party.
    auto partyId = lobby_->createParty(10, "Leader", makeRating(1500));
    (void)lobby_->addPartyMember(partyId.value(), 11, "M1", makeRating(1520));
    (void)lobby_->addPartyMember(partyId.value(), 12, "M2", makeRating(1480));

    auto result = lobby_->enqueueParty(partyId.value());
    EXPECT_TRUE(result.hasValue());

    // All 3 members should be queued.
    EXPECT_TRUE(lobby_->isPlayerQueued(10));
    EXPECT_TRUE(lobby_->isPlayerQueued(11));
    EXPECT_TRUE(lobby_->isPlayerQueued(12));

    // Dequeue party removes all members.
    (void)lobby_->dequeueParty(partyId.value());
    EXPECT_FALSE(lobby_->isPlayerQueued(10));
    EXPECT_FALSE(lobby_->isPlayerQueued(11));
    EXPECT_FALSE(lobby_->isPlayerQueued(12));
}

TEST_F(LobbyIntegrationTest, EloRatingConvergence) {
    // Simulate 10 games between two players of equal skill.
    PlayerRating p1;
    p1.mmr = 1500;
    p1.gamesPlayed = 0;

    PlayerRating p2;
    p2.mmr = 1500;
    p2.gamesPlayed = 0;

    // Alternate winners to simulate equal skill.
    for (int i = 0; i < 10; ++i) {
        if (i % 2 == 0) {
            LobbyServer::updateRatings(p1, p2);
        } else {
            LobbyServer::updateRatings(p2, p1);
        }
    }

    // After alternating wins, ratings should stay close to initial.
    EXPECT_NEAR(p1.mmr, 1500, 50);
    EXPECT_NEAR(p2.mmr, 1500, 50);
    EXPECT_EQ(p1.gamesPlayed, 10u);
    EXPECT_EQ(p2.gamesPlayed, 10u);
}

TEST_F(LobbyIntegrationTest, MatchmakingThroughputBenchmark) {
    // Enqueue 1000 players and process all matches.
    for (uint64_t i = 1; i <= 1000; ++i) {
        auto mmr = static_cast<int32_t>(1400 + static_cast<int32_t>(i % 200));
        (void)lobby_->enqueuePlayer(i, makeRating(mmr));
    }

    EXPECT_EQ(lobby_->stats().queuedPlayers, 1000u);

    auto matches = lobby_->processMatchmaking();

    // Should form a significant number of matches.
    EXPECT_GE(matches.size(), 400u);

    auto s = lobby_->stats();
    EXPECT_GE(s.matchesFormed, 400u);
}
