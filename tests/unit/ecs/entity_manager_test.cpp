#include <gtest/gtest.h>

#include <algorithm>
#include <unordered_set>
#include <vector>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity_manager.hpp"

using namespace cgs::ecs;

// ── Test component types ────────────────────────────────────────────────────

struct Position {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Health {
    int32_t current = 100;
    int32_t max = 100;
};

struct Tag {};

// ===========================================================================
// EntityManager: Basic creation
// ===========================================================================

TEST(EntityManagerTest, CreateReturnsValidEntity) {
    EntityManager mgr;
    Entity e = mgr.Create();
    EXPECT_TRUE(e.isValid());
    EXPECT_TRUE(mgr.IsAlive(e));
}

TEST(EntityManagerTest, CreateReturnsUniqueEntities) {
    EntityManager mgr;
    Entity a = mgr.Create();
    Entity b = mgr.Create();
    Entity c = mgr.Create();

    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
}

TEST(EntityManagerTest, CreateIncrementsCount) {
    EntityManager mgr;
    EXPECT_EQ(mgr.Count(), 0u);

    [[maybe_unused]] auto e1 = mgr.Create();
    EXPECT_EQ(mgr.Count(), 1u);

    [[maybe_unused]] auto e2 = mgr.Create();
    EXPECT_EQ(mgr.Count(), 2u);
}

TEST(EntityManagerTest, FirstEntityHasIndexZero) {
    EntityManager mgr;
    Entity e = mgr.Create();
    EXPECT_EQ(e.id(), 0u);
    EXPECT_EQ(e.version(), 0);
}

TEST(EntityManagerTest, SequentialIndices) {
    EntityManager mgr;
    for (uint32_t i = 0; i < 10; ++i) {
        Entity e = mgr.Create();
        EXPECT_EQ(e.id(), i);
        EXPECT_EQ(e.version(), 0);
    }
}

// ===========================================================================
// EntityManager: Immediate destruction
// ===========================================================================

TEST(EntityManagerTest, DestroyMakesEntityDead) {
    EntityManager mgr;
    Entity e = mgr.Create();

    mgr.Destroy(e);
    EXPECT_FALSE(mgr.IsAlive(e));
}

TEST(EntityManagerTest, DestroyDecrementsCount) {
    EntityManager mgr;
    Entity a = mgr.Create();
    Entity b = mgr.Create();
    EXPECT_EQ(mgr.Count(), 2u);

    mgr.Destroy(a);
    EXPECT_EQ(mgr.Count(), 1u);

    mgr.Destroy(b);
    EXPECT_EQ(mgr.Count(), 0u);
}

TEST(EntityManagerTest, DestroyDeadEntityIsNoop) {
    EntityManager mgr;
    Entity e = mgr.Create();
    mgr.Destroy(e);

    // Destroying again should be safe.
    mgr.Destroy(e);
    EXPECT_EQ(mgr.Count(), 0u);
}

TEST(EntityManagerTest, DestroyInvalidEntityIsNoop) {
    EntityManager mgr;
    mgr.Destroy(Entity::invalid());
    EXPECT_EQ(mgr.Count(), 0u);
}

// ===========================================================================
// EntityManager: ID recycling and versioning
// ===========================================================================

TEST(EntityManagerTest, RecycledEntityHasIncrementedVersion) {
    EntityManager mgr;
    Entity e1 = mgr.Create();
    uint32_t originalId = e1.id();

    mgr.Destroy(e1);
    Entity e2 = mgr.Create();

    // Same index but bumped version.
    EXPECT_EQ(e2.id(), originalId);
    EXPECT_EQ(e2.version(), 1);
}

TEST(EntityManagerTest, StaleHandleIsNotAlive) {
    EntityManager mgr;
    Entity e1 = mgr.Create();
    mgr.Destroy(e1);

    // e1 is now stale (version 0, but slot is at version 1).
    EXPECT_FALSE(mgr.IsAlive(e1));

    Entity e2 = mgr.Create();
    EXPECT_TRUE(mgr.IsAlive(e2));
    EXPECT_FALSE(mgr.IsAlive(e1));
}

TEST(EntityManagerTest, RecycleMultipleTimes) {
    EntityManager mgr;
    Entity e = mgr.Create();
    uint32_t idx = e.id();

    for (int v = 1; v <= 10; ++v) {
        mgr.Destroy(e);
        EXPECT_FALSE(mgr.IsAlive(e));

        e = mgr.Create();
        EXPECT_EQ(e.id(), idx);
        EXPECT_EQ(e.version(), static_cast<uint8_t>(v));
        EXPECT_TRUE(mgr.IsAlive(e));
    }
}

TEST(EntityManagerTest, FreeListFIFOOrder) {
    EntityManager mgr;
    Entity a = mgr.Create();                  // id=0
    [[maybe_unused]] Entity b = mgr.Create();  // id=1
    Entity c = mgr.Create();                   // id=2

    mgr.Destroy(a);
    mgr.Destroy(c);

    // Oldest destroyed (a, id=0) should be recycled first.
    Entity d = mgr.Create();
    EXPECT_EQ(d.id(), 0u);

    // Then c (id=2).
    Entity e = mgr.Create();
    EXPECT_EQ(e.id(), 2u);

    // Next create allocates a new index.
    Entity f = mgr.Create();
    EXPECT_EQ(f.id(), 3u);
}

// ===========================================================================
// EntityManager: IsAlive edge cases
// ===========================================================================

TEST(EntityManagerTest, IsAliveReturnsFalseForInvalid) {
    EntityManager mgr;
    EXPECT_FALSE(mgr.IsAlive(Entity::invalid()));
}

TEST(EntityManagerTest, IsAliveReturnsFalseForOutOfRange) {
    EntityManager mgr;
    // Entity with index 999 never created.
    Entity e(999, 0);
    EXPECT_FALSE(mgr.IsAlive(e));
}

TEST(EntityManagerTest, IsAliveReturnsFalseForVersionMismatch) {
    EntityManager mgr;
    Entity e = mgr.Create();  // version 0

    // Manually construct a handle with wrong version.
    Entity stale(e.id(), 5);
    EXPECT_FALSE(mgr.IsAlive(stale));
}

// ===========================================================================
// EntityManager: Deferred destruction
// ===========================================================================

TEST(EntityManagerTest, DeferredDoesNotDestroyImmediately) {
    EntityManager mgr;
    Entity e = mgr.Create();

    mgr.DestroyDeferred(e);
    EXPECT_TRUE(mgr.IsAlive(e));
    EXPECT_EQ(mgr.Count(), 1u);
}

TEST(EntityManagerTest, FlushDeferredDestroysQueued) {
    EntityManager mgr;
    Entity a = mgr.Create();
    Entity b = mgr.Create();

    mgr.DestroyDeferred(a);
    mgr.DestroyDeferred(b);

    mgr.FlushDeferred();
    EXPECT_FALSE(mgr.IsAlive(a));
    EXPECT_FALSE(mgr.IsAlive(b));
    EXPECT_EQ(mgr.Count(), 0u);
}

TEST(EntityManagerTest, FlushDeferredSkipsAlreadyDead) {
    EntityManager mgr;
    Entity a = mgr.Create();
    Entity b = mgr.Create();

    mgr.DestroyDeferred(a);
    mgr.Destroy(a);  // Immediate destroy before flush.

    mgr.FlushDeferred();
    EXPECT_FALSE(mgr.IsAlive(a));
    EXPECT_TRUE(mgr.IsAlive(b));
    EXPECT_EQ(mgr.Count(), 1u);
}

TEST(EntityManagerTest, FlushDeferredClearsQueue) {
    EntityManager mgr;
    Entity e = mgr.Create();

    mgr.DestroyDeferred(e);
    mgr.FlushDeferred();
    EXPECT_EQ(mgr.Count(), 0u);

    // Second flush should be a no-op.
    Entity f = mgr.Create();
    mgr.FlushDeferred();
    EXPECT_TRUE(mgr.IsAlive(f));
    EXPECT_EQ(mgr.Count(), 1u);
}

TEST(EntityManagerTest, DeferredDeadEntityIsNoop) {
    EntityManager mgr;
    Entity e = mgr.Create();
    mgr.Destroy(e);

    // Queueing a dead entity should not crash or cause issues.
    mgr.DestroyDeferred(e);
    mgr.FlushDeferred();
    EXPECT_EQ(mgr.Count(), 0u);
}

TEST(EntityManagerTest, FlushDeferredOnEmptyQueue) {
    EntityManager mgr;
    // Should be safe when nothing is queued.
    mgr.FlushDeferred();
    EXPECT_EQ(mgr.Count(), 0u);
}

// ===========================================================================
// EntityManager: Component auto-removal
// ===========================================================================

TEST(EntityManagerTest, DestroyRemovesComponents) {
    EntityManager mgr;
    ComponentStorage<Position> positions;
    ComponentStorage<Health> health;
    mgr.RegisterStorage(&positions);
    mgr.RegisterStorage(&health);

    Entity e = mgr.Create();
    positions.Add(e, Position{1.0f, 2.0f, 3.0f});
    health.Add(e, Health{50, 100});

    EXPECT_TRUE(positions.Has(e));
    EXPECT_TRUE(health.Has(e));

    mgr.Destroy(e);
    EXPECT_FALSE(positions.Has(e));
    EXPECT_FALSE(health.Has(e));
}

TEST(EntityManagerTest, DeferredDestroyRemovesComponents) {
    EntityManager mgr;
    ComponentStorage<Position> positions;
    mgr.RegisterStorage(&positions);

    Entity e = mgr.Create();
    positions.Add(e, Position{1.0f, 2.0f, 3.0f});

    mgr.DestroyDeferred(e);
    EXPECT_TRUE(positions.Has(e));  // Not yet destroyed.

    mgr.FlushDeferred();
    EXPECT_FALSE(positions.Has(e));
}

TEST(EntityManagerTest, DestroyOnlyAffectsTargetEntity) {
    EntityManager mgr;
    ComponentStorage<Position> positions;
    mgr.RegisterStorage(&positions);

    Entity a = mgr.Create();
    Entity b = mgr.Create();
    positions.Add(a, Position{1.0f, 0.0f, 0.0f});
    positions.Add(b, Position{2.0f, 0.0f, 0.0f});

    mgr.Destroy(a);
    EXPECT_FALSE(positions.Has(a));
    EXPECT_TRUE(positions.Has(b));
    EXPECT_FLOAT_EQ(positions.Get(b).x, 2.0f);
}

TEST(EntityManagerTest, DestroyEntityWithoutComponents) {
    EntityManager mgr;
    ComponentStorage<Position> positions;
    mgr.RegisterStorage(&positions);

    Entity e = mgr.Create();
    // Destroy without adding any components — should not crash.
    mgr.Destroy(e);
    EXPECT_FALSE(mgr.IsAlive(e));
}

// ===========================================================================
// EntityManager: Capacity
// ===========================================================================

TEST(EntityManagerTest, CapacityGrowsWithCreation) {
    EntityManager mgr;
    EXPECT_EQ(mgr.Capacity(), 0u);

    [[maybe_unused]] auto e1 = mgr.Create();
    EXPECT_EQ(mgr.Capacity(), 1u);

    [[maybe_unused]] auto e2 = mgr.Create();
    EXPECT_EQ(mgr.Capacity(), 2u);
}

TEST(EntityManagerTest, CapacityDoesNotShrinkOnDestroy) {
    EntityManager mgr;
    Entity a = mgr.Create();
    Entity b = mgr.Create();
    EXPECT_EQ(mgr.Capacity(), 2u);

    mgr.Destroy(a);
    mgr.Destroy(b);
    // Capacity stays the same (slots are on free list).
    EXPECT_EQ(mgr.Capacity(), 2u);
    EXPECT_EQ(mgr.Count(), 0u);
}

// ===========================================================================
// EntityManager: Bulk operations
// ===========================================================================

TEST(EntityManagerTest, CreateAndDestroyManyEntities) {
    EntityManager mgr;
    constexpr std::size_t N = 1000;

    std::vector<Entity> entities;
    entities.reserve(N);
    for (std::size_t i = 0; i < N; ++i) {
        entities.push_back(mgr.Create());
    }
    EXPECT_EQ(mgr.Count(), N);

    // All entities should be unique.
    std::unordered_set<uint32_t> rawSet;
    for (const auto& e : entities) {
        rawSet.insert(e.raw);
    }
    EXPECT_EQ(rawSet.size(), N);

    // Destroy all.
    for (const auto& e : entities) {
        mgr.Destroy(e);
    }
    EXPECT_EQ(mgr.Count(), 0u);
}

TEST(EntityManagerTest, CreateAfterDestroyAll) {
    EntityManager mgr;
    Entity a = mgr.Create();
    Entity b = mgr.Create();
    Entity c = mgr.Create();

    mgr.Destroy(a);
    mgr.Destroy(b);
    mgr.Destroy(c);

    // Create new entities — should recycle indices 0, 1, 2 (FIFO).
    Entity d = mgr.Create();
    Entity e = mgr.Create();
    Entity f = mgr.Create();

    EXPECT_EQ(d.id(), 0u);
    EXPECT_EQ(d.version(), 1);
    EXPECT_EQ(e.id(), 1u);
    EXPECT_EQ(e.version(), 1);
    EXPECT_EQ(f.id(), 2u);
    EXPECT_EQ(f.version(), 1);

    EXPECT_TRUE(mgr.IsAlive(d));
    EXPECT_TRUE(mgr.IsAlive(e));
    EXPECT_TRUE(mgr.IsAlive(f));
}

// ===========================================================================
// EntityManager: Recycled entity with new components
// ===========================================================================

TEST(EntityManagerTest, RecycledEntityCanHaveNewComponents) {
    EntityManager mgr;
    ComponentStorage<Position> positions;
    mgr.RegisterStorage(&positions);

    Entity e1 = mgr.Create();
    positions.Add(e1, Position{1.0f, 2.0f, 3.0f});

    mgr.Destroy(e1);
    EXPECT_FALSE(positions.Has(e1));

    Entity e2 = mgr.Create();
    EXPECT_EQ(e2.id(), e1.id());
    EXPECT_NE(e2.version(), e1.version());

    // New entity can get its own components.
    positions.Add(e2, Position{10.0f, 20.0f, 30.0f});
    EXPECT_TRUE(positions.Has(e2));
    EXPECT_FLOAT_EQ(positions.Get(e2).x, 10.0f);
}

// ===========================================================================
// EntityManager: Mixed deferred and immediate
// ===========================================================================

TEST(EntityManagerTest, MixedDeferredAndImmediate) {
    EntityManager mgr;
    Entity a = mgr.Create();
    Entity b = mgr.Create();
    Entity c = mgr.Create();

    mgr.DestroyDeferred(a);
    mgr.Destroy(b);  // Immediate.
    mgr.DestroyDeferred(c);

    EXPECT_TRUE(mgr.IsAlive(a));    // Deferred, not yet flushed.
    EXPECT_FALSE(mgr.IsAlive(b));   // Immediately destroyed.
    EXPECT_TRUE(mgr.IsAlive(c));    // Deferred, not yet flushed.
    EXPECT_EQ(mgr.Count(), 2u);

    mgr.FlushDeferred();
    EXPECT_FALSE(mgr.IsAlive(a));
    EXPECT_FALSE(mgr.IsAlive(c));
    EXPECT_EQ(mgr.Count(), 0u);
}

// ===========================================================================
// EntityManager: Version wrapping
// ===========================================================================

TEST(EntityManagerTest, VersionWrapsAround) {
    EntityManager mgr;
    Entity e = mgr.Create();
    uint32_t idx = e.id();

    // Cycle through all 256 version values.
    for (int i = 0; i < 256; ++i) {
        mgr.Destroy(e);
        e = mgr.Create();
        EXPECT_EQ(e.id(), idx);
    }

    // After 256 destroy/create cycles starting from version 0,
    // the version should wrap back to 0.
    EXPECT_EQ(e.version(), 0);
    EXPECT_TRUE(mgr.IsAlive(e));
}

// ===========================================================================
// EntityManager: Multiple storages
// ===========================================================================

TEST(EntityManagerTest, MultipleStoragesAllCleaned) {
    EntityManager mgr;
    ComponentStorage<Position> positions;
    ComponentStorage<Health> health;
    ComponentStorage<Tag> tags;
    mgr.RegisterStorage(&positions);
    mgr.RegisterStorage(&health);
    mgr.RegisterStorage(&tags);

    Entity e = mgr.Create();
    positions.Add(e, Position{1.0f, 2.0f, 3.0f});
    health.Add(e, Health{50, 100});
    tags.Add(e);

    mgr.Destroy(e);
    EXPECT_FALSE(positions.Has(e));
    EXPECT_FALSE(health.Has(e));
    EXPECT_FALSE(tags.Has(e));
}
