#pragma once

/// @file lobby_server.hpp
/// @brief Lobby Server orchestrating matchmaking, party management,
///        and game server instance allocation.
///
/// LobbyServer unifies MatchmakingQueue, EloCalculator, and PartyManager
/// into a single service entry point. Players can queue solo or as a
/// party, and matches are allocated to game server instances.
///
/// @see SRS-SVC-004
/// @see SDS-MOD-033

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cgs/foundation/game_result.hpp"
#include "cgs/service/lobby_types.hpp"

namespace cgs::service {

/// Lobby Server orchestrating matchmaking and party management.
///
/// Usage:
/// @code
///   LobbyConfig config;
///   LobbyServer lobby(config);
///   lobby.start();
///
///   // Solo queue
///   lobby.enqueuePlayer(playerId, rating, GameMode::Duel, Region::NAEast);
///
///   // Party operations
///   auto pid = lobby.createParty(leaderId, "Leader", rating);
///   lobby.addPartyMember(pid.value(), memberId, "Member", rating);
///   lobby.enqueueParty(pid.value());
///
///   // Periodic match processing
///   auto matches = lobby.processMatchmaking();
///
///   lobby.stop();
/// @endcode
class LobbyServer {
public:
    explicit LobbyServer(LobbyConfig config);
    ~LobbyServer();

    LobbyServer(const LobbyServer&) = delete;
    LobbyServer& operator=(const LobbyServer&) = delete;
    LobbyServer(LobbyServer&&) noexcept;
    LobbyServer& operator=(LobbyServer&&) noexcept;

    // -- Lifecycle ------------------------------------------------------------

    /// Start the lobby server.
    [[nodiscard]] cgs::foundation::GameResult<void> start();

    /// Stop the lobby server.
    void stop();

    /// Check if the server is running.
    [[nodiscard]] bool isRunning() const noexcept;

    // -- Solo matchmaking -----------------------------------------------------

    /// Enqueue a single player for matchmaking.
    [[nodiscard]] cgs::foundation::GameResult<void> enqueuePlayer(
        uint64_t playerId,
        PlayerRating rating,
        GameMode mode = GameMode::Duel,
        Region region = Region::Any);

    /// Remove a player from the matchmaking queue.
    [[nodiscard]] bool dequeuePlayer(uint64_t playerId);

    /// Check if a player is currently queued.
    [[nodiscard]] bool isPlayerQueued(uint64_t playerId) const;

    // -- Party management -----------------------------------------------------

    /// Create a new party with the given player as leader.
    [[nodiscard]] cgs::foundation::GameResult<PartyId> createParty(
        uint64_t leaderId,
        const std::string& leaderName,
        PlayerRating leaderRating);

    /// Disband a party (leader only).
    [[nodiscard]] cgs::foundation::GameResult<void> disbandParty(
        PartyId partyId, uint64_t requesterId);

    /// Add a member to a party.
    [[nodiscard]] cgs::foundation::GameResult<void> addPartyMember(
        PartyId partyId,
        uint64_t playerId,
        const std::string& name,
        PlayerRating rating);

    /// Remove a member from a party (leave or kick).
    [[nodiscard]] cgs::foundation::GameResult<void> removePartyMember(
        PartyId partyId, uint64_t playerId);

    /// Promote a member to party leader.
    [[nodiscard]] cgs::foundation::GameResult<void> promotePartyLeader(
        PartyId partyId, uint64_t requesterId, uint64_t newLeaderId);

    /// Get party info.
    [[nodiscard]] std::optional<Party> getParty(PartyId partyId) const;

    /// Get the party a player belongs to.
    [[nodiscard]] std::optional<PartyId> getPlayerParty(
        uint64_t playerId) const;

    // -- Party matchmaking ----------------------------------------------------

    /// Enqueue an entire party for matchmaking.
    ///
    /// All party members are enqueued using the party's average rating.
    /// The party is marked as in-queue.
    [[nodiscard]] cgs::foundation::GameResult<void> enqueueParty(
        PartyId partyId);

    /// Remove a party from the matchmaking queue.
    [[nodiscard]] cgs::foundation::GameResult<void> dequeueParty(
        PartyId partyId);

    // -- Match processing -----------------------------------------------------

    /// Run one matchmaking cycle.
    ///
    /// Attempts to form as many matches as possible from the current queue.
    /// @return The list of matches formed in this cycle.
    [[nodiscard]] std::vector<MatchResult> processMatchmaking();

    // -- ELO updates ----------------------------------------------------------

    /// Update player ratings after a match result.
    ///
    /// @param winnerRating  The winner's current rating (modified in-place).
    /// @param loserRating   The loser's current rating (modified in-place).
    static void updateRatings(
        PlayerRating& winnerRating,
        PlayerRating& loserRating);

    // -- Statistics -----------------------------------------------------------

    /// Get a snapshot of current lobby statistics.
    [[nodiscard]] LobbyStats stats() const;

    /// Get the configuration.
    [[nodiscard]] const LobbyConfig& config() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cgs::service
