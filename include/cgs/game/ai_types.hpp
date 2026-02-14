#pragma once

/// @file ai_types.hpp
/// @brief Enumerations and constants for the AI system.
///
/// @see SDS-MOD-023
/// @see SRS-GML-004

#include <cstdint>

namespace cgs::game {

/// Maximum number of waypoints in a patrol path.
constexpr std::size_t kMaxPatrolWaypoints = 32;

/// Default AI update interval in seconds for throttled updates.
constexpr float kDefaultAITickInterval = 0.1f;

/// Default distance threshold for MoveTo completion (world units).
constexpr float kMoveToArrivalDistance = 1.0f;

/// Default flee distance from threat source (world units).
constexpr float kDefaultFleeDistance = 20.0f;

/// Default melee attack range (world units).
constexpr float kDefaultAttackRange = 3.0f;

/// Behavior Tree node tick result.
enum class BTStatus : uint8_t {
    Success,  ///< Node completed successfully.
    Failure,  ///< Node failed.
    Running   ///< Node still in progress (resume next tick).
};

/// High-level AI state for the AIBrain component.
enum class AIState : uint8_t {
    Idle,        ///< Standing still, awaiting stimuli.
    Patrolling,  ///< Walking between waypoints.
    Chasing,     ///< Moving toward a target.
    Attacking,   ///< Engaged in combat.
    Fleeing,     ///< Running away from a threat.
    Dead         ///< Entity is dead, AI suspended.
};

/// Parallel node completion policy.
enum class BTParallelPolicy : uint8_t {
    RequireAll,  ///< Succeed only when all children succeed.
    RequireOne   ///< Succeed when at least one child succeeds.
};

}  // namespace cgs::game
