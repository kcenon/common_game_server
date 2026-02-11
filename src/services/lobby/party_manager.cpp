/// @file party_manager.cpp
/// @brief PartyManager implementation for party lifecycle and member management.

#include "cgs/service/party_manager.hpp"

#include <algorithm>
#include <numeric>

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"

namespace cgs::service {

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;

PartyManager::PartyManager(uint32_t maxPartySize)
    : maxPartySize_(maxPartySize) {}

// -- Party lifecycle ----------------------------------------------------------

GameResult<PartyId> PartyManager::createParty(
    uint64_t leaderId,
    const std::string& leaderName,
    PlayerRating leaderRating) {
    std::lock_guard lock(mutex_);

    if (playerParty_.contains(leaderId)) {
        return GameResult<PartyId>::err(
            GameError(ErrorCode::PlayerAlreadyInParty,
                      "player is already in a party"));
    }

    auto partyId = nextPartyId();

    Party party;
    party.id = partyId;
    party.leaderId = leaderId;
    party.maxSize = maxPartySize_;

    PartyMember leader;
    leader.playerId = leaderId;
    leader.name = leaderName;
    leader.rating = leaderRating;
    leader.isReady = true;
    party.members.push_back(std::move(leader));

    parties_.emplace(partyId, std::move(party));
    playerParty_.emplace(leaderId, partyId);

    return GameResult<PartyId>::ok(partyId);
}

GameResult<void> PartyManager::disbandParty(
    PartyId partyId, uint64_t requesterId) {
    std::lock_guard lock(mutex_);

    auto it = parties_.find(partyId);
    if (it == parties_.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PartyNotFound,
                      "party not found"));
    }

    if (it->second.leaderId != requesterId) {
        return GameResult<void>::err(
            GameError(ErrorCode::NotPartyLeader,
                      "only the party leader can disband the party"));
    }

    removePartyInternal(partyId);
    return GameResult<void>::ok();
}

// -- Member management --------------------------------------------------------

GameResult<void> PartyManager::addMember(
    PartyId partyId,
    uint64_t playerId,
    const std::string& name,
    PlayerRating rating) {
    std::lock_guard lock(mutex_);

    if (playerParty_.contains(playerId)) {
        return GameResult<void>::err(
            GameError(ErrorCode::PlayerAlreadyInParty,
                      "player is already in a party"));
    }

    auto it = parties_.find(partyId);
    if (it == parties_.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PartyNotFound,
                      "party not found"));
    }

    auto& party = it->second;

    if (party.members.size() >= static_cast<std::size_t>(party.maxSize)) {
        return GameResult<void>::err(
            GameError(ErrorCode::PartyFull,
                      "party is full"));
    }

    PartyMember member;
    member.playerId = playerId;
    member.name = name;
    member.rating = rating;
    party.members.push_back(std::move(member));

    playerParty_.emplace(playerId, partyId);

    return GameResult<void>::ok();
}

GameResult<void> PartyManager::removeMember(
    PartyId partyId, uint64_t playerId) {
    std::lock_guard lock(mutex_);

    auto partyIt = parties_.find(partyId);
    if (partyIt == parties_.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PartyNotFound,
                      "party not found"));
    }

    auto& party = partyIt->second;

    // Find the member.
    auto memberIt = std::find_if(
        party.members.begin(), party.members.end(),
        [playerId](const PartyMember& m) { return m.playerId == playerId; });

    if (memberIt == party.members.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PlayerNotInParty,
                      "player is not in the party"));
    }

    // If the leader leaves, disband the entire party.
    if (playerId == party.leaderId) {
        removePartyInternal(partyId);
        return GameResult<void>::ok();
    }

    // Remove non-leader member.
    party.members.erase(memberIt);
    playerParty_.erase(playerId);

    return GameResult<void>::ok();
}

GameResult<void> PartyManager::promoteLeader(
    PartyId partyId, uint64_t requesterId, uint64_t newLeaderId) {
    std::lock_guard lock(mutex_);

    auto it = parties_.find(partyId);
    if (it == parties_.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PartyNotFound,
                      "party not found"));
    }

    auto& party = it->second;

    if (party.leaderId != requesterId) {
        return GameResult<void>::err(
            GameError(ErrorCode::NotPartyLeader,
                      "only the party leader can promote"));
    }

    // Verify new leader is in the party.
    auto memberIt = std::find_if(
        party.members.begin(), party.members.end(),
        [newLeaderId](const PartyMember& m) {
            return m.playerId == newLeaderId;
        });

    if (memberIt == party.members.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PlayerNotInParty,
                      "target player is not in the party"));
    }

    party.leaderId = newLeaderId;

    return GameResult<void>::ok();
}

// -- Queue integration --------------------------------------------------------

GameResult<void> PartyManager::setInQueue(PartyId partyId, bool inQueue) {
    std::lock_guard lock(mutex_);

    auto it = parties_.find(partyId);
    if (it == parties_.end()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PartyNotFound,
                      "party not found"));
    }

    it->second.inQueue = inQueue;
    return GameResult<void>::ok();
}

// -- Queries ------------------------------------------------------------------

std::optional<Party> PartyManager::getParty(PartyId partyId) const {
    std::lock_guard lock(mutex_);

    auto it = parties_.find(partyId);
    if (it == parties_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<PartyId> PartyManager::getPlayerParty(
    uint64_t playerId) const {
    std::lock_guard lock(mutex_);

    auto it = playerParty_.find(playerId);
    if (it == playerParty_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool PartyManager::isInParty(uint64_t playerId) const {
    std::lock_guard lock(mutex_);
    return playerParty_.contains(playerId);
}

std::size_t PartyManager::partyCount() const {
    std::lock_guard lock(mutex_);
    return parties_.size();
}

std::optional<int32_t> PartyManager::averagePartyRating(
    PartyId partyId) const {
    std::lock_guard lock(mutex_);

    auto it = parties_.find(partyId);
    if (it == parties_.end()) {
        return std::nullopt;
    }

    const auto& members = it->second.members;
    if (members.empty()) {
        return std::nullopt;
    }

    int64_t sum = 0;
    for (const auto& m : members) {
        sum += static_cast<int64_t>(m.rating.mmr);
    }

    return static_cast<int32_t>(sum / static_cast<int64_t>(members.size()));
}

// -- Private helpers ----------------------------------------------------------

PartyId PartyManager::nextPartyId() {
    return nextPartyId_++;
}

void PartyManager::removePartyInternal(PartyId partyId) {
    auto it = parties_.find(partyId);
    if (it == parties_.end()) {
        return;
    }

    // Remove all player→party mappings.
    for (const auto& member : it->second.members) {
        playerParty_.erase(member.playerId);
    }

    parties_.erase(it);
}

} // namespace cgs::service
