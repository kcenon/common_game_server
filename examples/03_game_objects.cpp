// examples/03_game_objects.cpp
//
// Tutorial: Game objects — Transform, Identity, Stats, Movement
// See: docs/tutorial_game_objects.dox
//
// This example spawns 100 NPCs arranged in a 10x10 grid, then ticks
// them forward with ObjectUpdateSystem so you can see how Movement
// drives Transform each frame.
//
// The pattern you will learn:
//   1. Create ComponentStorage<T> for every component type you need
//   2. Create an EntityManager and RegisterStorage() each storage
//   3. Create entities with EntityManager::Create()
//   4. Attach components via ComponentStorage<T>::Add(entity, ...)
//   5. Drive one tick of the world with ObjectUpdateSystem::Execute(dt)

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/game/components.hpp"
#include "cgs/game/math_types.hpp"
#include "cgs/game/object_system.hpp"
#include "cgs/game/object_types.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using cgs::ecs::ComponentStorage;
using cgs::ecs::Entity;
using cgs::ecs::EntityManager;
using cgs::game::GenerateGUID;
using cgs::game::Identity;
using cgs::game::Movement;
using cgs::game::MovementState;
using cgs::game::ObjectType;
using cgs::game::ObjectUpdateSystem;
using cgs::game::Stats;
using cgs::game::Transform;
using cgs::game::Vector3;

namespace {

/// Spawn one NPC at the given position. Returns the entity handle.
///
/// Notice that the caller owns the three storages; this helper only
/// allocates a new Entity and attaches components to existing storages.
Entity SpawnNpc(EntityManager& em,
                ComponentStorage<Transform>& transforms,
                ComponentStorage<Identity>& identities,
                ComponentStorage<Stats>& stats,
                ComponentStorage<Movement>& movements,
                const std::string& name,
                const Vector3& position,
                const Vector3& heading) {
    const Entity e = em.Create();

    // Transform — world-space position, rotation, scale.
    Transform t;
    t.position = position;
    // Default Quaternion rotation and (1,1,1) scale are already set by
    // the in-class member initializers.
    transforms.Add(e, t);

    // Identity — a globally unique GUID plus classification.
    Identity id;
    id.guid = GenerateGUID();
    id.name = name;
    id.type = ObjectType::Creature;
    id.entry = 101;  // Pretend this is a creature template ID.
    identities.Add(e, id);

    // Stats — health / mana / attributes.
    Stats s;
    s.maxHealth = 200;
    s.maxMana = 50;
    s.SetHealth(200);
    s.SetMana(50);
    stats.Add(e, s);

    // Movement — speed + direction drives ObjectUpdateSystem.
    Movement m;
    m.baseSpeed = 3.0f;
    m.speed = 3.0f;
    m.direction = heading;
    m.state = MovementState::Walking;
    movements.Add(e, m);

    return e;
}

}  // namespace

int main() {
    // Step 1 — Declare component storages. Each type you want to
    // iterate on gets its own storage. Keep them on the stack next
    // to the EntityManager so lifetime is trivial.
    ComponentStorage<Transform> transforms;
    ComponentStorage<Identity> identities;
    ComponentStorage<Stats> stats;
    ComponentStorage<Movement> movements;

    // Step 2 — Construct the EntityManager and register every
    // storage so Destroy(entity) can fan out to all of them.
    EntityManager em;
    em.RegisterStorage(&transforms);
    em.RegisterStorage(&identities);
    em.RegisterStorage(&stats);
    em.RegisterStorage(&movements);

    // Step 3 — Spawn 100 NPCs in a 10x10 grid, each walking "+x".
    constexpr int kGridSize = 10;
    constexpr float kSpacing = 5.0f;
    const Vector3 heading{1.0f, 0.0f, 0.0f};

    for (int z = 0; z < kGridSize; ++z) {
        for (int x = 0; x < kGridSize; ++x) {
            const Vector3 pos{
                static_cast<float>(x) * kSpacing,
                0.0f,
                static_cast<float>(z) * kSpacing,
            };
            SpawnNpc(em, transforms, identities, stats, movements,
                     "npc_" + std::to_string(z * kGridSize + x), pos, heading);
        }
    }

    std::cout << "spawned " << em.Count() << " entities\n";

    // Step 4 — Construct the ObjectUpdateSystem. It holds references to
    // the storages it needs. The system outlives the entities it operates
    // on, so passing the storages by reference is safe for the duration
    // of the tick loop.
    ObjectUpdateSystem updateSystem(transforms, movements);

    // Step 5 — Drive 10 ticks of the world at 20 Hz (50 ms per tick).
    // Each tick advances every entity's Transform::position by
    // (direction * speed * deltaTime).
    constexpr float kDeltaTime = 0.05f;
    constexpr int kTickCount = 10;
    for (int tick = 0; tick < kTickCount; ++tick) {
        updateSystem.Execute(kDeltaTime);
    }

    // Step 6 — Sanity check: read the corner entity's position.
    // After 10 ticks of +x motion at speed 3, the x coordinate should
    // have advanced by 10 * 0.05 * 3 = 1.5 units.
    //
    // We find the corner entity by reusing the EntityManager's
    // contiguous-entity semantics: Create() handed out IDs in the
    // order we spawned them. The first entity is the (0, 0, 0) NPC.
    // (In production code, stash the handle you care about when you
    // spawn it; do not re-derive it from the manager.)
    const Entity first = Entity(0, 0);
    if (transforms.Has(first)) {
        const auto& t = transforms.Get(first);
        std::cout << "first NPC at ("
                  << t.position.x << ", "
                  << t.position.y << ", "
                  << t.position.z << ") after "
                  << kTickCount << " ticks\n";
    }

    // Step 7 — Destroy a specific entity. EntityManager::Destroy fans
    // out to every registered storage and removes the components.
    em.Destroy(first);
    std::cout << "after destroy: " << em.Count() << " entities remain\n";

    return EXIT_SUCCESS;
}
