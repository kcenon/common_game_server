#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/components.hpp"
#include "cgs/game/math_types.hpp"
#include "cgs/game/spatial_index.hpp"
#include "cgs/game/world_components.hpp"
#include "cgs/game/world_system.hpp"
#include "cgs/game/world_types.hpp"

using namespace cgs::ecs;
using namespace cgs::game;

// ═══════════════════════════════════════════════════════════════════════════
// World type tests
// ═══════════════════════════════════════════════════════════════════════════

TEST(WorldTypesTest, ZoneFlagsOperations) {
    auto flags = ZoneFlags::NoCombat | ZoneFlags::Sanctuary;
    EXPECT_TRUE(HasFlag(flags, ZoneFlags::NoCombat));
    EXPECT_TRUE(HasFlag(flags, ZoneFlags::Sanctuary));
    EXPECT_FALSE(HasFlag(flags, ZoneFlags::NoMount));
    EXPECT_FALSE(HasFlag(flags, ZoneFlags::None));
}

TEST(WorldTypesTest, ZoneFlagsBitwiseAnd) {
    auto flags = ZoneFlags::NoCombat | ZoneFlags::NoMount | ZoneFlags::Indoor;
    auto masked = flags & ZoneFlags::NoCombat;
    EXPECT_TRUE(HasFlag(masked, ZoneFlags::NoCombat));
    EXPECT_FALSE(HasFlag(masked, ZoneFlags::NoMount));
}

TEST(WorldTypesTest, ZoneFlagsNone) {
    EXPECT_FALSE(HasFlag(ZoneFlags::None, ZoneFlags::NoCombat));
    EXPECT_FALSE(HasFlag(ZoneFlags::None, ZoneFlags::Sanctuary));
}

// ═══════════════════════════════════════════════════════════════════════════
// MapInstance component tests (SRS-GML-003.1)
// ═══════════════════════════════════════════════════════════════════════════

TEST(MapInstanceTest, DefaultValues) {
    MapInstance map;
    EXPECT_EQ(map.mapId, 0u);
    EXPECT_EQ(map.instanceId, 0u);
    EXPECT_EQ(map.type, MapType::OpenWorld);
}

TEST(MapInstanceTest, StorageRoundTrip) {
    ComponentStorage<MapInstance> storage;
    Entity e(0, 0);

    storage.Add(e, MapInstance{100, 1, MapType::Dungeon});
    const auto& map = storage.Get(e);
    EXPECT_EQ(map.mapId, 100u);
    EXPECT_EQ(map.instanceId, 1u);
    EXPECT_EQ(map.type, MapType::Dungeon);
}

TEST(MapInstanceTest, AllMapTypes) {
    MapInstance openWorld;
    openWorld.type = MapType::OpenWorld;
    EXPECT_EQ(openWorld.type, MapType::OpenWorld);

    MapInstance dungeon;
    dungeon.type = MapType::Dungeon;
    EXPECT_EQ(dungeon.type, MapType::Dungeon);

    MapInstance bg;
    bg.type = MapType::Battleground;
    EXPECT_EQ(bg.type, MapType::Battleground);
}

// ═══════════════════════════════════════════════════════════════════════════
// Zone component tests (SRS-GML-003.2)
// ═══════════════════════════════════════════════════════════════════════════

TEST(ZoneTest, DefaultValues) {
    Zone zone;
    EXPECT_EQ(zone.zoneId, 0u);
    EXPECT_EQ(zone.type, ZoneType::Normal);
    EXPECT_EQ(zone.flags, ZoneFlags::None);
    EXPECT_FALSE(zone.mapEntity.isValid());
}

TEST(ZoneTest, PvPZoneWithFlags) {
    Zone zone;
    zone.zoneId = 42;
    zone.type = ZoneType::PvP;
    zone.flags = ZoneFlags::FreeForAll | ZoneFlags::NoMount;
    zone.mapEntity = Entity(0, 0);

    EXPECT_EQ(zone.zoneId, 42u);
    EXPECT_EQ(zone.type, ZoneType::PvP);
    EXPECT_TRUE(HasFlag(zone.flags, ZoneFlags::FreeForAll));
    EXPECT_TRUE(HasFlag(zone.flags, ZoneFlags::NoMount));
    EXPECT_FALSE(HasFlag(zone.flags, ZoneFlags::NoCombat));
}

TEST(ZoneTest, SafeZoneFlags) {
    Zone zone;
    zone.type = ZoneType::Safe;
    zone.flags = ZoneFlags::NoCombat | ZoneFlags::Sanctuary | ZoneFlags::Resting;

    EXPECT_TRUE(HasFlag(zone.flags, ZoneFlags::NoCombat));
    EXPECT_TRUE(HasFlag(zone.flags, ZoneFlags::Sanctuary));
    EXPECT_TRUE(HasFlag(zone.flags, ZoneFlags::Resting));
}

TEST(ZoneTest, StorageRoundTrip) {
    ComponentStorage<Zone> storage;
    Entity e(0, 0);

    storage.Add(e, Zone{10, ZoneType::Contested, ZoneFlags::NoFly, Entity(1, 0)});
    const auto& zone = storage.Get(e);
    EXPECT_EQ(zone.zoneId, 10u);
    EXPECT_EQ(zone.type, ZoneType::Contested);
    EXPECT_TRUE(HasFlag(zone.flags, ZoneFlags::NoFly));
    EXPECT_EQ(zone.mapEntity, Entity(1, 0));
}

// ═══════════════════════════════════════════════════════════════════════════
// MapMembership component tests
// ═══════════════════════════════════════════════════════════════════════════

TEST(MapMembershipTest, DefaultValues) {
    MapMembership membership;
    EXPECT_FALSE(membership.mapEntity.isValid());
    EXPECT_EQ(membership.zoneId, 0u);
}

TEST(MapMembershipTest, StorageRoundTrip) {
    ComponentStorage<MapMembership> storage;
    Entity player(0, 0);

    storage.Add(player, MapMembership{Entity(10, 0), 5});
    const auto& m = storage.Get(player);
    EXPECT_EQ(m.mapEntity, Entity(10, 0));
    EXPECT_EQ(m.zoneId, 5u);
}

// ═══════════════════════════════════════════════════════════════════════════
// VisibilityRange component tests (SRS-GML-003.4)
// ═══════════════════════════════════════════════════════════════════════════

TEST(VisibilityRangeTest, DefaultIsGlobalDefault) {
    VisibilityRange vr;
    EXPECT_FLOAT_EQ(vr.range, kDefaultVisibilityRange);
}

TEST(VisibilityRangeTest, CustomRange) {
    VisibilityRange vr;
    vr.range = 50.0f;
    EXPECT_FLOAT_EQ(vr.range, 50.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
// SpatialIndex tests (SRS-GML-003.3)
// ═══════════════════════════════════════════════════════════════════════════

class SpatialIndexTest : public ::testing::Test {
protected:
    SpatialIndex index{16.0f};  // 16 unit cells.
};

TEST_F(SpatialIndexTest, DefaultConstructor) {
    SpatialIndex defaultIndex;
    EXPECT_FLOAT_EQ(defaultIndex.CellSize(), kDefaultCellSize);
    EXPECT_EQ(defaultIndex.Size(), 0u);
}

TEST_F(SpatialIndexTest, InsertAndContains) {
    Entity e(0, 0);
    index.Insert(e, Vector3(5.0f, 0.0f, 5.0f));

    EXPECT_TRUE(index.Contains(e));
    EXPECT_EQ(index.Size(), 1u);
}

TEST_F(SpatialIndexTest, InsertDuplicateIsUpdate) {
    Entity e(0, 0);
    index.Insert(e, Vector3(5.0f, 0.0f, 5.0f));
    index.Insert(e, Vector3(100.0f, 0.0f, 100.0f));

    EXPECT_TRUE(index.Contains(e));
    EXPECT_EQ(index.Size(), 1u);

    // Should be in the new cell now.
    auto cell = index.WorldToCell(Vector3(100.0f, 0.0f, 100.0f));
    auto entities = index.QueryCell(cell.x, cell.y);
    EXPECT_EQ(entities.size(), 1u);
}

TEST_F(SpatialIndexTest, RemoveEntity) {
    Entity e(0, 0);
    index.Insert(e, Vector3(5.0f, 0.0f, 5.0f));
    index.Remove(e);

    EXPECT_FALSE(index.Contains(e));
    EXPECT_EQ(index.Size(), 0u);
}

TEST_F(SpatialIndexTest, RemoveNonexistentIsNoop) {
    Entity e(0, 0);
    index.Remove(e);  // Should not crash.
    EXPECT_EQ(index.Size(), 0u);
}

TEST_F(SpatialIndexTest, UpdateChangesCell) {
    Entity e(0, 0);
    index.Insert(e, Vector3(1.0f, 0.0f, 1.0f));

    auto oldCell = index.WorldToCell(Vector3(1.0f, 0.0f, 1.0f));
    auto oldEntities = index.QueryCell(oldCell.x, oldCell.y);
    EXPECT_EQ(oldEntities.size(), 1u);

    // Move to a far-away position (different cell).
    index.Update(e, Vector3(100.0f, 0.0f, 100.0f));

    // Old cell should be empty.
    oldEntities = index.QueryCell(oldCell.x, oldCell.y);
    EXPECT_TRUE(oldEntities.empty());

    // New cell should have the entity.
    auto newCell = index.WorldToCell(Vector3(100.0f, 0.0f, 100.0f));
    auto newEntities = index.QueryCell(newCell.x, newCell.y);
    EXPECT_EQ(newEntities.size(), 1u);
}

TEST_F(SpatialIndexTest, UpdateSameCellIsNoop) {
    Entity e(0, 0);
    index.Insert(e, Vector3(1.0f, 0.0f, 1.0f));
    index.Update(e, Vector3(2.0f, 0.0f, 2.0f));  // Still in cell (0,0).

    auto cell = index.WorldToCell(Vector3(2.0f, 0.0f, 2.0f));
    auto entities = index.QueryCell(cell.x, cell.y);
    EXPECT_EQ(entities.size(), 1u);
    EXPECT_EQ(index.Size(), 1u);
}

TEST_F(SpatialIndexTest, QueryCellReturnsCorrectEntities) {
    Entity e1(0, 0);
    Entity e2(1, 0);
    Entity e3(2, 0);

    index.Insert(e1, Vector3(1.0f, 0.0f, 1.0f));   // Cell (0, 0)
    index.Insert(e2, Vector3(2.0f, 0.0f, 2.0f));   // Cell (0, 0) — same
    index.Insert(e3, Vector3(50.0f, 0.0f, 50.0f));  // Cell (3, 3)

    auto cell00 = index.QueryCell(0, 0);
    EXPECT_EQ(cell00.size(), 2u);

    auto cell33 = index.WorldToCell(Vector3(50.0f, 0.0f, 50.0f));
    auto farEntities = index.QueryCell(cell33.x, cell33.y);
    EXPECT_EQ(farEntities.size(), 1u);
}

TEST_F(SpatialIndexTest, QueryPositionMatchesQueryCell) {
    Entity e(0, 0);
    Vector3 pos(10.0f, 0.0f, 10.0f);
    index.Insert(e, pos);

    auto byPos = index.QueryPosition(pos);
    auto cell = index.WorldToCell(pos);
    auto byCell = index.QueryCell(cell.x, cell.y);

    EXPECT_EQ(byPos.size(), byCell.size());
}

TEST_F(SpatialIndexTest, QueryRadiusFindsNearbyEntities) {
    Entity e1(0, 0);
    Entity e2(1, 0);
    Entity e3(2, 0);

    index.Insert(e1, Vector3(0.0f, 0.0f, 0.0f));
    index.Insert(e2, Vector3(5.0f, 0.0f, 5.0f));
    index.Insert(e3, Vector3(100.0f, 0.0f, 100.0f));

    auto nearby = index.QueryRadius(Vector3(0.0f, 0.0f, 0.0f), 20.0f);
    // e1 and e2 should be in nearby cells; e3 should not.
    EXPECT_GE(nearby.size(), 2u);

    // e3 should not be in the results.
    auto it = std::find(nearby.begin(), nearby.end(), e3);
    EXPECT_EQ(it, nearby.end());
}

TEST_F(SpatialIndexTest, QueryRadiusZeroReturnsEmpty) {
    Entity e(0, 0);
    index.Insert(e, Vector3(0.0f, 0.0f, 0.0f));

    auto result = index.QueryRadius(Vector3(0.0f, 0.0f, 0.0f), 0.0f);
    EXPECT_TRUE(result.empty());
}

TEST_F(SpatialIndexTest, QueryRadiusNegativeReturnsEmpty) {
    Entity e(0, 0);
    index.Insert(e, Vector3(0.0f, 0.0f, 0.0f));

    auto result = index.QueryRadius(Vector3(0.0f, 0.0f, 0.0f), -10.0f);
    EXPECT_TRUE(result.empty());
}

TEST_F(SpatialIndexTest, QueryEmptyIndex) {
    auto result = index.QueryRadius(Vector3(0.0f, 0.0f, 0.0f), 100.0f);
    EXPECT_TRUE(result.empty());

    auto cellResult = index.QueryCell(0, 0);
    EXPECT_TRUE(cellResult.empty());
}

TEST_F(SpatialIndexTest, NegativeCoordinates) {
    Entity e(0, 0);
    index.Insert(e, Vector3(-50.0f, 0.0f, -50.0f));
    EXPECT_TRUE(index.Contains(e));

    auto cell = index.WorldToCell(Vector3(-50.0f, 0.0f, -50.0f));
    auto entities = index.QueryCell(cell.x, cell.y);
    EXPECT_EQ(entities.size(), 1u);
}

TEST_F(SpatialIndexTest, ClearRemovesAllEntities) {
    for (uint32_t i = 0; i < 100; ++i) {
        Entity e(i, 0);
        auto x = static_cast<float>(i) * 5.0f;
        index.Insert(e, Vector3(x, 0.0f, x));
    }
    EXPECT_EQ(index.Size(), 100u);

    index.Clear();
    EXPECT_EQ(index.Size(), 0u);

    auto result = index.QueryRadius(Vector3(0.0f, 0.0f, 0.0f), 10000.0f);
    EXPECT_TRUE(result.empty());
}

TEST_F(SpatialIndexTest, WorldToCellMapping) {
    // Cell size is 16.0f
    auto c1 = index.WorldToCell(Vector3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(c1.x, 0);
    EXPECT_EQ(c1.y, 0);

    auto c2 = index.WorldToCell(Vector3(15.9f, 0.0f, 15.9f));
    EXPECT_EQ(c2.x, 0);
    EXPECT_EQ(c2.y, 0);

    auto c3 = index.WorldToCell(Vector3(16.0f, 0.0f, 16.0f));
    EXPECT_EQ(c3.x, 1);
    EXPECT_EQ(c3.y, 1);

    auto c4 = index.WorldToCell(Vector3(-1.0f, 0.0f, -1.0f));
    EXPECT_EQ(c4.x, -1);
    EXPECT_EQ(c4.y, -1);
}

TEST_F(SpatialIndexTest, InvalidCellSizeFallsBackToDefault) {
    SpatialIndex badIndex(0.0f);
    EXPECT_FLOAT_EQ(badIndex.CellSize(), kDefaultCellSize);

    SpatialIndex negIndex(-5.0f);
    EXPECT_FLOAT_EQ(negIndex.CellSize(), kDefaultCellSize);
}

// ═══════════════════════════════════════════════════════════════════════════
// WorldSystem tests (SRS-GML-003.3, SRS-GML-003.4, SRS-GML-003.5)
// ═══════════════════════════════════════════════════════════════════════════

class WorldSystemTest : public ::testing::Test {
protected:
    ComponentStorage<Transform> transforms;
    ComponentStorage<MapMembership> memberships;
    ComponentStorage<MapInstance> mapInstances;
    ComponentStorage<VisibilityRange> visibilityRanges;
    ComponentStorage<Zone> zones;

    Entity mapEntity{0, 0};

    void SetUp() override {
        // Create a map instance.
        mapInstances.Add(mapEntity, MapInstance{1, 1, MapType::OpenWorld});
    }

    /// Helper: create a player entity at a position on the map.
    Entity createEntityOnMap(uint32_t id, const Vector3& pos) {
        Entity e(id, 0);
        transforms.Add(e, Transform{pos, {}, {1.0f, 1.0f, 1.0f}});
        memberships.Add(e, MapMembership{mapEntity, 0});
        return e;
    }
};

TEST_F(WorldSystemTest, SynchronizesEntityPositions) {
    Entity player = createEntityOnMap(1, Vector3(10.0f, 0.0f, 20.0f));

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    // After execution, the spatial index should contain the player.
    const auto* spatial = system.GetSpatialIndex(mapEntity);
    ASSERT_NE(spatial, nullptr);
    EXPECT_TRUE(spatial->Contains(player));
}

TEST_F(WorldSystemTest, MultipleEntitiesOnSameMap) {
    Entity e1 = createEntityOnMap(1, Vector3(0.0f, 0.0f, 0.0f));
    Entity e2 = createEntityOnMap(2, Vector3(10.0f, 0.0f, 10.0f));
    Entity e3 = createEntityOnMap(3, Vector3(500.0f, 0.0f, 500.0f));

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    const auto* spatial = system.GetSpatialIndex(mapEntity);
    ASSERT_NE(spatial, nullptr);
    EXPECT_TRUE(spatial->Contains(e1));
    EXPECT_TRUE(spatial->Contains(e2));
    EXPECT_TRUE(spatial->Contains(e3));
    EXPECT_EQ(spatial->Size(), 3u);
}

TEST_F(WorldSystemTest, DifferentMapsGetSeparateIndices) {
    Entity map2Entity(10, 0);
    mapInstances.Add(map2Entity, MapInstance{2, 1, MapType::Dungeon});

    Entity e1 = createEntityOnMap(1, Vector3(0.0f, 0.0f, 0.0f));

    Entity e2(2, 0);
    transforms.Add(e2, Transform{{50.0f, 0.0f, 50.0f}, {}, {1.0f, 1.0f, 1.0f}});
    memberships.Add(e2, MapMembership{map2Entity, 0});

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    EXPECT_EQ(system.MapCount(), 2u);

    const auto* spatial1 = system.GetSpatialIndex(mapEntity);
    ASSERT_NE(spatial1, nullptr);
    EXPECT_EQ(spatial1->Size(), 1u);
    EXPECT_TRUE(spatial1->Contains(e1));
    EXPECT_FALSE(spatial1->Contains(e2));

    const auto* spatial2 = system.GetSpatialIndex(map2Entity);
    ASSERT_NE(spatial2, nullptr);
    EXPECT_EQ(spatial2->Size(), 1u);
    EXPECT_TRUE(spatial2->Contains(e2));
}

TEST_F(WorldSystemTest, PositionUpdatesReflectInIndex) {
    Entity player = createEntityOnMap(1, Vector3(0.0f, 0.0f, 0.0f));

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    // Move the player.
    transforms.Get(player).position = Vector3(200.0f, 0.0f, 200.0f);
    system.Execute(0.016f);

    // Spatial index should reflect the new position.
    const auto* spatial = system.GetSpatialIndex(mapEntity);
    ASSERT_NE(spatial, nullptr);
    auto oldCell = spatial->WorldToCell(Vector3(0.0f, 0.0f, 0.0f));
    auto newCell = spatial->WorldToCell(Vector3(200.0f, 0.0f, 200.0f));

    auto oldEntities = spatial->QueryCell(oldCell.x, oldCell.y);
    auto found = std::find(oldEntities.begin(), oldEntities.end(), player);
    EXPECT_EQ(found, oldEntities.end());  // Not in old cell.

    auto newEntities = spatial->QueryCell(newCell.x, newCell.y);
    found = std::find(newEntities.begin(), newEntities.end(), player);
    EXPECT_NE(found, newEntities.end());  // In new cell.
}

// ── Interest management tests (SRS-GML-003.4) ───────────────────────────

TEST_F(WorldSystemTest, GetVisibleEntitiesDefaultRange) {
    Entity viewer = createEntityOnMap(1, Vector3(0.0f, 0.0f, 0.0f));
    Entity nearby = createEntityOnMap(2, Vector3(10.0f, 0.0f, 10.0f));
    Entity farAway = createEntityOnMap(3, Vector3(500.0f, 0.0f, 500.0f));

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    auto visible = system.GetVisibleEntities(viewer);

    // viewer and nearby should be visible (within default 100 range).
    auto hasViewer = std::find(visible.begin(), visible.end(), viewer);
    auto hasNearby = std::find(visible.begin(), visible.end(), nearby);
    EXPECT_NE(hasViewer, visible.end());
    EXPECT_NE(hasNearby, visible.end());

    // farAway should NOT be visible.
    auto hasFar = std::find(visible.begin(), visible.end(), farAway);
    EXPECT_EQ(hasFar, visible.end());
}

TEST_F(WorldSystemTest, GetVisibleEntitiesCustomRange) {
    Entity viewer = createEntityOnMap(1, Vector3(0.0f, 0.0f, 0.0f));
    visibilityRanges.Add(viewer, VisibilityRange{20.0f});

    Entity nearby = createEntityOnMap(2, Vector3(10.0f, 0.0f, 10.0f));
    Entity justOutside = createEntityOnMap(3, Vector3(30.0f, 0.0f, 30.0f));

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    auto visible = system.GetVisibleEntities(viewer);

    auto hasViewer = std::find(visible.begin(), visible.end(), viewer);
    auto hasNearby = std::find(visible.begin(), visible.end(), nearby);
    EXPECT_NE(hasViewer, visible.end());
    EXPECT_NE(hasNearby, visible.end());

    // justOutside is ~42.4 units away (sqrt(30^2 + 30^2)), outside 20 range.
    auto hasOutside = std::find(visible.begin(), visible.end(), justOutside);
    EXPECT_EQ(hasOutside, visible.end());
}

TEST_F(WorldSystemTest, GetVisibleEntitiesNoMembership) {
    Entity orphan(99, 0);
    transforms.Add(orphan, Transform{{0.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    // No MapMembership for orphan.

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    auto visible = system.GetVisibleEntities(orphan);
    EXPECT_TRUE(visible.empty());
}

TEST_F(WorldSystemTest, GetVisibleEntitiesCrossMapIsolation) {
    Entity map2Entity(10, 0);
    mapInstances.Add(map2Entity, MapInstance{2, 1, MapType::Dungeon});

    Entity viewer = createEntityOnMap(1, Vector3(0.0f, 0.0f, 0.0f));

    Entity otherMapEntity(2, 0);
    transforms.Add(otherMapEntity,
                   Transform{{5.0f, 0.0f, 5.0f}, {}, {1.0f, 1.0f, 1.0f}});
    memberships.Add(otherMapEntity, MapMembership{map2Entity, 0});

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    auto visible = system.GetVisibleEntities(viewer);
    // otherMapEntity should NOT be visible (different map).
    auto hasOther = std::find(visible.begin(), visible.end(), otherMapEntity);
    EXPECT_EQ(hasOther, visible.end());
}

// ── Map transition tests (SRS-GML-003.5) ────────────────────────────────

TEST_F(WorldSystemTest, TransferEntitySuccess) {
    Entity map2Entity(10, 0);
    mapInstances.Add(map2Entity, MapInstance{2, 1, MapType::Dungeon});

    Entity player = createEntityOnMap(1, Vector3(0.0f, 0.0f, 0.0f));

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    // Transfer player to map2.
    Vector3 destination(50.0f, 0.0f, 50.0f);
    auto result = system.TransferEntity(player, map2Entity, destination);
    EXPECT_EQ(result, TransitionResult::Success);

    // Verify position updated.
    EXPECT_FLOAT_EQ(transforms.Get(player).position.x, 50.0f);
    EXPECT_FLOAT_EQ(transforms.Get(player).position.z, 50.0f);

    // Verify membership updated.
    EXPECT_EQ(memberships.Get(player).mapEntity, map2Entity);

    // Verify spatial index updated.
    const auto* oldSpatial = system.GetSpatialIndex(mapEntity);
    ASSERT_NE(oldSpatial, nullptr);
    EXPECT_FALSE(oldSpatial->Contains(player));

    const auto* newSpatial = system.GetSpatialIndex(map2Entity);
    ASSERT_NE(newSpatial, nullptr);
    EXPECT_TRUE(newSpatial->Contains(player));
}

TEST_F(WorldSystemTest, TransferEntityInvalidMap) {
    Entity player = createEntityOnMap(1, Vector3(0.0f, 0.0f, 0.0f));

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    Entity nonexistentMap(999, 0);
    auto result = system.TransferEntity(player, nonexistentMap,
                                        Vector3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(result, TransitionResult::InvalidMap);

    // Player should still be on original map.
    EXPECT_EQ(memberships.Get(player).mapEntity, mapEntity);
}

TEST_F(WorldSystemTest, TransferEntityNotFound) {
    Entity map2Entity(10, 0);
    mapInstances.Add(map2Entity, MapInstance{2, 1, MapType::Dungeon});

    Entity nonexistent(999, 0);

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    auto result = system.TransferEntity(nonexistent, map2Entity,
                                        Vector3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(result, TransitionResult::EntityNotFound);
}

TEST_F(WorldSystemTest, TransferPreservesStateExceptPosition) {
    Entity map2Entity(10, 0);
    mapInstances.Add(map2Entity, MapInstance{2, 1, MapType::Dungeon});

    Entity player = createEntityOnMap(1, Vector3(0.0f, 0.0f, 0.0f));
    // Set non-default transform values.
    auto& t = transforms.Get(player);
    t.scale = Vector3(2.0f, 2.0f, 2.0f);
    t.rotation = Quaternion(0.0f, 0.0f, 0.707f, 0.707f);

    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);
    system.Execute(0.016f);

    auto transferResult = system.TransferEntity(player, map2Entity, Vector3(100.0f, 0.0f, 100.0f));
    EXPECT_EQ(transferResult, TransitionResult::Success);

    // Scale and rotation should be preserved.
    const auto& after = transforms.Get(player);
    EXPECT_FLOAT_EQ(after.scale.x, 2.0f);
    EXPECT_FLOAT_EQ(after.rotation.z, 0.707f);
    // Position should be updated.
    EXPECT_FLOAT_EQ(after.position.x, 100.0f);
}

// ── System metadata tests ───────────────────────────────────────────────

TEST_F(WorldSystemTest, SystemMetadata) {
    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);

    EXPECT_EQ(system.GetName(), "WorldSystem");
    EXPECT_EQ(system.GetStage(), SystemStage::PreUpdate);

    auto access = system.GetAccessInfo();
    EXPECT_FALSE(access.reads.empty());
    EXPECT_TRUE(access.writes.empty());  // Read-only system.
}

TEST_F(WorldSystemTest, GetSpatialIndexNonexistentMap) {
    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones);

    Entity nonexistent(999, 0);
    EXPECT_EQ(system.GetSpatialIndex(nonexistent), nullptr);
}

// ═══════════════════════════════════════════════════════════════════════════
// Integration: full world scenario
// ═══════════════════════════════════════════════════════════════════════════

TEST(WorldIntegration, FullWorldScenario) {
    ComponentStorage<Transform> transforms;
    ComponentStorage<MapMembership> memberships;
    ComponentStorage<MapInstance> mapInstances;
    ComponentStorage<VisibilityRange> visibilityRanges;
    ComponentStorage<Zone> zones;

    EntityManager entityManager;
    entityManager.RegisterStorage(&transforms);
    entityManager.RegisterStorage(&memberships);
    entityManager.RegisterStorage(&mapInstances);
    entityManager.RegisterStorage(&visibilityRanges);
    entityManager.RegisterStorage(&zones);

    // Create an open world map.
    Entity worldMap = entityManager.Create();
    mapInstances.Add(worldMap, MapInstance{1, 1, MapType::OpenWorld});

    // Create a dungeon map.
    Entity dungeonMap = entityManager.Create();
    mapInstances.Add(dungeonMap, MapInstance{2, 1, MapType::Dungeon});

    // Create zones on the world map.
    Entity safeZone = entityManager.Create();
    zones.Add(safeZone, Zone{1, ZoneType::Safe,
                             ZoneFlags::NoCombat | ZoneFlags::Sanctuary,
                             worldMap});

    Entity pvpZone = entityManager.Create();
    zones.Add(pvpZone, Zone{2, ZoneType::PvP,
                            ZoneFlags::FreeForAll, worldMap});

    // Create players on the world map.
    Entity player1 = entityManager.Create();
    transforms.Add(player1,
                   Transform{{0.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    memberships.Add(player1, MapMembership{worldMap, 1});

    Entity player2 = entityManager.Create();
    transforms.Add(player2,
                   Transform{{20.0f, 0.0f, 20.0f}, {}, {1.0f, 1.0f, 1.0f}});
    memberships.Add(player2, MapMembership{worldMap, 1});

    Entity player3 = entityManager.Create();
    transforms.Add(player3,
                   Transform{{500.0f, 0.0f, 500.0f}, {}, {1.0f, 1.0f, 1.0f}});
    memberships.Add(player3, MapMembership{worldMap, 2});

    // Set up the world system.
    WorldSystem system(transforms, memberships, mapInstances,
                       visibilityRanges, zones, 32.0f);

    // Tick 1: synchronize positions.
    system.Execute(0.016f);

    // Verify spatial indexing.
    const auto* worldSpatial = system.GetSpatialIndex(worldMap);
    ASSERT_NE(worldSpatial, nullptr);
    EXPECT_EQ(worldSpatial->Size(), 3u);

    // Interest management: player1 should see player2 but not player3.
    auto visible1 = system.GetVisibleEntities(player1);
    auto has2 = std::find(visible1.begin(), visible1.end(), player2);
    auto has3 = std::find(visible1.begin(), visible1.end(), player3);
    EXPECT_NE(has2, visible1.end());
    EXPECT_EQ(has3, visible1.end());

    // Map transition: transfer player1 to the dungeon.
    auto result = system.TransferEntity(
        player1, dungeonMap, Vector3(10.0f, 0.0f, 10.0f));
    EXPECT_EQ(result, TransitionResult::Success);

    // Verify transfer.
    EXPECT_EQ(memberships.Get(player1).mapEntity, dungeonMap);
    EXPECT_FLOAT_EQ(transforms.Get(player1).position.x, 10.0f);

    // Spatial indices should reflect the change.
    EXPECT_FALSE(worldSpatial->Contains(player1));

    const auto* dungeonSpatial = system.GetSpatialIndex(dungeonMap);
    ASSERT_NE(dungeonSpatial, nullptr);
    EXPECT_TRUE(dungeonSpatial->Contains(player1));

    // Player2 can no longer see player1 (different map).
    system.Execute(0.016f);
    auto visible2 = system.GetVisibleEntities(player2);
    auto hasP1 = std::find(visible2.begin(), visible2.end(), player1);
    EXPECT_EQ(hasP1, visible2.end());

    // Clean up.
    entityManager.Destroy(player1);
    entityManager.Destroy(player2);
    entityManager.Destroy(player3);
    entityManager.Destroy(worldMap);
    entityManager.Destroy(dungeonMap);
    entityManager.Destroy(safeZone);
    entityManager.Destroy(pvpZone);
}

TEST(WorldIntegration, SchedulerRegistration) {
    ComponentStorage<Transform> transforms;
    ComponentStorage<MapMembership> memberships;
    ComponentStorage<MapInstance> mapInstances;
    ComponentStorage<VisibilityRange> visibilityRanges;
    ComponentStorage<Zone> zones;

    SystemScheduler scheduler;
    auto& system = scheduler.Register<WorldSystem>(
        transforms, memberships, mapInstances,
        visibilityRanges, zones);

    EXPECT_EQ(system.GetName(), "WorldSystem");
    EXPECT_EQ(scheduler.SystemCount(), 1u);
    EXPECT_TRUE(scheduler.Build());

    // Execute with no entities (should not crash).
    scheduler.Execute(1.0f / 60.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
// Performance test: spatial query on many entities (SRS-GML-003.3)
// ═══════════════════════════════════════════════════════════════════════════

TEST(WorldPerformance, SpatialQueryTenThousandEntities) {
    SpatialIndex index(32.0f);

    // Insert 10,000 entities spread across the world.
    constexpr uint32_t kEntityCount = 10000;
    for (uint32_t i = 0; i < kEntityCount; ++i) {
        Entity e(i, 0);
        // Spread entities in a 1000x1000 area.
        float x = static_cast<float>(i % 100) * 10.0f;
        float z = static_cast<float>(i / 100) * 10.0f;
        index.Insert(e, Vector3(x, 0.0f, z));
    }
    EXPECT_EQ(index.Size(), kEntityCount);

    // Perform a radius query.
    auto result = index.QueryRadius(Vector3(500.0f, 0.0f, 500.0f), 100.0f);

    // Should find some entities but not all.
    EXPECT_GT(result.size(), 0u);
    EXPECT_LT(result.size(), kEntityCount);
}
