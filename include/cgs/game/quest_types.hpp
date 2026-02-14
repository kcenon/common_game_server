#pragma once

/// @file quest_types.hpp
/// @brief Enumerations and constants for the quest system.
///
/// @see SDS-MOD-034
/// @see SRS-GML-005

#include <cstdint>

namespace cgs::game {

/// Maximum number of active quests per player.
constexpr std::size_t kMaxActiveQuests = 25;

/// Maximum number of objectives per quest.
constexpr std::size_t kMaxObjectivesPerQuest = 8;

/// Quest progression state machine.
///
/// Available -> Accepted -> ObjectivesComplete -> TurnedIn
///                      \-> Failed (expired/abandoned)
enum class QuestState : uint8_t {
    Available,           ///< Prerequisites met, can be accepted.
    Accepted,            ///< Accepted by player, objectives in progress.
    ObjectivesComplete,  ///< All objectives fulfilled, awaiting turn-in.
    TurnedIn,            ///< Turned in, rewards collected.
    Failed               ///< Timed out or abandoned.
};

/// Objective type classification.
enum class ObjectiveType : uint8_t {
    Kill,      ///< Kill N creatures of entry X.
    Collect,   ///< Collect N items of type X.
    Explore,   ///< Visit zone/area X.
    Interact,  ///< Interact with game object X.
    Escort,    ///< Escort NPC to location.
    Custom     ///< Plugin-defined objective.
};

/// Bitfield flags for quest properties.
enum class QuestFlags : uint16_t {
    None = 0x0000,
    Repeatable = 0x0001,  ///< Can be completed multiple times.
    Daily = 0x0002,       ///< Resets daily.
    Weekly = 0x0004,      ///< Resets weekly.
    Shareable = 0x0008,   ///< Can be shared with party.
    AutoAccept = 0x0010,  ///< Auto-accepted when prerequisites met.
    Timed = 0x0020        ///< Has a time limit.
};

/// Bitwise OR for QuestFlags.
constexpr QuestFlags operator|(QuestFlags lhs, QuestFlags rhs) noexcept {
    return static_cast<QuestFlags>(static_cast<uint16_t>(lhs) | static_cast<uint16_t>(rhs));
}

/// Bitwise AND for QuestFlags.
constexpr QuestFlags operator&(QuestFlags lhs, QuestFlags rhs) noexcept {
    return static_cast<QuestFlags>(static_cast<uint16_t>(lhs) & static_cast<uint16_t>(rhs));
}

/// Check whether a specific flag is set.
constexpr bool HasQuestFlag(QuestFlags flags, QuestFlags flag) noexcept {
    return (static_cast<uint16_t>(flags) & static_cast<uint16_t>(flag)) != 0;
}

/// Type of quest-related event for objective tracking.
enum class QuestEventType : uint8_t {
    Kill,     ///< A creature was killed.
    Collect,  ///< An item was collected.
    Explore,  ///< A zone/area was entered.
    Interact  ///< An object was interacted with.
};

}  // namespace cgs::game
