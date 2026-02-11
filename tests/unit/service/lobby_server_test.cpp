/// @file lobby_server_test.cpp
/// @brief Unit tests for LobbyServer (orchestration, party queuing, ELO).

#include <gtest/gtest.h>

#include "cgs/foundation/error_code.hpp"
#include "cgs/service/lobby_server.hpp"
#include "cgs/service/lobby_types.hpp"

using namespace cgs::service;
using cgs::foundation::ErrorCode;

// ============================================================================
// LobbyServer Tests
// ============================================================================

class LobbyServerTest : public ::testing::Test {
protected:
    LobbyConfig config_;
    std::unique_ptr<LobbyServer> lobby_;

    void SetUp() override {
        config_.queueConfig.minPlayersPerMatch = 2;
        config_.queueConfig.maxPlayersPerMatch = 2;
        config_.queueConfig.initialRatingTolerance = 100;
        config_.queueConfig.maxQueueSize = 100;
        config_.maxPartySize = 5;

        lobby_ = std::make_unique<LobbyServer>(config_);
        (void)lobby_->start();
    }

    PlayerRating makeRating(int32_t mmr, uint32_t gamesPlayed = 50) {
        PlayerRating r;
        r.mmr = mmr;
        r.gamesPlayed = gamesPlayed;
        return r;
    }
};

// -- Lifecycle ----------------------------------------------------------------

TEST_F(LobbyServerTest, StartSucceeds) {
    LobbyServer lobby(config_);
    auto result = lobby.start();
    EXPECT_TRUE(result.hasValue());
    EXPECT_TRUE(lobby.isRunning());
}

TEST_F(LobbyServerTest, DoubleStartFails) {
    auto result = lobby_->start();
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::LobbyAlreadyStarted);
}

TEST_F(LobbyServerTest, StopAndRestart) {
    lobby_->stop();
    EXPECT_FALSE(lobby_->isRunning());

    auto result = lobby_->start();
    EXPECT_TRUE(result.hasValue());
    EXPECT_TRUE(lobby_->isRunning());
}

// -- Solo matchmaking ---------------------------------------------------------

TEST_F(LobbyServerTest, EnqueuePlayerSucceeds) {
    auto result = lobby_->enqueuePlayer(1, makeRating(1500));
    EXPECT_TRUE(result.hasValue());
    EXPECT_TRUE(lobby_->isPlayerQueued(1));
}

TEST_F(LobbyServerTest, EnqueuePlayerNotStartedFails) {
    LobbyServer lobby(config_);
    auto result = lobby.enqueuePlayer(1, makeRating(1500));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::LobbyNotStarted);
}

TEST_F(LobbyServerTest, EnqueueDuplicatePlayerFails) {
    (void)lobby_->enqueuePlayer(1, makeRating(1500));
    auto result = lobby_->enqueuePlayer(1, makeRating(1500));
    EXPECT_TRUE(result.hasError());
}

TEST_F(LobbyServerTest, DequeuePlayerSucceeds) {
    (void)lobby_->enqueuePlayer(1, makeRating(1500));
    EXPECT_TRUE(lobby_->dequeuePlayer(1));
    EXPECT_FALSE(lobby_->isPlayerQueued(1));
}

TEST_F(LobbyServerTest, DequeueNonexistentReturnsFalse) {
    EXPECT_FALSE(lobby_->dequeuePlayer(999));
}

// -- Party management via LobbyServer ----------------------------------------

TEST_F(LobbyServerTest, CreatePartySucceeds) {
    auto result = lobby_->createParty(1, "Alice", makeRating(1500));
    ASSERT_TRUE(result.hasValue());
    EXPECT_GT(result.value(), 0u);
}

TEST_F(LobbyServerTest, CreatePartyNotStartedFails) {
    LobbyServer lobby(config_);
    auto result = lobby.createParty(1, "Alice", makeRating(1500));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::LobbyNotStarted);
}

TEST_F(LobbyServerTest, AddAndRemovePartyMember) {
    auto partyId = lobby_->createParty(1, "Alice", makeRating(1500));
    auto result = lobby_->addPartyMember(
        partyId.value(), 2, "Bob", makeRating(1600));
    EXPECT_TRUE(result.hasValue());

    auto party = lobby_->getParty(partyId.value());
    ASSERT_TRUE(party.has_value());
    EXPECT_EQ(party->members.size(), 2u);

    result = lobby_->removePartyMember(partyId.value(), 2);
    EXPECT_TRUE(result.hasValue());

    party = lobby_->getParty(partyId.value());
    EXPECT_EQ(party->members.size(), 1u);
}

TEST_F(LobbyServerTest, DisbandPartySucceeds) {
    auto partyId = lobby_->createParty(1, "Alice", makeRating(1500));
    auto result = lobby_->disbandParty(partyId.value(), 1);
    EXPECT_TRUE(result.hasValue());
    EXPECT_FALSE(lobby_->getParty(partyId.value()).has_value());
}

TEST_F(LobbyServerTest, PromotePartyLeader) {
    auto partyId = lobby_->createParty(1, "Alice", makeRating(1500));
    (void)lobby_->addPartyMember(partyId.value(), 2, "Bob", makeRating(1500));

    auto result = lobby_->promotePartyLeader(partyId.value(), 1, 2);
    EXPECT_TRUE(result.hasValue());

    auto party = lobby_->getParty(partyId.value());
    EXPECT_EQ(party->leaderId, 2u);
}

TEST_F(LobbyServerTest, GetPlayerParty) {
    auto partyId = lobby_->createParty(1, "Alice", makeRating(1500));
    auto result = lobby_->getPlayerParty(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, partyId.value());
}

// -- Party matchmaking --------------------------------------------------------

TEST_F(LobbyServerTest, EnqueuePartySucceeds) {
    auto partyId = lobby_->createParty(1, "Alice", makeRating(1500));
    (void)lobby_->addPartyMember(partyId.value(), 2, "Bob", makeRating(1600));

    auto result = lobby_->enqueueParty(partyId.value());
    EXPECT_TRUE(result.hasValue());
    EXPECT_TRUE(lobby_->isPlayerQueued(1));
    EXPECT_TRUE(lobby_->isPlayerQueued(2));
}

TEST_F(LobbyServerTest, EnqueuePartyNotStartedFails) {
    LobbyServer lobby(config_);
    auto result = lobby.enqueueParty(999);
    EXPECT_TRUE(result.hasError());
}

TEST_F(LobbyServerTest, EnqueuePartyNotFoundFails) {
    auto result = lobby_->enqueueParty(999);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PartyNotFound);
}

TEST_F(LobbyServerTest, DequeuePartySucceeds) {
    auto partyId = lobby_->createParty(1, "Alice", makeRating(1500));
    (void)lobby_->addPartyMember(partyId.value(), 2, "Bob", makeRating(1600));
    (void)lobby_->enqueueParty(partyId.value());

    auto result = lobby_->dequeueParty(partyId.value());
    EXPECT_TRUE(result.hasValue());
    EXPECT_FALSE(lobby_->isPlayerQueued(1));
    EXPECT_FALSE(lobby_->isPlayerQueued(2));
}

TEST_F(LobbyServerTest, DequeuePartyNotFoundFails) {
    auto result = lobby_->dequeueParty(999);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PartyNotFound);
}

// -- Match processing ---------------------------------------------------------

TEST_F(LobbyServerTest, ProcessMatchmakingFormsMatch) {
    (void)lobby_->enqueuePlayer(1, makeRating(1500));
    (void)lobby_->enqueuePlayer(2, makeRating(1550));

    auto matches = lobby_->processMatchmaking();
    EXPECT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0].players.size(), 2u);
}

TEST_F(LobbyServerTest, ProcessMatchmakingNoMatchPossible) {
    (void)lobby_->enqueuePlayer(1, makeRating(1500));
    auto matches = lobby_->processMatchmaking();
    EXPECT_TRUE(matches.empty());
}

TEST_F(LobbyServerTest, ProcessMatchmakingMultipleMatches) {
    (void)lobby_->enqueuePlayer(1, makeRating(1500));
    (void)lobby_->enqueuePlayer(2, makeRating(1510));
    (void)lobby_->enqueuePlayer(3, makeRating(1600));
    (void)lobby_->enqueuePlayer(4, makeRating(1610));

    auto matches = lobby_->processMatchmaking();
    EXPECT_EQ(matches.size(), 2u);
}

// -- ELO updates --------------------------------------------------------------

TEST_F(LobbyServerTest, UpdateRatingsWinnerGains) {
    PlayerRating winner = makeRating(1500);
    PlayerRating loser = makeRating(1500);

    LobbyServer::updateRatings(winner, loser);

    EXPECT_GT(winner.mmr, 1500);
    EXPECT_LT(loser.mmr, 1500);
    EXPECT_EQ(winner.gamesPlayed, 51u);
    EXPECT_EQ(loser.gamesPlayed, 51u);
}

TEST_F(LobbyServerTest, UpdateRatingsUpsetLargerSwing) {
    PlayerRating underdog = makeRating(1400);
    PlayerRating favorite = makeRating(1600);

    LobbyServer::updateRatings(underdog, favorite);

    int32_t underdogGain = underdog.mmr - 1400;
    EXPECT_GT(underdogGain, 16); // Upset win yields larger gain.
}

TEST_F(LobbyServerTest, UpdateRatingsConservation) {
    PlayerRating winner = makeRating(1500, 50);
    PlayerRating loser = makeRating(1500, 50);

    LobbyServer::updateRatings(winner, loser);

    // Same K-factor → total rating change is zero-sum.
    EXPECT_EQ(winner.mmr + loser.mmr, 3000);
}

// -- Statistics ---------------------------------------------------------------

TEST_F(LobbyServerTest, StatsInitiallyZero) {
    auto s = lobby_->stats();
    EXPECT_EQ(s.queuedPlayers, 0u);
    EXPECT_EQ(s.matchesFormed, 0u);
    EXPECT_EQ(s.activeParties, 0u);
    EXPECT_EQ(s.partiesCreated, 0u);
    EXPECT_EQ(s.partiesDisbanded, 0u);
}

TEST_F(LobbyServerTest, StatsTracksQueuesAndMatches) {
    (void)lobby_->enqueuePlayer(1, makeRating(1500));
    (void)lobby_->enqueuePlayer(2, makeRating(1510));

    auto s1 = lobby_->stats();
    EXPECT_EQ(s1.queuedPlayers, 2u);

    (void)lobby_->processMatchmaking();

    auto s2 = lobby_->stats();
    EXPECT_EQ(s2.queuedPlayers, 0u);
    EXPECT_EQ(s2.matchesFormed, 1u);
}

TEST_F(LobbyServerTest, StatsTracksParties) {
    auto p1 = lobby_->createParty(1, "Alice", makeRating(1500));
    (void)lobby_->createParty(2, "Bob", makeRating(1500));

    auto s1 = lobby_->stats();
    EXPECT_EQ(s1.activeParties, 2u);
    EXPECT_EQ(s1.partiesCreated, 2u);

    (void)lobby_->disbandParty(p1.value(), 1);

    auto s2 = lobby_->stats();
    EXPECT_EQ(s2.activeParties, 1u);
    EXPECT_EQ(s2.partiesDisbanded, 1u);
}

TEST_F(LobbyServerTest, ConfigAccessor) {
    EXPECT_EQ(lobby_->config().maxPartySize, 5u);
    EXPECT_EQ(lobby_->config().queueConfig.minPlayersPerMatch, 2u);
}
