#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity.hpp"

using namespace cgs::ecs;

// ── Test component types ────────────────────────────────────────────────────

struct Position {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Velocity {
    float dx = 0.0f;
    float dy = 0.0f;
};

struct Health {
    int32_t current = 100;
    int32_t max = 100;
};

struct Name {
    std::string value;
};

// Move-only component.
struct UniqueResource {
    int id = 0;
    UniqueResource() = default;
    explicit UniqueResource(int i) : id(i) {}
    UniqueResource(const UniqueResource&) = delete;
    UniqueResource& operator=(const UniqueResource&) = delete;
    UniqueResource(UniqueResource&& o) noexcept : id(o.id) { o.id = -1; }
    UniqueResource& operator=(UniqueResource&& o) noexcept {
        id = o.id;
        o.id = -1;
        return *this;
    }
};

// Tag component (zero-size).
struct DeadTag {};

// ===========================================================================
// Entity: construction and accessors
// ===========================================================================

TEST(EntityTest, DefaultIsInvalid) {
    Entity e;
    EXPECT_FALSE(e.isValid());
    EXPECT_EQ(e, Entity::invalid());
}

TEST(EntityTest, ConstructFromIdAndVersion) {
    Entity e(42, 3);
    EXPECT_TRUE(e.isValid());
    EXPECT_EQ(e.id(), 42u);
    EXPECT_EQ(e.version(), 3);
}

TEST(EntityTest, MaxIdValue) {
    Entity e(Entity::kMaxId, 0);
    EXPECT_TRUE(e.isValid());
    EXPECT_EQ(e.id(), Entity::kMaxId);
}

TEST(EntityTest, Comparison) {
    Entity a(1, 0);
    Entity b(1, 0);
    Entity c(2, 0);
    Entity d(1, 1);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

TEST(EntityTest, SizeIsFourBytes) {
    static_assert(sizeof(Entity) == 4);
}

TEST(EntityTest, HashWorks) {
    std::unordered_map<Entity, int> map;
    Entity e(10, 0);
    map[e] = 42;
    EXPECT_EQ(map[e], 42);
}

// ===========================================================================
// ComponentTypeId: compile-time identification
// ===========================================================================

TEST(ComponentTypeIdTest, UniquePerType) {
    auto posId = ComponentType<Position>::Id();
    auto velId = ComponentType<Velocity>::Id();
    auto healthId = ComponentType<Health>::Id();
    EXPECT_NE(posId, velId);
    EXPECT_NE(posId, healthId);
    EXPECT_NE(velId, healthId);
}

TEST(ComponentTypeIdTest, StableAcrossCalls) {
    auto first = ComponentType<Position>::Id();
    auto second = ComponentType<Position>::Id();
    EXPECT_EQ(first, second);
}

// ===========================================================================
// ComponentStorage: Add / Get / Has
// ===========================================================================

TEST(ComponentStorageTest, AddAndGet) {
    ComponentStorage<Position> storage;
    Entity e(1, 0);

    auto& pos = storage.Add(e, Position{1.0f, 2.0f, 3.0f});
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
    EXPECT_FLOAT_EQ(pos.y, 2.0f);
    EXPECT_FLOAT_EQ(pos.z, 3.0f);

    EXPECT_TRUE(storage.Has(e));
    EXPECT_EQ(storage.Size(), 1u);

    auto& got = storage.Get(e);
    EXPECT_FLOAT_EQ(got.x, 1.0f);
}

TEST(ComponentStorageTest, AddDefaultConstructed) {
    ComponentStorage<Health> storage;
    Entity e(5, 0);
    auto& h = storage.Add(e);
    EXPECT_EQ(h.current, 100);
    EXPECT_EQ(h.max, 100);
}

TEST(ComponentStorageTest, HasReturnsFalseForAbsent) {
    ComponentStorage<Position> storage;
    Entity e(99, 0);
    EXPECT_FALSE(storage.Has(e));
}

TEST(ComponentStorageTest, HasReturnsFalseForInvalid) {
    ComponentStorage<Position> storage;
    EXPECT_FALSE(storage.Has(Entity::invalid()));
}

TEST(ComponentStorageTest, GetConst) {
    ComponentStorage<Position> storage;
    Entity e(1, 0);
    storage.Add(e, Position{5.0f, 6.0f, 7.0f});

    const auto& constStorage = storage;
    EXPECT_FLOAT_EQ(constStorage.Get(e).x, 5.0f);
}

TEST(ComponentStorageTest, MutateViaGet) {
    ComponentStorage<Health> storage;
    Entity e(1, 0);
    storage.Add(e, Health{50, 100});
    storage.Get(e).current = 75;
    EXPECT_EQ(storage.Get(e).current, 75);
}

// ===========================================================================
// ComponentStorage: Remove
// ===========================================================================

TEST(ComponentStorageTest, RemoveSingle) {
    ComponentStorage<Position> storage;
    Entity e(1, 0);
    storage.Add(e, Position{1.0f, 2.0f, 3.0f});

    storage.Remove(e);
    EXPECT_FALSE(storage.Has(e));
    EXPECT_EQ(storage.Size(), 0u);
}

TEST(ComponentStorageTest, RemoveSwapsWithLast) {
    ComponentStorage<Position> storage;
    Entity a(1, 0);
    Entity b(2, 0);
    Entity c(3, 0);

    storage.Add(a, Position{1.0f, 0.0f, 0.0f});
    storage.Add(b, Position{2.0f, 0.0f, 0.0f});
    storage.Add(c, Position{3.0f, 0.0f, 0.0f});

    // Remove middle element; last element (c) should take its place.
    storage.Remove(b);

    EXPECT_EQ(storage.Size(), 2u);
    EXPECT_TRUE(storage.Has(a));
    EXPECT_FALSE(storage.Has(b));
    EXPECT_TRUE(storage.Has(c));

    // Verify values are correct after swap.
    EXPECT_FLOAT_EQ(storage.Get(a).x, 1.0f);
    EXPECT_FLOAT_EQ(storage.Get(c).x, 3.0f);
}

TEST(ComponentStorageTest, RemoveNonexistentIsNoop) {
    ComponentStorage<Position> storage;
    Entity e(42, 0);
    storage.Remove(e);  // should not crash
    EXPECT_EQ(storage.Size(), 0u);
}

TEST(ComponentStorageTest, RemoveLastElement) {
    ComponentStorage<Position> storage;
    Entity a(1, 0);
    Entity b(2, 0);
    storage.Add(a, Position{1.0f, 0.0f, 0.0f});
    storage.Add(b, Position{2.0f, 0.0f, 0.0f});

    storage.Remove(b);
    EXPECT_EQ(storage.Size(), 1u);
    EXPECT_TRUE(storage.Has(a));
    EXPECT_FLOAT_EQ(storage.Get(a).x, 1.0f);
}

TEST(ComponentStorageTest, AddAfterRemove) {
    ComponentStorage<Health> storage;
    Entity e(1, 0);

    storage.Add(e, Health{50, 100});
    storage.Remove(e);
    EXPECT_FALSE(storage.Has(e));

    // Re-add to the same entity.
    storage.Add(e, Health{75, 100});
    EXPECT_TRUE(storage.Has(e));
    EXPECT_EQ(storage.Get(e).current, 75);
}

// ===========================================================================
// ComponentStorage: Replace
// ===========================================================================

TEST(ComponentStorageTest, ReplaceUpdatesValue) {
    ComponentStorage<Position> storage;
    Entity e(1, 0);
    storage.Add(e, Position{1.0f, 2.0f, 3.0f});
    storage.Replace(e, Position{10.0f, 20.0f, 30.0f});

    EXPECT_FLOAT_EQ(storage.Get(e).x, 10.0f);
    EXPECT_FLOAT_EQ(storage.Get(e).y, 20.0f);
}

// ===========================================================================
// ComponentStorage: GetOrAdd
// ===========================================================================

TEST(ComponentStorageTest, GetOrAddExisting) {
    ComponentStorage<Health> storage;
    Entity e(1, 0);
    storage.Add(e, Health{42, 100});

    auto& h = storage.GetOrAdd(e);
    EXPECT_EQ(h.current, 42);
    EXPECT_EQ(storage.Size(), 1u);
}

TEST(ComponentStorageTest, GetOrAddNew) {
    ComponentStorage<Health> storage;
    Entity e(1, 0);

    auto& h = storage.GetOrAdd(e);
    EXPECT_EQ(h.current, 100);  // default
    EXPECT_EQ(storage.Size(), 1u);
}

// ===========================================================================
// ComponentStorage: Clear
// ===========================================================================

TEST(ComponentStorageTest, ClearRemovesAll) {
    ComponentStorage<Position> storage;
    for (uint32_t i = 0; i < 100; ++i) {
        storage.Add(Entity(i, 0), Position{});
    }
    EXPECT_EQ(storage.Size(), 100u);

    storage.Clear();
    EXPECT_EQ(storage.Size(), 0u);
    EXPECT_TRUE(storage.Empty());

    // Verify all entities removed.
    for (uint32_t i = 0; i < 100; ++i) {
        EXPECT_FALSE(storage.Has(Entity(i, 0)));
    }
}

// ===========================================================================
// ComponentStorage: Iteration
// ===========================================================================

TEST(ComponentStorageTest, IterateAll) {
    ComponentStorage<Position> storage;
    for (uint32_t i = 0; i < 5; ++i) {
        storage.Add(Entity(i, 0), Position{static_cast<float>(i), 0.0f, 0.0f});
    }

    float sum = 0.0f;
    for (const auto& pos : storage) {
        sum += pos.x;
    }
    EXPECT_FLOAT_EQ(sum, 0.0f + 1.0f + 2.0f + 3.0f + 4.0f);
}

TEST(ComponentStorageTest, IterateEmpty) {
    ComponentStorage<Position> storage;
    int count = 0;
    for ([[maybe_unused]] const auto& pos : storage) {
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST(ComponentStorageTest, EntityAtReturnsCorrectId) {
    ComponentStorage<Position> storage;
    Entity a(10, 0);
    Entity b(20, 0);
    storage.Add(a, Position{});
    storage.Add(b, Position{});

    EXPECT_EQ(storage.EntityAt(0), 10u);
    EXPECT_EQ(storage.EntityAt(1), 20u);
}

// ===========================================================================
// ComponentStorage: Version tracking
// ===========================================================================

TEST(ComponentStorageTest, VersionIncreasesOnAdd) {
    ComponentStorage<Position> storage;
    Entity a(1, 0);
    Entity b(2, 0);

    storage.Add(a, Position{});
    auto vA = storage.GetVersion(a);

    storage.Add(b, Position{});
    auto vB = storage.GetVersion(b);

    EXPECT_LT(vA, vB);
}

TEST(ComponentStorageTest, VersionIncreasesOnReplace) {
    ComponentStorage<Position> storage;
    Entity e(1, 0);
    storage.Add(e, Position{});
    auto v1 = storage.GetVersion(e);

    storage.Replace(e, Position{1.0f, 2.0f, 3.0f});
    auto v2 = storage.GetVersion(e);

    EXPECT_GT(v2, v1);
}

TEST(ComponentStorageTest, MarkChangedBumpsVersion) {
    ComponentStorage<Position> storage;
    Entity e(1, 0);
    storage.Add(e, Position{});
    auto v1 = storage.GetVersion(e);

    storage.MarkChanged(e);
    auto v2 = storage.GetVersion(e);

    EXPECT_GT(v2, v1);
}

TEST(ComponentStorageTest, HasChangedDetectsModification) {
    ComponentStorage<Position> storage;
    Entity e(1, 0);
    storage.Add(e, Position{});

    auto snapshot = storage.GlobalVersion();
    EXPECT_FALSE(storage.HasChanged(e, snapshot));

    storage.MarkChanged(e);
    EXPECT_TRUE(storage.HasChanged(e, snapshot));
}

TEST(ComponentStorageTest, GlobalVersionMonotonicallyIncreases) {
    ComponentStorage<Position> storage;
    auto v0 = storage.GlobalVersion();

    storage.Add(Entity(1, 0), Position{});
    auto v1 = storage.GlobalVersion();

    storage.Add(Entity(2, 0), Position{});
    auto v2 = storage.GlobalVersion();

    storage.Remove(Entity(1, 0));
    auto v3 = storage.GlobalVersion();

    EXPECT_LT(v0, v1);
    EXPECT_LT(v1, v2);
    EXPECT_LT(v2, v3);
}

// ===========================================================================
// ComponentStorage: Move-only types
// ===========================================================================

TEST(ComponentStorageTest, MoveOnlyAdd) {
    ComponentStorage<UniqueResource> storage;
    Entity e(1, 0);
    auto& res = storage.Add(e, UniqueResource{42});
    EXPECT_EQ(res.id, 42);
}

TEST(ComponentStorageTest, MoveOnlyRemoveSwap) {
    ComponentStorage<UniqueResource> storage;
    Entity a(1, 0);
    Entity b(2, 0);
    Entity c(3, 0);

    storage.Add(a, UniqueResource{10});
    storage.Add(b, UniqueResource{20});
    storage.Add(c, UniqueResource{30});

    storage.Remove(a);
    EXPECT_EQ(storage.Size(), 2u);
    EXPECT_EQ(storage.Get(b).id, 20);
    EXPECT_EQ(storage.Get(c).id, 30);
}

// ===========================================================================
// ComponentStorage: Tag (zero-size) components
// ===========================================================================

TEST(ComponentStorageTest, TagComponent) {
    ComponentStorage<DeadTag> storage;
    Entity e(1, 0);
    storage.Add(e);
    EXPECT_TRUE(storage.Has(e));
    storage.Remove(e);
    EXPECT_FALSE(storage.Has(e));
}

// ===========================================================================
// ComponentStorage: String component
// ===========================================================================

TEST(ComponentStorageTest, StringComponent) {
    ComponentStorage<Name> storage;
    Entity e(1, 0);
    storage.Add(e, Name{"Hello World"});
    EXPECT_EQ(storage.Get(e).value, "Hello World");

    storage.Replace(e, Name{"Replaced"});
    EXPECT_EQ(storage.Get(e).value, "Replaced");
}

// ===========================================================================
// ComponentStorage: IComponentStorage interface
// ===========================================================================

TEST(IComponentStorageTest, PolymorphicAccess) {
    auto storage = std::make_unique<ComponentStorage<Position>>();
    IComponentStorage* base = storage.get();

    Entity e(1, 0);
    storage->Add(e, Position{});

    EXPECT_TRUE(base->Has(e));
    EXPECT_EQ(base->Size(), 1u);

    base->Remove(e);
    EXPECT_FALSE(base->Has(e));

    storage->Add(e, Position{});
    base->Clear();
    EXPECT_EQ(base->Size(), 0u);
}

// ===========================================================================
// ComponentStorage: Sparse array growth
// ===========================================================================

TEST(ComponentStorageTest, SparseGrowsForLargeEntityId) {
    ComponentStorage<Position> storage;
    Entity e(10000, 0);
    storage.Add(e, Position{1.0f, 2.0f, 3.0f});
    EXPECT_TRUE(storage.Has(e));
    EXPECT_FLOAT_EQ(storage.Get(e).x, 1.0f);
    EXPECT_EQ(storage.Size(), 1u);
}

TEST(ComponentStorageTest, NonContiguousEntityIds) {
    ComponentStorage<Health> storage;
    Entity a(5, 0);
    Entity b(500, 0);
    Entity c(50000, 0);

    storage.Add(a, Health{10, 100});
    storage.Add(b, Health{20, 100});
    storage.Add(c, Health{30, 100});

    EXPECT_EQ(storage.Size(), 3u);
    EXPECT_EQ(storage.Get(a).current, 10);
    EXPECT_EQ(storage.Get(b).current, 20);
    EXPECT_EQ(storage.Get(c).current, 30);
}

// ===========================================================================
// ComponentStorage: TypeId
// ===========================================================================

TEST(ComponentStorageTest, TypeIdMatchesComponentType) {
    EXPECT_EQ(ComponentStorage<Position>::TypeId(), ComponentType<Position>::Id());
    EXPECT_EQ(ComponentStorage<Health>::TypeId(), ComponentType<Health>::Id());
    EXPECT_NE(ComponentStorage<Position>::TypeId(), ComponentStorage<Health>::TypeId());
}

// ===========================================================================
// Performance: Dense iteration vs unordered_map
// ===========================================================================

TEST(ComponentStoragePerformanceTest, DenseIterationFasterThanMap) {
    constexpr std::size_t N = 10'000;

    // Populate ComponentStorage.
    ComponentStorage<Position> storage;
    for (uint32_t i = 0; i < N; ++i) {
        storage.Add(Entity(i, 0), Position{static_cast<float>(i), 0.0f, 0.0f});
    }

    // Populate unordered_map.
    std::unordered_map<uint32_t, Position> map;
    map.reserve(N);
    for (uint32_t i = 0; i < N; ++i) {
        map[i] = Position{static_cast<float>(i), 0.0f, 0.0f};
    }

    // Benchmark: sum x values via dense iteration.
    constexpr int kRuns = 100;

    auto storageStart = std::chrono::steady_clock::now();
    volatile float storageSink = 0.0f;
    for (int run = 0; run < kRuns; ++run) {
        float sum = 0.0f;
        for (const auto& pos : storage) {
            sum += pos.x;
        }
        storageSink = sum;
    }
    auto storageElapsed = std::chrono::steady_clock::now() - storageStart;

    // Benchmark: sum x values via map iteration.
    auto mapStart = std::chrono::steady_clock::now();
    volatile float mapSink = 0.0f;
    for (int run = 0; run < kRuns; ++run) {
        float sum = 0.0f;
        for (const auto& [id, pos] : map) {
            sum += pos.x;
        }
        mapSink = sum;
    }
    auto mapElapsed = std::chrono::steady_clock::now() - mapStart;

    // Suppress unused-but-set-variable warnings.
    (void)storageSink;
    (void)mapSink;

    auto storageUs = std::chrono::duration_cast<std::chrono::microseconds>(storageElapsed).count();
    auto mapUs = std::chrono::duration_cast<std::chrono::microseconds>(mapElapsed).count();

    // ComponentStorage should be faster due to cache-friendly dense layout.
    EXPECT_LT(storageUs, mapUs)
        << "ComponentStorage (" << storageUs << "us) should be faster than "
        << "unordered_map (" << mapUs << "us) for 10K iteration x " << kRuns << " runs";
}
