/// @file elo_calculator.cpp
/// @brief EloCalculator implementation.

#include "cgs/service/elo_calculator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace cgs::service {

float EloCalculator::expectedScore(int32_t ratingA, int32_t ratingB) {
    float exponent =
        static_cast<float>(ratingB - ratingA) / 400.0f;
    return 1.0f / (1.0f + std::pow(10.0f, exponent));
}

int32_t EloCalculator::newRating(
    int32_t currentRating,
    float actualScore,
    float expected,
    int32_t kFactor) {
    float delta =
        static_cast<float>(kFactor) * (actualScore - expected);
    return currentRating + static_cast<int32_t>(std::round(delta));
}

float EloCalculator::matchQuality(const std::vector<int32_t>& ratings) {
    if (ratings.size() < 2) {
        return 1.0f;
    }

    // Average rating.
    double sum = std::accumulate(ratings.begin(), ratings.end(), 0.0);
    double avg = sum / static_cast<double>(ratings.size());

    // Standard deviation of ratings.
    double sqSum = 0.0;
    for (auto r : ratings) {
        double diff = static_cast<double>(r) - avg;
        sqSum += diff * diff;
    }
    double stddev = std::sqrt(sqSum / static_cast<double>(ratings.size()));

    // Map standard deviation to quality: 0 stddev = 1.0, 400 stddev = 0.0.
    // Using exponential decay: quality = exp(-stddev / 200).
    float quality = std::exp(static_cast<float>(-stddev / 200.0));
    return std::clamp(quality, 0.0f, 1.0f);
}

bool EloCalculator::isWithinTolerance(
    int32_t ratingA, int32_t ratingB, int32_t tolerance) {
    return std::abs(ratingA - ratingB) <= tolerance;
}

int32_t EloCalculator::suggestedKFactor(uint32_t gamesPlayed) {
    if (gamesPlayed < 30) {
        return 40; // Provisional: converge quickly.
    }
    if (gamesPlayed < 100) {
        return 32; // Standard.
    }
    return 16; // Veteran: stable rating.
}

} // namespace cgs::service
