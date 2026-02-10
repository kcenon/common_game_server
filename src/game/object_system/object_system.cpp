/// @file object_system.cpp
/// @brief ObjectUpdateSystem implementation.
///
/// Updates entity positions by integrating movement direction and speed
/// over the frame delta time.  Uses Query<Transform, Movement> to
/// iterate only entities that possess both components.
///
/// @see SRS-GML-001.5
/// @see SDS-MOD-020

#include "cgs/game/object_system.hpp"

namespace cgs::game {

ObjectUpdateSystem::ObjectUpdateSystem(
    cgs::ecs::ComponentStorage<Transform>& transforms,
    cgs::ecs::ComponentStorage<Movement>& movements)
    : transforms_(transforms), movements_(movements) {}

void ObjectUpdateSystem::Execute(float deltaTime) {
    cgs::ecs::Query<Transform, Movement> query(transforms_, movements_);

    query.ForEach([deltaTime](cgs::ecs::Entity /*entity*/,
                              Transform& transform,
                              Movement& movement) {
        // Skip idle entities — no position change needed.
        if (movement.state == MovementState::Idle) {
            return;
        }

        // Integrate: position += direction * speed * dt.
        transform.position += movement.direction * movement.speed * deltaTime;
    });
}

cgs::ecs::SystemAccessInfo ObjectUpdateSystem::GetAccessInfo() const {
    cgs::ecs::SystemAccessInfo info;
    cgs::ecs::Write<Transform>::Apply(info);
    cgs::ecs::Read<Movement>::Apply(info);
    return info;
}

} // namespace cgs::game
