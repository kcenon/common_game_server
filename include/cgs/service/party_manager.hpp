#pragma once

/// @file party_manager.hpp
/// @brief Party (group) management for the Lobby Server.
///
/// PartyManager provides create, invite, join, leave, kick, promote,
/// and disband operations for player parties. Parties can queue for
/// matchmaking as a unit.
///
/// @see SRS-SVC-004.3
/// @see SDS-MOD-033

#include "cgs/foundation/game_result.hpp"
#include "cgs/service/lobby_types.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cgs::service {

/// Thread-safe party manager.
///
/// Usage:
/// @code
///   PartyManager pm(5); // max 5 members per party
///   auto partyId = pm.createParty(leaderId, "LeaderName", leaderRating);
///   pm.addMember(partyId.value(), playerId, "Player", rating);
///   pm.removeMember(partyId.value(), playerId);
/// @endcode
class PartyManager {
public:
    /// @param maxPartySize Maximum members per party (default: 5).
    explicit PartyManager(uint32_t maxPartySize = 5);

    // -- Party lifecycle ------------------------------------------------------

    /// Create a new party with the given player as leader.
    [[nodiscard]] cgs::foundation::GameResult<PartyId> createParty(uint64_t leaderId,
                                                                   const std::string& leaderName,
                                                                   PlayerRating leaderRating);

    /// Disband a party. Only the leader can do this.
    [[nodiscard]] cgs::foundation::GameResult<void> disbandParty(PartyId partyId,
                                                                 uint64_t requesterId);

    // -- Member management ----------------------------------------------------

    /// Add a member to a party.
    [[nodiscard]] cgs::foundation::GameResult<void> addMember(PartyId partyId,
                                                              uint64_t playerId,
                                                              const std::string& name,
                                                              PlayerRating rating);

    /// Remove a member from a party (leave or kick).
    ///
    /// If the leader leaves, the party is automatically disbanded.
    /// If a non-leader member is removed by the leader (kick), it succeeds.
    /// If a member removes themselves (leave), it always succeeds.
    [[nodiscard]] cgs::foundation::GameResult<void> removeMember(PartyId partyId,
                                                                 uint64_t playerId);

    /// Promote a member to party leader.
    [[nodiscard]] cgs::foundation::GameResult<void> promoteLeader(PartyId partyId,
                                                                  uint64_t requesterId,
                                                                  uint64_t newLeaderId);

    // -- Queue integration ----------------------------------------------------

    /// Mark a party as in-queue.
    [[nodiscard]] cgs::foundation::GameResult<void> setInQueue(PartyId partyId, bool inQueue);

    // -- Queries --------------------------------------------------------------

    /// Get party info.
    [[nodiscard]] std::optional<Party> getParty(PartyId partyId) const;

    /// Get the party a player belongs to.
    [[nodiscard]] std::optional<PartyId> getPlayerParty(uint64_t playerId) const;

    /// Check if a player is in any party.
    [[nodiscard]] bool isInParty(uint64_t playerId) const;

    /// Get the number of active parties.
    [[nodiscard]] std::size_t partyCount() const;

    /// Calculate the average MMR across all party members.
    [[nodiscard]] std::optional<int32_t> averagePartyRating(PartyId partyId) const;

private:
    /// Generate a unique party ID.
    [[nodiscard]] PartyId nextPartyId();

    /// Internal: remove party and all player mappings.
    void removePartyInternal(PartyId partyId);

    uint32_t maxPartySize_;
    std::unordered_map<PartyId, Party> parties_;
    std::unordered_map<uint64_t, PartyId> playerParty_;
    uint64_t nextPartyId_ = 1;
    mutable std::mutex mutex_;
};

}  // namespace cgs::service
