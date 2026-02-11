/// @file matchmaking_test.cpp
/// @brief Unit tests for EloCalculator and MatchmakingQueue.

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

#include "cgs/service/elo_calculator.hpp"
#include "cgs/service/lobby_types.hpp"
#include "cgs/service/matchmaking_queue.hpp"

using namespace cgs::service;
using namespace std::chrono_literals;

// ============================================================================
// EloCalculator Tests
// ============================================================================

class EloCalculatorTest : public ::testing::Test {};

TEST_F(EloCalculatorTest, ExpectedScoreEqualRatings) {
    float score = EloCalculator::expectedScore(1500, 1500);
    EXPECT_NEAR(score, 0.5f, 0.001f);
}

TEST_F(EloCalculatorTest, ExpectedScoreHigherRatingAdvantage) {
    float score = EloCalculator::expectedScore(1600, 1400);
    EXPECT_GT(score, 0.5f);
    EXPECT_LT(score, 1.0f);
}

TEST_F(EloCalculatorTest, ExpectedScoreLowerRatingDisadvantage) {
    float score = EloCalculator::expectedScore(1400, 1600);
    EXPECT_LT(score, 0.5f);
    EXPECT_GT(score, 0.0f);
}

TEST_F(EloCalculatorTest, ExpectedScoreSymmetry) {
    float scoreA = EloCalculator::expectedScore(1500, 1700);
    float scoreB = EloCalculator::expectedScore(1700, 1500);
    EXPECT_NEAR(scoreA + scoreB, 1.0f, 0.001f);
}

TEST_F(EloCalculatorTest, ExpectedScore400Difference) {
    // 400 point difference → expected ~0.909 for stronger player.
    float score = EloCalculator::expectedScore(1900, 1500);
    EXPECT_NEAR(score, 0.909f, 0.01f);
}

TEST_F(EloCalculatorTest, NewRatingWinIncreasesRating) {
    float expected = EloCalculator::expectedScore(1500, 1500);
    int32_t updated = EloCalculator::newRating(1500, 1.0f, expected);
    EXPECT_GT(updated, 1500);
}

TEST_F(EloCalculatorTest, NewRatingLossDecreasesRating) {
    float expected = EloCalculator::expectedScore(1500, 1500);
    int32_t updated = EloCalculator::newRating(1500, 0.0f, expected);
    EXPECT_LT(updated, 1500);
}

TEST_F(EloCalculatorTest, NewRatingDrawAgainstEqual) {
    float expected = EloCalculator::expectedScore(1500, 1500);
    int32_t updated = EloCalculator::newRating(1500, 0.5f, expected);
    EXPECT_EQ(updated, 1500);
}

TEST_F(EloCalculatorTest, NewRatingUpsetWinLargerGain) {
    // Weaker player (1400) beats stronger (1600).
    float expected = EloCalculator::expectedScore(1400, 1600);
    int32_t updated = EloCalculator::newRating(1400, 1.0f, expected, 32);

    // Expected win probability ~0.24, so gain = 32 * (1.0 - 0.24) ≈ 24.
    EXPECT_GT(updated - 1400, 20);
}

TEST_F(EloCalculatorTest, NewRatingHigherKFactorLargerChange) {
    float expected = EloCalculator::expectedScore(1500, 1500);
    int32_t k16 = EloCalculator::newRating(1500, 1.0f, expected, 16);
    int32_t k32 = EloCalculator::newRating(1500, 1.0f, expected, 32);
    int32_t k40 = EloCalculator::newRating(1500, 1.0f, expected, 40);

    EXPECT_LT(k16 - 1500, k32 - 1500);
    EXPECT_LT(k32 - 1500, k40 - 1500);
}

TEST_F(EloCalculatorTest, MatchQualityPerfectMatch) {
    float quality = EloCalculator::matchQuality({1500, 1500, 1500});
    EXPECT_NEAR(quality, 1.0f, 0.001f);
}

TEST_F(EloCalculatorTest, MatchQualityDecreasesWithSpread) {
    float perfect = EloCalculator::matchQuality({1500, 1500});
    float close = EloCalculator::matchQuality({1500, 1550});
    float wide = EloCalculator::matchQuality({1500, 1700});
    float veryWide = EloCalculator::matchQuality({1500, 2000});

    EXPECT_GT(perfect, close);
    EXPECT_GT(close, wide);
    EXPECT_GT(wide, veryWide);
}

TEST_F(EloCalculatorTest, MatchQualitySinglePlayer) {
    float quality = EloCalculator::matchQuality({1500});
    EXPECT_EQ(quality, 1.0f);
}

TEST_F(EloCalculatorTest, MatchQualityEmptyList) {
    float quality = EloCalculator::matchQuality({});
    EXPECT_EQ(quality, 1.0f);
}

TEST_F(EloCalculatorTest, IsWithinToleranceTrue) {
    EXPECT_TRUE(EloCalculator::isWithinTolerance(1500, 1550, 100));
    EXPECT_TRUE(EloCalculator::isWithinTolerance(1500, 1600, 100));
    EXPECT_TRUE(EloCalculator::isWithinTolerance(1500, 1500, 0));
}

TEST_F(EloCalculatorTest, IsWithinToleranceFalse) {
    EXPECT_FALSE(EloCalculator::isWithinTolerance(1500, 1601, 100));
    EXPECT_FALSE(EloCalculator::isWithinTolerance(1500, 1400, 99));
}

TEST_F(EloCalculatorTest, SuggestedKFactorProvisional) {
    EXPECT_EQ(EloCalculator::suggestedKFactor(0), 40);
    EXPECT_EQ(EloCalculator::suggestedKFactor(15), 40);
    EXPECT_EQ(EloCalculator::suggestedKFactor(29), 40);
}

TEST_F(EloCalculatorTest, SuggestedKFactorStandard) {
    EXPECT_EQ(EloCalculator::suggestedKFactor(30), 32);
    EXPECT_EQ(EloCalculator::suggestedKFactor(50), 32);
    EXPECT_EQ(EloCalculator::suggestedKFactor(99), 32);
}

TEST_F(EloCalculatorTest, SuggestedKFactorVeteran) {
    EXPECT_EQ(EloCalculator::suggestedKFactor(100), 16);
    EXPECT_EQ(EloCalculator::suggestedKFactor(1000), 16);
}

// ============================================================================
// LobbyTypes Tests
// ============================================================================

TEST(LobbyTypesTest, PlayerRatingDefaults) {
    PlayerRating rating;
    EXPECT_EQ(rating.mmr, 1500);
    EXPECT_FLOAT_EQ(rating.uncertainty, 350.0f);
    EXPECT_EQ(rating.gamesPlayed, 0u);
}

TEST(LobbyTypesTest, QueueConfigDefaults) {
    QueueConfig config;
    EXPECT_EQ(config.minPlayersPerMatch, 2u);
    EXPECT_EQ(config.maxPlayersPerMatch, 2u);
    EXPECT_EQ(config.initialRatingTolerance, 100);
    EXPECT_EQ(config.maxRatingTolerance, 500);
    EXPECT_EQ(config.expansionStep, 50);
    EXPECT_EQ(config.expansionInterval.count(), 10);
    EXPECT_EQ(config.maxQueueSize, 10000u);
}

// ============================================================================
// MatchmakingQueue Tests
// ============================================================================

class MatchmakingQueueTest : public ::testing::Test {
protected:
    QueueConfig config_;
    std::unique_ptr<MatchmakingQueue> queue_;

    void SetUp() override {
        config_.minPlayersPerMatch = 2;
        config_.maxPlayersPerMatch = 2;
        config_.initialRatingTolerance = 100;
        config_.maxRatingTolerance = 500;
        config_.expansionStep = 50;
        config_.expansionInterval = 10s;
        config_.maxQueueSize = 100;
        queue_ = std::make_unique<MatchmakingQueue>(config_);
    }

    MatchmakingTicket makeTicket(uint64_t playerId, int32_t mmr,
                                 GameMode mode = GameMode::Duel,
                                 Region region = Region::Any) {
        MatchmakingTicket ticket;
        ticket.playerId = playerId;
        ticket.rating.mmr = mmr;
        ticket.mode = mode;
        ticket.region = region;
        ticket.enqueuedAt = std::chrono::steady_clock::now();
        return ticket;
    }
};

TEST_F(MatchmakingQueueTest, InitialQueueIsEmpty) {
    EXPECT_EQ(queue_->queueSize(), 0u);
}

TEST_F(MatchmakingQueueTest, EnqueuePlayer) {
    auto result = queue_->enqueue(makeTicket(1, 1500));
    EXPECT_TRUE(result.hasValue());
    EXPECT_EQ(queue_->queueSize(), 1u);
}

TEST_F(MatchmakingQueueTest, EnqueueMultiplePlayers) {
    (void)queue_->enqueue(makeTicket(1, 1500));
    (void)queue_->enqueue(makeTicket(2, 1550));
    (void)queue_->enqueue(makeTicket(3, 1600));
    EXPECT_EQ(queue_->queueSize(), 3u);
}

TEST_F(MatchmakingQueueTest, EnqueueDuplicatePlayerFails) {
    (void)queue_->enqueue(makeTicket(1, 1500));
    auto result = queue_->enqueue(makeTicket(1, 1600));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(queue_->queueSize(), 1u);
}

TEST_F(MatchmakingQueueTest, EnqueueFullQueueFails) {
    QueueConfig small;
    small.maxQueueSize = 2;
    MatchmakingQueue smallQueue(small);

    (void)smallQueue.enqueue(makeTicket(1, 1500));
    (void)smallQueue.enqueue(makeTicket(2, 1500));
    auto result = smallQueue.enqueue(makeTicket(3, 1500));
    EXPECT_TRUE(result.hasError());
}

TEST_F(MatchmakingQueueTest, DequeuePlayer) {
    (void)queue_->enqueue(makeTicket(1, 1500));
    EXPECT_TRUE(queue_->dequeue(1));
    EXPECT_EQ(queue_->queueSize(), 0u);
}

TEST_F(MatchmakingQueueTest, DequeueNonexistentPlayerReturnsFalse) {
    EXPECT_FALSE(queue_->dequeue(999));
}

TEST_F(MatchmakingQueueTest, IsQueued) {
    (void)queue_->enqueue(makeTicket(1, 1500));
    EXPECT_TRUE(queue_->isQueued(1));
    EXPECT_FALSE(queue_->isQueued(2));
}

TEST_F(MatchmakingQueueTest, GetTicket) {
    (void)queue_->enqueue(makeTicket(42, 1750));
    auto ticket = queue_->getTicket(42);

    ASSERT_TRUE(ticket.has_value());
    EXPECT_EQ(ticket->playerId, 42u);
    EXPECT_EQ(ticket->rating.mmr, 1750);
}

TEST_F(MatchmakingQueueTest, GetTicketNonexistent) {
    auto ticket = queue_->getTicket(999);
    EXPECT_FALSE(ticket.has_value());
}

TEST_F(MatchmakingQueueTest, TryMatchNotEnoughPlayers) {
    (void)queue_->enqueue(makeTicket(1, 1500));
    auto match = queue_->tryMatch();
    EXPECT_FALSE(match.has_value());
}

TEST_F(MatchmakingQueueTest, TryMatchEmptyQueue) {
    auto match = queue_->tryMatch();
    EXPECT_FALSE(match.has_value());
}

TEST_F(MatchmakingQueueTest, TryMatchTwoCloseRatings) {
    (void)queue_->enqueue(makeTicket(1, 1500));
    (void)queue_->enqueue(makeTicket(2, 1550));

    auto match = queue_->tryMatch();
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->players.size(), 2u);
    EXPECT_GT(match->matchId, 0u);
    EXPECT_NEAR(match->averageRating, 1525.0f, 0.1f);
    EXPECT_GT(match->matchQuality, 0.5f);
}

TEST_F(MatchmakingQueueTest, MatchRemovesPlayersFromQueue) {
    (void)queue_->enqueue(makeTicket(1, 1500));
    (void)queue_->enqueue(makeTicket(2, 1550));

    auto match = queue_->tryMatch();
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(queue_->queueSize(), 0u);
    EXPECT_FALSE(queue_->isQueued(1));
    EXPECT_FALSE(queue_->isQueued(2));
}

TEST_F(MatchmakingQueueTest, TryMatchRatingsTooFarApart) {
    (void)queue_->enqueue(makeTicket(1, 1500));
    (void)queue_->enqueue(makeTicket(2, 1700)); // 200 apart > 100 tolerance.

    auto match = queue_->tryMatch();
    EXPECT_FALSE(match.has_value());
    EXPECT_EQ(queue_->queueSize(), 2u);
}

TEST_F(MatchmakingQueueTest, TryMatchDifferentGameModes) {
    (void)queue_->enqueue(makeTicket(1, 1500, GameMode::Duel));
    (void)queue_->enqueue(makeTicket(2, 1510, GameMode::Arena));

    auto match = queue_->tryMatch();
    EXPECT_FALSE(match.has_value());
}

TEST_F(MatchmakingQueueTest, TryMatchDifferentRegions) {
    (void)queue_->enqueue(makeTicket(1, 1500, GameMode::Duel, Region::EU));
    (void)queue_->enqueue(makeTicket(2, 1510, GameMode::Duel, Region::Asia));

    auto match = queue_->tryMatch();
    EXPECT_FALSE(match.has_value());
}

TEST_F(MatchmakingQueueTest, TryMatchAnyRegionMatchesAll) {
    (void)queue_->enqueue(makeTicket(1, 1500, GameMode::Duel, Region::Any));
    (void)queue_->enqueue(makeTicket(2, 1510, GameMode::Duel, Region::EU));

    auto match = queue_->tryMatch();
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->players.size(), 2u);
}

TEST_F(MatchmakingQueueTest, TryMatchSameRegion) {
    (void)queue_->enqueue(makeTicket(1, 1500, GameMode::Duel, Region::EU));
    (void)queue_->enqueue(makeTicket(2, 1510, GameMode::Duel, Region::EU));

    auto match = queue_->tryMatch();
    ASSERT_TRUE(match.has_value());
}

TEST_F(MatchmakingQueueTest, MatchIdsAreUnique) {
    (void)queue_->enqueue(makeTicket(1, 1500));
    (void)queue_->enqueue(makeTicket(2, 1510));
    auto m1 = queue_->tryMatch();

    (void)queue_->enqueue(makeTicket(3, 1500));
    (void)queue_->enqueue(makeTicket(4, 1510));
    auto m2 = queue_->tryMatch();

    ASSERT_TRUE(m1.has_value());
    ASSERT_TRUE(m2.has_value());
    EXPECT_NE(m1->matchId, m2->matchId);
}

TEST_F(MatchmakingQueueTest, MultiPlayerMatch) {
    QueueConfig arena;
    arena.minPlayersPerMatch = 3;
    arena.maxPlayersPerMatch = 3;
    arena.initialRatingTolerance = 200;
    MatchmakingQueue arenaQueue(arena);

    (void)arenaQueue.enqueue(makeTicket(1, 1500));
    (void)arenaQueue.enqueue(makeTicket(2, 1520));

    // Not enough for 3-player match.
    auto match = arenaQueue.tryMatch();
    EXPECT_FALSE(match.has_value());

    (void)arenaQueue.enqueue(makeTicket(3, 1540));

    match = arenaQueue.tryMatch();
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->players.size(), 3u);
}

TEST_F(MatchmakingQueueTest, MaxPlayersPerMatchRespected) {
    QueueConfig twoMax;
    twoMax.minPlayersPerMatch = 2;
    twoMax.maxPlayersPerMatch = 2;
    twoMax.initialRatingTolerance = 200;
    MatchmakingQueue twoQueue(twoMax);

    (void)twoQueue.enqueue(makeTicket(1, 1500));
    (void)twoQueue.enqueue(makeTicket(2, 1510));
    (void)twoQueue.enqueue(makeTicket(3, 1520));

    auto match = twoQueue.tryMatch();
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->players.size(), 2u);
    EXPECT_EQ(twoQueue.queueSize(), 1u);
}

TEST_F(MatchmakingQueueTest, ConfigAccessor) {
    EXPECT_EQ(queue_->config().minPlayersPerMatch, 2u);
    EXPECT_EQ(queue_->config().maxPlayersPerMatch, 2u);
    EXPECT_EQ(queue_->config().initialRatingTolerance, 100);
}

TEST_F(MatchmakingQueueTest, DequeueAfterEnqueueMultiple) {
    (void)queue_->enqueue(makeTicket(1, 1500));
    (void)queue_->enqueue(makeTicket(2, 1550));
    (void)queue_->enqueue(makeTicket(3, 1600));

    EXPECT_TRUE(queue_->dequeue(2));
    EXPECT_EQ(queue_->queueSize(), 2u);
    EXPECT_FALSE(queue_->isQueued(2));
    EXPECT_TRUE(queue_->isQueued(1));
    EXPECT_TRUE(queue_->isQueued(3));
}

TEST_F(MatchmakingQueueTest, MatchSelectsBestQualityGroup) {
    // Enqueue players: 1500, 1510, 1700.
    // With tolerance 100, only 1500+1510 should match.
    (void)queue_->enqueue(makeTicket(1, 1500));
    (void)queue_->enqueue(makeTicket(2, 1510));
    (void)queue_->enqueue(makeTicket(3, 1700));

    auto match = queue_->tryMatch();
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->players.size(), 2u);

    // Check that matched players are 1500 and 1510.
    bool has1 = false, has2 = false;
    for (const auto& p : match->players) {
        if (p.playerId == 1) has1 = true;
        if (p.playerId == 2) has2 = true;
    }
    EXPECT_TRUE(has1);
    EXPECT_TRUE(has2);

    // Player 3 should still be in queue.
    EXPECT_EQ(queue_->queueSize(), 1u);
    EXPECT_TRUE(queue_->isQueued(3));
}

TEST_F(MatchmakingQueueTest, ConsecutiveMatchesFromLargerPool) {
    // Enqueue 4 close-rated players.
    (void)queue_->enqueue(makeTicket(1, 1500));
    (void)queue_->enqueue(makeTicket(2, 1510));
    (void)queue_->enqueue(makeTicket(3, 1520));
    (void)queue_->enqueue(makeTicket(4, 1530));

    auto m1 = queue_->tryMatch();
    ASSERT_TRUE(m1.has_value());
    EXPECT_EQ(m1->players.size(), 2u);
    EXPECT_EQ(queue_->queueSize(), 2u);

    auto m2 = queue_->tryMatch();
    ASSERT_TRUE(m2.has_value());
    EXPECT_EQ(m2->players.size(), 2u);
    EXPECT_EQ(queue_->queueSize(), 0u);
}
