#pragma once

/// @file quest_system.hpp
/// @brief QuestSystem: per-tick quest processing and event-driven updates.
///
/// Processes quest timers and incoming quest events each frame.
/// Integrates with Kill/Collect/Explore/Interact events from other systems
/// through the QuestEvent component (same pattern as DamageEvent).
///
/// @see SRS-GML-005.4
/// @see SDS-MOD-034

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/quest_components.hpp"

#include <string_view>
#include <vector>

namespace cgs::game {

/// System that processes quest logic each tick.
///
/// Execution within a single tick:
///   1. Update timed quest timers (fail expired quests)
///   2. Process incoming quest events (objective updates)
///   3. Cleanup finished quests
///
/// Runs in PostUpdate stage so that combat kills, zone entries, and
/// other game events from the Update stage are available as QuestEvents.
class QuestSystem final : public cgs::ecs::ISystem {
public:
    QuestSystem(cgs::ecs::ComponentStorage<QuestLog>& questLogs,
                cgs::ecs::ComponentStorage<QuestEvent>& questEvents);

    void Execute(float deltaTime) override;

    [[nodiscard]] cgs::ecs::SystemStage GetStage() const override {
        return cgs::ecs::SystemStage::PostUpdate;
    }

    [[nodiscard]] std::string_view GetName() const override { return "QuestSystem"; }

    [[nodiscard]] cgs::ecs::SystemAccessInfo GetAccessInfo() const override;

    /// Register a quest template for lookup during acceptance and validation.
    void RegisterTemplate(QuestTemplate tmpl);

    /// Get a registered quest template by ID.
    [[nodiscard]] const QuestTemplate* GetTemplate(uint32_t templateId) const;

private:
    /// Update timers on timed quests and fail expired ones.
    void updateTimers(float deltaTime);

    /// Process pending quest events from other systems.
    void processEvents();

    cgs::ecs::ComponentStorage<QuestLog>& questLogs_;
    cgs::ecs::ComponentStorage<QuestEvent>& questEvents_;
    std::vector<QuestTemplate> templates_;
};

}  // namespace cgs::game
