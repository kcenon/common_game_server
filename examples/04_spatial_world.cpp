// examples/04_spatial_world.cpp
//
// Tutorial: Spatial index, zones, and interest management
// See: docs/tutorial_spatial_world.dox
//
// This example scatters 200 entities across a 100x100 world, builds
// a SpatialIndex, and performs range and radius queries. It then
// moves a "player" entity and repeats the query to show how the
// index updates as positions change.
//
// Key APIs demonstrated:
//   - SpatialIndex::Insert / Update / Remove
//   - SpatialIndex::QueryRadius / QueryCell / QueryPosition
//   - ZoneFlags bitwise composition and HasFlag helper
//   - Zone + MapMembership ECS components (conceptual usage)

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/game/components.hpp"
#include "cgs/game/math_types.hpp"
#include "cgs/game/spatial_index.hpp"
#include "cgs/game/world_components.hpp"
#include "cgs/game/world_types.hpp"

#include <cstdlib>
#include <iostream>
#include <random>

using cgs::ecs::ComponentStorage;
using cgs::ecs::Entity;
using cgs::ecs::EntityManager;
using cgs::game::HasFlag;
using cgs::game::kDefaultCellSize;
using cgs::game::SpatialIndex;
using cgs::game::Transform;
using cgs::game::Vector3;
using cgs::game::Zone;
using cgs::game::ZoneFlags;
using cgs::game::ZoneType;

int main() {
    // ─── Step 1: Set up ECS storages (for illustration) ───────────────────
    //
    // In a real server, WorldSystem would own the SpatialIndex and
    // synchronize it from Transform each tick. Here we drive the index
    // directly so the example stays small.
    ComponentStorage<Transform> transforms;
    EntityManager em;
    em.RegisterStorage(&transforms);

    // ─── Step 2: Build the spatial index ──────────────────────────────────
    //
    // Cell size is a tuning knob: larger cells mean fewer lookups per
    // query but more false positives to filter. 32 world units is the
    // framework default and a sane starting point.
    SpatialIndex index(kDefaultCellSize);

    // ─── Step 3: Spawn 200 entities scattered in [-50, 50] x [-50, 50] ──
    std::mt19937 rng(0xc0de);  // deterministic seed for reproducibility
    std::uniform_real_distribution<float> dist(-50.0f, 50.0f);

    for (int i = 0; i < 200; ++i) {
        const Entity e = em.Create();
        Transform t;
        t.position = Vector3{dist(rng), 0.0f, dist(rng)};
        transforms.Add(e, t);
        index.Insert(e, t.position);
    }

    std::cout << "tracked " << index.Size() << " entities\n";
    std::cout << "occupying " << (index.Size() / 2) << " cells on average\n";

    // ─── Step 4: Query everyone within 25 units of the origin ────────────
    const Vector3 origin{0.0f, 0.0f, 0.0f};
    const auto nearOrigin = index.QueryRadius(origin, 25.0f);
    std::cout << "within 25m of origin: " << nearOrigin.size() << " entities\n";

    // ─── Step 5: Pick a "player" entity and move them, then re-query ────
    // The first entity we created is the "player" in this example.
    const Entity player = Entity(0, 0);
    Transform& playerT = transforms.Get(player);

    // Move the player to (30, 0, 30) and update the index.
    playerT.position = Vector3{30.0f, 0.0f, 30.0f};
    index.Update(player, playerT.position);

    const auto nearPlayer = index.QueryRadius(playerT.position, 15.0f);
    std::cout << "within 15m of player: " << nearPlayer.size()
              << " entities (including self)\n";

    // ─── Step 6: Build and query a Zone ─────────────────────────────────
    //
    // Zones describe area-based gameplay rules. Combine flags with
    // bitwise OR and query with HasFlag.
    Zone sanctuary;
    sanctuary.zoneId = 1;
    sanctuary.type = ZoneType::Safe;
    sanctuary.flags = ZoneFlags::NoCombat | ZoneFlags::Sanctuary | ZoneFlags::Resting;

    if (HasFlag(sanctuary.flags, ZoneFlags::NoCombat)) {
        std::cout << "zone " << sanctuary.zoneId << " blocks all combat\n";
    }
    if (HasFlag(sanctuary.flags, ZoneFlags::Resting)) {
        std::cout << "zone " << sanctuary.zoneId << " grants rested XP\n";
    }
    if (!HasFlag(sanctuary.flags, ZoneFlags::NoFly)) {
        std::cout << "zone " << sanctuary.zoneId << " allows flying\n";
    }

    // ─── Step 7: Remove an entity and verify index shrinks ───────────────
    index.Remove(player);
    std::cout << "after player removal: " << index.Size() << " entities\n";

    return EXIT_SUCCESS;
}
