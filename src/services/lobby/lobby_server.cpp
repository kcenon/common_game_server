/// @file lobby_server.cpp
/// @brief LobbyServer implementation orchestrating matchmaking,
///        party management, and match processing.

#include "cgs/service/lobby_server.hpp"

#include <atomic>

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"
#include "cgs/service/elo_calculator.hpp"
#include "cgs/service/matchmaking_queue.hpp"
#include "cgs/service/party_manager.hpp"

namespace cgs::service {

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;

// -- Impl ---------------------------------------------------------------------

struct LobbyServer::Impl {
    LobbyConfig config;

    MatchmakingQueue queue;
    PartyManager parties;

    std::atomic<bool> running{false};
    std::atomic<uint64_t> matchesFormed{0};
    std::atomic<uint64_t> partiesCreated{0};
    std::atomic<uint64_t> partiesDisbanded{0};

    explicit Impl(LobbyConfig cfg)
        : config(std::move(cfg))
        , queue(config.queueConfig)
        , parties(config.maxPartySize) {}
};

// -- Construction / destruction / move ----------------------------------------

LobbyServer::LobbyServer(LobbyConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

LobbyServer::~LobbyServer() {
    if (impl_ && impl_->running.load()) {
        stop();
    }
}

LobbyServer::LobbyServer(LobbyServer&&) noexcept = default;
LobbyServer& LobbyServer::operator=(LobbyServer&&) noexcept = default;

// -- Lifecycle ----------------------------------------------------------------

GameResult<void> LobbyServer::start() {
    if (impl_->running.load()) {
        return GameResult<void>::err(
            GameError(ErrorCode::LobbyAlreadyStarted,
                      "lobby server is already running"));
    }

    impl_->running.store(true);
    return GameResult<void>::ok();
}

void LobbyServer::stop() {
    impl_->running.store(false);
}

bool LobbyServer::isRunning() const noexcept {
    return impl_->running.load();
}

// -- Solo matchmaking ---------------------------------------------------------

GameResult<void> LobbyServer::enqueuePlayer(
    uint64_t playerId,
    PlayerRating rating,
    GameMode mode,
    Region region) {
    if (!impl_->running.load()) {
        return GameResult<void>::err(
            GameError(ErrorCode::LobbyNotStarted,
                      "lobby server is not running"));
    }

    MatchmakingTicket ticket;
    ticket.playerId = playerId;
    ticket.rating = rating;
    ticket.mode = mode;
    ticket.region = region;
    ticket.enqueuedAt = std::chrono::steady_clock::now();

    return impl_->queue.enqueue(std::move(ticket));
}

bool LobbyServer::dequeuePlayer(uint64_t playerId) {
    return impl_->queue.dequeue(playerId);
}

bool LobbyServer::isPlayerQueued(uint64_t playerId) const {
    return impl_->queue.isQueued(playerId);
}

// -- Party management ---------------------------------------------------------

GameResult<PartyId> LobbyServer::createParty(
    uint64_t leaderId,
    const std::string& leaderName,
    PlayerRating leaderRating) {
    if (!impl_->running.load()) {
        return GameResult<PartyId>::err(
            GameError(ErrorCode::LobbyNotStarted,
                      "lobby server is not running"));
    }

    auto result = impl_->parties.createParty(
        leaderId, leaderName, leaderRating);

    if (result.hasValue()) {
        impl_->partiesCreated.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

GameResult<void> LobbyServer::disbandParty(
    PartyId partyId, uint64_t requesterId) {
    auto result = impl_->parties.disbandParty(partyId, requesterId);

    if (result.hasValue()) {
        impl_->partiesDisbanded.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

GameResult<void> LobbyServer::addPartyMember(
    PartyId partyId,
    uint64_t playerId,
    const std::string& name,
    PlayerRating rating) {
    return impl_->parties.addMember(partyId, playerId, name, rating);
}

GameResult<void> LobbyServer::removePartyMember(
    PartyId partyId, uint64_t playerId) {
    return impl_->parties.removeMember(partyId, playerId);
}

GameResult<void> LobbyServer::promotePartyLeader(
    PartyId partyId, uint64_t requesterId, uint64_t newLeaderId) {
    return impl_->parties.promoteLeader(partyId, requesterId, newLeaderId);
}

std::optional<Party> LobbyServer::getParty(PartyId partyId) const {
    return impl_->parties.getParty(partyId);
}

std::optional<PartyId> LobbyServer::getPlayerParty(
    uint64_t playerId) const {
    return impl_->parties.getPlayerParty(playerId);
}

// -- Party matchmaking --------------------------------------------------------

GameResult<void> LobbyServer::enqueueParty(PartyId partyId) {
    if (!impl_->running.load()) {
        return GameResult<void>::err(
            GameError(ErrorCode::LobbyNotStarted,
                      "lobby server is not running"));
    }

    auto party = impl_->parties.getParty(partyId);
    if (!party.has_value()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PartyNotFound,
                      "party not found"));
    }

    // Calculate average rating for the party.
    auto avgRating = impl_->parties.averagePartyRating(partyId);
    if (!avgRating.has_value()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PartyNotFound,
                      "party has no members"));
    }

    // Enqueue each member with the party's average rating.
    PlayerRating partyRating;
    partyRating.mmr = *avgRating;

    for (const auto& member : party->members) {
        MatchmakingTicket ticket;
        ticket.playerId = member.playerId;
        ticket.rating = partyRating;
        ticket.mode = party->preferredMode;
        ticket.region = Region::Any;
        ticket.enqueuedAt = std::chrono::steady_clock::now();

        auto enqueueResult = impl_->queue.enqueue(std::move(ticket));
        if (enqueueResult.hasError()) {
            // Rollback: remove already-enqueued members.
            for (const auto& m : party->members) {
                if (m.playerId == member.playerId) {
                    break;
                }
                (void)impl_->queue.dequeue(m.playerId);
            }
            return enqueueResult;
        }
    }

    // Mark party as in-queue.
    (void)impl_->parties.setInQueue(partyId, true);

    return GameResult<void>::ok();
}

GameResult<void> LobbyServer::dequeueParty(PartyId partyId) {
    auto party = impl_->parties.getParty(partyId);
    if (!party.has_value()) {
        return GameResult<void>::err(
            GameError(ErrorCode::PartyNotFound,
                      "party not found"));
    }

    for (const auto& member : party->members) {
        (void)impl_->queue.dequeue(member.playerId);
    }

    (void)impl_->parties.setInQueue(partyId, false);

    return GameResult<void>::ok();
}

// -- Match processing ---------------------------------------------------------

std::vector<MatchResult> LobbyServer::processMatchmaking() {
    std::vector<MatchResult> matches;

    // Drain all possible matches from the queue.
    while (auto match = impl_->queue.tryMatch()) {
        impl_->matchesFormed.fetch_add(1, std::memory_order_relaxed);
        matches.push_back(std::move(*match));
    }

    return matches;
}

// -- ELO updates --------------------------------------------------------------

void LobbyServer::updateRatings(
    PlayerRating& winnerRating,
    PlayerRating& loserRating) {
    float expectedWin = EloCalculator::expectedScore(
        winnerRating.mmr, loserRating.mmr);
    float expectedLoss = EloCalculator::expectedScore(
        loserRating.mmr, winnerRating.mmr);

    int32_t kWinner = EloCalculator::suggestedKFactor(
        winnerRating.gamesPlayed);
    int32_t kLoser = EloCalculator::suggestedKFactor(
        loserRating.gamesPlayed);

    winnerRating.mmr = EloCalculator::newRating(
        winnerRating.mmr, 1.0f, expectedWin, kWinner);
    loserRating.mmr = EloCalculator::newRating(
        loserRating.mmr, 0.0f, expectedLoss, kLoser);

    winnerRating.gamesPlayed++;
    loserRating.gamesPlayed++;
}

// -- Statistics ---------------------------------------------------------------

LobbyStats LobbyServer::stats() const {
    LobbyStats s;
    s.queuedPlayers = impl_->queue.queueSize();
    s.matchesFormed = impl_->matchesFormed.load(std::memory_order_relaxed);
    s.activeParties = impl_->parties.partyCount();
    s.partiesCreated = impl_->partiesCreated.load(std::memory_order_relaxed);
    s.partiesDisbanded = impl_->partiesDisbanded.load(std::memory_order_relaxed);
    return s;
}

const LobbyConfig& LobbyServer::config() const noexcept {
    return impl_->config;
}

} // namespace cgs::service
