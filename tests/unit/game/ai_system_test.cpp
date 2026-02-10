#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <vector>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/ai_components.hpp"
#include "cgs/game/ai_system.hpp"
#include "cgs/game/ai_types.hpp"
#include "cgs/game/behavior_tree.hpp"
#include "cgs/game/combat_components.hpp"
#include "cgs/game/components.hpp"
#include "cgs/game/math_types.hpp"

using namespace cgs::ecs;
using namespace cgs::game;

// ═══════════════════════════════════════════════════════════════════════════
// AI type tests
// ═══════════════════════════════════════════════════════════════════════════

TEST(AITypesTest, BTStatusValues) {
    EXPECT_NE(BTStatus::Success, BTStatus::Failure);
    EXPECT_NE(BTStatus::Success, BTStatus::Running);
    EXPECT_NE(BTStatus::Failure, BTStatus::Running);
}

TEST(AITypesTest, AIStateValues) {
    AIState state = AIState::Idle;
    EXPECT_EQ(state, AIState::Idle);
    state = AIState::Dead;
    EXPECT_EQ(state, AIState::Dead);
}

TEST(AITypesTest, Constants) {
    EXPECT_GT(kDefaultAITickInterval, 0.0f);
    EXPECT_GT(kMoveToArrivalDistance, 0.0f);
    EXPECT_GT(kDefaultFleeDistance, 0.0f);
    EXPECT_GT(kDefaultAttackRange, 0.0f);
    EXPECT_GT(kMaxPatrolWaypoints, 0u);
}

// ═══════════════════════════════════════════════════════════════════════════
// Blackboard tests
// ═══════════════════════════════════════════════════════════════════════════

TEST(BlackboardTest, SetAndGetValue) {
    Blackboard bb;
    bb.Set<int>("health", 100);

    auto* val = bb.Get<int>("health");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 100);
}

TEST(BlackboardTest, GetMissingKeyReturnsNull) {
    Blackboard bb;
    auto* val = bb.Get<int>("nonexistent");
    EXPECT_EQ(val, nullptr);
}

TEST(BlackboardTest, GetWrongTypeReturnsNull) {
    Blackboard bb;
    bb.Set<int>("value", 42);
    auto* val = bb.Get<float>("value");
    EXPECT_EQ(val, nullptr);
}

TEST(BlackboardTest, OverwriteValue) {
    Blackboard bb;
    bb.Set<int>("count", 1);
    bb.Set<int>("count", 2);

    auto* val = bb.Get<int>("count");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 2);
}

TEST(BlackboardTest, HasKey) {
    Blackboard bb;
    EXPECT_FALSE(bb.Has("key"));
    bb.Set<int>("key", 0);
    EXPECT_TRUE(bb.Has("key"));
}

TEST(BlackboardTest, EraseKey) {
    Blackboard bb;
    bb.Set<int>("key", 42);
    EXPECT_TRUE(bb.Has("key"));
    bb.Erase("key");
    EXPECT_FALSE(bb.Has("key"));
}

TEST(BlackboardTest, Clear) {
    Blackboard bb;
    bb.Set<int>("a", 1);
    bb.Set<float>("b", 2.0f);
    bb.Clear();
    EXPECT_FALSE(bb.Has("a"));
    EXPECT_FALSE(bb.Has("b"));
}

TEST(BlackboardTest, StoreVector3) {
    Blackboard bb;
    bb.Set<Vector3>("position", Vector3(10.0f, 0.0f, 20.0f));

    auto* pos = bb.Get<Vector3>("position");
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 10.0f);
    EXPECT_FLOAT_EQ(pos->z, 20.0f);
}

TEST(BlackboardTest, StoreEntity) {
    Blackboard bb;
    Entity target(42, 0);
    bb.Set<Entity>("target", target);

    auto* val = bb.Get<Entity>("target");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, target);
}

TEST(BlackboardTest, ConstAccess) {
    Blackboard bb;
    bb.Set<int>("x", 99);

    const auto& constBb = bb;
    auto* val = constBb.Get<int>("x");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 99);
}

// ═══════════════════════════════════════════════════════════════════════════
// BT leaf node tests
// ═══════════════════════════════════════════════════════════════════════════

TEST(BTConditionTest, TruePredicateReturnsSuccess) {
    BTCondition node([](BTContext&) { return true; });
    BTContext ctx;
    EXPECT_EQ(node.Tick(ctx), BTStatus::Success);
}

TEST(BTConditionTest, FalsePredicateReturnsFailure) {
    BTCondition node([](BTContext&) { return false; });
    BTContext ctx;
    EXPECT_EQ(node.Tick(ctx), BTStatus::Failure);
}

TEST(BTActionTest, ReturnsActionResult) {
    BTAction successAction([](BTContext&) { return BTStatus::Success; });
    BTAction failAction([](BTContext&) { return BTStatus::Failure; });
    BTAction runAction([](BTContext&) { return BTStatus::Running; });

    BTContext ctx;
    EXPECT_EQ(successAction.Tick(ctx), BTStatus::Success);
    EXPECT_EQ(failAction.Tick(ctx), BTStatus::Failure);
    EXPECT_EQ(runAction.Tick(ctx), BTStatus::Running);
}

TEST(BTActionTest, AccessesContext) {
    BTAction action([](BTContext& ctx) -> BTStatus {
        auto* val = ctx.blackboard->Get<int>("counter");
        if (!val) return BTStatus::Failure;
        ctx.blackboard->Set<int>("counter", *val + 1);
        return BTStatus::Success;
    });

    Blackboard bb;
    bb.Set<int>("counter", 0);
    BTContext ctx;
    ctx.blackboard = &bb;

    action.Tick(ctx);
    auto* val = bb.Get<int>("counter");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// BT Sequence tests (SRS-GML-004.2)
// ═══════════════════════════════════════════════════════════════════════════

TEST(BTSequenceTest, EmptySequenceSucceeds) {
    BTSequence seq;
    BTContext ctx;
    EXPECT_EQ(seq.Tick(ctx), BTStatus::Success);
}

TEST(BTSequenceTest, AllSucceedReturnsSuccess) {
    BTSequence seq;
    seq.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; }));
    seq.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; }));

    BTContext ctx;
    EXPECT_EQ(seq.Tick(ctx), BTStatus::Success);
}

TEST(BTSequenceTest, FailureStopsExecution) {
    int callCount = 0;
    BTSequence seq;
    seq.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Success; }));
    seq.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Failure; }));
    seq.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Success; }));

    BTContext ctx;
    EXPECT_EQ(seq.Tick(ctx), BTStatus::Failure);
    EXPECT_EQ(callCount, 2);  // Third child should not be called.
}

TEST(BTSequenceTest, RunningPausesExecution) {
    int callCount = 0;
    BTSequence seq;
    seq.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Success; }));
    seq.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Running; }));
    seq.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Success; }));

    BTContext ctx;
    EXPECT_EQ(seq.Tick(ctx), BTStatus::Running);
    EXPECT_EQ(callCount, 2);  // Third child should not be called.
}

TEST(BTSequenceTest, ResumesFromRunningChild) {
    int tickCount = 0;
    BTSequence seq;
    seq.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++tickCount; return BTStatus::Success; }));
    seq.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) {
            ++tickCount;
            return tickCount <= 2 ? BTStatus::Running : BTStatus::Success;
        }));

    BTContext ctx;
    EXPECT_EQ(seq.Tick(ctx), BTStatus::Running);   // First tick: child 0 OK, child 1 running
    EXPECT_EQ(seq.Tick(ctx), BTStatus::Success);    // Second tick: resumes at child 1, succeeds
}

TEST(BTSequenceTest, ResetClearsState) {
    BTSequence seq;
    seq.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; }));
    seq.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Running; }));

    BTContext ctx;
    seq.Tick(ctx);  // Paused at child 1.
    seq.Reset();
    // After reset, should start from child 0 again.
    // Verify reset doesn't crash and child count is preserved.
    EXPECT_EQ(seq.ChildCount(), 2u);
}

// ═══════════════════════════════════════════════════════════════════════════
// BT Selector tests (SRS-GML-004.2)
// ═══════════════════════════════════════════════════════════════════════════

TEST(BTSelectorTest, EmptySelectorFails) {
    BTSelector sel;
    BTContext ctx;
    EXPECT_EQ(sel.Tick(ctx), BTStatus::Failure);
}

TEST(BTSelectorTest, FirstSuccessStopsExecution) {
    int callCount = 0;
    BTSelector sel;
    sel.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Failure; }));
    sel.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Success; }));
    sel.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Success; }));

    BTContext ctx;
    EXPECT_EQ(sel.Tick(ctx), BTStatus::Success);
    EXPECT_EQ(callCount, 2);  // Third child should not be called.
}

TEST(BTSelectorTest, AllFailReturnsFailure) {
    BTSelector sel;
    sel.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Failure; }));
    sel.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Failure; }));

    BTContext ctx;
    EXPECT_EQ(sel.Tick(ctx), BTStatus::Failure);
}

TEST(BTSelectorTest, RunningPausesExecution) {
    BTSelector sel;
    sel.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Failure; }));
    sel.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Running; }));
    sel.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; }));

    BTContext ctx;
    EXPECT_EQ(sel.Tick(ctx), BTStatus::Running);
}

TEST(BTSelectorTest, ResumesFromRunningChild) {
    int tickCount = 0;
    BTSelector sel;
    sel.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++tickCount; return BTStatus::Failure; }));
    sel.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) {
            ++tickCount;
            return tickCount <= 2 ? BTStatus::Running : BTStatus::Success;
        }));

    BTContext ctx;
    EXPECT_EQ(sel.Tick(ctx), BTStatus::Running);    // child 0 fails, child 1 running
    EXPECT_EQ(sel.Tick(ctx), BTStatus::Success);     // resumes at child 1, succeeds
}

// ═══════════════════════════════════════════════════════════════════════════
// BT Parallel tests (SRS-GML-004.2)
// ═══════════════════════════════════════════════════════════════════════════

TEST(BTParallelTest, RequireAllSucceeds) {
    BTParallel par(BTParallelPolicy::RequireAll);
    par.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; }));
    par.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; }));

    BTContext ctx;
    EXPECT_EQ(par.Tick(ctx), BTStatus::Success);
}

TEST(BTParallelTest, RequireAllFailsOnAnyFailure) {
    BTParallel par(BTParallelPolicy::RequireAll);
    par.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; }));
    par.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Failure; }));

    BTContext ctx;
    EXPECT_EQ(par.Tick(ctx), BTStatus::Failure);
}

TEST(BTParallelTest, RequireAllRunningWhenNotAllDone) {
    BTParallel par(BTParallelPolicy::RequireAll);
    par.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; }));
    par.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Running; }));

    BTContext ctx;
    EXPECT_EQ(par.Tick(ctx), BTStatus::Running);
}

TEST(BTParallelTest, RequireOneSucceedsOnFirstSuccess) {
    BTParallel par(BTParallelPolicy::RequireOne);
    par.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Failure; }));
    par.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; }));

    BTContext ctx;
    EXPECT_EQ(par.Tick(ctx), BTStatus::Success);
}

TEST(BTParallelTest, RequireOneFailsWhenAllFail) {
    BTParallel par(BTParallelPolicy::RequireOne);
    par.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Failure; }));
    par.AddChild(std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Failure; }));

    BTContext ctx;
    EXPECT_EQ(par.Tick(ctx), BTStatus::Failure);
}

TEST(BTParallelTest, TicksAllChildren) {
    int callCount = 0;
    BTParallel par(BTParallelPolicy::RequireAll);
    par.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Success; }));
    par.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Success; }));
    par.AddChild(std::make_unique<BTAction>(
        [&](BTContext&) { ++callCount; return BTStatus::Success; }));

    BTContext ctx;
    par.Tick(ctx);
    EXPECT_EQ(callCount, 3);  // All children ticked.
}

TEST(BTParallelTest, PolicyAccessor) {
    BTParallel par(BTParallelPolicy::RequireOne);
    EXPECT_EQ(par.GetPolicy(), BTParallelPolicy::RequireOne);
}

// ═══════════════════════════════════════════════════════════════════════════
// BT Inverter tests (SRS-GML-004.2)
// ═══════════════════════════════════════════════════════════════════════════

TEST(BTInverterTest, InvertsSuccess) {
    auto child = std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; });
    BTInverter inv(std::move(child));

    BTContext ctx;
    EXPECT_EQ(inv.Tick(ctx), BTStatus::Failure);
}

TEST(BTInverterTest, InvertsFailure) {
    auto child = std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Failure; });
    BTInverter inv(std::move(child));

    BTContext ctx;
    EXPECT_EQ(inv.Tick(ctx), BTStatus::Success);
}

TEST(BTInverterTest, PassesThroughRunning) {
    auto child = std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Running; });
    BTInverter inv(std::move(child));

    BTContext ctx;
    EXPECT_EQ(inv.Tick(ctx), BTStatus::Running);
}

// ═══════════════════════════════════════════════════════════════════════════
// BT Repeater tests (SRS-GML-004.2)
// ═══════════════════════════════════════════════════════════════════════════

TEST(BTRepeaterTest, FiniteRepeatCompletesAfterN) {
    int childTicks = 0;
    auto child = std::make_unique<BTAction>(
        [&](BTContext&) { ++childTicks; return BTStatus::Success; });
    BTRepeater rep(std::move(child), 3);

    BTContext ctx;
    EXPECT_EQ(rep.Tick(ctx), BTStatus::Running);  // 1st
    EXPECT_EQ(rep.Tick(ctx), BTStatus::Running);  // 2nd
    EXPECT_EQ(rep.Tick(ctx), BTStatus::Success);  // 3rd — done
    EXPECT_EQ(childTicks, 3);
}

TEST(BTRepeaterTest, InfiniteRepeatNeverCompletes) {
    auto child = std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; });
    BTRepeater rep(std::move(child), 0);  // 0 = infinite

    BTContext ctx;
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(rep.Tick(ctx), BTStatus::Running);
    }
}

TEST(BTRepeaterTest, PassesThroughRunning) {
    auto child = std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Running; });
    BTRepeater rep(std::move(child), 3);

    BTContext ctx;
    EXPECT_EQ(rep.Tick(ctx), BTStatus::Running);
}

TEST(BTRepeaterTest, CountsFailuresAsIterations) {
    int childTicks = 0;
    auto child = std::make_unique<BTAction>(
        [&](BTContext&) { ++childTicks; return BTStatus::Failure; });
    BTRepeater rep(std::move(child), 2);

    BTContext ctx;
    EXPECT_EQ(rep.Tick(ctx), BTStatus::Running);  // 1st (failure still counts)
    EXPECT_EQ(rep.Tick(ctx), BTStatus::Success);  // 2nd — done
    EXPECT_EQ(childTicks, 2);
}

TEST(BTRepeaterTest, ResetClearsCount) {
    auto child = std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; });
    BTRepeater rep(std::move(child), 3);

    BTContext ctx;
    rep.Tick(ctx);  // 1st
    rep.Reset();
    // After reset, should start fresh (need 3 more).
    EXPECT_EQ(rep.Tick(ctx), BTStatus::Running);  // 1st again
    EXPECT_EQ(rep.Tick(ctx), BTStatus::Running);  // 2nd
    EXPECT_EQ(rep.Tick(ctx), BTStatus::Success);  // 3rd — done
}

TEST(BTRepeaterTest, MaxRepeatsAccessor) {
    auto child = std::make_unique<BTAction>(
        [](BTContext&) { return BTStatus::Success; });
    BTRepeater rep(std::move(child), 5);
    EXPECT_EQ(rep.MaxRepeats(), 5u);
}

// ═══════════════════════════════════════════════════════════════════════════
// AIBrain component tests (SRS-GML-004.1)
// ═══════════════════════════════════════════════════════════════════════════

TEST(AIBrainTest, DefaultValues) {
    AIBrain brain;
    EXPECT_EQ(brain.state, AIState::Idle);
    EXPECT_EQ(brain.behaviorTree, nullptr);
    EXPECT_FLOAT_EQ(brain.timeSinceLastTick, 0.0f);
    EXPECT_FLOAT_EQ(brain.tickInterval, 0.0f);
    EXPECT_FALSE(brain.target.isValid());
}

TEST(AIBrainTest, StorageRoundTrip) {
    ComponentStorage<AIBrain> storage;
    Entity e(0, 0);

    AIBrain brain;
    brain.state = AIState::Patrolling;
    brain.homePosition = Vector3(10.0f, 0.0f, 20.0f);
    storage.Add(e, std::move(brain));

    const auto& stored = storage.Get(e);
    EXPECT_EQ(stored.state, AIState::Patrolling);
    EXPECT_FLOAT_EQ(stored.homePosition.x, 10.0f);
    EXPECT_FLOAT_EQ(stored.homePosition.z, 20.0f);
}

TEST(AIBrainTest, SharedBehaviorTree) {
    auto tree = std::make_shared<BTAction>(
        [](BTContext&) { return BTStatus::Success; });

    AIBrain brain1;
    brain1.behaviorTree = tree;

    AIBrain brain2;
    brain2.behaviorTree = tree;

    // Both brains share the same tree.
    EXPECT_EQ(brain1.behaviorTree.get(), brain2.behaviorTree.get());
    EXPECT_EQ(tree.use_count(), 3);  // tree + brain1 + brain2
}

// ═══════════════════════════════════════════════════════════════════════════
// AISystem tests (SRS-GML-004.4)
// ═══════════════════════════════════════════════════════════════════════════

class AISystemTest : public ::testing::Test {
protected:
    ComponentStorage<AIBrain> brains;
    ComponentStorage<Transform> transforms;
    ComponentStorage<Movement> movements;
    ComponentStorage<Stats> stats;
    ComponentStorage<ThreatList> threatLists;

    /// Helper: create an AI entity with a simple always-success BT.
    Entity createAIEntity(uint32_t id, const Vector3& pos) {
        Entity e(id, 0);
        transforms.Add(e, Transform{pos, {}, {1.0f, 1.0f, 1.0f}});
        movements.Add(e, Movement{5.0f, 5.0f, {}, MovementState::Idle});

        AIBrain brain;
        brain.behaviorTree = std::make_shared<BTAction>(
            [](BTContext&) { return BTStatus::Success; });
        brains.Add(e, std::move(brain));

        return e;
    }
};

TEST_F(AISystemTest, ThrottledUpdates) {
    createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));

    // Default tick interval is 0.1s.
    AISystem system(brains, transforms, movements, stats, threatLists, 0.1f);

    // First tick at 0.05s — should NOT trigger AI update.
    system.Execute(0.05f);
    EXPECT_EQ(system.GetLastTickUpdateCount(), 0u);

    // Second tick at 0.05s — total 0.1s, SHOULD trigger.
    system.Execute(0.05f);
    EXPECT_EQ(system.GetLastTickUpdateCount(), 1u);
}

TEST_F(AISystemTest, PerEntityTickInterval) {
    Entity fast = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));
    Entity slow = createAIEntity(1, Vector3(10.0f, 0.0f, 10.0f));

    // Fast entity updates every 0.05s, slow every 0.2s.
    brains.Get(fast).tickInterval = 0.05f;
    brains.Get(slow).tickInterval = 0.2f;

    AISystem system(brains, transforms, movements, stats, threatLists, 0.1f);

    system.Execute(0.05f);
    EXPECT_EQ(system.GetLastTickUpdateCount(), 1u);  // Only fast.

    system.Execute(0.05f);
    EXPECT_EQ(system.GetLastTickUpdateCount(), 1u);  // Fast again.

    system.Execute(0.1f);
    EXPECT_EQ(system.GetLastTickUpdateCount(), 2u);  // Both (fast=0.15 > 0.05, slow=0.2 >= 0.2).
}

TEST_F(AISystemTest, SkipsDeadEntities) {
    Entity e = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));
    brains.Get(e).state = AIState::Dead;

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    system.Execute(0.1f);
    EXPECT_EQ(system.GetLastTickUpdateCount(), 0u);
}

TEST_F(AISystemTest, SkipsEntitiesWithoutBT) {
    Entity e(0, 0);
    transforms.Add(e, Transform{{}, {}, {1.0f, 1.0f, 1.0f}});
    movements.Add(e, Movement{});

    AIBrain brain;
    // No behavior tree set.
    brains.Add(e, std::move(brain));

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    system.Execute(0.1f);
    EXPECT_EQ(system.GetLastTickUpdateCount(), 0u);
}

TEST_F(AISystemTest, BTContextReceivesCorrectData) {
    Entity e(0, 0);
    transforms.Add(e, Transform{{}, {}, {1.0f, 1.0f, 1.0f}});
    movements.Add(e, Movement{});

    Entity capturedEntity;
    float capturedDelta = 0.0f;
    bool capturedHasBlackboard = false;

    AIBrain brain;
    brain.behaviorTree = std::make_shared<BTAction>(
        [&](BTContext& ctx) -> BTStatus {
            capturedEntity = ctx.entity;
            capturedDelta = ctx.deltaTime;
            capturedHasBlackboard = (ctx.blackboard != nullptr);
            return BTStatus::Success;
        });
    brains.Add(e, std::move(brain));

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    system.Execute(0.016f);

    EXPECT_EQ(capturedEntity, e);
    EXPECT_GT(capturedDelta, 0.0f);
    EXPECT_TRUE(capturedHasBlackboard);
}

TEST_F(AISystemTest, TimerResetsAfterTick) {
    Entity e = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));
    brains.Get(e).tickInterval = 0.1f;

    AISystem system(brains, transforms, movements, stats, threatLists, 0.1f);

    system.Execute(0.15f);
    EXPECT_EQ(system.GetLastTickUpdateCount(), 1u);
    EXPECT_FLOAT_EQ(brains.Get(e).timeSinceLastTick, 0.0f);
}

// ── Built-in task tests (SRS-GML-004.3) ─────────────────────────────

TEST_F(AISystemTest, MoveToTaskArrivesAtTarget) {
    Entity e = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto moveToTask = system.CreateMoveToTask();

    Blackboard bb;
    bb.Set<Vector3>("move_target", Vector3(10.0f, 0.0f, 0.0f));
    BTContext ctx{e, 0.016f, &bb};

    // Far from target — should be Running.
    auto status = moveToTask->Tick(ctx);
    EXPECT_EQ(status, BTStatus::Running);
    EXPECT_EQ(movements.Get(e).state, MovementState::Running);

    // Move entity close to target.
    transforms.Get(e).position = Vector3(10.0f, 0.0f, 0.0f);
    status = moveToTask->Tick(ctx);
    EXPECT_EQ(status, BTStatus::Success);
    EXPECT_EQ(movements.Get(e).state, MovementState::Idle);
}

TEST_F(AISystemTest, MoveToTaskFailsWithoutTarget) {
    Entity e = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto moveToTask = system.CreateMoveToTask();

    Blackboard bb;  // No "move_target" set.
    BTContext ctx{e, 0.016f, &bb};

    EXPECT_EQ(moveToTask->Tick(ctx), BTStatus::Failure);
}

TEST_F(AISystemTest, MoveToTaskSetsDirection) {
    Entity e = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto moveToTask = system.CreateMoveToTask();

    Blackboard bb;
    bb.Set<Vector3>("move_target", Vector3(10.0f, 0.0f, 0.0f));
    BTContext ctx{e, 0.016f, &bb};

    moveToTask->Tick(ctx);

    // Direction should point toward the target (positive X).
    auto& dir = movements.Get(e).direction;
    EXPECT_GT(dir.x, 0.0f);
    EXPECT_NEAR(dir.z, 0.0f, 0.01f);
}

TEST_F(AISystemTest, AttackTaskInRange) {
    Entity attacker = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));
    Entity target(1, 0);
    transforms.Add(target, Transform{{2.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    stats.Add(target, Stats{100, 100, 0, 0, {}});

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto attackTask = system.CreateAttackTask();

    Blackboard bb;
    bb.Set<Entity>("target", target);
    BTContext ctx{attacker, 0.016f, &bb};

    // Within default attack range (3.0) — distance is 2.0.
    EXPECT_EQ(attackTask->Tick(ctx), BTStatus::Success);
}

TEST_F(AISystemTest, AttackTaskOutOfRange) {
    Entity attacker = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));
    Entity target(1, 0);
    transforms.Add(target, Transform{{50.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    stats.Add(target, Stats{100, 100, 0, 0, {}});

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto attackTask = system.CreateAttackTask();

    Blackboard bb;
    bb.Set<Entity>("target", target);
    BTContext ctx{attacker, 0.016f, &bb};

    EXPECT_EQ(attackTask->Tick(ctx), BTStatus::Failure);
}

TEST_F(AISystemTest, AttackTaskFailsOnDeadTarget) {
    Entity attacker = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));
    Entity target(1, 0);
    transforms.Add(target, Transform{{1.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    stats.Add(target, Stats{0, 100, 0, 0, {}});  // Dead (health = 0).

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto attackTask = system.CreateAttackTask();

    Blackboard bb;
    bb.Set<Entity>("target", target);
    BTContext ctx{attacker, 0.016f, &bb};

    EXPECT_EQ(attackTask->Tick(ctx), BTStatus::Failure);
}

TEST_F(AISystemTest, AttackTaskFailsWithoutTarget) {
    Entity attacker = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto attackTask = system.CreateAttackTask();

    Blackboard bb;  // No "target" set.
    BTContext ctx{attacker, 0.016f, &bb};

    EXPECT_EQ(attackTask->Tick(ctx), BTStatus::Failure);
}

TEST_F(AISystemTest, PatrolTaskWalksBetweenWaypoints) {
    Entity e = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto patrolTask = system.CreatePatrolTask();

    std::vector<Vector3> waypoints = {
        Vector3(10.0f, 0.0f, 0.0f),
        Vector3(10.0f, 0.0f, 10.0f),
        Vector3(0.0f, 0.0f, 10.0f)
    };

    Blackboard bb;
    bb.Set<std::vector<Vector3>>("waypoints", waypoints);
    BTContext ctx{e, 0.016f, &bb};

    // Far from first waypoint — walking toward it.
    auto status = patrolTask->Tick(ctx);
    EXPECT_EQ(status, BTStatus::Running);
    EXPECT_EQ(movements.Get(e).state, MovementState::Walking);

    // Arrive at first waypoint.
    transforms.Get(e).position = Vector3(10.0f, 0.0f, 0.0f);
    status = patrolTask->Tick(ctx);
    EXPECT_EQ(status, BTStatus::Running);

    // Patrol index should have advanced to 1.
    auto* idx = bb.Get<std::size_t>("patrol_index");
    ASSERT_NE(idx, nullptr);
    EXPECT_EQ(*idx, 1u);
}

TEST_F(AISystemTest, PatrolTaskWrapsAround) {
    Entity e = createAIEntity(0, Vector3(0.0f, 0.0f, 10.0f));

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto patrolTask = system.CreatePatrolTask();

    std::vector<Vector3> waypoints = {
        Vector3(10.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, 10.0f)
    };

    Blackboard bb;
    bb.Set<std::vector<Vector3>>("waypoints", waypoints);
    bb.Set<std::size_t>("patrol_index", std::size_t{1});
    BTContext ctx{e, 0.016f, &bb};

    // At waypoint 1 — should wrap to 0.
    patrolTask->Tick(ctx);
    auto* idx = bb.Get<std::size_t>("patrol_index");
    ASSERT_NE(idx, nullptr);
    EXPECT_EQ(*idx, 0u);
}

TEST_F(AISystemTest, PatrolTaskFailsWithoutWaypoints) {
    Entity e = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto patrolTask = system.CreatePatrolTask();

    Blackboard bb;  // No waypoints.
    BTContext ctx{e, 0.016f, &bb};

    EXPECT_EQ(patrolTask->Tick(ctx), BTStatus::Failure);
}

TEST_F(AISystemTest, FleeTaskMovesAwayFromThreat) {
    Entity e = createAIEntity(0, Vector3(5.0f, 0.0f, 0.0f));
    Entity threat(1, 0);
    transforms.Add(threat, Transform{{0.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    threatLists.Add(e, ThreatList{});
    threatLists.Get(e).AddThreat(threat, 100.0f);

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto fleeTask = system.CreateFleeTask();

    Blackboard bb;
    BTContext ctx{e, 0.016f, &bb};

    // Entity is at (5,0,0), threat at origin — distance 5 < 20 (flee distance).
    auto status = fleeTask->Tick(ctx);
    EXPECT_EQ(status, BTStatus::Running);
    EXPECT_EQ(movements.Get(e).state, MovementState::Running);

    // Direction should point away from threat (positive X).
    auto& dir = movements.Get(e).direction;
    EXPECT_GT(dir.x, 0.0f);
}

TEST_F(AISystemTest, FleeTaskSucceedsWhenFarEnough) {
    Entity e = createAIEntity(0, Vector3(50.0f, 0.0f, 0.0f));
    Entity threat(1, 0);
    transforms.Add(threat, Transform{{0.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    threatLists.Add(e, ThreatList{});
    threatLists.Get(e).AddThreat(threat, 100.0f);

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto fleeTask = system.CreateFleeTask();

    Blackboard bb;
    BTContext ctx{e, 0.016f, &bb};

    // Distance 50 >= 20 (flee distance) — should succeed.
    EXPECT_EQ(fleeTask->Tick(ctx), BTStatus::Success);
    EXPECT_EQ(movements.Get(e).state, MovementState::Idle);
}

TEST_F(AISystemTest, FleeTaskFailsWithoutThreat) {
    Entity e = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto fleeTask = system.CreateFleeTask();

    Blackboard bb;
    BTContext ctx{e, 0.016f, &bb};

    EXPECT_EQ(fleeTask->Tick(ctx), BTStatus::Failure);
}

TEST_F(AISystemTest, IdleTaskSetsIdleState) {
    Entity e = createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));
    movements.Get(e).state = MovementState::Running;
    movements.Get(e).direction = Vector3(1.0f, 0.0f, 0.0f);

    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);
    auto idleTask = system.CreateIdleTask();

    Blackboard bb;
    BTContext ctx{e, 0.016f, &bb};

    EXPECT_EQ(idleTask->Tick(ctx), BTStatus::Success);
    EXPECT_EQ(movements.Get(e).state, MovementState::Idle);
    EXPECT_FLOAT_EQ(movements.Get(e).direction.x, 0.0f);
}

// ── System metadata tests ───────────────────────────────────────────

TEST_F(AISystemTest, SystemMetadata) {
    AISystem system(brains, transforms, movements, stats, threatLists);

    EXPECT_EQ(system.GetName(), "AISystem");
    EXPECT_EQ(system.GetStage(), SystemStage::Update);

    auto access = system.GetAccessInfo();
    EXPECT_FALSE(access.reads.empty());
    EXPECT_FALSE(access.writes.empty());
}

TEST_F(AISystemTest, ConfigurableTickInterval) {
    // Create at least one entity so system has something to process.
    createAIEntity(0, Vector3(0.0f, 0.0f, 0.0f));

    AISystem system(brains, transforms, movements, stats, threatLists, 0.5f);
    EXPECT_FLOAT_EQ(system.GetDefaultTickInterval(), 0.5f);

    system.SetDefaultTickInterval(0.2f);
    EXPECT_FLOAT_EQ(system.GetDefaultTickInterval(), 0.2f);

    // Negative value should be ignored.
    system.SetDefaultTickInterval(-1.0f);
    EXPECT_FLOAT_EQ(system.GetDefaultTickInterval(), 0.2f);
}

// ═══════════════════════════════════════════════════════════════════════════
// Integration: full AI scenario
// ═══════════════════════════════════════════════════════════════════════════

TEST(AIIntegration, FullAIScenario) {
    ComponentStorage<AIBrain> brains;
    ComponentStorage<Transform> transforms;
    ComponentStorage<Movement> movements;
    ComponentStorage<Stats> stats;
    ComponentStorage<ThreatList> threatLists;

    EntityManager entityManager;
    entityManager.RegisterStorage(&brains);
    entityManager.RegisterStorage(&transforms);
    entityManager.RegisterStorage(&movements);
    entityManager.RegisterStorage(&stats);
    entityManager.RegisterStorage(&threatLists);

    // Create a guard NPC.
    Entity guard = entityManager.Create();
    transforms.Add(guard, Transform{{0.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    movements.Add(guard, Movement{3.0f, 3.0f, {}, MovementState::Idle});
    stats.Add(guard, Stats{100, 100, 50, 50, {}});

    // Build a simple BT: Selector(Attack, Patrol, Idle).
    AISystem system(brains, transforms, movements, stats, threatLists, 0.0f);

    auto selector = std::make_unique<BTSelector>();
    selector->AddChild(system.CreateAttackTask());
    selector->AddChild(system.CreatePatrolTask());
    selector->AddChild(system.CreateIdleTask());

    AIBrain brain;
    brain.behaviorTree = std::move(selector);
    brain.blackboard.Set<std::vector<Vector3>>("waypoints",
        std::vector<Vector3>{
            Vector3(10.0f, 0.0f, 0.0f),
            Vector3(10.0f, 0.0f, 10.0f),
            Vector3(0.0f, 0.0f, 0.0f)
        });
    brains.Add(guard, std::move(brain));

    // Tick 1: No target in blackboard, attack fails -> patrol runs.
    system.Execute(0.016f);
    EXPECT_EQ(system.GetLastTickUpdateCount(), 1u);
    EXPECT_EQ(movements.Get(guard).state, MovementState::Walking);

    // Create an enemy in range.
    Entity enemy = entityManager.Create();
    transforms.Add(enemy, Transform{{2.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    stats.Add(enemy, Stats{50, 50, 0, 0, {}});

    // Set target in blackboard.
    brains.Get(guard).blackboard.Set<Entity>("target", enemy);

    // Tick 2: Attack should succeed (in range), selector stops.
    system.Execute(0.016f);
    // Attack returns Success, selector returns Success.
    EXPECT_EQ(system.GetLastTickUpdateCount(), 1u);

    // Kill the enemy.
    stats.Get(enemy).SetHealth(0);

    // Tick 3: Attack fails (dead target), patrol resumes.
    system.Execute(0.016f);
    EXPECT_EQ(movements.Get(guard).state, MovementState::Walking);

    // Clean up.
    entityManager.Destroy(guard);
    entityManager.Destroy(enemy);
}

TEST(AIIntegration, SchedulerRegistration) {
    ComponentStorage<AIBrain> brains;
    ComponentStorage<Transform> transforms;
    ComponentStorage<Movement> movements;
    ComponentStorage<Stats> stats;
    ComponentStorage<ThreatList> threatLists;

    SystemScheduler scheduler;
    auto& system = scheduler.Register<AISystem>(
        brains, transforms, movements, stats, threatLists);

    EXPECT_EQ(system.GetName(), "AISystem");
    EXPECT_EQ(scheduler.SystemCount(), 1u);
    EXPECT_TRUE(scheduler.Build());

    // Execute with no entities (should not crash).
    scheduler.Execute(1.0f / 60.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
// Performance test: throttled AI on many entities (SRS-GML-004.4)
// ═══════════════════════════════════════════════════════════════════════════

TEST(AIPerformance, ThrottledUpdateOneThousandEntities) {
    ComponentStorage<AIBrain> brains;
    ComponentStorage<Transform> transforms;
    ComponentStorage<Movement> movements;
    ComponentStorage<Stats> stats;
    ComponentStorage<ThreatList> threatLists;

    constexpr uint32_t kEntityCount = 1000;

    // Create 1000 AI entities with staggered tick intervals.
    for (uint32_t i = 0; i < kEntityCount; ++i) {
        Entity e(i, 0);
        float x = static_cast<float>(i % 100) * 5.0f;
        float z = static_cast<float>(i / 100) * 5.0f;
        transforms.Add(e, Transform{{x, 0.0f, z}, {}, {1.0f, 1.0f, 1.0f}});
        movements.Add(e, Movement{3.0f, 3.0f, {}, MovementState::Idle});

        AIBrain brain;
        brain.behaviorTree = std::make_shared<BTAction>(
            [](BTContext&) { return BTStatus::Success; });
        // Stagger tick intervals: 0.1s, 0.2s, 0.3s, 0.4s, ...
        brain.tickInterval = 0.1f * static_cast<float>((i % 4) + 1);
        brains.Add(e, std::move(brain));
    }

    AISystem system(brains, transforms, movements, stats, threatLists, 0.1f);

    // Tick at 0.1s — only entities with 0.1s interval should update.
    system.Execute(0.1f);
    uint32_t firstTick = system.GetLastTickUpdateCount();
    EXPECT_GT(firstTick, 0u);
    EXPECT_LT(firstTick, kEntityCount);

    // Tick at 0.3s total (0.1 + 0.2) — more entities should update.
    system.Execute(0.2f);
    uint32_t secondTick = system.GetLastTickUpdateCount();
    EXPECT_GT(secondTick, firstTick);

    // After enough time, all stagger groups should tick in a single frame.
    // Accumulate to 0.4s per entity — groups need 0.1, 0.2, 0.3, 0.4.
    system.Execute(0.1f);  // 0.4s total: timers = 0.1s:0.1, 0.2s:0.1, 0.3s:0.3, 0.4s:0.4
    system.Execute(0.4f);  // 0.8s total: all timers exceed their intervals
    EXPECT_EQ(system.GetLastTickUpdateCount(), kEntityCount);
}
