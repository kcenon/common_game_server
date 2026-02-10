#include <gtest/gtest.h>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/inventory_components.hpp"
#include "cgs/game/inventory_system.hpp"
#include "cgs/game/inventory_types.hpp"

using namespace cgs::ecs;
using namespace cgs::game;

// =============================================================================
// InventorySlot component tests
// =============================================================================

TEST(InventorySlotTest, DefaultIsEmpty) {
    InventorySlot slot;
    EXPECT_TRUE(slot.IsEmpty());
    EXPECT_EQ(slot.itemId, 0u);
    EXPECT_EQ(slot.count, 0u);
    EXPECT_FALSE(slot.IsBroken());
}

TEST(InventorySlotTest, ClearResetsSlot) {
    InventorySlot slot;
    slot.itemId = 42;
    slot.count = 5;
    slot.durability = 100;
    slot.maxDurability = 100;

    slot.Clear();
    EXPECT_TRUE(slot.IsEmpty());
    EXPECT_EQ(slot.durability, kIndestructible);
}

TEST(InventorySlotTest, ReduceDurability) {
    InventorySlot slot;
    slot.itemId = 1;
    slot.count = 1;
    slot.durability = 10;
    slot.maxDurability = 10;

    EXPECT_FALSE(slot.ReduceDurability(3));
    EXPECT_EQ(slot.durability, 7);

    EXPECT_TRUE(slot.ReduceDurability(10));  // Breaks.
    EXPECT_EQ(slot.durability, 0);
    EXPECT_TRUE(slot.IsBroken());
}

TEST(InventorySlotTest, ReduceDurabilityIndestructible) {
    InventorySlot slot;
    slot.itemId = 1;
    slot.count = 1;
    slot.durability = kIndestructible;

    EXPECT_FALSE(slot.ReduceDurability(100));
    EXPECT_EQ(slot.durability, kIndestructible);
}

TEST(InventorySlotTest, ReduceDurabilityAlreadyBroken) {
    InventorySlot slot;
    slot.itemId = 1;
    slot.count = 1;
    slot.durability = 0;
    slot.maxDurability = 10;

    EXPECT_FALSE(slot.ReduceDurability(5));
    EXPECT_EQ(slot.durability, 0);
}

TEST(InventorySlotTest, EnchantBonuses) {
    InventorySlot slot;
    slot.itemId = 1;
    slot.count = 1;

    Enchant enc1;
    enc1.enchantId = 1;
    enc1.bonuses.armor = 10;
    enc1.bonuses.attributes[0] = 5;

    Enchant enc2;
    enc2.enchantId = 2;
    enc2.bonuses.armor = 15;
    enc2.bonuses.minDamage = 3;

    slot.enchants = {enc1, enc2};

    auto bonuses = slot.GetEnchantBonuses();
    EXPECT_EQ(bonuses.armor, 25);
    EXPECT_EQ(bonuses.attributes[0], 5);
    EXPECT_EQ(bonuses.minDamage, 3);
}

TEST(InventorySlotTest, RemoveExpiredEnchants) {
    InventorySlot slot;
    slot.itemId = 1;
    slot.count = 1;

    Enchant permanent;
    permanent.enchantId = 1;
    permanent.durationRemaining = std::nullopt;  // Permanent.

    Enchant expired;
    expired.enchantId = 2;
    expired.durationRemaining = 0.0f;  // Expired.

    Enchant active;
    active.enchantId = 3;
    active.durationRemaining = 10.0f;  // Still active.

    slot.enchants = {permanent, expired, active};
    slot.RemoveExpiredEnchants();

    EXPECT_EQ(slot.enchants.size(), 2u);
    EXPECT_EQ(slot.enchants[0].enchantId, 1u);
    EXPECT_EQ(slot.enchants[1].enchantId, 3u);
}

// =============================================================================
// StatBonuses tests
// =============================================================================

TEST(StatBonusesTest, AdditionOperator) {
    StatBonuses a;
    a.armor = 10;
    a.attributes[0] = 5;
    a.minDamage = 3;

    StatBonuses b;
    b.armor = 20;
    b.attributes[0] = 10;
    b.maxDamage = 7;

    auto result = a + b;
    EXPECT_EQ(result.armor, 30);
    EXPECT_EQ(result.attributes[0], 15);
    EXPECT_EQ(result.minDamage, 3);
    EXPECT_EQ(result.maxDamage, 7);
}

TEST(StatBonusesTest, AddAssignOperator) {
    StatBonuses total;
    StatBonuses bonus;
    bonus.armor = 10;
    bonus.attackSpeed = 1.5f;

    total += bonus;
    total += bonus;

    EXPECT_EQ(total.armor, 20);
    EXPECT_FLOAT_EQ(total.attackSpeed, 3.0f);
}

// =============================================================================
// ItemTemplate tests
// =============================================================================

TEST(ItemTemplateTest, StackableCheck) {
    ItemTemplate tmpl;
    tmpl.maxStackSize = 1;
    EXPECT_FALSE(tmpl.IsStackable());

    tmpl.maxStackSize = 20;
    EXPECT_TRUE(tmpl.IsStackable());
}

TEST(ItemTemplateTest, EquippableCheck) {
    ItemTemplate tmpl;
    tmpl.equipSlot = EquipSlot::COUNT;
    EXPECT_FALSE(tmpl.IsEquippable());

    tmpl.equipSlot = EquipSlot::MainHand;
    EXPECT_TRUE(tmpl.IsEquippable());
}

// =============================================================================
// Inventory component tests (SRS-GML-006.1, SRS-GML-006.2)
// =============================================================================

class InventoryTest : public ::testing::Test {
protected:
    ItemTemplate makeStackable(uint32_t id, uint32_t maxStack = 20) {
        ItemTemplate tmpl;
        tmpl.id = id;
        tmpl.name = "Stackable " + std::to_string(id);
        tmpl.maxStackSize = maxStack;
        return tmpl;
    }

    ItemTemplate makeNonStackable(uint32_t id, int32_t durability = kIndestructible) {
        ItemTemplate tmpl;
        tmpl.id = id;
        tmpl.name = "Equipment " + std::to_string(id);
        tmpl.maxStackSize = 1;
        tmpl.maxDurability = durability;
        return tmpl;
    }
};

TEST_F(InventoryTest, InitializeCreatesSlots) {
    Inventory inv;
    inv.capacity = 10;
    inv.Initialize();

    EXPECT_EQ(inv.slots.size(), 10u);
    EXPECT_EQ(inv.FreeSlots(), 10u);
}

TEST_F(InventoryTest, AddNonStackableItem) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl = makeNonStackable(100, 50);
    uint32_t added = inv.AddItem(tmpl, 1);

    EXPECT_EQ(added, 1u);
    EXPECT_EQ(inv.FreeSlots(), 4u);
    ASSERT_NE(inv.GetItem(0), nullptr);
    EXPECT_EQ(inv.GetItem(0)->itemId, 100u);
    EXPECT_EQ(inv.GetItem(0)->durability, 50);
}

TEST_F(InventoryTest, AddStackableItemStacks) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl = makeStackable(200, 10);
    inv.AddItem(tmpl, 7);
    inv.AddItem(tmpl, 5);

    EXPECT_EQ(inv.FreeSlots(), 3u);
    // First slot: 7 + 3 = 10 (full stack).
    EXPECT_EQ(inv.slots[0].count, 10u);
    // Second slot: remaining 2.
    EXPECT_EQ(inv.slots[1].count, 2u);
}

TEST_F(InventoryTest, AddItemOverflowsToNewSlots) {
    Inventory inv;
    inv.capacity = 3;
    inv.Initialize();

    auto tmpl = makeStackable(300, 5);
    uint32_t added = inv.AddItem(tmpl, 12);

    EXPECT_EQ(added, 12u);
    EXPECT_EQ(inv.FreeSlots(), 0u);
    EXPECT_EQ(inv.slots[0].count, 5u);
    EXPECT_EQ(inv.slots[1].count, 5u);
    EXPECT_EQ(inv.slots[2].count, 2u);
}

TEST_F(InventoryTest, AddItemFullInventory) {
    Inventory inv;
    inv.capacity = 1;
    inv.Initialize();

    auto tmpl = makeStackable(400, 5);
    inv.AddItem(tmpl, 5);  // Fill the only slot.

    uint32_t added = inv.AddItem(tmpl, 3);
    EXPECT_EQ(added, 0u);
}

TEST_F(InventoryTest, RemoveItem) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl = makeStackable(100, 20);
    inv.AddItem(tmpl, 10);

    EXPECT_TRUE(inv.RemoveItem(0, 3));
    EXPECT_EQ(inv.slots[0].count, 7u);

    EXPECT_TRUE(inv.RemoveItem(0, 7));
    EXPECT_TRUE(inv.slots[0].IsEmpty());
    EXPECT_EQ(inv.FreeSlots(), 5u);
}

TEST_F(InventoryTest, RemoveItemNotEnough) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl = makeStackable(100, 20);
    inv.AddItem(tmpl, 3);

    EXPECT_FALSE(inv.RemoveItem(0, 5));  // Only have 3.
    EXPECT_EQ(inv.slots[0].count, 3u);   // Unchanged.
}

TEST_F(InventoryTest, RemoveItemInvalidSlot) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    EXPECT_FALSE(inv.RemoveItem(99, 1));
}

TEST_F(InventoryTest, MoveItemSwap) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl1 = makeNonStackable(100);
    auto tmpl2 = makeNonStackable(200);
    inv.AddItem(tmpl1, 1);
    inv.AddItem(tmpl2, 1);

    EXPECT_TRUE(inv.MoveItem(0, 1));
    EXPECT_EQ(inv.slots[0].itemId, 200u);
    EXPECT_EQ(inv.slots[1].itemId, 100u);
}

TEST_F(InventoryTest, MoveItemToEmptySlot) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl = makeNonStackable(100);
    inv.AddItem(tmpl, 1);

    EXPECT_TRUE(inv.MoveItem(0, 3));
    EXPECT_TRUE(inv.slots[0].IsEmpty());
    EXPECT_EQ(inv.slots[3].itemId, 100u);
}

TEST_F(InventoryTest, MoveItemFromEmptyFails) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    EXPECT_FALSE(inv.MoveItem(0, 1));
}

TEST_F(InventoryTest, MoveItemSameSlotFails) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl = makeNonStackable(100);
    inv.AddItem(tmpl, 1);

    EXPECT_FALSE(inv.MoveItem(0, 0));
}

TEST_F(InventoryTest, SplitStack) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl = makeStackable(100, 20);
    inv.AddItem(tmpl, 10);

    auto newSlot = inv.SplitStack(0, 4);
    ASSERT_TRUE(newSlot.has_value());
    EXPECT_EQ(inv.slots[0].count, 6u);
    EXPECT_EQ(inv.slots[*newSlot].count, 4u);
    EXPECT_EQ(inv.slots[*newSlot].itemId, 100u);
}

TEST_F(InventoryTest, SplitStackNoFreeSlot) {
    Inventory inv;
    inv.capacity = 1;
    inv.Initialize();

    auto tmpl = makeStackable(100, 20);
    inv.AddItem(tmpl, 10);

    auto newSlot = inv.SplitStack(0, 4);
    EXPECT_FALSE(newSlot.has_value());
}

TEST_F(InventoryTest, SplitStackEntireStackFails) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl = makeStackable(100, 20);
    inv.AddItem(tmpl, 5);

    // Can't split all 5 (would leave source empty).
    auto newSlot = inv.SplitStack(0, 5);
    EXPECT_FALSE(newSlot.has_value());
}

TEST_F(InventoryTest, SplitStackZeroCountFails) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl = makeStackable(100, 20);
    inv.AddItem(tmpl, 5);

    EXPECT_FALSE(inv.SplitStack(0, 0).has_value());
}

TEST_F(InventoryTest, FindItem) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl1 = makeNonStackable(100);
    auto tmpl2 = makeNonStackable(200);
    inv.AddItem(tmpl1, 1);
    inv.AddItem(tmpl2, 1);

    auto found = inv.FindItem(200);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, 1u);

    EXPECT_FALSE(inv.FindItem(999).has_value());
}

TEST_F(InventoryTest, CountItem) {
    Inventory inv;
    inv.capacity = 5;
    inv.Initialize();

    auto tmpl = makeStackable(100, 10);
    inv.AddItem(tmpl, 15);  // Fills two slots: 10 + 5.

    EXPECT_EQ(inv.CountItem(100), 15u);
    EXPECT_EQ(inv.CountItem(999), 0u);
}

TEST_F(InventoryTest, AutoInitializesOnAdd) {
    Inventory inv;
    inv.capacity = 5;
    // Don't call Initialize().

    auto tmpl = makeNonStackable(100);
    uint32_t added = inv.AddItem(tmpl, 1);

    EXPECT_EQ(added, 1u);
    EXPECT_EQ(inv.slots.size(), 5u);
}

// =============================================================================
// Equipment component tests (SRS-GML-006.3)
// =============================================================================

TEST(EquipmentTest, EquipItem) {
    Equipment equip;

    InventorySlot sword;
    sword.itemId = 1001;
    sword.count = 1;
    sword.durability = 100;
    sword.maxDurability = 100;

    auto previous = equip.Equip(EquipSlot::MainHand, sword);
    EXPECT_TRUE(previous.IsEmpty());

    auto* equipped = equip.GetEquipped(EquipSlot::MainHand);
    ASSERT_NE(equipped, nullptr);
    EXPECT_EQ(equipped->itemId, 1001u);
}

TEST(EquipmentTest, EquipReturnsExisting) {
    Equipment equip;

    InventorySlot sword1;
    sword1.itemId = 1001;
    sword1.count = 1;

    InventorySlot sword2;
    sword2.itemId = 1002;
    sword2.count = 1;

    equip.Equip(EquipSlot::MainHand, sword1);
    auto previous = equip.Equip(EquipSlot::MainHand, sword2);

    EXPECT_EQ(previous.itemId, 1001u);
    EXPECT_EQ(equip.GetEquipped(EquipSlot::MainHand)->itemId, 1002u);
}

TEST(EquipmentTest, UnequipItem) {
    Equipment equip;

    InventorySlot helm;
    helm.itemId = 2001;
    helm.count = 1;

    equip.Equip(EquipSlot::Head, helm);
    auto removed = equip.Unequip(EquipSlot::Head);

    EXPECT_EQ(removed.itemId, 2001u);
    EXPECT_EQ(equip.GetEquipped(EquipSlot::Head), nullptr);
}

TEST(EquipmentTest, UnequipEmptySlot) {
    Equipment equip;
    auto removed = equip.Unequip(EquipSlot::Head);
    EXPECT_TRUE(removed.IsEmpty());
}

TEST(EquipmentTest, GetEquippedEmptySlot) {
    Equipment equip;
    EXPECT_EQ(equip.GetEquipped(EquipSlot::Chest), nullptr);
}

TEST(EquipmentTest, CalculateStatBonuses) {
    Equipment equip;
    std::vector<ItemTemplate> templates;

    // Helmet: +5 armor, +2 attribute[0].
    ItemTemplate helmet;
    helmet.id = 2001;
    helmet.statBonuses.armor = 5;
    helmet.statBonuses.attributes[0] = 2;
    templates.push_back(helmet);

    // Chest: +15 armor, +5 attribute[0].
    ItemTemplate chestplate;
    chestplate.id = 2002;
    chestplate.statBonuses.armor = 15;
    chestplate.statBonuses.attributes[0] = 5;
    templates.push_back(chestplate);

    InventorySlot helmSlot;
    helmSlot.itemId = 2001;
    helmSlot.count = 1;
    equip.Equip(EquipSlot::Head, helmSlot);

    InventorySlot chestSlot;
    chestSlot.itemId = 2002;
    chestSlot.count = 1;
    equip.Equip(EquipSlot::Chest, chestSlot);

    auto bonuses = equip.CalculateStatBonuses(templates);
    EXPECT_EQ(bonuses.armor, 20);
    EXPECT_EQ(bonuses.attributes[0], 7);
}

TEST(EquipmentTest, BrokenItemGivesNoBonuses) {
    Equipment equip;
    std::vector<ItemTemplate> templates;

    ItemTemplate helmet;
    helmet.id = 2001;
    helmet.statBonuses.armor = 10;
    templates.push_back(helmet);

    InventorySlot helmSlot;
    helmSlot.itemId = 2001;
    helmSlot.count = 1;
    helmSlot.durability = 0;      // Broken!
    helmSlot.maxDurability = 50;
    equip.Equip(EquipSlot::Head, helmSlot);

    auto bonuses = equip.CalculateStatBonuses(templates);
    EXPECT_EQ(bonuses.armor, 0);  // Broken item gives no bonuses.
}

TEST(EquipmentTest, StatBonusesIncludeEnchants) {
    Equipment equip;
    std::vector<ItemTemplate> templates;

    ItemTemplate sword;
    sword.id = 3001;
    sword.statBonuses.minDamage = 10;
    sword.statBonuses.maxDamage = 20;
    templates.push_back(sword);

    InventorySlot swordSlot;
    swordSlot.itemId = 3001;
    swordSlot.count = 1;

    Enchant sharpening;
    sharpening.enchantId = 1;
    sharpening.bonuses.minDamage = 3;
    sharpening.bonuses.maxDamage = 5;
    swordSlot.enchants = {sharpening};

    equip.Equip(EquipSlot::MainHand, swordSlot);

    auto bonuses = equip.CalculateStatBonuses(templates);
    EXPECT_EQ(bonuses.minDamage, 13);
    EXPECT_EQ(bonuses.maxDamage, 25);
}

TEST(EquipmentTest, EmptySlotsGiveZeroBonuses) {
    Equipment equip;
    std::vector<ItemTemplate> templates;

    auto bonuses = equip.CalculateStatBonuses(templates);
    EXPECT_EQ(bonuses.armor, 0);
    EXPECT_FLOAT_EQ(bonuses.attackSpeed, 0.0f);
}

// =============================================================================
// InventorySystem tests (SRS-GML-006.4)
// =============================================================================

class InventorySystemTest : public ::testing::Test {
protected:
    ComponentStorage<Inventory> inventories;
    ComponentStorage<Equipment> equipment;
    ComponentStorage<DurabilityEvent> durabilityEvents;

    Entity player{0, 0};

    void SetUp() override {
        inventories.Add(player);
        inventories.Get(player).Initialize();
        equipment.Add(player);
    }
};

TEST_F(InventorySystemTest, ProcessDurabilityEvent) {
    auto& equip = equipment.Get(player);

    InventorySlot sword;
    sword.itemId = 1001;
    sword.count = 1;
    sword.durability = 100;
    sword.maxDurability = 100;
    equip.Equip(EquipSlot::MainHand, sword);

    // Create a durability event.
    Entity eventEntity(10, 0);
    DurabilityEvent event;
    event.player = player;
    event.slot = EquipSlot::MainHand;
    event.amount = 5;
    durabilityEvents.Add(eventEntity, std::move(event));

    InventorySystem system(inventories, equipment, durabilityEvents);
    system.Execute(0.016f);

    EXPECT_EQ(equip.GetEquipped(EquipSlot::MainHand)->durability, 95);
    EXPECT_TRUE(durabilityEvents.Get(eventEntity).processed);
}

TEST_F(InventorySystemTest, DurabilityEventBreaksItem) {
    auto& equip = equipment.Get(player);

    InventorySlot boots;
    boots.itemId = 2001;
    boots.count = 1;
    boots.durability = 3;
    boots.maxDurability = 100;
    equip.Equip(EquipSlot::Feet, boots);

    Entity eventEntity(10, 0);
    DurabilityEvent event;
    event.player = player;
    event.slot = EquipSlot::Feet;
    event.amount = 10;
    durabilityEvents.Add(eventEntity, std::move(event));

    InventorySystem system(inventories, equipment, durabilityEvents);
    system.Execute(0.016f);

    EXPECT_EQ(equip.GetEquipped(EquipSlot::Feet)->durability, 0);
    EXPECT_TRUE(equip.GetEquipped(EquipSlot::Feet)->IsBroken());
}

TEST_F(InventorySystemTest, DurabilityEventForMissingPlayerIgnored) {
    Entity unknownPlayer(99, 0);

    Entity eventEntity(10, 0);
    DurabilityEvent event;
    event.player = unknownPlayer;
    event.slot = EquipSlot::MainHand;
    event.amount = 5;
    durabilityEvents.Add(eventEntity, std::move(event));

    InventorySystem system(inventories, equipment, durabilityEvents);
    system.Execute(0.016f);

    EXPECT_TRUE(durabilityEvents.Get(eventEntity).processed);
}

TEST_F(InventorySystemTest, AlreadyProcessedEventSkipped) {
    auto& equip = equipment.Get(player);

    InventorySlot sword;
    sword.itemId = 1001;
    sword.count = 1;
    sword.durability = 100;
    sword.maxDurability = 100;
    equip.Equip(EquipSlot::MainHand, sword);

    Entity eventEntity(10, 0);
    DurabilityEvent event;
    event.player = player;
    event.slot = EquipSlot::MainHand;
    event.amount = 50;
    event.processed = true;  // Already processed.
    durabilityEvents.Add(eventEntity, std::move(event));

    InventorySystem system(inventories, equipment, durabilityEvents);
    system.Execute(0.016f);

    EXPECT_EQ(equip.GetEquipped(EquipSlot::MainHand)->durability, 100);
}

TEST_F(InventorySystemTest, EnchantTimerExpiration) {
    auto& equip = equipment.Get(player);

    InventorySlot sword;
    sword.itemId = 1001;
    sword.count = 1;

    Enchant permanent;
    permanent.enchantId = 1;
    permanent.durationRemaining = std::nullopt;

    Enchant timed;
    timed.enchantId = 2;
    timed.durationRemaining = 3.0f;

    sword.enchants = {permanent, timed};
    equip.Equip(EquipSlot::MainHand, sword);

    InventorySystem system(inventories, equipment, durabilityEvents);

    // Tick 2 seconds — timed enchant still active.
    system.Execute(2.0f);
    auto* equipped = equip.GetEquipped(EquipSlot::MainHand);
    EXPECT_EQ(equipped->enchants.size(), 2u);

    // Tick 2 more seconds — timed enchant expires.
    system.Execute(2.0f);
    equipped = equip.GetEquipped(EquipSlot::MainHand);
    EXPECT_EQ(equipped->enchants.size(), 1u);
    EXPECT_EQ(equipped->enchants[0].enchantId, 1u);  // Only permanent remains.
}

TEST_F(InventorySystemTest, InventoryEnchantExpiration) {
    auto& inv = inventories.Get(player);

    auto slot = InventorySlot{};
    slot.itemId = 500;
    slot.count = 1;

    Enchant timed;
    timed.enchantId = 10;
    timed.durationRemaining = 1.0f;
    slot.enchants = {timed};

    inv.slots[0] = slot;

    InventorySystem system(inventories, equipment, durabilityEvents);
    system.Execute(2.0f);

    EXPECT_TRUE(inv.slots[0].enchants.empty());
}

// =============================================================================
// InventorySystem template registry
// =============================================================================

TEST(InventorySystemTemplateTest, RegisterAndGetTemplate) {
    ComponentStorage<Inventory> invs;
    ComponentStorage<Equipment> equips;
    ComponentStorage<DurabilityEvent> events;

    InventorySystem system(invs, equips, events);

    ItemTemplate sword;
    sword.id = 1001;
    sword.name = "Iron Sword";
    sword.type = ItemType::Weapon;
    sword.equipSlot = EquipSlot::MainHand;
    system.RegisterTemplate(std::move(sword));

    const auto* found = system.GetTemplate(1001);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "Iron Sword");
}

TEST(InventorySystemTemplateTest, GetNonexistentTemplate) {
    ComponentStorage<Inventory> invs;
    ComponentStorage<Equipment> equips;
    ComponentStorage<DurabilityEvent> events;

    InventorySystem system(invs, equips, events);
    EXPECT_EQ(system.GetTemplate(999), nullptr);
}

TEST(InventorySystemTemplateTest, RegisterOverwritesExisting) {
    ComponentStorage<Inventory> invs;
    ComponentStorage<Equipment> equips;
    ComponentStorage<DurabilityEvent> events;

    InventorySystem system(invs, equips, events);

    ItemTemplate v1;
    v1.id = 1;
    v1.name = "Version 1";
    system.RegisterTemplate(std::move(v1));

    ItemTemplate v2;
    v2.id = 1;
    v2.name = "Version 2";
    system.RegisterTemplate(std::move(v2));

    EXPECT_EQ(system.GetTemplate(1)->name, "Version 2");
}

// =============================================================================
// InventorySystem metadata
// =============================================================================

TEST(InventorySystemMetaTest, SystemStageIsPostUpdate) {
    ComponentStorage<Inventory> invs;
    ComponentStorage<Equipment> equips;
    ComponentStorage<DurabilityEvent> events;

    InventorySystem system(invs, equips, events);
    EXPECT_EQ(system.GetStage(), SystemStage::PostUpdate);
}

TEST(InventorySystemMetaTest, SystemName) {
    ComponentStorage<Inventory> invs;
    ComponentStorage<Equipment> equips;
    ComponentStorage<DurabilityEvent> events;

    InventorySystem system(invs, equips, events);
    EXPECT_EQ(system.GetName(), "InventorySystem");
}

TEST(InventorySystemMetaTest, AccessInfoDeclaresWriteAccess) {
    ComponentStorage<Inventory> invs;
    ComponentStorage<Equipment> equips;
    ComponentStorage<DurabilityEvent> events;

    InventorySystem system(invs, equips, events);
    auto info = system.GetAccessInfo();

    EXPECT_TRUE(info.writes.count(ComponentType<Inventory>::Id()));
    EXPECT_TRUE(info.writes.count(ComponentType<Equipment>::Id()));
    EXPECT_TRUE(info.writes.count(ComponentType<DurabilityEvent>::Id()));
}

// =============================================================================
// Integration: full inventory scenario
// =============================================================================

TEST(InventoryIntegration, FullInventoryLifecycle) {
    ComponentStorage<Inventory> inventories;
    ComponentStorage<Equipment> equipment;
    ComponentStorage<DurabilityEvent> durabilityEvents;

    EntityManager entityManager;
    entityManager.RegisterStorage(&inventories);
    entityManager.RegisterStorage(&equipment);
    entityManager.RegisterStorage(&durabilityEvents);

    // Create a player.
    Entity player = entityManager.Create();
    auto& inv = inventories.Add(player);
    inv.capacity = 10;
    inv.Initialize();
    equipment.Add(player);

    // Define item templates.
    ItemTemplate healthPotion;
    healthPotion.id = 100;
    healthPotion.name = "Health Potion";
    healthPotion.type = ItemType::Consumable;
    healthPotion.maxStackSize = 20;

    ItemTemplate ironSword;
    ironSword.id = 200;
    ironSword.name = "Iron Sword";
    ironSword.type = ItemType::Weapon;
    ironSword.equipSlot = EquipSlot::MainHand;
    ironSword.maxDurability = 100;
    ironSword.statBonuses.minDamage = 10;
    ironSword.statBonuses.maxDamage = 18;

    ItemTemplate steelHelmet;
    steelHelmet.id = 300;
    steelHelmet.name = "Steel Helmet";
    steelHelmet.type = ItemType::Armor;
    steelHelmet.equipSlot = EquipSlot::Head;
    steelHelmet.maxDurability = 80;
    steelHelmet.statBonuses.armor = 15;
    steelHelmet.statBonuses.attributes[0] = 3;

    InventorySystem system(inventories, equipment, durabilityEvents);
    system.RegisterTemplate(healthPotion);
    system.RegisterTemplate(ironSword);
    system.RegisterTemplate(steelHelmet);

    // Phase 1: Loot items.
    EXPECT_EQ(inv.AddItem(healthPotion, 5), 5u);
    EXPECT_EQ(inv.AddItem(ironSword, 1), 1u);
    EXPECT_EQ(inv.AddItem(steelHelmet, 1), 1u);
    EXPECT_EQ(inv.FreeSlots(), 7u);

    // Phase 2: Stack more potions.
    EXPECT_EQ(inv.AddItem(healthPotion, 10), 10u);
    EXPECT_EQ(inv.CountItem(100), 15u);
    EXPECT_EQ(inv.FreeSlots(), 7u);  // Stacked onto existing slot.

    // Phase 3: Equip the sword.
    auto swordSlot = inv.FindItem(200);
    ASSERT_TRUE(swordSlot.has_value());
    InventorySlot swordItem = inv.slots[*swordSlot];
    inv.RemoveItem(*swordSlot, 1);

    auto& equip = equipment.Get(player);
    equip.Equip(EquipSlot::MainHand, swordItem);
    EXPECT_NE(equip.GetEquipped(EquipSlot::MainHand), nullptr);

    // Phase 4: Equip the helmet.
    auto helmSlot = inv.FindItem(300);
    ASSERT_TRUE(helmSlot.has_value());
    InventorySlot helmItem = inv.slots[*helmSlot];
    inv.RemoveItem(*helmSlot, 1);

    equip.Equip(EquipSlot::Head, helmItem);

    // Phase 5: Verify stat bonuses.
    auto bonuses = equip.CalculateStatBonuses(system.GetTemplates());
    EXPECT_EQ(bonuses.minDamage, 10);
    EXPECT_EQ(bonuses.maxDamage, 18);
    EXPECT_EQ(bonuses.armor, 15);
    EXPECT_EQ(bonuses.attributes[0], 3);

    // Phase 6: Combat causes durability loss.
    Entity durEvt = entityManager.Create();
    DurabilityEvent de;
    de.player = player;
    de.slot = EquipSlot::MainHand;
    de.amount = 5;
    durabilityEvents.Add(durEvt, std::move(de));

    system.Execute(0.016f);
    EXPECT_EQ(equip.GetEquipped(EquipSlot::MainHand)->durability, 95);

    // Phase 7: Split potions.
    auto potionSlot = inv.FindItem(100);
    ASSERT_TRUE(potionSlot.has_value());
    auto splitResult = inv.SplitStack(*potionSlot, 5);
    ASSERT_TRUE(splitResult.has_value());
    EXPECT_EQ(inv.slots[*potionSlot].count, 10u);
    EXPECT_EQ(inv.slots[*splitResult].count, 5u);

    // Phase 8: Unequip sword back to inventory.
    auto removed = equip.Unequip(EquipSlot::MainHand);
    EXPECT_EQ(removed.itemId, 200u);

    inv.AddItem(ironSword, 1);

    // Stat bonuses should decrease.
    auto bonuses2 = equip.CalculateStatBonuses(system.GetTemplates());
    EXPECT_EQ(bonuses2.minDamage, 0);
    EXPECT_EQ(bonuses2.maxDamage, 0);
    EXPECT_EQ(bonuses2.armor, 15);  // Helmet still equipped.

    entityManager.Destroy(player);
}

TEST(InventoryIntegration, SchedulerRegistration) {
    ComponentStorage<Inventory> invs;
    ComponentStorage<Equipment> equips;
    ComponentStorage<DurabilityEvent> events;

    SystemScheduler scheduler;
    auto& system = scheduler.Register<InventorySystem>(invs, equips, events);

    EXPECT_EQ(system.GetName(), "InventorySystem");
    EXPECT_EQ(scheduler.SystemCount(), 1u);
    EXPECT_TRUE(scheduler.Build());

    // Execute with no entities (should not crash).
    scheduler.Execute(1.0f / 60.0f);
}
