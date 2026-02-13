#pragma once

/// @file object_system.hpp
/// @brief ObjectUpdateSystem: per-tick processing of game objects.
///
/// Iterates all entities that possess Transform and Movement components
/// and updates their position each tick.  Entities in the Idle state are
/// efficiently skipped.
///
/// @see SRS-GML-001.5
/// @see SDS-MOD-020

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/query.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/components.hpp"

#include <string_view>

namespace cgs::game {

/// System that updates game object positions based on movement.
///
/// Each tick, for every entity with both Transform and Movement:
///   position += direction * speed * deltaTime
///
/// The system declares Read access to Movement and Write access to
/// Transform so the scheduler can parallelize it with non-conflicting
/// systems.
class ObjectUpdateSystem final : public cgs::ecs::ISystem {
public:
    /// Construct with references to the component storages.
    ///
    /// The storages must outlive this system.
    ObjectUpdateSystem(cgs::ecs::ComponentStorage<Transform>& transforms,
                       cgs::ecs::ComponentStorage<Movement>& movements);

    /// Execute the position update for all movable entities.
    void Execute(float deltaTime) override;

    /// This system runs in the main Update stage.
    [[nodiscard]] cgs::ecs::SystemStage GetStage() const override {
        return cgs::ecs::SystemStage::Update;
    }

    [[nodiscard]] std::string_view GetName() const override { return "ObjectUpdateSystem"; }

    /// Declare component access for parallel scheduling.
    [[nodiscard]] cgs::ecs::SystemAccessInfo GetAccessInfo() const override;

private:
    cgs::ecs::ComponentStorage<Transform>& transforms_;
    cgs::ecs::ComponentStorage<Movement>& movements_;
};

}  // namespace cgs::game
