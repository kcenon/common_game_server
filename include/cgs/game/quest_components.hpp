#pragma once

/// @file quest_components.hpp
/// @brief Quest ECS components: QuestLog, QuestEntry, QuestObjective, etc.
///
/// Each struct is a plain data component designed for sparse-set storage
/// via ComponentStorage<T>.  The QuestSystem operates on these components
/// to track quest state, update objectives, and process quest events.
///
/// @see SRS-GML-005.1 .. SRS-GML-005.4
/// @see SDS-MOD-034

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "cgs/ecs/entity.hpp"
#include "cgs/game/quest_types.hpp"

namespace cgs::game {

// -- QuestObjective (SRS-GML-005.2) ------------------------------------------

/// A single objective within a quest.
struct QuestObjective {
    ObjectiveType type = ObjectiveType::Kill;
    uint32_t targetId = 0;       ///< Creature entry, item ID, zone ID, or object ID.
    int32_t current = 0;         ///< Current progress count.
    int32_t required = 1;        ///< Required count for completion.
    bool completed = false;      ///< Whether this objective is fulfilled.

    /// Update progress and check completion.
    void AddProgress(int32_t amount) noexcept {
        if (completed) { return; }
        current = std::min(current + amount, required);
        if (current >= required) {
            completed = true;
        }
    }

    /// Check if the objective is fulfilled.
    [[nodiscard]] bool IsComplete() const noexcept {
        return completed || current >= required;
    }
};

// -- QuestReward --------------------------------------------------------------

/// Rewards granted upon quest turn-in.
struct QuestReward {
    int64_t experience = 0;
    int64_t currency = 0;
    std::vector<std::pair<uint32_t, uint32_t>> items; ///< {itemId, count}
};

// -- QuestTemplate ------------------------------------------------------------

/// Static quest definition data (shared, not per-entity).
struct QuestTemplate {
    uint32_t id = 0;
    std::string name;
    std::string description;
    uint32_t level = 0;
    std::vector<uint32_t> prerequisites;         ///< Required completed quest IDs.
    std::optional<uint32_t> chainNext;           ///< Next quest in chain.
    std::vector<QuestObjective> objectives;      ///< Objective templates.
    QuestReward rewards;
    QuestFlags flags = QuestFlags::None;
    float timeLimitSeconds = 0.0f;               ///< Time limit (0 = no limit).
};

// -- QuestEntry ---------------------------------------------------------------

/// A single active quest instance on a player.
struct QuestEntry {
    uint32_t questId = 0;
    uint32_t templateId = 0;
    QuestState state = QuestState::Accepted;
    std::vector<QuestObjective> objectives;
    float elapsedTime = 0.0f;                    ///< Time since acceptance.
    float timeLimit = 0.0f;                      ///< 0 = no limit.

    /// Check if all objectives are complete.
    [[nodiscard]] bool AllObjectivesComplete() const {
        return std::all_of(objectives.begin(), objectives.end(),
                           [](const QuestObjective& obj) {
                               return obj.IsComplete();
                           });
    }

    /// Update objective progress for a matching event.
    ///
    /// @return true if any objective was updated.
    bool UpdateObjective(ObjectiveType type, uint32_t targetId,
                         int32_t amount = 1) {
        if (state != QuestState::Accepted) { return false; }
        bool updated = false;
        for (auto& obj : objectives) {
            if (obj.type == type && obj.targetId == targetId && !obj.completed) {
                obj.AddProgress(amount);
                updated = true;
            }
        }
        if (updated && AllObjectivesComplete()) {
            state = QuestState::ObjectivesComplete;
        }
        return updated;
    }
};

// -- QuestLog (SRS-GML-005.1) ------------------------------------------------

/// Per-player quest tracking component.
///
/// Maintains active quests and a set of completed quest IDs for
/// prerequisite checking and chain unlocking.
struct QuestLog {
    std::vector<QuestEntry> activeQuests;
    std::unordered_set<uint32_t> completedQuestIds;
    uint32_t maxActiveQuests = kMaxActiveQuests;

    /// Accept a quest from a template.
    ///
    /// @return true if the quest was accepted.
    bool Accept(const QuestTemplate& tmpl) {
        if (activeQuests.size() >= maxActiveQuests) { return false; }
        if (HasQuest(tmpl.id)) { return false; }
        if (!CanAccept(tmpl)) { return false; }

        QuestEntry entry;
        entry.questId = tmpl.id;
        entry.templateId = tmpl.id;
        entry.state = QuestState::Accepted;
        entry.objectives = tmpl.objectives;
        entry.timeLimit = tmpl.timeLimitSeconds;
        activeQuests.push_back(std::move(entry));
        return true;
    }

    /// Abandon a quest.
    ///
    /// @return true if the quest was found and removed.
    bool Abandon(uint32_t questId) {
        auto it = std::find_if(activeQuests.begin(), activeQuests.end(),
                               [questId](const QuestEntry& e) {
                                   return e.questId == questId;
                               });
        if (it == activeQuests.end()) { return false; }
        activeQuests.erase(it);
        return true;
    }

    /// Turn in a completed quest.
    ///
    /// @return true if the quest was turned in successfully.
    bool TurnIn(uint32_t questId) {
        auto* entry = GetQuest(questId);
        if (!entry || entry->state != QuestState::ObjectivesComplete) {
            return false;
        }
        entry->state = QuestState::TurnedIn;
        completedQuestIds.insert(questId);
        return true;
    }

    /// Get an active quest entry by ID.
    [[nodiscard]] QuestEntry* GetQuest(uint32_t questId) {
        for (auto& entry : activeQuests) {
            if (entry.questId == questId) { return &entry; }
        }
        return nullptr;
    }

    /// Get an active quest entry by ID (const).
    [[nodiscard]] const QuestEntry* GetQuest(uint32_t questId) const {
        for (const auto& entry : activeQuests) {
            if (entry.questId == questId) { return &entry; }
        }
        return nullptr;
    }

    /// Check if a quest is currently active.
    [[nodiscard]] bool HasQuest(uint32_t questId) const {
        return std::any_of(activeQuests.begin(), activeQuests.end(),
                           [questId](const QuestEntry& e) {
                               return e.questId == questId;
                           });
    }

    /// Check if a quest has been completed (turned in) before.
    [[nodiscard]] bool IsCompleted(uint32_t questId) const {
        return completedQuestIds.contains(questId);
    }

    /// Check if all prerequisites for a quest template are met.
    [[nodiscard]] bool CanAccept(const QuestTemplate& tmpl) const {
        if (HasQuest(tmpl.id)) { return false; }
        if (IsCompleted(tmpl.id)
            && !HasQuestFlag(tmpl.flags, QuestFlags::Repeatable)) {
            return false;
        }
        return std::all_of(tmpl.prerequisites.begin(), tmpl.prerequisites.end(),
                           [this](uint32_t prereqId) {
                               return IsCompleted(prereqId);
                           });
    }

    /// Remove turned-in and failed quests from active list.
    void CleanupFinished() {
        std::erase_if(activeQuests, [](const QuestEntry& e) {
            return e.state == QuestState::TurnedIn
                   || e.state == QuestState::Failed;
        });
    }
};

// -- QuestEvent ---------------------------------------------------------------

/// Event component for quest objective tracking.
///
/// Other systems create QuestEvent entities to notify the QuestSystem
/// about kills, item pickups, zone entries, and object interactions.
/// Follows the same pattern as DamageEvent for inter-system communication.
struct QuestEvent {
    cgs::ecs::Entity player;
    QuestEventType type = QuestEventType::Kill;
    uint32_t targetId = 0;   ///< Creature entry, item ID, zone ID, or object ID.
    int32_t count = 1;       ///< Number of kills/items/etc.
    bool processed = false;  ///< Set to true after QuestSystem handles it.
};

} // namespace cgs::game
