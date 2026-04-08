// examples/07_inventory.cpp
//
// Tutorial: Inventory and equipment slots
// See: docs/tutorial_inventory.dox
//
// This example creates a player inventory, adds a stack of potions,
// a unique weapon, and a piece of armor, then demonstrates equipping,
// unequipping, swapping, and stack splitting. Everything happens in
// plain ECS components with no database or persistence layer — the
// inventory is a single Inventory component on one entity.

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/game/inventory_components.hpp"
#include "cgs/game/inventory_types.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

using cgs::ecs::ComponentStorage;
using cgs::ecs::Entity;
using cgs::ecs::EntityManager;
using cgs::game::EquipSlot;
using cgs::game::Equipment;
using cgs::game::Inventory;
using cgs::game::InventorySlot;
using cgs::game::ItemQuality;
using cgs::game::ItemTemplate;
using cgs::game::ItemType;

namespace {

/// Pretty-print the first N slots of an inventory for visual tracing.
void PrintInventory(const Inventory& inv, std::size_t show = 6) {
    std::cout << "  inventory (first " << show << " slots):\n";
    for (std::size_t i = 0; i < show && i < inv.slots.size(); ++i) {
        const auto& s = inv.slots[i];
        if (s.IsEmpty()) {
            std::cout << "    [" << i << "] empty\n";
        } else {
            std::cout << "    [" << i << "] item " << s.itemId
                      << " x" << s.count << " durability " << s.durability << "\n";
        }
    }
    std::cout << "  free slots: " << inv.FreeSlots()
              << ", currency: " << inv.currency << "\n";
}

}  // namespace

int main() {
    // ── Step 1: Build a small item catalog (static game data). ───────────
    //
    // In a real server these would be loaded from YAML/JSON/DB at
    // startup. Here we just declare a handful of templates inline.
    ItemTemplate healthPotion;
    healthPotion.id = 1001;
    healthPotion.name = "Minor Health Potion";
    healthPotion.type = ItemType::Consumable;
    healthPotion.quality = ItemQuality::Common;
    healthPotion.maxStackSize = 20;
    healthPotion.vendorPrice = 25;

    ItemTemplate sword;
    sword.id = 2001;
    sword.name = "Iron Longsword";
    sword.type = ItemType::Weapon;
    sword.quality = ItemQuality::Uncommon;
    sword.maxStackSize = 1;
    sword.maxDurability = 100;
    sword.equipSlot = EquipSlot::MainHand;
    sword.statBonuses.minDamage = 15;
    sword.statBonuses.maxDamage = 25;

    ItemTemplate shield;
    shield.id = 2002;
    shield.name = "Iron Buckler";
    shield.type = ItemType::Armor;
    shield.quality = ItemQuality::Common;
    shield.maxStackSize = 1;
    shield.maxDurability = 80;
    shield.equipSlot = EquipSlot::OffHand;
    shield.statBonuses.armor = 20;

    // ── Step 2: Create the player entity with an Inventory + Equipment. ─
    ComponentStorage<Inventory> inventories;
    ComponentStorage<Equipment> equipment;

    EntityManager em;
    em.RegisterStorage(&inventories);
    em.RegisterStorage(&equipment);

    const Entity player = em.Create();
    inventories.Add(player, Inventory{});
    equipment.Add(player, Equipment{});

    auto& inv = inventories.Get(player);
    inv.Initialize();        // allocate kDefaultInventoryCapacity (40) slots
    inv.currency = 500;      // starting currency

    // ── Step 3: Add 12 potions. Stacks up to 20 per slot. ───────────────
    const uint32_t addedPotions = inv.AddItem(healthPotion, 12);
    std::cout << "added " << addedPotions << " potions\n";

    // ── Step 4: Add a sword and a shield. Each takes one slot. ──────────
    inv.AddItem(sword, 1);
    inv.AddItem(shield, 1);
    PrintInventory(inv);

    // ── Step 5: Equip the sword. Find it in the bag, pull it out, then
    //         put it into Equipment::MainHand. Equipment::Equip returns
    //         whatever was in the slot (possibly empty).
    auto swordSlotIdx = inv.FindItem(sword.id);
    if (swordSlotIdx.has_value()) {
        InventorySlot toEquip = inv.slots[*swordSlotIdx];
        inv.slots[*swordSlotIdx].Clear();

        InventorySlot previous = equipment.Get(player).Equip(EquipSlot::MainHand, toEquip);
        if (!previous.IsEmpty()) {
            // Put the replaced item back in the bag.
            inv.AddItem(healthPotion, previous.count);  // sloppy stand-in
        }
        std::cout << "equipped sword in MainHand\n";
    }

    // ── Step 6: Split a potion stack. Move 5 potions to a new slot.
    auto potionSlot = inv.FindItem(healthPotion.id);
    if (potionSlot.has_value()) {
        auto newSlot = inv.SplitStack(*potionSlot, 5);
        if (newSlot.has_value()) {
            std::cout << "split 5 potions into slot " << *newSlot << "\n";
        }
    }
    PrintInventory(inv);

    // ── Step 7: Unequip the sword and put it back in the bag. ────────────
    {
        InventorySlot removed = equipment.Get(player).Unequip(EquipSlot::MainHand);
        if (!removed.IsEmpty()) {
            // Find an empty slot to deposit the unequipped sword.
            for (auto& slot : inv.slots) {
                if (slot.IsEmpty()) {
                    slot = removed;
                    break;
                }
            }
            std::cout << "unequipped sword\n";
        }
    }

    // ── Step 8: Count potions and weapons after all operations. ─────────
    std::cout << "total potions in bag: " << inv.CountItem(healthPotion.id) << "\n";
    std::cout << "total swords in bag: " << inv.CountItem(sword.id) << "\n";

    // ── Step 9: Look up bonuses from equipped gear. ─────────────────────
    const std::vector<ItemTemplate> templates = {healthPotion, sword, shield};
    const auto bonuses = equipment.Get(player).CalculateStatBonuses(templates);
    std::cout << "equipped stats: armor=" << bonuses.armor
              << ", minDamage=" << bonuses.minDamage
              << ", maxDamage=" << bonuses.maxDamage << "\n";

    return EXIT_SUCCESS;
}
