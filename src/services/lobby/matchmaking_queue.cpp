/// @file matchmaking_queue.cpp
/// @brief MatchmakingQueue implementation.

#include "cgs/service/matchmaking_queue.hpp"

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"
#include "cgs/service/elo_calculator.hpp"

#include <algorithm>
#include <numeric>

namespace cgs::service {

MatchmakingQueue::MatchmakingQueue(QueueConfig config) : config_(std::move(config)) {}

cgs::foundation::GameResult<void> MatchmakingQueue::enqueue(MatchmakingTicket ticket) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (tickets_.size() >= static_cast<std::size_t>(config_.maxQueueSize)) {
        return cgs::Result<void, cgs::foundation::GameError>::err(cgs::foundation::GameError(
            cgs::foundation::ErrorCode::QueueFull, "Matchmaking queue is full"));
    }

    if (playerIndex_.count(ticket.playerId) > 0) {
        return cgs::Result<void, cgs::foundation::GameError>::err(
            cgs::foundation::GameError(cgs::foundation::ErrorCode::AlreadyInQueue,
                                       "Player is already in the matchmaking queue"));
    }

    playerIndex_[ticket.playerId] = tickets_.size();
    tickets_.push_back(std::move(ticket));

    return cgs::Result<void, cgs::foundation::GameError>::ok();
}

bool MatchmakingQueue::dequeue(uint64_t playerId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = playerIndex_.find(playerId);
    if (it == playerIndex_.end()) {
        return false;
    }

    auto idx = it->second;
    auto lastIdx = tickets_.size() - 1;

    // Swap with last element for O(1) removal.
    if (idx != lastIdx) {
        std::swap(tickets_[idx], tickets_[lastIdx]);
        playerIndex_[tickets_[idx].playerId] = idx;
    }

    tickets_.pop_back();
    playerIndex_.erase(it);
    return true;
}

std::optional<MatchResult> MatchmakingQueue::tryMatch() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (tickets_.size() < static_cast<std::size_t>(config_.minPlayersPerMatch)) {
        return std::nullopt;
    }

    // Try to build a match starting from each ticket as the anchor.
    // Prioritize tickets that have waited longest (front of the vector).
    for (std::size_t anchorIdx = 0; anchorIdx < tickets_.size(); ++anchorIdx) {
        const auto& anchor = tickets_[anchorIdx];
        int32_t anchorTolerance = effectiveTolerance(anchor);

        // Collect compatible tickets.
        std::vector<std::size_t> candidates;
        candidates.push_back(anchorIdx);

        for (std::size_t i = 0; i < tickets_.size(); ++i) {
            if (i == anchorIdx) {
                continue;
            }

            const auto& candidate = tickets_[i];

            // Must share the same game mode.
            if (candidate.mode != anchor.mode) {
                continue;
            }

            // Region must be compatible (Any matches everything).
            if (anchor.region != Region::Any && candidate.region != Region::Any &&
                anchor.region != candidate.region) {
                continue;
            }

            // Check rating tolerance from both perspectives.
            int32_t candidateTolerance = effectiveTolerance(candidate);
            int32_t tolerance = std::max(anchorTolerance, candidateTolerance);

            if (!EloCalculator::isWithinTolerance(
                    anchor.rating.mmr, candidate.rating.mmr, tolerance)) {
                continue;
            }

            candidates.push_back(i);

            if (candidates.size() >= static_cast<std::size_t>(config_.maxPlayersPerMatch)) {
                break;
            }
        }

        // Check if we have enough players.
        if (candidates.size() < static_cast<std::size_t>(config_.minPlayersPerMatch)) {
            continue;
        }

        // Build the match result.
        MatchResult result;
        result.matchId = nextMatchId();

        std::vector<int32_t> ratings;
        for (auto idx : candidates) {
            result.players.push_back(tickets_[idx]);
            ratings.push_back(tickets_[idx].rating.mmr);
        }

        float sum = 0.0f;
        for (auto r : ratings) {
            sum += static_cast<float>(r);
        }
        result.averageRating = sum / static_cast<float>(ratings.size());
        result.matchQuality = EloCalculator::matchQuality(ratings);

        // Remove matched players from the queue (reverse order to
        // preserve indices during removal).
        std::sort(candidates.begin(), candidates.end(), std::greater<>());
        for (auto idx : candidates) {
            auto lastIdx = tickets_.size() - 1;
            auto pid = tickets_[idx].playerId;

            if (idx != lastIdx) {
                std::swap(tickets_[idx], tickets_[lastIdx]);
                playerIndex_[tickets_[idx].playerId] = idx;
            }

            tickets_.pop_back();
            playerIndex_.erase(pid);
        }

        return result;
    }

    return std::nullopt;
}

bool MatchmakingQueue::isQueued(uint64_t playerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return playerIndex_.count(playerId) > 0;
}

std::optional<MatchmakingTicket> MatchmakingQueue::getTicket(uint64_t playerId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = playerIndex_.find(playerId);
    if (it == playerIndex_.end()) {
        return std::nullopt;
    }
    return tickets_[it->second];
}

std::size_t MatchmakingQueue::queueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tickets_.size();
}

const QueueConfig& MatchmakingQueue::config() const noexcept {
    return config_;
}

int32_t MatchmakingQueue::effectiveTolerance(const MatchmakingTicket& ticket) const {
    auto now = std::chrono::steady_clock::now();
    auto waited = std::chrono::duration_cast<std::chrono::seconds>(now - ticket.enqueuedAt);

    if (config_.expansionInterval.count() <= 0) {
        return config_.initialRatingTolerance;
    }

    auto expansions = waited.count() / config_.expansionInterval.count();
    int32_t expanded =
        config_.initialRatingTolerance + static_cast<int32_t>(expansions) * config_.expansionStep;

    return std::min(expanded, config_.maxRatingTolerance);
}

uint64_t MatchmakingQueue::nextMatchId() {
    return nextMatchId_.fetch_add(1);
}

}  // namespace cgs::service
