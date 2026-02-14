#pragma once

/// @file ai_components.hpp
/// @brief AI ECS component: AIBrain.
///
/// The AIBrain component attaches AI behavior to an entity by holding
/// a reference to a shared behavior tree root and per-entity state.
///
/// @see SRS-GML-004.1
/// @see SDS-MOD-023

#include "cgs/ecs/entity.hpp"
#include "cgs/game/ai_types.hpp"
#include "cgs/game/behavior_tree.hpp"
#include "cgs/game/math_types.hpp"

#include <memory>

namespace cgs::game {

// ── AIBrain (SRS-GML-004.1) ─────────────────────────────────────────

/// AI brain component holding behavior tree reference and per-entity state.
///
/// Each AI entity has its own AIBrain with a shared_ptr to a behavior
/// tree root (trees can be shared across multiple entities of the same
/// type) and a private Blackboard for instance-specific data.
struct AIBrain {
    /// Root of the behavior tree (shared across same-type entities).
    std::shared_ptr<BTNode> behaviorTree;

    /// Per-entity data store for BT node communication.
    Blackboard blackboard;

    /// Current high-level AI state.
    AIState state = AIState::Idle;

    /// Accumulated time since last AI tick (for throttling).
    float timeSinceLastTick = 0.0f;

    /// Per-entity tick interval override (uses system default if <= 0).
    float tickInterval = 0.0f;

    /// Home position for patrol/return behavior.
    Vector3 homePosition;

    /// Current target entity for combat/chase behavior.
    cgs::ecs::Entity target;
};

}  // namespace cgs::game
