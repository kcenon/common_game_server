#pragma once

/// @file elo_calculator.hpp
/// @brief ELO/MMR rating calculations for skill-based matchmaking.
///
/// Provides the standard Elo expected-score formula, rating updates
/// with configurable K-factor, and match quality assessment.
///
/// @see SRS-SVC-004.2
/// @see SDS-MOD-033

#include <cstdint>
#include <vector>

namespace cgs::service {

/// Static utility class for ELO/MMR rating calculations.
///
/// Uses the standard Elo formula:
///   E(A) = 1 / (1 + 10^((R_B - R_A) / 400))
///
/// K-factor determines how much a single game can change a rating.
/// Typical values: 32 (new players), 24 (established), 16 (masters).
class EloCalculator {
public:
    EloCalculator() = delete;

    /// Default K-factor for rating adjustments.
    static constexpr int32_t kDefaultKFactor = 32;

    /// Calculate the expected score for player A vs player B.
    ///
    /// @param ratingA  Player A's current rating.
    /// @param ratingB  Player B's current rating.
    /// @return Expected score in [0.0, 1.0].
    [[nodiscard]] static float expectedScore(int32_t ratingA, int32_t ratingB);

    /// Calculate the new rating after a game.
    ///
    /// @param currentRating  Player's rating before the game.
    /// @param actualScore    Actual game result: 1.0 = win, 0.5 = draw, 0.0 = loss.
    /// @param expectedScore  Expected score from expectedScore().
    /// @param kFactor        Rating change sensitivity.
    /// @return Updated rating.
    [[nodiscard]] static int32_t newRating(
        int32_t currentRating,
        float actualScore,
        float expectedScore,
        int32_t kFactor = kDefaultKFactor);

    /// Assess the quality of a potential match.
    ///
    /// Returns a value in [0.0, 1.0] where 1.0 means all players
    /// have identical ratings (perfect match).  Quality decreases
    /// as the rating spread increases.
    ///
    /// @param ratings  MMR values of all players in the match.
    /// @return Quality score.
    [[nodiscard]] static float matchQuality(const std::vector<int32_t>& ratings);

    /// Check whether two ratings are within the given tolerance.
    [[nodiscard]] static bool isWithinTolerance(
        int32_t ratingA, int32_t ratingB, int32_t tolerance);

    /// Determine an appropriate K-factor based on games played.
    ///
    /// New players (< 30 games) get higher K to converge faster.
    /// Established players (30-100) get standard K.
    /// Veterans (100+) get lower K for stability.
    [[nodiscard]] static int32_t suggestedKFactor(uint32_t gamesPlayed);
};

} // namespace cgs::service
