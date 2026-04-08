// examples/08_quest.cpp
//
// Tutorial: Quest system and objectives
// See: docs/tutorial_quest.dox
//
// This example assigns a player a 2-objective quest ("Kill 5
// wolves and collect 3 pelts"), simulates progress by queuing
// QuestEvent components, drives the QuestSystem forward, and
// demonstrates turn-in + reward distribution.

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/game/quest_components.hpp"
#include "cgs/game/quest_system.hpp"
#include "cgs/game/quest_types.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using cgs::ecs::ComponentStorage;
using cgs::ecs::Entity;
using cgs::ecs::EntityManager;
using cgs::game::HasQuestFlag;
using cgs::game::ObjectiveType;
using cgs::game::QuestEntry;
using cgs::game::QuestEvent;
using cgs::game::QuestEventType;
using cgs::game::QuestFlags;
using cgs::game::QuestLog;
using cgs::game::QuestObjective;
using cgs::game::QuestReward;
using cgs::game::QuestState;
using cgs::game::QuestSystem;
using cgs::game::QuestTemplate;

namespace {

const char* StateName(QuestState s) {
    switch (s) {
        case QuestState::Available:          return "Available";
        case QuestState::Accepted:           return "Accepted";
        case QuestState::ObjectivesComplete: return "ObjectivesComplete";
        case QuestState::TurnedIn:           return "TurnedIn";
        case QuestState::Failed:             return "Failed";
    }
    return "?";
}

}  // namespace

int main() {
    // ── Step 1: Declare a quest template. ────────────────────────────
    //
    // Template data is static — loaded from YAML/DB at server startup
    // in production. Here we build it inline.
    QuestTemplate wolfQuest;
    wolfQuest.id = 50101;
    wolfQuest.name = "The Wolves of the Woods";
    wolfQuest.description = "Slay the wolves terrorizing the village.";
    wolfQuest.level = 5;
    wolfQuest.flags = QuestFlags::Shareable;

    // Two objectives.
    QuestObjective killWolves;
    killWolves.type = ObjectiveType::Kill;
    killWolves.targetId = 3001;  // wolf creature entry
    killWolves.required = 5;
    wolfQuest.objectives.push_back(killWolves);

    QuestObjective collectPelts;
    collectPelts.type = ObjectiveType::Collect;
    collectPelts.targetId = 4001;  // wolf pelt item ID
    collectPelts.required = 3;
    wolfQuest.objectives.push_back(collectPelts);

    // Rewards.
    wolfQuest.rewards.experience = 500;
    wolfQuest.rewards.currency = 100;
    wolfQuest.rewards.items.push_back({1001, 2});  // 2 health potions

    // ── Step 2: Create the player entity with a QuestLog. ────────────
    ComponentStorage<QuestLog> questLogs;
    ComponentStorage<QuestEvent> questEvents;

    EntityManager em;
    em.RegisterStorage(&questLogs);
    em.RegisterStorage(&questEvents);

    const Entity player = em.Create();
    questLogs.Add(player, QuestLog{});

    // ── Step 3: Accept the quest. ───────────────────────────────────
    auto& log = questLogs.Get(player);
    const bool accepted = log.Accept(wolfQuest);
    std::cout << "quest accepted: " << (accepted ? "yes" : "no") << "\n";

    auto* entry = log.GetQuest(wolfQuest.id);
    if (entry == nullptr) {
        std::cerr << "quest not found after accept\n";
        return EXIT_FAILURE;
    }
    std::cout << "state: " << StateName(entry->state) << "\n";

    // ── Step 4: Simulate wolf kills directly via UpdateObjective. ────
    //
    // In a real server the CombatSystem would emit QuestEvent
    // components on kill and QuestSystem would forward them to the
    // player's log. For this example we call UpdateObjective directly
    // to show how it threads progress into the entry.
    for (int i = 0; i < 5; ++i) {
        entry->UpdateObjective(ObjectiveType::Kill, 3001, 1);
    }

    // ── Step 5: Simulate pelt collection. ────────────────────────────
    entry->UpdateObjective(ObjectiveType::Collect, 4001, 3);

    std::cout << "state after progress: " << StateName(entry->state) << "\n";
    std::cout << "all objectives complete: "
              << (entry->AllObjectivesComplete() ? "yes" : "no") << "\n";

    // ── Step 6: Turn the quest in. ──────────────────────────────────
    const bool turnedIn = log.TurnIn(wolfQuest.id);
    std::cout << "turn-in success: " << (turnedIn ? "yes" : "no") << "\n";

    entry = log.GetQuest(wolfQuest.id);
    if (entry != nullptr) {
        std::cout << "state after turn-in: " << StateName(entry->state) << "\n";
    }

    // ── Step 7: Distribute rewards (game-layer logic). ───────────────
    //
    // QuestLog::TurnIn only flips the state and records completion.
    // Actually granting XP / currency / items is the caller's
    // responsibility — usually inside a ProcessReward function that
    // calls into Stats, Inventory, and other systems.
    std::cout << "rewards: " << wolfQuest.rewards.experience << " XP, "
              << wolfQuest.rewards.currency << " currency, "
              << wolfQuest.rewards.items.size() << " item(s)\n";

    // ── Step 8: Verify quest is in the completed set and can't be
    //         accepted again (unless Repeatable flag is set).
    std::cout << "is completed: " << (log.IsCompleted(wolfQuest.id) ? "yes" : "no") << "\n";

    const bool reaccepted = log.Accept(wolfQuest);
    std::cout << "re-accept succeeded: " << (reaccepted ? "yes" : "no") << "\n";

    return EXIT_SUCCESS;
}
