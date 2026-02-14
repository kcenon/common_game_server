/// @file inventory_system.cpp
/// @brief InventorySystem implementation.
///
/// Implements the two-phase inventory tick:
///   1. Durability event processing (reduce equipment durability)
///   2. Enchant timer updates (remove expired timed enchantments)
///
/// @see SRS-GML-006.4
/// @see SDS-MOD-035

#include "cgs/game/inventory_system.hpp"

#include <algorithm>

namespace cgs::game {

InventorySystem::InventorySystem(cgs::ecs::ComponentStorage<Inventory>& inventories,
                                 cgs::ecs::ComponentStorage<Equipment>& equipment,
                                 cgs::ecs::ComponentStorage<DurabilityEvent>& durabilityEvents)
    : inventories_(inventories), equipment_(equipment), durabilityEvents_(durabilityEvents) {}

void InventorySystem::Execute(float deltaTime) {
    processDurabilityEvents();
    updateEnchants(deltaTime);
}

cgs::ecs::SystemAccessInfo InventorySystem::GetAccessInfo() const {
    cgs::ecs::SystemAccessInfo info;
    cgs::ecs::Write<Inventory, Equipment, DurabilityEvent>::Apply(info);
    return info;
}

void InventorySystem::RegisterTemplate(ItemTemplate tmpl) {
    auto it = std::find_if(templates_.begin(), templates_.end(), [&tmpl](const ItemTemplate& t) {
        return t.id == tmpl.id;
    });
    if (it != templates_.end()) {
        *it = std::move(tmpl);
    } else {
        templates_.push_back(std::move(tmpl));
    }
}

const ItemTemplate* InventorySystem::GetTemplate(uint32_t templateId) const {
    auto it = std::find_if(templates_.begin(),
                           templates_.end(),
                           [templateId](const ItemTemplate& t) { return t.id == templateId; });
    return (it != templates_.end()) ? &(*it) : nullptr;
}

// -- Durability event processing ----------------------------------------------

void InventorySystem::processDurabilityEvents() {
    for (std::size_t i = 0; i < durabilityEvents_.Size(); ++i) {
        auto entityId = durabilityEvents_.EntityAt(i);
        cgs::ecs::Entity eventEntity(entityId, 0);
        auto& event = durabilityEvents_.Get(eventEntity);

        if (event.processed) {
            continue;
        }

        if (equipment_.Has(event.player)) {
            auto& equip = equipment_.Get(event.player);
            auto slotIdx = static_cast<std::size_t>(event.slot);
            if (slotIdx < kEquipSlotCount) {
                equip.slots[slotIdx].ReduceDurability(event.amount);
            }
        }

        event.processed = true;
    }
}

// -- Enchant timer updates ----------------------------------------------------

void InventorySystem::updateEnchants(float deltaTime) {
    // Tick enchants on equipment.
    for (std::size_t i = 0; i < equipment_.Size(); ++i) {
        auto entityId = equipment_.EntityAt(i);
        cgs::ecs::Entity entity(entityId, 0);
        auto& equip = equipment_.Get(entity);

        for (auto& slot : equip.slots) {
            if (slot.IsEmpty()) {
                continue;
            }
            for (auto& enc : slot.enchants) {
                if (enc.durationRemaining.has_value()) {
                    *enc.durationRemaining -= deltaTime;
                }
            }
            slot.RemoveExpiredEnchants();
        }
    }

    // Tick enchants on inventory items.
    for (std::size_t i = 0; i < inventories_.Size(); ++i) {
        auto entityId = inventories_.EntityAt(i);
        cgs::ecs::Entity entity(entityId, 0);
        auto& inv = inventories_.Get(entity);

        for (auto& slot : inv.slots) {
            if (slot.IsEmpty()) {
                continue;
            }
            for (auto& enc : slot.enchants) {
                if (enc.durationRemaining.has_value()) {
                    *enc.durationRemaining -= deltaTime;
                }
            }
            slot.RemoveExpiredEnchants();
        }
    }
}

}  // namespace cgs::game
