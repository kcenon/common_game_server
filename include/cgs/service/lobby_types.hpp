#pragma once

/// @file lobby_types.hpp
/// @brief Core types for the Lobby Server matchmaking system.
///
/// Defines game modes, regions, player ratings (ELO/MMR),
/// matchmaking tickets, match results, queue configuration,
/// and party management types.
///
/// @see SRS-SVC-004
/// @see SDS-MOD-033

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace cgs::service {

/// Available game modes for matchmaking.
enum class GameMode : uint8_t {
    Duel,          ///< 1v1 match.
    Arena,         ///< Small team PvP (e.g. 3v3).
    Battleground,  ///< Large team PvP (e.g. 10v10).
    Dungeon,       ///< PvE group content.
    Raid           ///< Large PvE content.
};

/// Server regions for latency-based matchmaking.
enum class Region : uint8_t {
    Any,     ///< No preference (match with any region).
    NAEast,  ///< North America East.
    NAWest,  ///< North America West.
    EU,      ///< Europe.
    Asia,    ///< Asia.
    Oceania  ///< Oceania.
};

/// Player skill rating for matchmaking.
struct PlayerRating {
    int32_t mmr = 1500;          ///< Match-Making Rating (Elo-based).
    float uncertainty = 350.0f;  ///< Rating uncertainty (higher = less confident).
    uint32_t gamesPlayed = 0;    ///< Total ranked games played.
};

/// A player's request to join a matchmaking queue.
struct MatchmakingTicket {
    uint64_t playerId = 0;
    PlayerRating rating;
    GameMode mode = GameMode::Duel;
    Region region = Region::Any;
    std::chrono::steady_clock::time_point enqueuedAt;
};

/// Result of a successful match.
struct MatchResult {
    uint64_t matchId = 0;
    std::vector<MatchmakingTicket> players;
    float averageRating = 0.0f;
    float matchQuality = 0.0f;  ///< 0.0 = worst, 1.0 = perfect.
};

/// Configuration for a matchmaking queue.
struct QueueConfig {
    uint32_t minPlayersPerMatch = 2;  ///< Minimum players to form a match.
    uint32_t maxPlayersPerMatch = 2;  ///< Maximum players per match.

    int32_t initialRatingTolerance = 100;  ///< Starting MMR window.
    int32_t maxRatingTolerance = 500;      ///< Upper bound on MMR window.
    int32_t expansionStep = 50;            ///< MMR window growth per interval.

    /// How often the tolerance expands for waiting players.
    std::chrono::seconds expansionInterval{10};

    uint32_t maxQueueSize = 10000;  ///< Maximum players in the queue.
};

// -- Party types (SRS-SVC-004.3) ---------------------------------------------

/// Unique identifier for a party.
using PartyId = uint64_t;

/// A member within a party.
struct PartyMember {
    uint64_t playerId = 0;
    std::string name;
    PlayerRating rating;
    bool isReady = false;
};

/// A party (group) of players.
struct Party {
    PartyId id = 0;
    uint64_t leaderId = 0;
    std::vector<PartyMember> members;
    uint32_t maxSize = 5;
    bool inQueue = false;
    GameMode preferredMode = GameMode::Dungeon;
};

/// Configuration for the lobby server.
struct LobbyConfig {
    /// Queue configuration for matchmaking.
    QueueConfig queueConfig;

    /// Maximum party size.
    uint32_t maxPartySize = 5;
};

/// Runtime statistics for the lobby server.
struct LobbyStats {
    std::size_t queuedPlayers = 0;
    uint64_t matchesFormed = 0;
    std::size_t activeParties = 0;
    uint64_t partiesCreated = 0;
    uint64_t partiesDisbanded = 0;
};

}  // namespace cgs::service
