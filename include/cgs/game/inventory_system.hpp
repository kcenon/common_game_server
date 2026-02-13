#pragma once

/// @file inventory_system.hpp
/// @brief InventorySystem: per-tick inventory processing.
///
/// Processes durability events and enchant timer expiration each frame.
///
/// @see SRS-GML-006.4
/// @see SDS-MOD-035

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/inventory_components.hpp"

#include <string_view>
#include <vector>

namespace cgs::game {

/// System that processes inventory logic each tick.
///
/// Execution within a single tick:
///   1. Process durability events from combat/use
///   2. Tick enchant durations and remove expired enchants
///
/// Runs in PostUpdate stage so that combat damage events from the
/// Update stage are available as DurabilityEvents.
class InventorySystem final : public cgs::ecs::ISystem {
public:
    InventorySystem(cgs::ecs::ComponentStorage<Inventory>& inventories,
                    cgs::ecs::ComponentStorage<Equipment>& equipment,
                    cgs::ecs::ComponentStorage<DurabilityEvent>& durabilityEvents);

    void Execute(float deltaTime) override;

    [[nodiscard]] cgs::ecs::SystemStage GetStage() const override {
        return cgs::ecs::SystemStage::PostUpdate;
    }

    [[nodiscard]] std::string_view GetName() const override { return "InventorySystem"; }

    [[nodiscard]] cgs::ecs::SystemAccessInfo GetAccessInfo() const override;

    /// Register an item template for lookup.
    void RegisterTemplate(ItemTemplate tmpl);

    /// Get a registered item template by ID.
    [[nodiscard]] const ItemTemplate* GetTemplate(uint32_t templateId) const;

    /// Get all registered templates (for stat bonus calculation).
    [[nodiscard]] const std::vector<ItemTemplate>& GetTemplates() const { return templates_; }

private:
    /// Process pending durability events.
    void processDurabilityEvents();

    /// Tick enchant durations on equipped items and inventory items.
    void updateEnchants(float deltaTime);

    cgs::ecs::ComponentStorage<Inventory>& inventories_;
    cgs::ecs::ComponentStorage<Equipment>& equipment_;
    cgs::ecs::ComponentStorage<DurabilityEvent>& durabilityEvents_;
    std::vector<ItemTemplate> templates_;
};

}  // namespace cgs::game
