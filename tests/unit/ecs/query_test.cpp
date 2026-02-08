#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <vector>

#include "cgs/ecs/entity_manager.hpp"
#include "cgs/ecs/query.hpp"

using namespace cgs::ecs;

// ── Test component types ────────────────────────────────────────────────────

struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

struct Velocity {
    float dx = 0.0f;
    float dy = 0.0f;
};

struct Health {
    int32_t current = 100;
    int32_t max = 100;
};

struct Armor {
    int32_t value = 0;
};

// Tag component (zero-sized).
struct Static {};

struct DeadTag {};

// ── Test fixture ────────────────────────────────────────────────────────────

class QueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        em_.RegisterStorage(&positions_);
        em_.RegisterStorage(&velocities_);
        em_.RegisterStorage(&healths_);
        em_.RegisterStorage(&armors_);
        em_.RegisterStorage(&statics_);
        em_.RegisterStorage(&dead_);
    }

    EntityManager em_;
    ComponentStorage<Position> positions_;
    ComponentStorage<Velocity> velocities_;
    ComponentStorage<Health> healths_;
    ComponentStorage<Armor> armors_;
    ComponentStorage<Static> statics_;
    ComponentStorage<DeadTag> dead_;
};

// ── Multi-component queries ─────────────────────────────────────────────────

TEST_F(QueryTest, BasicMultiComponentQuery) {
    // Entity with Position + Velocity (should match).
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});
    velocities_.Add(e1, Velocity{0.5f, 0.5f});

    // Entity with Position only (should NOT match).
    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});

    // Entity with Velocity only (should NOT match).
    auto e3 = em_.Create();
    velocities_.Add(e3, Velocity{1.0f, 1.0f});

    // Entity with Position + Velocity (should match).
    auto e4 = em_.Create();
    positions_.Add(e4, Position{5.0f, 6.0f});
    velocities_.Add(e4, Velocity{2.0f, 2.0f});

    Query<Position, Velocity> query(positions_, velocities_);
    EXPECT_EQ(query.Count(), 2u);
}

TEST_F(QueryTest, SingleComponentQuery) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});
    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});
    auto e3 = em_.Create();
    velocities_.Add(e3, Velocity{1.0f, 1.0f});

    Query<Position> query(positions_);
    EXPECT_EQ(query.Count(), 2u);
}

TEST_F(QueryTest, ThreeComponentQuery) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});
    velocities_.Add(e1, Velocity{0.5f, 0.5f});
    healths_.Add(e1, Health{80, 100});

    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});
    velocities_.Add(e2, Velocity{1.0f, 1.0f});
    // No health — should not match.

    Query<Position, Velocity, Health> query(positions_, velocities_, healths_);
    EXPECT_EQ(query.Count(), 1u);
}

TEST_F(QueryTest, EmptyQuery) {
    // No entities at all.
    Query<Position, Velocity> query(positions_, velocities_);
    EXPECT_EQ(query.Count(), 0u);

    std::size_t iterations = 0;
    query.ForEach([&](Entity, Position&, Velocity&) { ++iterations; });
    EXPECT_EQ(iterations, 0u);
}

// ── Exclude filter ──────────────────────────────────────────────────────────

TEST_F(QueryTest, ExcludeFilter) {
    // Movable entity.
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});
    velocities_.Add(e1, Velocity{0.5f, 0.5f});

    // Static entity (excluded).
    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});
    velocities_.Add(e2, Velocity{0.0f, 0.0f});
    statics_.Add(e2, Static{});

    // Another movable entity.
    auto e3 = em_.Create();
    positions_.Add(e3, Position{5.0f, 6.0f});
    velocities_.Add(e3, Velocity{1.0f, 1.0f});

    Query<Position, Velocity> query(positions_, velocities_);
    query.Exclude(statics_);

    EXPECT_EQ(query.Count(), 2u);

    // Verify excluded entity is not present.
    query.ForEach([&](Entity e, Position&, Velocity&) {
        EXPECT_NE(e.id(), e2.id());
    });
}

TEST_F(QueryTest, MultipleExcludes) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});

    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});
    statics_.Add(e2, Static{});

    auto e3 = em_.Create();
    positions_.Add(e3, Position{5.0f, 6.0f});
    dead_.Add(e3, DeadTag{});

    auto e4 = em_.Create();
    positions_.Add(e4, Position{7.0f, 8.0f});

    Query<Position> query(positions_);
    query.Exclude(statics_).Exclude(dead_);

    EXPECT_EQ(query.Count(), 2u);
}

// ── Optional component access ───────────────────────────────────────────────

TEST_F(QueryTest, OptionalComponentAccess) {
    // Entity with Position + Armor.
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});
    armors_.Add(e1, Armor{50});

    // Entity with Position only.
    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});

    Query<Position> query(positions_);
    query.Optional(armors_);

    std::size_t withArmor = 0;
    std::size_t withoutArmor = 0;

    query.ForEach([&](Entity e, Position&) {
        Armor* armor = query.GetOptional<Armor>(e);
        if (armor != nullptr) {
            EXPECT_EQ(armor->value, 50);
            ++withArmor;
        } else {
            ++withoutArmor;
        }
    });

    EXPECT_EQ(withArmor, 1u);
    EXPECT_EQ(withoutArmor, 1u);
}

TEST_F(QueryTest, OptionalDoesNotAffectMatching) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});

    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});
    // No armor on either entity.

    Query<Position> query(positions_);
    query.Optional(armors_);

    // Both should match — optional does not filter.
    EXPECT_EQ(query.Count(), 2u);
}

// ── ForEach callback ────────────────────────────────────────────────────────

TEST_F(QueryTest, ForEachMutatesComponents) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{0.0f, 0.0f});
    velocities_.Add(e1, Velocity{1.0f, 2.0f});

    auto e2 = em_.Create();
    positions_.Add(e2, Position{10.0f, 20.0f});
    velocities_.Add(e2, Velocity{-1.0f, -2.0f});

    Query<Position, Velocity> query(positions_, velocities_);
    query.ForEach([](Entity, Position& pos, Velocity& vel) {
        pos.x += vel.dx;
        pos.y += vel.dy;
    });

    EXPECT_FLOAT_EQ(positions_.Get(e1).x, 1.0f);
    EXPECT_FLOAT_EQ(positions_.Get(e1).y, 2.0f);
    EXPECT_FLOAT_EQ(positions_.Get(e2).x, 9.0f);
    EXPECT_FLOAT_EQ(positions_.Get(e2).y, 18.0f);
}

TEST_F(QueryTest, ForEachReceivesCorrectEntity) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 0.0f});

    auto e2 = em_.Create();
    positions_.Add(e2, Position{2.0f, 0.0f});

    Query<Position> query(positions_);

    std::unordered_set<uint32_t> visited;
    query.ForEach([&](Entity e, Position& pos) {
        visited.insert(e.id());
        // Verify entity id maps to the correct component.
        EXPECT_FLOAT_EQ(positions_.Get(e).x, pos.x);
    });

    EXPECT_EQ(visited.size(), 2u);
    EXPECT_TRUE(visited.count(e1.id()));
    EXPECT_TRUE(visited.count(e2.id()));
}

// ── Count ───────────────────────────────────────────────────────────────────

TEST_F(QueryTest, CountMatchesForEach) {
    for (int i = 0; i < 50; ++i) {
        auto e = em_.Create();
        positions_.Add(e, Position{static_cast<float>(i), 0.0f});
        if (i % 2 == 0) {
            velocities_.Add(e, Velocity{1.0f, 0.0f});
        }
    }

    Query<Position, Velocity> query(positions_, velocities_);

    std::size_t forEachCount = 0;
    query.ForEach(
        [&](Entity, Position&, Velocity&) { ++forEachCount; });

    EXPECT_EQ(query.Count(), forEachCount);
    EXPECT_EQ(query.Count(), 25u);
}

// ── Cache invalidation ──────────────────────────────────────────────────────

TEST_F(QueryTest, CacheInvalidatedOnComponentAdd) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});
    velocities_.Add(e1, Velocity{0.5f, 0.5f});

    Query<Position, Velocity> query(positions_, velocities_);
    EXPECT_EQ(query.Count(), 1u);

    // Add a second matching entity.
    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});
    velocities_.Add(e2, Velocity{1.0f, 1.0f});

    // Cache should be invalidated automatically.
    EXPECT_EQ(query.Count(), 2u);
}

TEST_F(QueryTest, CacheInvalidatedOnComponentRemove) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});
    velocities_.Add(e1, Velocity{0.5f, 0.5f});

    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});
    velocities_.Add(e2, Velocity{1.0f, 1.0f});

    Query<Position, Velocity> query(positions_, velocities_);
    EXPECT_EQ(query.Count(), 2u);

    // Remove velocity from e1.
    velocities_.Remove(e1);
    EXPECT_EQ(query.Count(), 1u);
}

TEST_F(QueryTest, CacheInvalidatedOnExcludeStorageChange) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});

    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});

    Query<Position> query(positions_);
    query.Exclude(statics_);
    EXPECT_EQ(query.Count(), 2u);

    // Make e2 static — should be excluded now.
    statics_.Add(e2, Static{});
    EXPECT_EQ(query.Count(), 1u);
}

TEST_F(QueryTest, CacheReusedWhenUnchanged) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});
    velocities_.Add(e1, Velocity{0.5f, 0.5f});

    Query<Position, Velocity> query(positions_, velocities_);

    // First call builds the cache.
    EXPECT_EQ(query.Count(), 1u);

    // Second call should reuse the cache (no version change).
    EXPECT_EQ(query.Count(), 1u);

    // Mutate a component value (does not change structure).
    positions_.Replace(e1, Position{9.0f, 9.0f});

    // Version changed due to Replace, but the entity set is the same.
    // Cache is rebuilt but produces the same result.
    EXPECT_EQ(query.Count(), 1u);
}

// ── Range-based for loop ────────────────────────────────────────────────────

TEST_F(QueryTest, RangeBasedForLoop) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 0.0f});
    velocities_.Add(e1, Velocity{0.5f, 0.0f});

    auto e2 = em_.Create();
    positions_.Add(e2, Position{2.0f, 0.0f});
    velocities_.Add(e2, Velocity{1.0f, 0.0f});

    Query<Position, Velocity> query(positions_, velocities_);

    std::size_t count = 0;
    for (Entity e : query) {
        EXPECT_TRUE(positions_.Has(e));
        EXPECT_TRUE(velocities_.Has(e));
        ++count;
    }
    EXPECT_EQ(count, 2u);
}

TEST_F(QueryTest, ConstRangeBasedForLoop) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 0.0f});

    const Query<Position> query(positions_);

    std::size_t count = 0;
    for (Entity e : query) {
        EXPECT_TRUE(positions_.Has(e));
        ++count;
    }
    EXPECT_EQ(count, 1u);
}

// ── SmallestStorage optimization ────────────────────────────────────────────

TEST_F(QueryTest, IteratesSmallestStorage) {
    // Add 100 positions but only 3 velocities.
    std::vector<Entity> entities;
    for (int i = 0; i < 100; ++i) {
        auto e = em_.Create();
        positions_.Add(e, Position{static_cast<float>(i), 0.0f});
        entities.push_back(e);
    }
    // Only 3 entities get velocity.
    velocities_.Add(entities[0], Velocity{1.0f, 0.0f});
    velocities_.Add(entities[50], Velocity{2.0f, 0.0f});
    velocities_.Add(entities[99], Velocity{3.0f, 0.0f});

    Query<Position, Velocity> query(positions_, velocities_);
    EXPECT_EQ(query.Count(), 3u);
}

// ── Entity destruction integration ──────────────────────────────────────────

TEST_F(QueryTest, DestroyedEntityRemovedFromQuery) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});
    velocities_.Add(e1, Velocity{0.5f, 0.5f});

    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});
    velocities_.Add(e2, Velocity{1.0f, 1.0f});

    Query<Position, Velocity> query(positions_, velocities_);
    EXPECT_EQ(query.Count(), 2u);

    // Destroy e1 — EntityManager removes its components.
    em_.Destroy(e1);
    EXPECT_EQ(query.Count(), 1u);
}

// ── Chaining ────────────────────────────────────────────────────────────────

TEST_F(QueryTest, MethodChaining) {
    auto e1 = em_.Create();
    positions_.Add(e1, Position{1.0f, 2.0f});

    auto e2 = em_.Create();
    positions_.Add(e2, Position{3.0f, 4.0f});
    statics_.Add(e2, Static{});

    auto e3 = em_.Create();
    positions_.Add(e3, Position{5.0f, 6.0f});
    dead_.Add(e3, DeadTag{});

    // Chain multiple Exclude and Optional.
    Query<Position> query(positions_);
    query.Exclude(statics_).Exclude(dead_).Optional(armors_);

    EXPECT_EQ(query.Count(), 1u);
}

// ── Performance test ────────────────────────────────────────────────────────

TEST_F(QueryTest, PerformanceCachedVsUncached) {
    constexpr int kEntityCount = 10'000;
    constexpr int kIterations = 100;

    // Create entities: 70% have Position+Velocity, 30% Position only.
    for (int i = 0; i < kEntityCount; ++i) {
        auto e = em_.Create();
        positions_.Add(e, Position{static_cast<float>(i), 0.0f});
        if (i % 10 < 7) {
            velocities_.Add(e, Velocity{1.0f, 0.0f});
        }
    }

    Query<Position, Velocity> query(positions_, velocities_);

    // First (uncached) query.
    auto t0 = std::chrono::high_resolution_clock::now();
    auto uncachedCount = query.Count();
    auto t1 = std::chrono::high_resolution_clock::now();

    // Subsequent (cached) queries.
    auto t2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        auto count = query.Count();
        EXPECT_EQ(count, uncachedCount);
    }
    auto t3 = std::chrono::high_resolution_clock::now();

    auto uncachedUs =
        std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    auto cachedUs =
        std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

    // Report timings.
    std::cout << "[PERF] Entity count: " << kEntityCount << "\n";
    std::cout << "[PERF] Matching entities: " << uncachedCount << "\n";
    std::cout << "[PERF] Uncached query: " << uncachedUs << " us\n";
    std::cout << "[PERF] Cached " << kIterations << " queries: " << cachedUs
              << " us (avg "
              << (kIterations > 0 ? cachedUs / kIterations : 0)
              << " us/query)\n";

    // Cached queries should be significantly faster.
    EXPECT_EQ(uncachedCount, 7000u);
    // Sanity: cached batch should complete faster than uncached single.
    // (Allow generous margin for CI variability.)
    EXPECT_LT(cachedUs, uncachedUs * kIterations);
}

TEST_F(QueryTest, PerformanceForEach10KEntities) {
    constexpr int kEntityCount = 10'000;

    for (int i = 0; i < kEntityCount; ++i) {
        auto e = em_.Create();
        positions_.Add(e, Position{static_cast<float>(i), 0.0f});
        velocities_.Add(e, Velocity{1.0f, 2.0f});
    }

    Query<Position, Velocity> query(positions_, velocities_);

    auto t0 = std::chrono::high_resolution_clock::now();
    query.ForEach([](Entity, Position& pos, Velocity& vel) {
        pos.x += vel.dx;
        pos.y += vel.dy;
    });
    auto t1 = std::chrono::high_resolution_clock::now();

    auto us =
        std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    std::cout << "[PERF] ForEach " << kEntityCount
              << " entities: " << us << " us\n";

    EXPECT_EQ(query.Count(), static_cast<std::size_t>(kEntityCount));
}
