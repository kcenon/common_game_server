#pragma once

/// @file matchmaking_queue.hpp
/// @brief Skill-based matchmaking queue with rating tolerance expansion.
///
/// MatchmakingQueue accepts player tickets and attempts to form
/// balanced matches based on ELO/MMR ratings.  Players who wait
/// longer get progressively wider rating tolerance, ensuring
/// everyone eventually finds a match.
///
/// @see SRS-SVC-004.1, SRS-SVC-004.2
/// @see SDS-MOD-033

#include "cgs/foundation/game_result.hpp"
#include "cgs/service/lobby_types.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cgs::service {

/// Thread-safe matchmaking queue with rating-based matching.
///
/// Usage:
/// @code
///   MatchmakingQueue queue(config);
///   queue.enqueue(ticket);
///   auto match = queue.tryMatch(); // Called periodically
///   if (match) { /* start game with match->players */ }
/// @endcode
class MatchmakingQueue {
public:
    /// Construct a queue with the given configuration.
    explicit MatchmakingQueue(QueueConfig config);

    /// Add a player to the queue.
    ///
    /// @return Error if the queue is full or the player is already queued.
    [[nodiscard]] cgs::foundation::GameResult<void> enqueue(MatchmakingTicket ticket);

    /// Remove a player from the queue.
    ///
    /// @return true if the player was found and removed.
    [[nodiscard]] bool dequeue(uint64_t playerId);

    /// Attempt to form a match from currently queued players.
    ///
    /// Scans the queue for a group of players within rating tolerance.
    /// Players who have waited longer get expanded tolerance windows.
    ///
    /// @return A MatchResult if a valid match was found, nullopt otherwise.
    [[nodiscard]] std::optional<MatchResult> tryMatch();

    /// Check if a player is currently in the queue.
    [[nodiscard]] bool isQueued(uint64_t playerId) const;

    /// Get a player's ticket.
    [[nodiscard]] std::optional<MatchmakingTicket> getTicket(uint64_t playerId) const;

    /// Get the current number of players in the queue.
    [[nodiscard]] std::size_t queueSize() const;

    /// Get the queue configuration.
    [[nodiscard]] const QueueConfig& config() const noexcept;

private:
    /// Calculate the effective rating tolerance for a ticket
    /// based on how long it has been waiting.
    [[nodiscard]] int32_t effectiveTolerance(const MatchmakingTicket& ticket) const;

    /// Generate a unique match ID.
    [[nodiscard]] uint64_t nextMatchId();

    QueueConfig config_;
    std::vector<MatchmakingTicket> tickets_;
    std::unordered_map<uint64_t, std::size_t> playerIndex_;
    std::atomic<uint64_t> nextMatchId_{1};
    mutable std::mutex mutex_;
};

}  // namespace cgs::service
