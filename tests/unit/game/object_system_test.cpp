#include <gtest/gtest.h>

#include <cmath>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/ecs/query.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/components.hpp"
#include "cgs/game/math_types.hpp"
#include "cgs/game/object_system.hpp"
#include "cgs/game/object_types.hpp"

using namespace cgs::ecs;
using namespace cgs::game;

// ═══════════════════════════════════════════════════════════════════════════
// Vector3 tests
// ═══════════════════════════════════════════════════════════════════════════

TEST(Vector3Test, DefaultIsZero) {
    Vector3 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vector3Test, ConstructorSetsValues) {
    Vector3 v(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST(Vector3Test, Addition) {
    Vector3 a(1.0f, 2.0f, 3.0f);
    Vector3 b(4.0f, 5.0f, 6.0f);
    auto c = a + b;
    EXPECT_FLOAT_EQ(c.x, 5.0f);
    EXPECT_FLOAT_EQ(c.y, 7.0f);
    EXPECT_FLOAT_EQ(c.z, 9.0f);
}

TEST(Vector3Test, ScalarMultiplication) {
    Vector3 v(1.0f, 2.0f, 3.0f);
    auto scaled = v * 2.0f;
    EXPECT_FLOAT_EQ(scaled.x, 2.0f);
    EXPECT_FLOAT_EQ(scaled.y, 4.0f);
    EXPECT_FLOAT_EQ(scaled.z, 6.0f);

    // Left-multiply scalar.
    auto left = 3.0f * v;
    EXPECT_FLOAT_EQ(left.x, 3.0f);
    EXPECT_FLOAT_EQ(left.y, 6.0f);
    EXPECT_FLOAT_EQ(left.z, 9.0f);
}

TEST(Vector3Test, DotProduct) {
    Vector3 a(1.0f, 0.0f, 0.0f);
    Vector3 b(0.0f, 1.0f, 0.0f);
    EXPECT_FLOAT_EQ(a.Dot(b), 0.0f);

    Vector3 c(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(c.Dot(c), 14.0f);
}

TEST(Vector3Test, LengthAndNormalize) {
    Vector3 v(3.0f, 4.0f, 0.0f);
    EXPECT_FLOAT_EQ(v.Length(), 5.0f);

    auto n = v.Normalized();
    EXPECT_NEAR(n.Length(), 1.0f, 1e-5f);
    EXPECT_FLOAT_EQ(n.x, 0.6f);
    EXPECT_FLOAT_EQ(n.y, 0.8f);
}

TEST(Vector3Test, NormalizeZeroVector) {
    Vector3 zero;
    auto n = zero.Normalized();
    EXPECT_FLOAT_EQ(n.x, 0.0f);
    EXPECT_FLOAT_EQ(n.y, 0.0f);
    EXPECT_FLOAT_EQ(n.z, 0.0f);
}

TEST(Vector3Test, PlusEqualsOperator) {
    Vector3 v(1.0f, 2.0f, 3.0f);
    v += Vector3(10.0f, 20.0f, 30.0f);
    EXPECT_FLOAT_EQ(v.x, 11.0f);
    EXPECT_FLOAT_EQ(v.y, 22.0f);
    EXPECT_FLOAT_EQ(v.z, 33.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
// Quaternion tests
// ═══════════════════════════════════════════════════════════════════════════

TEST(QuaternionTest, DefaultIsIdentity) {
    Quaternion q;
    EXPECT_FLOAT_EQ(q.w, 1.0f);
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
}

TEST(QuaternionTest, IdentityFactory) {
    auto q = Quaternion::Identity();
    EXPECT_FLOAT_EQ(q.w, 1.0f);
    EXPECT_FLOAT_EQ(q.x, 0.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
// Transform component tests (SRS-GML-001.1)
// ═══════════════════════════════════════════════════════════════════════════

TEST(TransformTest, DefaultValues) {
    Transform t;
    EXPECT_FLOAT_EQ(t.position.x, 0.0f);
    EXPECT_FLOAT_EQ(t.position.y, 0.0f);
    EXPECT_FLOAT_EQ(t.position.z, 0.0f);
    EXPECT_FLOAT_EQ(t.rotation.w, 1.0f);  // Identity quaternion.
    EXPECT_FLOAT_EQ(t.scale.x, 1.0f);
    EXPECT_FLOAT_EQ(t.scale.y, 1.0f);
    EXPECT_FLOAT_EQ(t.scale.z, 1.0f);
}

TEST(TransformTest, StorageRoundTrip) {
    ComponentStorage<Transform> storage;
    Entity e(0, 0);

    auto& t = storage.Add(e, Transform{{10.0f, 20.0f, 30.0f},
                                        Quaternion{0.0f, 0.0f, 0.707f, 0.707f},
                                        {2.0f, 2.0f, 2.0f}});
    EXPECT_FLOAT_EQ(t.position.x, 10.0f);
    EXPECT_FLOAT_EQ(t.rotation.z, 0.707f);
    EXPECT_FLOAT_EQ(t.scale.x, 2.0f);

    const auto& ref = storage.Get(e);
    EXPECT_FLOAT_EQ(ref.position.y, 20.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
// Identity component tests (SRS-GML-001.2)
// ═══════════════════════════════════════════════════════════════════════════

TEST(IdentityTest, DefaultValues) {
    Identity id;
    EXPECT_EQ(id.guid, kInvalidGUID);
    EXPECT_TRUE(id.name.empty());
    EXPECT_EQ(id.type, ObjectType::GameObject);
    EXPECT_EQ(id.entry, 0u);
}

TEST(IdentityTest, GuidGeneration) {
    auto g1 = GenerateGUID();
    auto g2 = GenerateGUID();
    EXPECT_NE(g1, g2);
    EXPECT_NE(g1, kInvalidGUID);
    EXPECT_NE(g2, kInvalidGUID);
}

TEST(IdentityTest, ObjectTypeEnum) {
    Identity player;
    player.type = ObjectType::Player;
    player.guid = GenerateGUID();
    player.name = "Hero";
    player.entry = 42;

    EXPECT_EQ(player.type, ObjectType::Player);
    EXPECT_EQ(player.name, "Hero");
    EXPECT_EQ(player.entry, 42u);
}

TEST(IdentityTest, StorageRoundTrip) {
    ComponentStorage<Identity> storage;
    Entity e(0, 0);

    auto guid = GenerateGUID();
    storage.Add(e, Identity{guid, "TestNPC", ObjectType::Creature, 100});

    const auto& id = storage.Get(e);
    EXPECT_EQ(id.guid, guid);
    EXPECT_EQ(id.name, "TestNPC");
    EXPECT_EQ(id.type, ObjectType::Creature);
    EXPECT_EQ(id.entry, 100u);
}

// ═══════════════════════════════════════════════════════════════════════════
// Stats component tests (SRS-GML-001.3)
// ═══════════════════════════════════════════════════════════════════════════

TEST(StatsTest, DefaultValues) {
    Stats s;
    EXPECT_EQ(s.health, 0);
    EXPECT_EQ(s.maxHealth, 0);
    EXPECT_EQ(s.mana, 0);
    EXPECT_EQ(s.maxMana, 0);
    for (auto attr : s.attributes) {
        EXPECT_EQ(attr, 0);
    }
}

TEST(StatsTest, SetHealthClampsToMax) {
    Stats s;
    s.maxHealth = 100;
    s.SetHealth(150);
    EXPECT_EQ(s.health, 100);
}

TEST(StatsTest, SetHealthClampsToZero) {
    Stats s;
    s.maxHealth = 100;
    s.SetHealth(-50);
    EXPECT_EQ(s.health, 0);
}

TEST(StatsTest, SetHealthNormalRange) {
    Stats s;
    s.maxHealth = 100;
    s.SetHealth(75);
    EXPECT_EQ(s.health, 75);
}

TEST(StatsTest, SetManaClampsToMax) {
    Stats s;
    s.maxMana = 200;
    s.SetMana(300);
    EXPECT_EQ(s.mana, 200);
}

TEST(StatsTest, SetManaClampsToZero) {
    Stats s;
    s.maxMana = 200;
    s.SetMana(-10);
    EXPECT_EQ(s.mana, 0);
}

TEST(StatsTest, AttributeArray) {
    Stats s;
    s.attributes[0] = 10;  // Strength
    s.attributes[1] = 15;  // Agility
    s.attributes[2] = 20;  // Intellect

    EXPECT_EQ(s.attributes[0], 10);
    EXPECT_EQ(s.attributes[1], 15);
    EXPECT_EQ(s.attributes[2], 20);
    EXPECT_EQ(s.attributes[3], 0);  // Default unset.
}

TEST(StatsTest, StorageRoundTrip) {
    ComponentStorage<Stats> storage;
    Entity e(0, 0);

    Stats initial;
    initial.maxHealth = 100;
    initial.maxMana = 50;
    initial.SetHealth(80);
    initial.SetMana(30);
    initial.attributes[0] = 25;

    storage.Add(e, std::move(initial));

    const auto& s = storage.Get(e);
    EXPECT_EQ(s.health, 80);
    EXPECT_EQ(s.maxHealth, 100);
    EXPECT_EQ(s.mana, 30);
    EXPECT_EQ(s.maxMana, 50);
    EXPECT_EQ(s.attributes[0], 25);
}

// ═══════════════════════════════════════════════════════════════════════════
// Movement component tests (SRS-GML-001.4)
// ═══════════════════════════════════════════════════════════════════════════

TEST(MovementTest, DefaultValues) {
    Movement m;
    EXPECT_FLOAT_EQ(m.speed, 0.0f);
    EXPECT_FLOAT_EQ(m.baseSpeed, 0.0f);
    EXPECT_EQ(m.state, MovementState::Idle);
    EXPECT_FLOAT_EQ(m.direction.x, 0.0f);
}

TEST(MovementTest, SpeedModifier) {
    Movement m;
    m.baseSpeed = 10.0f;
    m.speed = 10.0f;

    m.ApplySpeedModifier(1.5f);
    EXPECT_FLOAT_EQ(m.speed, 15.0f);

    m.ResetSpeed();
    EXPECT_FLOAT_EQ(m.speed, 10.0f);
}

TEST(MovementTest, StateTransitions) {
    Movement m;
    EXPECT_EQ(m.state, MovementState::Idle);

    m.state = MovementState::Walking;
    EXPECT_EQ(m.state, MovementState::Walking);

    m.state = MovementState::Running;
    EXPECT_EQ(m.state, MovementState::Running);

    m.state = MovementState::Falling;
    EXPECT_EQ(m.state, MovementState::Falling);
}

TEST(MovementTest, StorageRoundTrip) {
    ComponentStorage<Movement> storage;
    Entity e(0, 0);

    storage.Add(e, Movement{5.0f, 5.0f, {1.0f, 0.0f, 0.0f}, MovementState::Running});

    const auto& m = storage.Get(e);
    EXPECT_FLOAT_EQ(m.speed, 5.0f);
    EXPECT_FLOAT_EQ(m.baseSpeed, 5.0f);
    EXPECT_FLOAT_EQ(m.direction.x, 1.0f);
    EXPECT_EQ(m.state, MovementState::Running);
}

// ═══════════════════════════════════════════════════════════════════════════
// ObjectUpdateSystem tests (SRS-GML-001.5)
// ═══════════════════════════════════════════════════════════════════════════

class ObjectUpdateSystemTest : public ::testing::Test {
protected:
    ComponentStorage<Transform> transforms;
    ComponentStorage<Movement> movements;
};

TEST_F(ObjectUpdateSystemTest, UpdatesPositionBasedOnMovement) {
    Entity e(0, 0);
    transforms.Add(e, Transform{{0.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    movements.Add(e, Movement{10.0f, 10.0f, {1.0f, 0.0f, 0.0f}, MovementState::Running});

    ObjectUpdateSystem system(transforms, movements);
    system.Execute(0.1f);  // 100ms tick.

    const auto& t = transforms.Get(e);
    EXPECT_NEAR(t.position.x, 1.0f, 1e-5f);  // 10 * 1.0 * 0.1
    EXPECT_NEAR(t.position.y, 0.0f, 1e-5f);
    EXPECT_NEAR(t.position.z, 0.0f, 1e-5f);
}

TEST_F(ObjectUpdateSystemTest, SkipsIdleEntities) {
    Entity e(0, 0);
    transforms.Add(e, Transform{{5.0f, 5.0f, 5.0f}, {}, {1.0f, 1.0f, 1.0f}});
    movements.Add(e, Movement{10.0f, 10.0f, {1.0f, 0.0f, 0.0f}, MovementState::Idle});

    ObjectUpdateSystem system(transforms, movements);
    system.Execute(1.0f);

    const auto& t = transforms.Get(e);
    EXPECT_FLOAT_EQ(t.position.x, 5.0f);  // Unchanged.
    EXPECT_FLOAT_EQ(t.position.y, 5.0f);
    EXPECT_FLOAT_EQ(t.position.z, 5.0f);
}

TEST_F(ObjectUpdateSystemTest, UpdatesMultipleEntities) {
    Entity e1(0, 0);
    Entity e2(1, 0);

    transforms.Add(e1, Transform{{0.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    movements.Add(e1, Movement{5.0f, 5.0f, {0.0f, 1.0f, 0.0f}, MovementState::Walking});

    transforms.Add(e2, Transform{{10.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    movements.Add(e2, Movement{20.0f, 20.0f, {-1.0f, 0.0f, 0.0f}, MovementState::Running});

    ObjectUpdateSystem system(transforms, movements);
    system.Execute(0.5f);

    const auto& t1 = transforms.Get(e1);
    EXPECT_NEAR(t1.position.x, 0.0f, 1e-5f);
    EXPECT_NEAR(t1.position.y, 2.5f, 1e-5f);  // 5 * 1.0 * 0.5

    const auto& t2 = transforms.Get(e2);
    EXPECT_NEAR(t2.position.x, 0.0f, 1e-5f);  // 10 + 20*(-1)*0.5 = 0
}

TEST_F(ObjectUpdateSystemTest, OnlyAffectsEntitiesWithBothComponents) {
    Entity withBoth(0, 0);
    Entity onlyTransform(1, 0);

    transforms.Add(withBoth, Transform{{0.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    movements.Add(withBoth, Movement{10.0f, 10.0f, {1.0f, 0.0f, 0.0f}, MovementState::Walking});

    transforms.Add(onlyTransform, Transform{{100.0f, 100.0f, 100.0f}, {}, {1.0f, 1.0f, 1.0f}});

    ObjectUpdateSystem system(transforms, movements);
    system.Execute(1.0f);

    const auto& t1 = transforms.Get(withBoth);
    EXPECT_NEAR(t1.position.x, 10.0f, 1e-5f);

    // Entity with only Transform should be untouched.
    const auto& t2 = transforms.Get(onlyTransform);
    EXPECT_FLOAT_EQ(t2.position.x, 100.0f);
}

TEST_F(ObjectUpdateSystemTest, ZeroDeltaTimeNoChange) {
    Entity e(0, 0);
    transforms.Add(e, Transform{{5.0f, 5.0f, 5.0f}, {}, {1.0f, 1.0f, 1.0f}});
    movements.Add(e, Movement{10.0f, 10.0f, {1.0f, 0.0f, 0.0f}, MovementState::Running});

    ObjectUpdateSystem system(transforms, movements);
    system.Execute(0.0f);

    const auto& t = transforms.Get(e);
    EXPECT_FLOAT_EQ(t.position.x, 5.0f);
}

TEST_F(ObjectUpdateSystemTest, SystemMetadata) {
    ObjectUpdateSystem system(transforms, movements);

    EXPECT_EQ(system.GetName(), "ObjectUpdateSystem");
    EXPECT_EQ(system.GetStage(), SystemStage::Update);

    auto access = system.GetAccessInfo();
    EXPECT_FALSE(access.reads.empty());
    EXPECT_FALSE(access.writes.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// Integration test: full entity lifecycle with all components
// ═══════════════════════════════════════════════════════════════════════════

TEST(ObjectSystemIntegration, FullEntityLifecycle) {
    // Create storages for all four components.
    ComponentStorage<Transform> transformStorage;
    ComponentStorage<Identity> identityStorage;
    ComponentStorage<Stats> statsStorage;
    ComponentStorage<Movement> movementStorage;

    // Create an entity manager and register storages.
    EntityManager entityManager;
    entityManager.RegisterStorage(&transformStorage);
    entityManager.RegisterStorage(&identityStorage);
    entityManager.RegisterStorage(&statsStorage);
    entityManager.RegisterStorage(&movementStorage);

    // Create a player entity with all four components.
    Entity player = entityManager.Create();
    ASSERT_TRUE(player.isValid());

    transformStorage.Add(player, Transform{{0.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    identityStorage.Add(player, Identity{GenerateGUID(), "Player1", ObjectType::Player, 1});
    statsStorage.Add(player, Stats{100, 100, 50, 50, {}});
    movementStorage.Add(player, Movement{7.0f, 7.0f, {0.0f, 0.0f, 1.0f}, MovementState::Running});

    // Verify components are attached.
    EXPECT_TRUE(transformStorage.Has(player));
    EXPECT_TRUE(identityStorage.Has(player));
    EXPECT_TRUE(statsStorage.Has(player));
    EXPECT_TRUE(movementStorage.Has(player));

    // Run the update system for several ticks.
    ObjectUpdateSystem updateSystem(transformStorage, movementStorage);

    constexpr float dt = 1.0f / 60.0f;
    for (int tick = 0; tick < 60; ++tick) {
        updateSystem.Execute(dt);
    }

    // After 60 ticks at dt=1/60, roughly 1 second elapsed.
    // Position should have moved ~7 units along +Z.
    const auto& transform = transformStorage.Get(player);
    EXPECT_NEAR(transform.position.z, 7.0f, 0.05f);
    EXPECT_NEAR(transform.position.x, 0.0f, 1e-5f);

    // Stats and Identity should be unchanged.
    const auto& identity = identityStorage.Get(player);
    EXPECT_EQ(identity.name, "Player1");
    EXPECT_EQ(identity.type, ObjectType::Player);

    const auto& stats = statsStorage.Get(player);
    EXPECT_EQ(stats.health, 100);
    EXPECT_EQ(stats.mana, 50);

    // Destroy the entity — all components should be removed.
    entityManager.Destroy(player);
    EXPECT_FALSE(entityManager.IsAlive(player));
    EXPECT_FALSE(transformStorage.Has(player));
    EXPECT_FALSE(identityStorage.Has(player));
    EXPECT_FALSE(statsStorage.Has(player));
    EXPECT_FALSE(movementStorage.Has(player));
}

TEST(ObjectSystemIntegration, SchedulerRegistration) {
    ComponentStorage<Transform> transforms;
    ComponentStorage<Movement> movements;

    SystemScheduler scheduler;
    auto& system = scheduler.Register<ObjectUpdateSystem>(transforms, movements);

    EXPECT_EQ(system.GetName(), "ObjectUpdateSystem");
    EXPECT_EQ(scheduler.SystemCount(), 1u);
    EXPECT_TRUE(scheduler.Build());

    // Execute with no entities (should not crash).
    scheduler.Execute(1.0f / 60.0f);
}
