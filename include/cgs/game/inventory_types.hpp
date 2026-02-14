#pragma once

/// @file inventory_types.hpp
/// @brief Enumerations and constants for the inventory system.
///
/// @see SDS-MOD-035
/// @see SRS-GML-006

#include <cstdint>

namespace cgs::game {

/// Default number of inventory bag slots.
constexpr uint32_t kDefaultInventoryCapacity = 40;

/// Equipment slot classification.
///
/// COUNT is a sentinel used for array sizing.
enum class EquipSlot : uint8_t {
    Head,
    Neck,
    Shoulders,
    Chest,
    Waist,
    Legs,
    Feet,
    Wrists,
    Hands,
    Finger1,
    Finger2,
    Trinket1,
    Trinket2,
    MainHand,
    OffHand,
    Ranged,
    Tabard,
    COUNT  ///< Sentinel — total number of equipment slots.
};

/// Number of equipment slots.
constexpr std::size_t kEquipSlotCount = static_cast<std::size_t>(EquipSlot::COUNT);

/// Item type classification.
enum class ItemType : uint8_t {
    Consumable,
    Weapon,
    Armor,
    Accessory,
    Material,
    Quest,
    Container,
    Reagent,
    Miscellaneous
};

/// Item quality / rarity tier.
enum class ItemQuality : uint8_t { Poor, Common, Uncommon, Rare, Epic, Legendary };

/// Durability value meaning the item is indestructible.
constexpr int32_t kIndestructible = -1;

}  // namespace cgs::game
