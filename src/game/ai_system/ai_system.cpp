/// @file ai_system.cpp
/// @brief AISystem implementation.
///
/// Executes behavior trees for AI entities with frame-distributed
/// throttling.  Built-in task factories create reusable BT leaf nodes
/// that drive Movement, check combat range, and manage patrol paths.
///
/// @see SRS-GML-004.4
/// @see SDS-MOD-023

#include "cgs/game/ai_system.hpp"

#include <vector>

namespace cgs::game {

AISystem::AISystem(cgs::ecs::ComponentStorage<AIBrain>& brains,
                   cgs::ecs::ComponentStorage<Transform>& transforms,
                   cgs::ecs::ComponentStorage<Movement>& movements,
                   cgs::ecs::ComponentStorage<Stats>& stats,
                   cgs::ecs::ComponentStorage<ThreatList>& threatLists,
                   float defaultTickInterval)
    : brains_(brains),
      transforms_(transforms),
      movements_(movements),
      stats_(stats),
      threatLists_(threatLists),
      defaultTickInterval_(defaultTickInterval) {}

void AISystem::Execute(float deltaTime) {
    uint32_t updateCount = 0;

    for (std::size_t i = 0; i < brains_.Size(); ++i) {
        auto entityId = brains_.EntityAt(i);
        cgs::ecs::Entity entity(entityId, 0);
        auto& brain = brains_.Get(entity);

        // Skip dead AI entities.
        if (brain.state == AIState::Dead) {
            continue;
        }

        // Skip entities without a behavior tree.
        if (!brain.behaviorTree) {
            continue;
        }

        // Accumulate time.
        brain.timeSinceLastTick += deltaTime;

        // Determine effective tick interval.
        float interval = brain.tickInterval > 0.0f ? brain.tickInterval : defaultTickInterval_;

        // Only tick when the interval has elapsed.
        if (brain.timeSinceLastTick < interval) {
            continue;
        }

        // Execute the behavior tree.
        BTContext context;
        context.entity = entity;
        context.deltaTime = brain.timeSinceLastTick;
        context.blackboard = &brain.blackboard;

        brain.behaviorTree->Tick(context);
        brain.timeSinceLastTick = 0.0f;
        ++updateCount;
    }

    lastTickUpdateCount_ = updateCount;
}

cgs::ecs::SystemAccessInfo AISystem::GetAccessInfo() const {
    cgs::ecs::SystemAccessInfo info;
    cgs::ecs::Write<AIBrain, Movement>::Apply(info);
    cgs::ecs::Read<Transform, Stats, ThreatList>::Apply(info);
    return info;
}

// ── Built-in task factories ─────────────────────────────────────────

std::unique_ptr<BTNode> AISystem::CreateMoveToTask() {
    auto& transforms = transforms_;
    auto& movements = movements_;

    return std::make_unique<BTAction>([&transforms, &movements](BTContext& ctx) -> BTStatus {
        auto* target = ctx.blackboard->Get<Vector3>("move_target");
        if (!target) {
            return BTStatus::Failure;
        }

        if (!transforms.Has(ctx.entity) || !movements.Has(ctx.entity)) {
            return BTStatus::Failure;
        }

        auto& transform = transforms.Get(ctx.entity);
        auto& movement = movements.Get(ctx.entity);

        auto diff = *target - transform.position;
        float distSq = diff.x * diff.x + diff.z * diff.z;

        if (distSq <= kMoveToArrivalDistance * kMoveToArrivalDistance) {
            movement.state = MovementState::Idle;
            movement.direction = Vector3::Zero();
            return BTStatus::Success;
        }

        movement.direction = diff.Normalized();
        movement.state = MovementState::Running;
        return BTStatus::Running;
    });
}

std::unique_ptr<BTNode> AISystem::CreateAttackTask() {
    auto& transforms = transforms_;
    auto& stats = stats_;

    return std::make_unique<BTAction>([&transforms, &stats](BTContext& ctx) -> BTStatus {
        auto* targetPtr = ctx.blackboard->Get<cgs::ecs::Entity>("target");
        if (!targetPtr || !targetPtr->isValid()) {
            return BTStatus::Failure;
        }

        auto target = *targetPtr;

        if (!transforms.Has(ctx.entity) || !transforms.Has(target)) {
            return BTStatus::Failure;
        }

        // Check if target is alive.
        if (stats.Has(target) && stats.Get(target).health <= 0) {
            return BTStatus::Failure;
        }

        const auto& attackerPos = transforms.Get(ctx.entity).position;
        const auto& targetPos = transforms.Get(target).position;

        auto diff = targetPos - attackerPos;
        float distSq = diff.x * diff.x + diff.z * diff.z;

        if (distSq > kDefaultAttackRange * kDefaultAttackRange) {
            return BTStatus::Failure;
        }

        // In range — signal success.  Actual damage application is
        // handled by the CombatSystem via DamageEvent components.
        return BTStatus::Success;
    });
}

std::unique_ptr<BTNode> AISystem::CreatePatrolTask() {
    auto& transforms = transforms_;
    auto& movements = movements_;

    return std::make_unique<BTAction>([&transforms, &movements](BTContext& ctx) -> BTStatus {
        auto* waypoints = ctx.blackboard->Get<std::vector<Vector3>>("waypoints");
        if (!waypoints || waypoints->empty()) {
            return BTStatus::Failure;
        }

        auto* indexPtr = ctx.blackboard->Get<std::size_t>("patrol_index");
        std::size_t waypointIndex = indexPtr ? *indexPtr : 0;

        if (waypointIndex >= waypoints->size()) {
            waypointIndex = 0;
        }

        if (!transforms.Has(ctx.entity) || !movements.Has(ctx.entity)) {
            return BTStatus::Failure;
        }

        auto& transform = transforms.Get(ctx.entity);
        auto& movement = movements.Get(ctx.entity);

        const auto& target = (*waypoints)[waypointIndex];
        auto diff = target - transform.position;
        float distSq = diff.x * diff.x + diff.z * diff.z;

        if (distSq <= kMoveToArrivalDistance * kMoveToArrivalDistance) {
            // Arrived at current waypoint, advance to next.
            waypointIndex = (waypointIndex + 1) % waypoints->size();
            ctx.blackboard->Set<std::size_t>("patrol_index", waypointIndex);
            return BTStatus::Running;
        }

        movement.direction = diff.Normalized();
        movement.state = MovementState::Walking;
        return BTStatus::Running;
    });
}

std::unique_ptr<BTNode> AISystem::CreateFleeTask() {
    auto& transforms = transforms_;
    auto& movements = movements_;
    auto& threatLists = threatLists_;

    return std::make_unique<BTAction>(
        [&transforms, &movements, &threatLists](BTContext& ctx) -> BTStatus {
            if (!transforms.Has(ctx.entity) || !movements.Has(ctx.entity)) {
                return BTStatus::Failure;
            }

            // Get the top threat source to flee from.
            cgs::ecs::Entity threatSource = cgs::ecs::Entity::invalid();
            if (threatLists.Has(ctx.entity)) {
                threatSource = threatLists.Get(ctx.entity).GetTopThreat();
            }

            if (!threatSource.isValid() || !transforms.Has(threatSource)) {
                return BTStatus::Failure;
            }

            const auto& entityPos = transforms.Get(ctx.entity).position;
            const auto& threatPos = transforms.Get(threatSource).position;

            auto awayDir = entityPos - threatPos;
            float distSq = awayDir.x * awayDir.x + awayDir.z * awayDir.z;

            // Already far enough away.
            if (distSq >= kDefaultFleeDistance * kDefaultFleeDistance) {
                auto& movement = movements.Get(ctx.entity);
                movement.state = MovementState::Idle;
                movement.direction = Vector3::Zero();
                return BTStatus::Success;
            }

            auto& movement = movements.Get(ctx.entity);
            movement.direction = awayDir.Normalized();
            movement.state = MovementState::Running;
            return BTStatus::Running;
        });
}

std::unique_ptr<BTNode> AISystem::CreateIdleTask() {
    auto& movements = movements_;

    return std::make_unique<BTAction>([&movements](BTContext& ctx) -> BTStatus {
        if (!movements.Has(ctx.entity)) {
            return BTStatus::Success;
        }

        auto& movement = movements.Get(ctx.entity);
        movement.state = MovementState::Idle;
        movement.direction = Vector3::Zero();
        return BTStatus::Success;
    });
}

void AISystem::SetDefaultTickInterval(float interval) {
    if (interval > 0.0f) {
        defaultTickInterval_ = interval;
    }
}

}  // namespace cgs::game
