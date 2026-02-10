#pragma once

/// @file ai_system.hpp
/// @brief AISystem: per-tick AI behavior tree execution with throttling.
///
/// Iterates all entities with AIBrain components, throttles updates
/// across frames to distribute CPU load, and executes behavior trees.
///
/// @see SRS-GML-004.4
/// @see SDS-MOD-023

#include <cstdint>
#include <memory>
#include <string_view>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/ai_components.hpp"
#include "cgs/game/combat_components.hpp"
#include "cgs/game/components.hpp"

namespace cgs::game {

/// System that executes AI behavior trees with frame-distributed throttling.
///
/// Each tick:
///   1. Accumulate deltaTime on each AIBrain's timer.
///   2. For brains whose timer exceeds their tick interval, execute the BT.
///   3. Reset the timer after execution.
///
/// This ensures AI updates are spread across multiple frames rather than
/// all running simultaneously, reducing per-frame CPU spikes.
class AISystem final : public cgs::ecs::ISystem {
public:
    AISystem(cgs::ecs::ComponentStorage<AIBrain>& brains,
             cgs::ecs::ComponentStorage<Transform>& transforms,
             cgs::ecs::ComponentStorage<Movement>& movements,
             cgs::ecs::ComponentStorage<Stats>& stats,
             cgs::ecs::ComponentStorage<ThreatList>& threatLists,
             float defaultTickInterval = kDefaultAITickInterval);

    void Execute(float deltaTime) override;

    [[nodiscard]] cgs::ecs::SystemStage GetStage() const override {
        return cgs::ecs::SystemStage::Update;
    }

    [[nodiscard]] std::string_view GetName() const override {
        return "AISystem";
    }

    [[nodiscard]] cgs::ecs::SystemAccessInfo GetAccessInfo() const override;

    // ── Built-in task factories (SRS-GML-004.3) ─────────────────────

    /// Create a MoveTo action node.
    /// Reads "move_target" (Vector3) from the blackboard and drives
    /// the entity's Movement toward it.
    [[nodiscard]] std::unique_ptr<BTNode> CreateMoveToTask();

    /// Create an Attack action node.
    /// Reads "target" (Entity) from the blackboard and checks range;
    /// returns Success if in range, Failure otherwise.
    [[nodiscard]] std::unique_ptr<BTNode> CreateAttackTask();

    /// Create a Patrol action node.
    /// Reads "waypoints" (vector<Vector3>) and "patrol_index" (size_t)
    /// from the blackboard and walks between waypoints in sequence.
    [[nodiscard]] std::unique_ptr<BTNode> CreatePatrolTask();

    /// Create a Flee action node.
    /// Moves the entity away from its top threat source.
    [[nodiscard]] std::unique_ptr<BTNode> CreateFleeTask();

    /// Create an Idle action node.
    /// Sets Movement to Idle state and returns Success immediately.
    [[nodiscard]] std::unique_ptr<BTNode> CreateIdleTask();

    // ── Configuration ────────────────────────────────────────────────

    /// Set the default tick interval for AI entities that don't override it.
    void SetDefaultTickInterval(float interval);

    /// Get the current default tick interval.
    [[nodiscard]] float GetDefaultTickInterval() const noexcept {
        return defaultTickInterval_;
    }

    /// Get the number of AI entities updated on the last Execute call.
    [[nodiscard]] uint32_t GetLastTickUpdateCount() const noexcept {
        return lastTickUpdateCount_;
    }

private:
    cgs::ecs::ComponentStorage<AIBrain>& brains_;
    cgs::ecs::ComponentStorage<Transform>& transforms_;
    cgs::ecs::ComponentStorage<Movement>& movements_;
    cgs::ecs::ComponentStorage<Stats>& stats_;
    cgs::ecs::ComponentStorage<ThreatList>& threatLists_;

    float defaultTickInterval_;
    uint32_t lastTickUpdateCount_ = 0;
};

} // namespace cgs::game
