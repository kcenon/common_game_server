// examples/06_ai_behavior.cpp
//
// Tutorial: AI and behavior trees
// See: docs/tutorial_ai_behavior.dox
//
// This example builds a small behavior tree for a patrol-aggro-flee
// NPC using the pure C++ API of cgs::game::behavior_tree. It ticks
// the tree a handful of times with a mock blackboard so you can see
// how the tree status changes as conditions change.
//
// The tree has three branches inside a top-level Selector:
//
//   Selector (root)
//     ├── Sequence ("flee when low HP")
//     │     ├── Condition: health < threshold
//     │     └── Action:    MoveAwayFromThreat
//     ├── Sequence ("chase & attack nearby enemy")
//     │     ├── Condition: enemy within aggro range
//     │     └── Action:    AttackTarget
//     └── Action ("patrol waypoint")
//           └── MoveToNextWaypoint
//
// The Selector succeeds as soon as one branch succeeds, so the NPC
// always does the highest-priority thing it can. Patrol is the
// fallback when nothing else applies.

#include "cgs/game/ai_types.hpp"
#include "cgs/game/behavior_tree.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using cgs::game::Blackboard;
using cgs::game::BTAction;
using cgs::game::BTCondition;
using cgs::game::BTContext;
using cgs::game::BTNode;
using cgs::game::BTSelector;
using cgs::game::BTSequence;
using cgs::game::BTStatus;

namespace {

/// Build the patrol-aggro-flee tree described above.
///
/// The returned root is a unique_ptr<BTSelector>. You can either keep
/// it per-NPC (cheap for this small tree) or share it across NPCs via
/// a shared_ptr — the framework supports both.
std::unique_ptr<BTNode> BuildBehaviorTree() {
    auto root = std::make_unique<BTSelector>();

    // ── Branch 1: flee when health is below threshold ────────────────
    auto fleeSeq = std::make_unique<BTSequence>();
    fleeSeq->AddChild(std::make_unique<BTCondition>([](BTContext& ctx) {
        const int* hp = ctx.blackboard->Get<int>("health");
        return hp != nullptr && *hp < 30;
    }));
    fleeSeq->AddChild(std::make_unique<BTAction>([](BTContext& ctx) {
        ctx.blackboard->Set<std::string>("last_action", "flee");
        return BTStatus::Success;
    }));
    root->AddChild(std::move(fleeSeq));

    // ── Branch 2: chase and attack a nearby enemy ────────────────────
    auto attackSeq = std::make_unique<BTSequence>();
    attackSeq->AddChild(std::make_unique<BTCondition>([](BTContext& ctx) {
        const float* dist = ctx.blackboard->Get<float>("enemy_distance");
        return dist != nullptr && *dist < 10.0f;
    }));
    attackSeq->AddChild(std::make_unique<BTAction>([](BTContext& ctx) {
        ctx.blackboard->Set<std::string>("last_action", "attack");
        return BTStatus::Success;
    }));
    root->AddChild(std::move(attackSeq));

    // ── Branch 3: patrol waypoint (default fallback) ─────────────────
    root->AddChild(std::make_unique<BTAction>([](BTContext& ctx) {
        int* wp = ctx.blackboard->Get<int>("waypoint");
        if (wp == nullptr) {
            ctx.blackboard->Set<int>("waypoint", 1);
        } else {
            *wp = (*wp % 3) + 1;
        }
        ctx.blackboard->Set<std::string>("last_action", "patrol");
        return BTStatus::Success;
    }));

    return root;
}

void PrintTickResult(int tick, BTStatus status, const Blackboard& bb) {
    const char* name = "?";
    switch (status) {
        case BTStatus::Success: name = "Success"; break;
        case BTStatus::Failure: name = "Failure"; break;
        case BTStatus::Running: name = "Running"; break;
    }
    const auto* last = bb.Get<std::string>("last_action");
    std::cout << "tick " << tick << ": " << name
              << ", last_action=" << (last ? *last : "(none)") << "\n";
}

}  // namespace

int main() {
    auto tree = BuildBehaviorTree();

    // Construct a blackboard and an execution context. Both stay
    // alive for the whole tick loop; we just mutate the blackboard
    // between ticks to simulate changing world state.
    Blackboard bb;
    BTContext ctx;
    ctx.entity = {};              // no concrete Entity for this example
    ctx.deltaTime = 0.05f;
    ctx.blackboard = &bb;

    // ── Tick 0: no stimulus, NPC patrols. ──────────────────────────
    PrintTickResult(0, tree->Tick(ctx), bb);

    // ── Tick 1: an enemy approaches (distance 6). ──────────────────
    bb.Set<float>("enemy_distance", 6.0f);
    PrintTickResult(1, tree->Tick(ctx), bb);

    // ── Tick 2: enemy keeps pressing, NPC keeps attacking. ─────────
    PrintTickResult(2, tree->Tick(ctx), bb);

    // ── Tick 3: NPC takes damage, health drops below 30. Flee wins. ─
    bb.Set<int>("health", 20);
    PrintTickResult(3, tree->Tick(ctx), bb);

    // ── Tick 4: NPC escapes, health is restored to 100, enemy lost. ─
    bb.Set<int>("health", 100);
    bb.Erase("enemy_distance");
    PrintTickResult(4, tree->Tick(ctx), bb);

    // ── Tick 5: back to patrol, waypoint advances to 2. ─────────────
    PrintTickResult(5, tree->Tick(ctx), bb);

    return EXIT_SUCCESS;
}
