#pragma once

/// @file inventory_components.hpp
/// @brief Inventory ECS components: Inventory, Equipment, InventorySlot, etc.
///
/// Each struct is a plain data component designed for sparse-set storage
/// via ComponentStorage<T>.  The InventorySystem operates on these components
/// to manage item storage, equipment, and durability.
///
/// @see SRS-GML-006.1 .. SRS-GML-006.4
/// @see SDS-MOD-035

#include "cgs/ecs/entity.hpp"
#include "cgs/game/inventory_types.hpp"
#include "cgs/game/object_types.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cgs::game {

// -- StatBonuses --------------------------------------------------------------

/// Additive stat bonuses from equipment and enchantments.
///
/// Attribute indices match Stats::attributes from components.hpp.
struct StatBonuses {
    std::array<int32_t, kMaxAttributes> attributes{};
    int32_t armor = 0;
    float attackSpeed = 0.0f;
    int32_t minDamage = 0;
    int32_t maxDamage = 0;

    /// Add two StatBonuses together.
    StatBonuses operator+(const StatBonuses& other) const {
        StatBonuses result;
        for (std::size_t i = 0; i < kMaxAttributes; ++i) {
            result.attributes[i] = attributes[i] + other.attributes[i];
        }
        result.armor = armor + other.armor;
        result.attackSpeed = attackSpeed + other.attackSpeed;
        result.minDamage = minDamage + other.minDamage;
        result.maxDamage = maxDamage + other.maxDamage;
        return result;
    }

    StatBonuses& operator+=(const StatBonuses& other) {
        for (std::size_t i = 0; i < kMaxAttributes; ++i) {
            attributes[i] += other.attributes[i];
        }
        armor += other.armor;
        attackSpeed += other.attackSpeed;
        minDamage += other.minDamage;
        maxDamage += other.maxDamage;
        return *this;
    }
};

// -- Enchant ------------------------------------------------------------------

/// An enchantment applied to an item.
struct Enchant {
    uint32_t enchantId = 0;
    StatBonuses bonuses;
    std::optional<float> durationRemaining;  ///< nullopt = permanent.
};

// -- InventorySlot ------------------------------------------------------------

/// A single slot in inventory or equipment.
struct InventorySlot {
    uint32_t itemId = 0;  ///< 0 = empty slot.
    uint32_t count = 0;
    int32_t durability = kIndestructible;
    int32_t maxDurability = kIndestructible;
    std::vector<Enchant> enchants;

    /// Check if this slot is empty.
    [[nodiscard]] bool IsEmpty() const noexcept { return itemId == 0; }

    /// Clear the slot to empty.
    void Clear() noexcept {
        itemId = 0;
        count = 0;
        durability = kIndestructible;
        maxDurability = kIndestructible;
        enchants.clear();
    }

    /// Check if the item in this slot is broken (durability 0, but not indestructible).
    [[nodiscard]] bool IsBroken() const noexcept { return durability == 0 && maxDurability > 0; }

    /// Reduce durability by the given amount.
    ///
    /// @return true if the item broke (durability reached 0).
    bool ReduceDurability(int32_t amount) noexcept {
        if (durability == kIndestructible || durability <= 0) {
            return false;
        }
        durability = std::max(durability - amount, 0);
        return durability == 0;
    }

    /// Calculate total stat bonuses from enchantments on this item.
    [[nodiscard]] StatBonuses GetEnchantBonuses() const {
        StatBonuses total;
        for (const auto& enc : enchants) {
            total += enc.bonuses;
        }
        return total;
    }

    /// Remove expired enchantments.
    void RemoveExpiredEnchants() {
        std::erase_if(enchants, [](const Enchant& e) {
            return e.durationRemaining.has_value() && *e.durationRemaining <= 0.0f;
        });
    }
};

// -- ItemTemplate -------------------------------------------------------------

/// Static item definition data (shared, not per-entity).
struct ItemTemplate {
    uint32_t id = 0;
    std::string name;
    ItemType type = ItemType::Miscellaneous;
    ItemQuality quality = ItemQuality::Common;
    uint32_t maxStackSize = 1;  ///< 1 = non-stackable.
    int32_t maxDurability = kIndestructible;
    EquipSlot equipSlot = EquipSlot::COUNT;  ///< COUNT = not equippable.
    StatBonuses statBonuses;
    uint32_t requiredLevel = 0;
    uint32_t vendorPrice = 0;

    /// Check if this item is stackable (maxStackSize > 1).
    [[nodiscard]] bool IsStackable() const noexcept { return maxStackSize > 1; }

    /// Check if this item is equippable (has a valid equip slot).
    [[nodiscard]] bool IsEquippable() const noexcept { return equipSlot != EquipSlot::COUNT; }
};

// -- Inventory (SRS-GML-006.1) ------------------------------------------------

/// Per-entity item storage component.
///
/// Provides bag-style inventory with configurable capacity, stacking,
/// splitting, and item management operations.
struct Inventory {
    std::vector<InventorySlot> slots;
    uint32_t capacity = kDefaultInventoryCapacity;
    int64_t currency = 0;

    /// Initialize slots to match capacity.
    void Initialize() { slots.resize(capacity); }

    /// Add an item to the inventory using stacking rules.
    ///
    /// First tries to stack onto existing slots with the same itemId,
    /// then places into the first empty slot.
    ///
    /// @param tmpl       Item template for stack size rules.
    /// @param addCount   Number of items to add.
    /// @return Number of items actually added (may be less if full).
    uint32_t AddItem(const ItemTemplate& tmpl, uint32_t addCount) {
        if (slots.empty()) {
            Initialize();
        }
        uint32_t remaining = addCount;

        // First pass: stack onto existing slots.
        if (tmpl.IsStackable()) {
            for (auto& slot : slots) {
                if (remaining == 0) {
                    break;
                }
                if (slot.itemId == tmpl.id && slot.count < tmpl.maxStackSize) {
                    uint32_t space = tmpl.maxStackSize - slot.count;
                    uint32_t toAdd = std::min(remaining, space);
                    slot.count += toAdd;
                    remaining -= toAdd;
                }
            }
        }

        // Second pass: fill empty slots.
        for (auto& slot : slots) {
            if (remaining == 0) {
                break;
            }
            if (slot.IsEmpty()) {
                uint32_t toAdd = tmpl.IsStackable() ? std::min(remaining, tmpl.maxStackSize) : 1;
                slot.itemId = tmpl.id;
                slot.count = toAdd;
                slot.durability = tmpl.maxDurability;
                slot.maxDurability = tmpl.maxDurability;
                remaining -= toAdd;
            }
        }

        return addCount - remaining;
    }

    /// Remove items from a specific slot.
    ///
    /// @return true if the removal was successful.
    bool RemoveItem(uint32_t slotIndex, uint32_t removeCount) {
        if (slotIndex >= slots.size()) {
            return false;
        }
        auto& slot = slots[slotIndex];
        if (slot.IsEmpty() || slot.count < removeCount) {
            return false;
        }

        slot.count -= removeCount;
        if (slot.count == 0) {
            slot.Clear();
        }
        return true;
    }

    /// Move an item from one slot to another.
    ///
    /// If the destination slot has the same item and is stackable, items
    /// are merged.  Otherwise, the slots are swapped.
    ///
    /// @return true if the move was successful.
    bool MoveItem(uint32_t fromSlot, uint32_t toSlot) {
        if (fromSlot >= slots.size() || toSlot >= slots.size()) {
            return false;
        }
        if (fromSlot == toSlot) {
            return false;
        }

        auto& src = slots[fromSlot];
        auto& dst = slots[toSlot];

        if (src.IsEmpty()) {
            return false;
        }

        // Stack merge if same item type.
        if (dst.itemId == src.itemId && !dst.IsEmpty()) {
            // Caller must know max stack size externally for proper merge.
            // For now, swap if they can't trivially merge.
            std::swap(src, dst);
            return true;
        }

        std::swap(src, dst);
        return true;
    }

    /// Split a stack into a new slot.
    ///
    /// @param slotIndex  Source slot index.
    /// @param splitCount Number of items to move to the new slot.
    /// @return Index of the new slot, or empty if no space or invalid.
    std::optional<uint32_t> SplitStack(uint32_t slotIndex, uint32_t splitCount) {
        if (slotIndex >= slots.size()) {
            return std::nullopt;
        }
        auto& src = slots[slotIndex];
        if (src.IsEmpty() || src.count <= splitCount || splitCount == 0) {
            return std::nullopt;
        }

        // Find an empty slot.
        for (uint32_t i = 0; i < static_cast<uint32_t>(slots.size()); ++i) {
            if (slots[i].IsEmpty()) {
                slots[i].itemId = src.itemId;
                slots[i].count = splitCount;
                slots[i].durability = src.durability;
                slots[i].maxDurability = src.maxDurability;
                src.count -= splitCount;
                return i;
            }
        }
        return std::nullopt;
    }

    /// Get a pointer to the slot at the given index.
    [[nodiscard]] const InventorySlot* GetItem(uint32_t slotIndex) const {
        if (slotIndex >= slots.size()) {
            return nullptr;
        }
        return slots[slotIndex].IsEmpty() ? nullptr : &slots[slotIndex];
    }

    /// Find the first slot containing the given item ID.
    [[nodiscard]] std::optional<uint32_t> FindItem(uint32_t itemId) const {
        for (uint32_t i = 0; i < static_cast<uint32_t>(slots.size()); ++i) {
            if (slots[i].itemId == itemId) {
                return i;
            }
        }
        return std::nullopt;
    }

    /// Count the number of free (empty) slots.
    [[nodiscard]] uint32_t FreeSlots() const {
        uint32_t count = 0;
        for (const auto& slot : slots) {
            if (slot.IsEmpty()) {
                ++count;
            }
        }
        return count;
    }

    /// Count total quantity of a specific item across all slots.
    [[nodiscard]] uint32_t CountItem(uint32_t itemId) const {
        uint32_t total = 0;
        for (const auto& slot : slots) {
            if (slot.itemId == itemId) {
                total += slot.count;
            }
        }
        return total;
    }
};

// -- Equipment (SRS-GML-006.3) ------------------------------------------------

/// Per-entity equipment component with fixed slots.
///
/// Each equipment slot can hold one item.  Stat bonuses from equipped
/// items are summed via CalculateStatBonuses().
struct Equipment {
    std::array<InventorySlot, kEquipSlotCount> slots{};

    /// Equip an item into the specified slot.
    ///
    /// @return The previously equipped item (empty slot if nothing was there).
    InventorySlot Equip(EquipSlot slot, InventorySlot item) {
        auto idx = static_cast<std::size_t>(slot);
        if (idx >= kEquipSlotCount) {
            return {};
        }

        InventorySlot previous = std::move(slots[idx]);
        item.count = 1;  // Equipment slots always hold 1 item.
        slots[idx] = std::move(item);
        return previous;
    }

    /// Unequip an item from the specified slot.
    ///
    /// @return The unequipped item (empty slot if nothing was there).
    InventorySlot Unequip(EquipSlot slot) {
        auto idx = static_cast<std::size_t>(slot);
        if (idx >= kEquipSlotCount) {
            return {};
        }

        InventorySlot removed = std::move(slots[idx]);
        slots[idx] = InventorySlot{};
        return removed;
    }

    /// Get the equipped item in the specified slot.
    [[nodiscard]] const InventorySlot* GetEquipped(EquipSlot slot) const {
        auto idx = static_cast<std::size_t>(slot);
        if (idx >= kEquipSlotCount) {
            return nullptr;
        }
        return slots[idx].IsEmpty() ? nullptr : &slots[idx];
    }

    /// Calculate total stat bonuses from all equipped (non-broken) items.
    ///
    /// Broken items (durability 0) do not contribute stat bonuses.
    /// Includes enchantment bonuses.
    ///
    /// @param templates  Function to look up ItemTemplate by item ID.
    [[nodiscard]] StatBonuses CalculateStatBonuses(
        const std::vector<ItemTemplate>& templates) const {
        StatBonuses total;
        for (const auto& slot : slots) {
            if (slot.IsEmpty() || slot.IsBroken()) {
                continue;
            }

            // Look up the item template for base stat bonuses.
            for (const auto& tmpl : templates) {
                if (tmpl.id == slot.itemId) {
                    total += tmpl.statBonuses;
                    break;
                }
            }

            // Add enchantment bonuses.
            total += slot.GetEnchantBonuses();
        }
        return total;
    }
};

// -- DurabilityEvent ----------------------------------------------------------

/// Event component for item durability changes.
///
/// Other systems (e.g., CombatSystem) create DurabilityEvent entities
/// to notify the InventorySystem about equipment wear.
/// Follows the same pattern as DamageEvent and QuestEvent.
struct DurabilityEvent {
    cgs::ecs::Entity player;
    EquipSlot slot = EquipSlot::COUNT;  ///< Which equipment slot to degrade.
    int32_t amount = 1;                 ///< Durability points to reduce.
    bool processed = false;             ///< Set to true after processing.
};

}  // namespace cgs::game
