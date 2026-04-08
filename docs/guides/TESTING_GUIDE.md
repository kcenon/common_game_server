# Testing Strategy Reference

> Version: 0.1.0.0
> Last Updated: 2026-02-03
> Status: Foundation Reference

## Overview

This document defines the testing strategy for the unified game server, including unit testing, integration testing, performance testing, and game-specific testing patterns.

---

## Table of Contents

1. [Testing Philosophy](#testing-philosophy)
2. [Test Architecture](#test-architecture)
3. [Unit Testing](#unit-testing)
4. [Integration Testing](#integration-testing)
5. [Performance Testing](#performance-testing)
6. [Game Logic Testing](#game-logic-testing)
7. [Network Testing](#network-testing)
8. [Test Fixtures & Mocks](#test-fixtures--mocks)
9. [CI/CD Integration](#cicd-integration)
10. [Test Coverage](#test-coverage)

---

## Testing Philosophy

### Testing Pyramid

```
                    ┌──────────┐
                   │   E2E    │  Few, expensive, slow
                  │  Tests   │  10% of tests
                 └──────────┘
               ┌──────────────┐
              │  Integration  │  More, moderate
             │    Tests      │  20% of tests
            └──────────────┘
         ┌────────────────────┐
        │     Unit Tests      │  Many, cheap, fast
       │                      │  70% of tests
      └────────────────────────┘
```

### Testing Principles

| Principle | Description |
|-----------|-------------|
| Fast | Unit tests < 1ms, integration < 1s |
| Isolated | Tests don't depend on each other |
| Repeatable | Same result every time |
| Self-validating | Pass or fail, no manual inspection |
| Thorough | Cover edge cases and error paths |

### Game Server Specific Considerations

```cpp
// Game servers have unique testing challenges:

// 1. Time-dependent logic
class TimeBasedTest {
    // Use mockable time source
    MockClock clock_;
    void testBuffExpiration() {
        clock_.setTime(1000);
        applyBuff();
        clock_.advance(30000);  // 30 seconds
        EXPECT_TRUE(isBuffExpired());
    }
};

// 2. Random-dependent logic
class RandomBasedTest {
    // Use seeded random for deterministic tests
    DeterministicRandom rng_(42);
    void testCriticalHit() {
        rng_.setSeed(CRIT_SEED);  // Known to produce crit
        EXPECT_TRUE(calculateDamage().isCritical);
    }
};

// 3. Concurrent operations
class ConcurrencyTest {
    // Use synchronization primitives
    void testPlayerMovement() {
        std::latch done(2);
        // Simulate concurrent updates
    }
};
```

---

## Test Architecture

### Directory Structure

```
tests/
├── unit/                          # Unit tests
│   ├── core/                      # Core system tests
│   │   ├── entity_manager_test.cpp
│   │   ├── component_pool_test.cpp
│   │   └── system_scheduler_test.cpp
│   ├── ecs/                       # ECS tests
│   │   ├── world_test.cpp
│   │   └── query_test.cpp
│   ├── game/                      # Game logic tests
│   │   ├── combat_test.cpp
│   │   ├── movement_test.cpp
│   │   └── inventory_test.cpp
│   └── network/                   # Network tests
│       ├── packet_test.cpp
│       └── serialization_test.cpp
│
├── integration/                   # Integration tests
│   ├── database/
│   │   ├── character_repository_test.cpp
│   │   └── inventory_repository_test.cpp
│   ├── network/
│   │   ├── client_server_test.cpp
│   │   └── protocol_test.cpp
│   └── systems/
│       └── combat_integration_test.cpp
│
├── performance/                   # Performance benchmarks
│   ├── ecs_benchmark.cpp
│   ├── network_benchmark.cpp
│   └── database_benchmark.cpp
│
├── e2e/                          # End-to-end tests
│   ├── login_flow_test.cpp
│   ├── combat_flow_test.cpp
│   └── trading_flow_test.cpp
│
├── fixtures/                      # Test data
│   ├── characters.json
│   ├── items.json
│   └── maps.json
│
├── mocks/                        # Mock implementations
│   ├── mock_database.h
│   ├── mock_network.h
│   └── mock_time.h
│
└── helpers/                      # Test utilities
    ├── test_server.h
    ├── test_client.h
    └── assertions.h
```

### Test Framework Setup

```cpp
// tests/test_main.cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Global test environment
class GameTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Initialize logging
        Logger::setLevel(LogLevel::Error);

        // Load test configurations
        Config::load("tests/config/test.yaml");

        // Initialize mock time
        MockClock::install();
    }

    void TearDown() override {
        MockClock::uninstall();
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new GameTestEnvironment);
    return RUN_ALL_TESTS();
}
```

### CMake Configuration

```cmake
# tests/CMakeLists.txt

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Unit tests
add_executable(unit_tests
    test_main.cpp
    unit/core/entity_manager_test.cpp
    unit/core/component_pool_test.cpp
    unit/ecs/world_test.cpp
    unit/game/combat_test.cpp
    unit/game/movement_test.cpp
    unit/network/packet_test.cpp
)

target_link_libraries(unit_tests
    game_server_lib
    GTest::gtest
    GTest::gmock
)

# Integration tests
add_executable(integration_tests
    test_main.cpp
    integration/database/character_repository_test.cpp
    integration/network/client_server_test.cpp
)

target_link_libraries(integration_tests
    game_server_lib
    GTest::gtest
    GTest::gmock
)

# Performance benchmarks
add_executable(benchmarks
    performance/ecs_benchmark.cpp
    performance/network_benchmark.cpp
)

target_link_libraries(benchmarks
    game_server_lib
    benchmark::benchmark
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(unit_tests)
gtest_discover_tests(integration_tests)

# Custom test targets
add_custom_target(test-unit
    COMMAND unit_tests
    DEPENDS unit_tests
)

add_custom_target(test-integration
    COMMAND integration_tests
    DEPENDS integration_tests
)

add_custom_target(test-all
    COMMAND unit_tests && integration_tests
    DEPENDS unit_tests integration_tests
)
```

---

## Unit Testing

### ECS Component Testing

```cpp
// tests/unit/ecs/component_pool_test.cpp
#include <gtest/gtest.h>
#include "ecs/component_pool.h"

class ComponentPoolTest : public ::testing::Test {
protected:
    ComponentPool<PositionComponent> pool_;

    void SetUp() override {
        pool_.reserve(1000);
    }
};

TEST_F(ComponentPoolTest, AddComponent_ReturnsValidReference) {
    Entity entity{1};
    auto& component = pool_.add(entity, 10.0f, 20.0f, 30.0f);

    EXPECT_FLOAT_EQ(component.x, 10.0f);
    EXPECT_FLOAT_EQ(component.y, 20.0f);
    EXPECT_FLOAT_EQ(component.z, 30.0f);
}

TEST_F(ComponentPoolTest, GetComponent_ReturnsCorrectComponent) {
    Entity entity{42};
    pool_.add(entity, 1.0f, 2.0f, 3.0f);

    auto& component = pool_.get(entity);

    EXPECT_FLOAT_EQ(component.x, 1.0f);
}

TEST_F(ComponentPoolTest, HasComponent_ReturnsTrueForExisting) {
    Entity entity{100};
    pool_.add(entity, 0.0f, 0.0f, 0.0f);

    EXPECT_TRUE(pool_.has(entity));
}

TEST_F(ComponentPoolTest, HasComponent_ReturnsFalseForNonExisting) {
    Entity entity{999};

    EXPECT_FALSE(pool_.has(entity));
}

TEST_F(ComponentPoolTest, RemoveComponent_RemovesSuccessfully) {
    Entity entity{50};
    pool_.add(entity, 5.0f, 5.0f, 5.0f);

    pool_.remove(entity);

    EXPECT_FALSE(pool_.has(entity));
}

TEST_F(ComponentPoolTest, ManyComponents_MaintainsIntegrity) {
    constexpr int COUNT = 10000;

    for (int i = 0; i < COUNT; ++i) {
        pool_.add(Entity{static_cast<uint64_t>(i)},
                  static_cast<float>(i),
                  static_cast<float>(i * 2),
                  static_cast<float>(i * 3));
    }

    EXPECT_EQ(pool_.size(), COUNT);

    for (int i = 0; i < COUNT; ++i) {
        auto& comp = pool_.get(Entity{static_cast<uint64_t>(i)});
        EXPECT_FLOAT_EQ(comp.x, static_cast<float>(i));
    }
}
```

### Combat System Testing

```cpp
// tests/unit/game/combat_test.cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "game/combat_system.h"
#include "mocks/mock_random.h"

class CombatSystemTest : public ::testing::Test {
protected:
    std::unique_ptr<CombatSystem> combatSystem_;
    MockRandom mockRandom_;

    void SetUp() override {
        combatSystem_ = std::make_unique<CombatSystem>(&mockRandom_);
    }

    CombatComponent createAttacker(int attack = 100) {
        return CombatComponent{
            .attackPower = attack,
            .criticalRate = 0.1f,
            .criticalDamage = 1.5f
        };
    }

    CombatComponent createDefender(int defense = 50, int hp = 1000) {
        return CombatComponent{
            .defense = defense,
            .currentHp = hp,
            .maxHp = hp
        };
    }
};

TEST_F(CombatSystemTest, CalculateDamage_BasicDamage) {
    auto attacker = createAttacker(100);
    auto defender = createDefender(0);

    EXPECT_CALL(mockRandom_, nextFloat())
        .WillOnce(::testing::Return(0.5f));  // No crit

    auto result = combatSystem_->calculateDamage(attacker, defender, 0);

    EXPECT_EQ(result.damage, 100);
    EXPECT_FALSE(result.isCritical);
}

TEST_F(CombatSystemTest, CalculateDamage_DefenseReducesDamage) {
    auto attacker = createAttacker(100);
    auto defender = createDefender(50);

    EXPECT_CALL(mockRandom_, nextFloat())
        .WillOnce(::testing::Return(0.5f));

    auto result = combatSystem_->calculateDamage(attacker, defender, 0);

    EXPECT_LT(result.damage, 100);  // Damage should be reduced
    EXPECT_GT(result.damage, 0);    // But still positive
}

TEST_F(CombatSystemTest, CalculateDamage_CriticalHit) {
    auto attacker = createAttacker(100);
    attacker.criticalRate = 0.5f;
    auto defender = createDefender(0);

    EXPECT_CALL(mockRandom_, nextFloat())
        .WillOnce(::testing::Return(0.1f));  // Below crit rate = crit

    auto result = combatSystem_->calculateDamage(attacker, defender, 0);

    EXPECT_TRUE(result.isCritical);
    EXPECT_EQ(result.damage, 150);  // 100 * 1.5 crit multiplier
}

TEST_F(CombatSystemTest, CalculateDamage_MinimumDamageIsOne) {
    auto attacker = createAttacker(1);
    auto defender = createDefender(9999);

    EXPECT_CALL(mockRandom_, nextFloat())
        .WillOnce(::testing::Return(0.5f));

    auto result = combatSystem_->calculateDamage(attacker, defender, 0);

    EXPECT_GE(result.damage, 1);  // Minimum damage
}

TEST_F(CombatSystemTest, ApplyDamage_ReducesHp) {
    auto defender = createDefender(0, 1000);

    combatSystem_->applyDamage(defender, 250);

    EXPECT_EQ(defender.currentHp, 750);
}

TEST_F(CombatSystemTest, ApplyDamage_HpNeverNegative) {
    auto defender = createDefender(0, 100);

    combatSystem_->applyDamage(defender, 500);

    EXPECT_EQ(defender.currentHp, 0);
    EXPECT_TRUE(defender.isDead);
}

TEST_F(CombatSystemTest, SkillDamage_AppliesMultiplier) {
    auto attacker = createAttacker(100);
    auto defender = createDefender(0);

    SkillData skill{
        .damageMultiplier = 2.0f,
        .baseAdditionalDamage = 50
    };

    EXPECT_CALL(mockRandom_, nextFloat())
        .WillOnce(::testing::Return(0.5f));

    auto result = combatSystem_->calculateSkillDamage(
        attacker, defender, skill
    );

    EXPECT_EQ(result.damage, 250);  // 100 * 2 + 50
}
```

### Inventory System Testing

```cpp
// tests/unit/game/inventory_test.cpp
#include <gtest/gtest.h>
#include "game/inventory_system.h"

class InventorySystemTest : public ::testing::Test {
protected:
    InventoryComponent inventory_;

    void SetUp() override {
        inventory_.slots.resize(100);
        inventory_.maxSlots = 100;
    }

    ItemInstance createItem(uint32_t id, int quantity = 1) {
        return ItemInstance{
            .itemId = id,
            .quantity = quantity,
            .instanceData = {}
        };
    }
};

TEST_F(InventorySystemTest, AddItem_ToEmptySlot) {
    auto item = createItem(1001, 10);

    auto result = InventorySystem::addItem(inventory_, item);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.slot, 0);
    EXPECT_EQ(inventory_.slots[0].itemId, 1001);
    EXPECT_EQ(inventory_.slots[0].quantity, 10);
}

TEST_F(InventorySystemTest, AddItem_StacksWithExisting) {
    inventory_.slots[5] = createItem(1001, 50);

    auto item = createItem(1001, 30);
    auto result = InventorySystem::addItem(inventory_, item, 999);  // Max stack 999

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.slot, 5);
    EXPECT_EQ(inventory_.slots[5].quantity, 80);
}

TEST_F(InventorySystemTest, AddItem_SplitsOverflow) {
    inventory_.slots[0] = createItem(1001, 990);

    auto item = createItem(1001, 20);
    auto result = InventorySystem::addItem(inventory_, item, 999);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(inventory_.slots[0].quantity, 999);  // Filled to max
    EXPECT_EQ(inventory_.slots[1].quantity, 11);   // Overflow
}

TEST_F(InventorySystemTest, AddItem_FailsWhenFull) {
    // Fill all slots with non-stackable items
    for (int i = 0; i < 100; ++i) {
        inventory_.slots[i] = createItem(i + 1000, 1);
    }

    auto item = createItem(9999, 1);
    auto result = InventorySystem::addItem(inventory_, item);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, InventoryError::NoSpace);
}

TEST_F(InventorySystemTest, RemoveItem_BySlot) {
    inventory_.slots[10] = createItem(1001, 50);

    auto result = InventorySystem::removeItem(inventory_, 10, 30);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(inventory_.slots[10].quantity, 20);
}

TEST_F(InventorySystemTest, RemoveItem_ClearsEmptySlot) {
    inventory_.slots[10] = createItem(1001, 50);

    auto result = InventorySystem::removeItem(inventory_, 10, 50);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(inventory_.slots[10].itemId, 0);
    EXPECT_EQ(inventory_.slots[10].quantity, 0);
}

TEST_F(InventorySystemTest, MoveItem_SwapsSlots) {
    inventory_.slots[0] = createItem(1001, 10);
    inventory_.slots[5] = createItem(1002, 20);

    auto result = InventorySystem::moveItem(inventory_, 0, 5);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(inventory_.slots[0].itemId, 1002);
    EXPECT_EQ(inventory_.slots[5].itemId, 1001);
}

TEST_F(InventorySystemTest, FindItem_ReturnsCorrectSlot) {
    inventory_.slots[42] = createItem(1234, 1);

    auto result = InventorySystem::findItem(inventory_, 1234);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST_F(InventorySystemTest, CountItem_SumsAllStacks) {
    inventory_.slots[0] = createItem(1001, 100);
    inventory_.slots[10] = createItem(1001, 200);
    inventory_.slots[50] = createItem(1001, 50);

    int count = InventorySystem::countItem(inventory_, 1001);

    EXPECT_EQ(count, 350);
}
```

---

## Integration Testing

### Database Integration Testing

```cpp
// tests/integration/database/character_repository_test.cpp
#include <gtest/gtest.h>
#include "database/character_repository.h"
#include "helpers/test_database.h"

class CharacterRepositoryTest : public ::testing::Test {
protected:
    std::unique_ptr<TestDatabase> testDb_;
    std::unique_ptr<CharacterRepository> repository_;

    void SetUp() override {
        // Create test database
        testDb_ = std::make_unique<TestDatabase>("test_game_db");
        testDb_->migrate();

        repository_ = std::make_unique<CharacterRepository>(
            testDb_->getConnection()
        );
    }

    void TearDown() override {
        testDb_->truncateAll();
    }

    uint64_t createTestAccount() {
        return testDb_->insert("accounts", {
            {"username", "testuser"},
            {"email", "test@example.com"},
            {"password_hash", "hash"},
            {"salt", "salt"}
        });
    }
};

TEST_F(CharacterRepositoryTest, Create_ReturnsCharacterId) {
    auto accountId = createTestAccount();

    CharacterCreateData data{
        .accountId = accountId,
        .name = "TestCharacter",
        .classId = 1,
        .gender = 0
    };

    auto result = repository_->create(data);

    ASSERT_TRUE(result.has_value());
    EXPECT_GT(*result, 0);
}

TEST_F(CharacterRepositoryTest, Create_FailsOnDuplicateName) {
    auto accountId = createTestAccount();

    CharacterCreateData data{
        .accountId = accountId,
        .name = "TestCharacter",
        .classId = 1,
        .gender = 0
    };

    repository_->create(data);

    // Try to create another with same name
    auto result = repository_->create(data);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::DuplicateName);
}

TEST_F(CharacterRepositoryTest, FindById_ReturnsCharacter) {
    auto accountId = createTestAccount();
    auto charId = repository_->create({
        .accountId = accountId,
        .name = "FindTest",
        .classId = 2,
        .gender = 1
    }).value();

    auto character = repository_->findById(charId);

    ASSERT_TRUE(character.has_value());
    EXPECT_EQ(character->name, "FindTest");
    EXPECT_EQ(character->classId, 2);
}

TEST_F(CharacterRepositoryTest, FindByAccountId_ReturnsAllCharacters) {
    auto accountId = createTestAccount();

    repository_->create({.accountId = accountId, .name = "Char1", .classId = 1});
    repository_->create({.accountId = accountId, .name = "Char2", .classId = 2});
    repository_->create({.accountId = accountId, .name = "Char3", .classId = 3});

    auto characters = repository_->findByAccountId(accountId);

    EXPECT_EQ(characters.size(), 3);
}

TEST_F(CharacterRepositoryTest, Update_SavesChanges) {
    auto accountId = createTestAccount();
    auto charId = repository_->create({
        .accountId = accountId,
        .name = "UpdateTest",
        .classId = 1
    }).value();

    // Load, modify, save
    auto character = repository_->findById(charId).value();
    character.level = 50;
    character.experience = 1000000;
    character.positionX = 100.0f;

    auto result = repository_->update(character);
    ASSERT_TRUE(result.has_value());

    // Reload and verify
    auto reloaded = repository_->findById(charId).value();
    EXPECT_EQ(reloaded.level, 50);
    EXPECT_EQ(reloaded.experience, 1000000);
    EXPECT_FLOAT_EQ(reloaded.positionX, 100.0f);
}

TEST_F(CharacterRepositoryTest, Delete_SoftDeletes) {
    auto accountId = createTestAccount();
    auto charId = repository_->create({
        .accountId = accountId,
        .name = "DeleteTest",
        .classId = 1
    }).value();

    auto result = repository_->deleteCharacter(charId);
    ASSERT_TRUE(result.has_value());

    // Should not be found normally
    auto character = repository_->findById(charId);
    EXPECT_FALSE(character.has_value());

    // But should exist as deleted
    auto deleted = repository_->findDeletedById(charId);
    EXPECT_TRUE(deleted.has_value());
}
```

### Client-Server Integration Testing

```cpp
// tests/integration/network/client_server_test.cpp
#include <gtest/gtest.h>
#include "helpers/test_server.h"
#include "helpers/test_client.h"

class ClientServerTest : public ::testing::Test {
protected:
    std::unique_ptr<TestServer> server_;
    std::unique_ptr<TestClient> client_;

    void SetUp() override {
        server_ = std::make_unique<TestServer>(0);  // Random port
        server_->start();

        client_ = std::make_unique<TestClient>();
        client_->connect("localhost", server_->port());
    }

    void TearDown() override {
        client_->disconnect();
        server_->stop();
    }
};

TEST_F(ClientServerTest, Connect_Succeeds) {
    EXPECT_TRUE(client_->isConnected());
}

TEST_F(ClientServerTest, Ping_ReceivesPong) {
    auto response = client_->sendAndWait<PongMessage>(
        PingMessage{.clientTime = 12345}
    );

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->clientTime, 12345);
    EXPECT_GT(response->serverTime, 0);
}

TEST_F(ClientServerTest, Login_ValidCredentials_Succeeds) {
    // Pre-create test account in database
    server_->createTestAccount("testuser", "password123");

    auto response = client_->sendAndWait<LoginResponse>(
        LoginRequest{
            .username = "testuser",
            .passwordHash = hashPassword("password123"),
            .clientVersion = "1.0.0"
        }
    );

    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response->success);
    EXPECT_FALSE(response->sessionToken.empty());
}

TEST_F(ClientServerTest, Login_InvalidCredentials_Fails) {
    server_->createTestAccount("testuser", "password123");

    auto response = client_->sendAndWait<LoginResponse>(
        LoginRequest{
            .username = "testuser",
            .passwordHash = hashPassword("wrongpassword"),
            .clientVersion = "1.0.0"
        }
    );

    ASSERT_TRUE(response.has_value());
    EXPECT_FALSE(response->success);
    EXPECT_EQ(response->errorCode, ErrorCode::InvalidCredentials);
}

TEST_F(ClientServerTest, CharacterList_ReturnsCharacters) {
    auto session = loginTestUser();

    auto response = client_->sendAndWait<CharacterListResponse>(
        CharacterListRequest{},
        session.token
    );

    ASSERT_TRUE(response.has_value());
    // New account has no characters
    EXPECT_EQ(response->characters.size(), 0);
}

TEST_F(ClientServerTest, Movement_BroadcastsToNearby) {
    // Setup: Two clients in same area
    auto client1 = createLoggedInClient("player1");
    auto client2 = createLoggedInClient("player2");

    // Enter same map
    enterMap(client1, 1, {100, 0, 100});
    enterMap(client2, 1, {110, 0, 110});  // Within view distance

    // Client1 moves
    client1->send(MoveStartMessage{
        .destination = {120, 0, 120},
        .speed = 5.0f
    });

    // Client2 should receive the movement
    auto received = client2->waitForMessage<PositionUpdate>(1000);

    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->entityId, client1->characterId());
}
```

---

## Performance Testing

### Benchmark Framework

```cpp
// tests/performance/ecs_benchmark.cpp
#include <benchmark/benchmark.h>
#include "ecs/world.h"

// Benchmark entity creation
static void BM_EntityCreation(benchmark::State& state) {
    World world;

    for (auto _ : state) {
        for (int i = 0; i < state.range(0); ++i) {
            world.createEntity();
        }
        state.PauseTiming();
        world.clear();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_EntityCreation)->Range(100, 100000);

// Benchmark component access
static void BM_ComponentAccess(benchmark::State& state) {
    World world;

    // Setup: Create entities with components
    std::vector<Entity> entities;
    for (int i = 0; i < state.range(0); ++i) {
        auto entity = world.createEntity();
        world.addComponent<PositionComponent>(entity, 0, 0, 0);
        world.addComponent<VelocityComponent>(entity, 1, 0, 0);
        entities.push_back(entity);
    }

    for (auto _ : state) {
        for (auto entity : entities) {
            auto& pos = world.getComponent<PositionComponent>(entity);
            auto& vel = world.getComponent<VelocityComponent>(entity);
            pos.x += vel.x;
            pos.y += vel.y;
            pos.z += vel.z;
        }
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_ComponentAccess)->Range(1000, 100000);

// Benchmark system iteration
static void BM_SystemIteration(benchmark::State& state) {
    World world;

    // Create entities
    for (int i = 0; i < state.range(0); ++i) {
        auto entity = world.createEntity();
        world.addComponent<PositionComponent>(entity, i, i, i);
        world.addComponent<VelocityComponent>(entity, 1, 0, 0);
    }

    MovementSystem movementSystem;

    for (auto _ : state) {
        movementSystem.update(world, 0.016f);  // 60fps delta
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_SystemIteration)->Range(1000, 100000);

// Benchmark query performance
static void BM_ComponentQuery(benchmark::State& state) {
    World world;

    // Create mixed entities (some with all components, some without)
    for (int i = 0; i < state.range(0); ++i) {
        auto entity = world.createEntity();
        world.addComponent<PositionComponent>(entity, i, i, i);
        if (i % 2 == 0) {
            world.addComponent<VelocityComponent>(entity, 1, 0, 0);
        }
        if (i % 3 == 0) {
            world.addComponent<CombatComponent>(entity);
        }
    }

    for (auto _ : state) {
        int count = 0;
        world.query<PositionComponent, VelocityComponent>()
            .forEach([&count](auto& pos, auto& vel) {
                count++;
            });
        benchmark::DoNotOptimize(count);
    }
}
BENCHMARK(BM_ComponentQuery)->Range(1000, 100000);

BENCHMARK_MAIN();
```

### Load Testing

```cpp
// tests/performance/load_test.cpp
#include <thread>
#include <atomic>
#include <chrono>
#include "helpers/test_client.h"

class LoadTest {
public:
    struct Results {
        int totalRequests;
        int successfulRequests;
        int failedRequests;
        double avgLatencyMs;
        double p99LatencyMs;
        double requestsPerSecond;
    };

    Results runLoadTest(
        const std::string& host,
        int port,
        int numClients,
        int durationSeconds
    ) {
        std::atomic<int> totalRequests{0};
        std::atomic<int> successfulRequests{0};
        std::atomic<int> failedRequests{0};
        std::vector<double> latencies;
        std::mutex latencyMutex;

        std::atomic<bool> running{true};
        std::vector<std::thread> clients;

        auto startTime = std::chrono::steady_clock::now();

        // Spawn client threads
        for (int i = 0; i < numClients; ++i) {
            clients.emplace_back([&, i]() {
                TestClient client;
                if (!client.connect(host, port)) {
                    return;
                }

                while (running.load()) {
                    auto reqStart = std::chrono::steady_clock::now();

                    auto response = client.sendAndWait<PongMessage>(
                        PingMessage{.clientTime = 0},
                        1000  // 1 second timeout
                    );

                    auto reqEnd = std::chrono::steady_clock::now();
                    double latency = std::chrono::duration<double, std::milli>(
                        reqEnd - reqStart
                    ).count();

                    totalRequests++;

                    if (response.has_value()) {
                        successfulRequests++;
                        std::lock_guard lock(latencyMutex);
                        latencies.push_back(latency);
                    } else {
                        failedRequests++;
                    }
                }
            });
        }

        // Run for duration
        std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
        running.store(false);

        // Wait for clients to finish
        for (auto& t : clients) {
            t.join();
        }

        auto endTime = std::chrono::steady_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(
            endTime - startTime
        ).count();

        // Calculate results
        Results results;
        results.totalRequests = totalRequests.load();
        results.successfulRequests = successfulRequests.load();
        results.failedRequests = failedRequests.load();
        results.requestsPerSecond = results.totalRequests / (durationMs / 1000.0);

        if (!latencies.empty()) {
            std::sort(latencies.begin(), latencies.end());
            double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
            results.avgLatencyMs = sum / latencies.size();
            results.p99LatencyMs = latencies[latencies.size() * 99 / 100];
        }

        return results;
    }
};

TEST(LoadTest, SustainedLoad_1000Clients) {
    LoadTest loadTest;
    auto results = loadTest.runLoadTest("localhost", 7777, 1000, 60);

    std::cout << "Load Test Results:\n"
              << "  Total Requests: " << results.totalRequests << "\n"
              << "  Success Rate: " << (results.successfulRequests * 100.0 / results.totalRequests) << "%\n"
              << "  Requests/sec: " << results.requestsPerSecond << "\n"
              << "  Avg Latency: " << results.avgLatencyMs << "ms\n"
              << "  P99 Latency: " << results.p99LatencyMs << "ms\n";

    EXPECT_GT(results.successfulRequests * 100.0 / results.totalRequests, 99.0);
    EXPECT_LT(results.p99LatencyMs, 100.0);
}
```

---

## Game Logic Testing

### Combat Scenario Testing

```cpp
// tests/unit/game/combat_scenarios_test.cpp
#include <gtest/gtest.h>
#include "game/combat_system.h"
#include "mocks/mock_world.h"

class CombatScenarioTest : public ::testing::Test {
protected:
    MockWorld world_;
    CombatSystem combatSystem_;

    Entity createPlayer(int level, int attack, int hp) {
        auto entity = world_.createEntity();
        world_.addComponent<CombatComponent>(entity, CombatComponent{
            .attackPower = attack,
            .defense = level * 5,
            .currentHp = hp,
            .maxHp = hp
        });
        world_.addComponent<UnitComponent>(entity, UnitComponent{
            .unitType = UnitType::Player,
            .level = level
        });
        return entity;
    }

    Entity createMonster(int level, int attack, int hp) {
        auto entity = world_.createEntity();
        world_.addComponent<CombatComponent>(entity, CombatComponent{
            .attackPower = attack,
            .defense = level * 3,
            .currentHp = hp,
            .maxHp = hp
        });
        world_.addComponent<UnitComponent>(entity, UnitComponent{
            .unitType = UnitType::Monster,
            .level = level
        });
        return entity;
    }
};

TEST_F(CombatScenarioTest, PlayerVsMonster_LevelAdvantage) {
    auto player = createPlayer(50, 500, 5000);
    auto monster = createMonster(30, 200, 2000);

    // Player attacks monster
    auto result = combatSystem_.attack(world_, player, monster);

    EXPECT_TRUE(result.hit);
    EXPECT_GT(result.damage, 0);

    // High level player should deal significant damage
    auto& monsterCombat = world_.getComponent<CombatComponent>(monster);
    EXPECT_LT(monsterCombat.currentHp, 2000);
}

TEST_F(CombatScenarioTest, PlayerVsMonster_LevelDisadvantage) {
    auto player = createPlayer(10, 50, 500);
    auto monster = createMonster(50, 500, 5000);

    // Monster attacks player
    auto result = combatSystem_.attack(world_, monster, player);

    EXPECT_TRUE(result.hit);

    // High level monster should nearly one-shot low level player
    auto& playerCombat = world_.getComponent<CombatComponent>(player);
    EXPECT_LT(playerCombat.currentHp, 100);
}

TEST_F(CombatScenarioTest, PvP_EqualLevel) {
    auto player1 = createPlayer(50, 500, 5000);
    auto player2 = createPlayer(50, 500, 5000);

    // Exchange blows
    for (int i = 0; i < 10; ++i) {
        combatSystem_.attack(world_, player1, player2);
        combatSystem_.attack(world_, player2, player1);
    }

    auto& p1Combat = world_.getComponent<CombatComponent>(player1);
    auto& p2Combat = world_.getComponent<CombatComponent>(player2);

    // Both should have taken significant damage
    EXPECT_LT(p1Combat.currentHp, 4000);
    EXPECT_LT(p2Combat.currentHp, 4000);
}

TEST_F(CombatScenarioTest, GroupVsBoss_TankingMechanic) {
    // Tank: High HP, high defense
    auto tank = createPlayer(50, 200, 10000);
    world_.getComponent<CombatComponent>(tank).defense = 500;
    world_.addComponent<ThreatComponent>(tank, ThreatComponent{.threat = 1000});

    // DPS: High attack, low HP
    auto dps = createPlayer(50, 800, 3000);
    world_.addComponent<ThreatComponent>(dps, ThreatComponent{.threat = 500});

    // Boss
    auto boss = createMonster(55, 600, 50000);
    world_.addComponent<AIComponent>(boss, AIComponent{
        .behavior = AIBehavior::AggroHighestThreat
    });

    // Boss should attack tank (highest threat)
    auto target = combatSystem_.getAITarget(world_, boss);
    EXPECT_EQ(target, tank);

    // DPS deals damage, generates threat
    combatSystem_.attack(world_, dps, boss);
    world_.getComponent<ThreatComponent>(dps).threat += 200;

    // Tank still has more threat
    target = combatSystem_.getAITarget(world_, boss);
    EXPECT_EQ(target, tank);
}
```

### Quest System Testing

```cpp
// tests/unit/game/quest_test.cpp
#include <gtest/gtest.h>
#include "game/quest_system.h"

class QuestSystemTest : public ::testing::Test {
protected:
    QuestSystem questSystem_;
    Entity player_;

    void SetUp() override {
        player_ = Entity{1};
    }

    QuestDefinition createKillQuest(int targetCount) {
        return QuestDefinition{
            .id = 1001,
            .name = "Kill Monsters",
            .objectives = {{
                .type = QuestObjectiveType::Kill,
                .targetId = 100,  // Monster ID
                .requiredCount = targetCount
            }}
        };
    }

    QuestDefinition createCollectQuest(int itemId, int count) {
        return QuestDefinition{
            .id = 1002,
            .name = "Collect Items",
            .objectives = {{
                .type = QuestObjectiveType::Collect,
                .targetId = itemId,
                .requiredCount = count
            }}
        };
    }
};

TEST_F(QuestSystemTest, AcceptQuest_AddsToActiveQuests) {
    auto quest = createKillQuest(10);

    auto result = questSystem_.acceptQuest(player_, quest);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(questSystem_.hasActiveQuest(player_, quest.id));
}

TEST_F(QuestSystemTest, AcceptQuest_FailsWhenAlreadyActive) {
    auto quest = createKillQuest(10);
    questSystem_.acceptQuest(player_, quest);

    auto result = questSystem_.acceptQuest(player_, quest);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, QuestError::AlreadyActive);
}

TEST_F(QuestSystemTest, KillProgress_UpdatesObjective) {
    auto quest = createKillQuest(10);
    questSystem_.acceptQuest(player_, quest);

    // Kill 3 monsters
    for (int i = 0; i < 3; ++i) {
        questSystem_.onMonsterKill(player_, 100);  // Monster ID 100
    }

    auto progress = questSystem_.getQuestProgress(player_, quest.id);
    EXPECT_EQ(progress.objectives[0].currentCount, 3);
    EXPECT_FALSE(progress.isComplete);
}

TEST_F(QuestSystemTest, KillProgress_CompletesQuest) {
    auto quest = createKillQuest(5);
    questSystem_.acceptQuest(player_, quest);

    // Kill 5 monsters
    for (int i = 0; i < 5; ++i) {
        questSystem_.onMonsterKill(player_, 100);
    }

    auto progress = questSystem_.getQuestProgress(player_, quest.id);
    EXPECT_TRUE(progress.isComplete);
}

TEST_F(QuestSystemTest, CompleteQuest_GrantsRewards) {
    auto quest = createKillQuest(1);
    quest.rewards = {
        .experience = 1000,
        .gold = 500,
        .items = {{.itemId = 2001, .quantity = 1}}
    };

    questSystem_.acceptQuest(player_, quest);
    questSystem_.onMonsterKill(player_, 100);

    auto result = questSystem_.completeQuest(player_, quest.id);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.rewards.experience, 1000);
    EXPECT_EQ(result.rewards.gold, 500);
    EXPECT_EQ(result.rewards.items.size(), 1);
}

TEST_F(QuestSystemTest, AbandonQuest_RemovesFromActive) {
    auto quest = createKillQuest(10);
    questSystem_.acceptQuest(player_, quest);

    auto result = questSystem_.abandonQuest(player_, quest.id);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(questSystem_.hasActiveQuest(player_, quest.id));
}
```

---

## Network Testing

### Protocol Testing

```cpp
// tests/unit/network/packet_test.cpp
#include <gtest/gtest.h>
#include "network/packet.h"
#include "network/serialization.h"

class PacketTest : public ::testing::Test {
protected:
    template<typename T>
    void roundTrip(const T& original) {
        auto serialized = serialize(original);
        auto deserialized = deserialize<T>(serialized);

        // Compare using operator== or member-by-member
        compareMessages(original, deserialized);
    }
};

TEST_F(PacketTest, PacketHeader_ValidMagic) {
    PacketHeader header{
        .magic = PACKET_MAGIC,
        .version = PROTOCOL_VERSION,
        .type = 0x0101,
        .length = 100,
        .sequence = 1
    };

    EXPECT_TRUE(isValidPacketHeader(header));
}

TEST_F(PacketTest, PacketHeader_InvalidMagic) {
    PacketHeader header{
        .magic = 0xDEADBEEF,
        .version = PROTOCOL_VERSION,
        .type = 0x0101,
        .length = 100,
        .sequence = 1
    };

    EXPECT_FALSE(isValidPacketHeader(header));
}

TEST_F(PacketTest, Serialize_LoginRequest) {
    LoginRequest original{
        .username = "testuser",
        .passwordHash = "abc123hash",
        .clientVersion = "1.0.0",
        .deviceId = "device-001"
    };

    roundTrip(original);
}

TEST_F(PacketTest, Serialize_PositionUpdate) {
    PositionUpdate original{
        .entityId = 12345,
        .position = {100.5f, 200.25f, 50.0f},
        .rotation = 3.14159f,
        .moveFlags = 0x05,
        .timestamp = 1000000
    };

    roundTrip(original);
}

TEST_F(PacketTest, Serialize_EntitySpawn) {
    EntitySpawnMessage original{
        .entityId = 99999,
        .entityType = EntityType::Player,
        .templateId = 1001,
        .position = {0, 0, 0},
        .rotation = 0,
        .name = "TestPlayer",
        .level = 50,
        .currentHp = 5000,
        .maxHp = 5000,
        .extraData = {0x01, 0x02, 0x03, 0x04}
    };

    roundTrip(original);
}

TEST_F(PacketTest, Serialize_ChatMessage_Unicode) {
    ChatMessage original{
        .channel = ChatChannel::World,
        .senderId = 1234,
        .senderName = "테스트유저",  // Korean
        .message = "안녕하세요! 🎮",  // Korean + emoji
        .timestamp = 1706918400000
    };

    roundTrip(original);
}

TEST_F(PacketTest, PacketBuilder_CreatesValidPacket) {
    auto packet = PacketBuilder()
        .setType(AuthMessages::LoginRequest)
        .setSequence(1)
        .setPayload(LoginRequest{"user", "hash", "1.0", "dev"})
        .build();

    EXPECT_GE(packet.size(), sizeof(PacketHeader));

    PacketHeader header;
    std::memcpy(&header, packet.data(), sizeof(header));

    EXPECT_EQ(header.magic, PACKET_MAGIC);
    EXPECT_EQ(header.type, AuthMessages::LoginRequest);
}

TEST_F(PacketTest, PacketParser_ParsesValidPacket) {
    auto packet = PacketBuilder()
        .setType(SystemMessages::Ping)
        .setPayload(PingMessage{.clientTime = 12345})
        .build();

    auto result = PacketParser::parse(packet);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->header.type, SystemMessages::Ping);
}

TEST_F(PacketTest, PacketParser_RejectsInvalidMagic) {
    std::vector<uint8_t> badPacket(20, 0);

    auto result = PacketParser::parse(badPacket);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ParseError::InvalidMagic);
}
```

---

## Test Fixtures & Mocks

### Mock Database

```cpp
// tests/mocks/mock_database.h
#pragma once

#include <gmock/gmock.h>
#include "database/database_interface.h"

class MockDatabase : public IDatabase {
public:
    MOCK_METHOD(Result<void>, connect, (), (override));
    MOCK_METHOD(Result<void>, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));

    MOCK_METHOD(Result<QueryResult>, execute,
                (const std::string& query), (override));

    MOCK_METHOD(Result<QueryResult>, executeWithParams,
                (const std::string& query, const std::vector<std::any>& params),
                (override));

    MOCK_METHOD(Result<int64_t>, insert,
                (const std::string& table, const std::map<std::string, std::any>& values),
                (override));

    MOCK_METHOD(Result<int>, update,
                (const std::string& table,
                 const std::map<std::string, std::any>& values,
                 const std::string& where),
                (override));

    MOCK_METHOD(Result<int>, remove,
                (const std::string& table, const std::string& where),
                (override));

    MOCK_METHOD(Result<void>, beginTransaction, (), (override));
    MOCK_METHOD(Result<void>, commit, (), (override));
    MOCK_METHOD(Result<void>, rollback, (), (override));
};
```

### Test Server Helper

```cpp
// tests/helpers/test_server.h
#pragma once

#include <memory>
#include "server/game_server.h"
#include "mocks/mock_database.h"

class TestServer {
public:
    explicit TestServer(int port = 0) : port_(port) {
        config_ = loadTestConfig();
        if (port_ == 0) {
            port_ = findAvailablePort();
        }
        config_.ports.game = port_;
    }

    void start() {
        server_ = std::make_unique<GameServer>(config_);
        server_->start();
    }

    void stop() {
        if (server_) {
            server_->stop();
        }
    }

    int port() const { return port_; }

    // Test helpers
    void createTestAccount(const std::string& username, const std::string& password) {
        server_->getDatabase().insert("accounts", {
            {"username", username},
            {"password_hash", hashPassword(password)},
            {"salt", "test_salt"},
            {"email", username + "@test.com"}
        });
    }

    void createTestCharacter(uint64_t accountId, const std::string& name) {
        server_->getDatabase().insert("player_characters", {
            {"account_id", accountId},
            {"name", name},
            {"class_id", 1},
            {"level", 1},
            {"map_id", 1},
            {"position_x", 0.0f},
            {"position_y", 0.0f},
            {"position_z", 0.0f}
        });
    }

private:
    int port_;
    ServerConfig config_;
    std::unique_ptr<GameServer> server_;
};
```

### Test Client Helper

```cpp
// tests/helpers/test_client.h
#pragma once

#include "network/client.h"
#include <future>

class TestClient {
public:
    bool connect(const std::string& host, int port) {
        return client_.connect(host, port);
    }

    void disconnect() {
        client_.disconnect();
    }

    bool isConnected() const {
        return client_.isConnected();
    }

    template<typename Request>
    void send(const Request& request) {
        client_.send(request);
    }

    template<typename Response, typename Request>
    std::optional<Response> sendAndWait(
        const Request& request,
        int timeoutMs = 5000
    ) {
        std::promise<Response> promise;
        auto future = promise.get_future();

        client_.setResponseHandler<Response>([&promise](const Response& resp) {
            promise.set_value(resp);
        });

        client_.send(request);

        if (future.wait_for(std::chrono::milliseconds(timeoutMs))
            == std::future_status::ready) {
            return future.get();
        }

        return std::nullopt;
    }

    template<typename Message>
    std::optional<Message> waitForMessage(int timeoutMs = 5000) {
        std::promise<Message> promise;
        auto future = promise.get_future();

        client_.setMessageHandler<Message>([&promise](const Message& msg) {
            promise.set_value(msg);
        });

        if (future.wait_for(std::chrono::milliseconds(timeoutMs))
            == std::future_status::ready) {
            return future.get();
        }

        return std::nullopt;
    }

private:
    NetworkClient client_;
};
```

---

## CI/CD Integration

### GitHub Actions Workflow

```yaml
# .github/workflows/test.yml
name: Tests

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main, develop]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake g++-13 libpq-dev

      - name: Configure CMake
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON

      - name: Build
        run: cmake --build build --parallel

      - name: Run unit tests
        run: ./build/tests/unit_tests --gtest_output=xml:test-results/unit.xml

      - name: Upload results
        uses: actions/upload-artifact@v4
        if: always()
        with:
          name: test-results-unit
          path: test-results/

  integration-tests:
    runs-on: ubuntu-latest
    services:
      postgres:
        image: postgres:15
        env:
          POSTGRES_USER: test
          POSTGRES_PASSWORD: test
          POSTGRES_DB: test_game
        ports:
          - 5432:5432
        options: >-
          --health-cmd pg_isready
          --health-interval 10s
          --health-timeout 5s
          --health-retries 5

      redis:
        image: redis:7
        ports:
          - 6379:6379

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake g++-13 libpq-dev

      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON

      - name: Build
        run: cmake --build build --parallel

      - name: Run migrations
        run: ./build/tools/migrate up
        env:
          DB_HOST: localhost
          DB_PORT: 5432
          DB_NAME: test_game
          DB_USER: test
          DB_PASSWORD: test

      - name: Run integration tests
        run: ./build/tests/integration_tests --gtest_output=xml:test-results/integration.xml
        env:
          DB_HOST: localhost
          REDIS_HOST: localhost

  performance-tests:
    runs-on: ubuntu-latest
    if: github.event_name == 'push' && github.ref == 'refs/heads/main'
    steps:
      - uses: actions/checkout@v4

      - name: Build
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
          cmake --build build --parallel

      - name: Run benchmarks
        run: ./build/tests/benchmarks --benchmark_format=json > benchmark-results.json

      - name: Store benchmark results
        uses: benchmark-action/github-action-benchmark@v1
        with:
          tool: 'googlecpp'
          output-file-path: benchmark-results.json
          github-token: ${{ secrets.GITHUB_TOKEN }}
          auto-push: true
```

---

## Test Coverage

### Coverage Configuration

```cmake
# CMakeLists.txt - Coverage option

option(ENABLE_COVERAGE "Enable code coverage" OFF)

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()
```

### Coverage Report Generation

```bash
#!/bin/bash
# scripts/coverage.sh

# Build with coverage
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build build

# Run tests
./build/tests/unit_tests
./build/tests/integration_tests

# Generate coverage report
lcov --capture --directory build --output-file coverage.info

# Filter out system headers and test files
lcov --remove coverage.info \
    '/usr/*' \
    '*/tests/*' \
    '*/third_party/*' \
    --output-file coverage.filtered.info

# Generate HTML report
genhtml coverage.filtered.info --output-directory coverage-report

# Print summary
lcov --summary coverage.filtered.info
```

### Coverage Requirements

| Component | Minimum Coverage | Target Coverage |
|-----------|------------------|-----------------|
| Core ECS | 90% | 95% |
| Game Logic | 85% | 90% |
| Network | 80% | 90% |
| Database | 75% | 85% |
| Utilities | 70% | 80% |
| Overall | 80% | 90% |

---

*This document provides the complete testing strategy for the unified game server.*
