/// @file quest_system.cpp
/// @brief QuestSystem implementation.
///
/// Implements the two-phase quest tick:
///   1. Timer updates for timed quests (fail expired)
///   2. Event processing (map QuestEvents to objective progress)
///
/// @see SRS-GML-005.4
/// @see SDS-MOD-034

#include "cgs/game/quest_system.hpp"

#include <algorithm>

namespace cgs::game {

QuestSystem::QuestSystem(cgs::ecs::ComponentStorage<QuestLog>& questLogs,
                         cgs::ecs::ComponentStorage<QuestEvent>& questEvents)
    : questLogs_(questLogs), questEvents_(questEvents) {}

void QuestSystem::Execute(float deltaTime) {
    updateTimers(deltaTime);
    processEvents();
}

cgs::ecs::SystemAccessInfo QuestSystem::GetAccessInfo() const {
    cgs::ecs::SystemAccessInfo info;
    cgs::ecs::Write<QuestLog, QuestEvent>::Apply(info);
    return info;
}

void QuestSystem::RegisterTemplate(QuestTemplate tmpl) {
    auto it = std::find_if(templates_.begin(), templates_.end(), [&tmpl](const QuestTemplate& t) {
        return t.id == tmpl.id;
    });
    if (it != templates_.end()) {
        *it = std::move(tmpl);
    } else {
        templates_.push_back(std::move(tmpl));
    }
}

const QuestTemplate* QuestSystem::GetTemplate(uint32_t templateId) const {
    auto it = std::find_if(templates_.begin(),
                           templates_.end(),
                           [templateId](const QuestTemplate& t) { return t.id == templateId; });
    return (it != templates_.end()) ? &(*it) : nullptr;
}

// -- Timer updates ------------------------------------------------------------

void QuestSystem::updateTimers(float deltaTime) {
    for (std::size_t i = 0; i < questLogs_.Size(); ++i) {
        auto entityId = questLogs_.EntityAt(i);
        cgs::ecs::Entity entity(entityId, 0);
        auto& log = questLogs_.Get(entity);

        for (auto& quest : log.activeQuests) {
            if (quest.state != QuestState::Accepted) {
                continue;
            }
            if (quest.timeLimit <= 0.0f) {
                continue;
            }

            quest.elapsedTime += deltaTime;
            if (quest.elapsedTime >= quest.timeLimit) {
                quest.state = QuestState::Failed;
            }
        }
    }
}

// -- Event processing ---------------------------------------------------------

void QuestSystem::processEvents() {
    for (std::size_t i = 0; i < questEvents_.Size(); ++i) {
        auto entityId = questEvents_.EntityAt(i);
        cgs::ecs::Entity eventEntity(entityId, 0);
        auto& event = questEvents_.Get(eventEntity);

        if (event.processed) {
            continue;
        }

        // Map QuestEventType to ObjectiveType.
        ObjectiveType objType{};
        switch (event.type) {
            case QuestEventType::Kill:
                objType = ObjectiveType::Kill;
                break;
            case QuestEventType::Collect:
                objType = ObjectiveType::Collect;
                break;
            case QuestEventType::Explore:
                objType = ObjectiveType::Explore;
                break;
            case QuestEventType::Interact:
                objType = ObjectiveType::Interact;
                break;
            default:
                continue;
        }

        // Update matching objectives in the player's quest log.
        if (questLogs_.Has(event.player)) {
            auto& log = questLogs_.Get(event.player);
            for (auto& quest : log.activeQuests) {
                quest.UpdateObjective(objType, event.targetId, event.count);
            }
        }

        event.processed = true;
    }
}

}  // namespace cgs::game
