# Coding Standards Reference

> Version: 0.1.0.0
> Last Updated: 2026-02-03
> Status: Foundation Reference

## Overview

This document defines the C++20 coding standards for the unified game server project, ensuring consistency, maintainability, and performance across the codebase.

---

## Table of Contents

1. [General Principles](#general-principles)
2. [Naming Conventions](#naming-conventions)
3. [Code Organization](#code-organization)
4. [Modern C++ Usage](#modern-c-usage)
5. [Memory Management](#memory-management)
6. [Error Handling](#error-handling)
7. [Concurrency](#concurrency)
8. [Performance Guidelines](#performance-guidelines)
9. [Documentation](#documentation)
10. [Code Review Checklist](#code-review-checklist)

---

## General Principles

### Core Values

| Principle | Description |
|-----------|-------------|
| Clarity | Code should be readable and self-documenting |
| Correctness | Correct behavior takes priority over performance |
| Consistency | Follow established patterns throughout the codebase |
| Safety | Prefer compile-time checks over runtime checks |
| Performance | Optimize where it matters, measure first |

### SOLID Principles

```cpp
// Single Responsibility: Each class has one job
class PlayerMovement {
    // Only handles movement logic
};

class PlayerCombat {
    // Only handles combat logic
};

// Open/Closed: Open for extension, closed for modification
class Skill {
public:
    virtual ~Skill() = default;
    virtual void execute(Entity& caster, Entity& target) = 0;
};

class FireballSkill : public Skill {
    void execute(Entity& caster, Entity& target) override;
};

// Liskov Substitution: Subtypes must be substitutable
class Entity {
public:
    virtual Position getPosition() const = 0;
};

class Player : public Entity {
    // Must correctly implement getPosition()
};

// Interface Segregation: Many specific interfaces
class IMovable {
public:
    virtual void move(const Vector3& delta) = 0;
};

class IDamageable {
public:
    virtual void takeDamage(int amount) = 0;
};

// Dependency Inversion: Depend on abstractions
class CombatSystem {
public:
    explicit CombatSystem(ILogger& logger, IDatabase& db);
private:
    ILogger& logger_;
    IDatabase& db_;
};
```

---

## Naming Conventions

### General Rules

| Element | Convention | Example |
|---------|------------|---------|
| Namespaces | lowercase | `game::combat` |
| Classes | PascalCase | `PlayerCharacter` |
| Structs | PascalCase | `PositionData` |
| Functions | camelCase | `calculateDamage()` |
| Variables | camelCase | `playerHealth` |
| Member variables | trailing underscore | `currentHp_` |
| Constants | UPPER_SNAKE_CASE | `MAX_PLAYERS` |
| Enum classes | PascalCase | `EntityType::Player` |
| Template params | PascalCase | `template<typename T>` |
| Macros | UPPER_SNAKE_CASE | `GAME_ASSERT()` |
| Files | snake_case | `player_character.h` |

### Examples

```cpp
// Namespaces
namespace game::ecs {
namespace detail {
} // namespace detail
} // namespace game::ecs

// Classes and structs
class PlayerCharacter {
public:
    // Public methods
    void updatePosition(const Vector3& newPosition);
    [[nodiscard]] int getCurrentHealth() const;

private:
    // Member variables with trailing underscore
    int currentHealth_;
    float movementSpeed_;
    std::string playerName_;
};

// Enums
enum class EntityType : uint8_t {
    Player,
    Monster,
    Npc,
    Item,
};

enum class DamageType {
    Physical,
    Magical,
    True,
};

// Constants
namespace constants {
    constexpr int MAX_PLAYERS = 5000;
    constexpr float DEFAULT_MOVE_SPEED = 5.0f;
    constexpr std::string_view SERVER_NAME = "GameServer";
}

// Template parameters
template<typename EntityT, typename ComponentT>
class ComponentPool {
    // ...
};

// File naming
// player_character.h
// player_character.cpp
// combat_system.h
// combat_system.cpp
```

### Abbreviations

```cpp
// Acceptable abbreviations
Hp      // Hit points
Mp      // Mana points
Npc     // Non-player character
Id      // Identifier
Db      // Database
Io      // Input/Output
Ui      // User interface
Api     // Application programming interface

// Use full names when not commonly known
characterLevel  // NOT charLvl
positionX       // NOT posX (context dependent)
experiencePoints // NOT exp (ambiguous)
```

---

## Code Organization

### File Structure

```cpp
// player_character.h

#pragma once  // Use pragma once, not include guards

// Standard library headers (alphabetical)
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Third-party headers
#include <boost/asio.hpp>

// Project headers
#include "core/entity.h"
#include "game/components.h"

// Forward declarations when possible
namespace game {
class CombatSystem;
class InventorySystem;
}

namespace game {

/**
 * @brief Represents a player character in the game.
 *
 * PlayerCharacter handles the state and behavior of player-controlled
 * entities, including stats, equipment, and skills.
 */
class PlayerCharacter final : public Entity {
public:
    // Type aliases first
    using Skills = std::vector<std::unique_ptr<Skill>>;

    // Static constants
    static constexpr int MAX_LEVEL = 100;
    static constexpr int BASE_HP = 100;

    // Constructors and destructor
    PlayerCharacter();
    explicit PlayerCharacter(uint64_t id);
    ~PlayerCharacter() override;

    // Deleted/defaulted special members
    PlayerCharacter(const PlayerCharacter&) = delete;
    PlayerCharacter& operator=(const PlayerCharacter&) = delete;
    PlayerCharacter(PlayerCharacter&&) noexcept = default;
    PlayerCharacter& operator=(PlayerCharacter&&) noexcept = default;

    // Public interface (grouped by functionality)
    // -- State queries
    [[nodiscard]] int getLevel() const noexcept { return level_; }
    [[nodiscard]] int getCurrentHp() const noexcept { return currentHp_; }
    [[nodiscard]] bool isAlive() const noexcept { return currentHp_ > 0; }

    // -- State modifiers
    void setLevel(int level);
    void heal(int amount);
    void takeDamage(int amount);

    // -- Actions
    void moveTo(const Vector3& destination);
    void useSkill(SkillId skillId, Entity& target);

    // Overrides from base class
    void update(float deltaTime) override;
    void serialize(Serializer& s) const override;

private:
    // Private methods
    void recalculateStats();
    void applyBuffs();

    // Member variables (grouped logically)
    // -- Identity
    std::string name_;
    uint8_t classId_ = 0;

    // -- Stats
    int level_ = 1;
    int currentHp_ = BASE_HP;
    int maxHp_ = BASE_HP;
    int currentMp_ = 0;
    int maxMp_ = 0;

    // -- Position
    Vector3 position_;
    float rotation_ = 0.0f;

    // -- Systems
    std::unique_ptr<Inventory> inventory_;
    Skills skills_;
};

} // namespace game
```

### Implementation Files

```cpp
// player_character.cpp

#include "game/player_character.h"

// Other includes needed for implementation
#include "game/combat_system.h"
#include "game/skill_manager.h"
#include "util/logger.h"

namespace game {

PlayerCharacter::PlayerCharacter() = default;

PlayerCharacter::PlayerCharacter(uint64_t id)
    : Entity(id)
    , inventory_(std::make_unique<Inventory>())
{
    recalculateStats();
}

PlayerCharacter::~PlayerCharacter() = default;

void PlayerCharacter::setLevel(int level) {
    if (level < 1 || level > MAX_LEVEL) {
        LOG_WARN("Invalid level: {}", level);
        return;
    }

    level_ = level;
    recalculateStats();
}

void PlayerCharacter::heal(int amount) {
    if (amount <= 0) {
        return;
    }

    currentHp_ = std::min(currentHp_ + amount, maxHp_);
}

void PlayerCharacter::takeDamage(int amount) {
    if (amount <= 0 || !isAlive()) {
        return;
    }

    currentHp_ = std::max(0, currentHp_ - amount);

    if (!isAlive()) {
        onDeath();
    }
}

void PlayerCharacter::update(float deltaTime) {
    applyBuffs();
    updatePosition(deltaTime);
}

// Private methods

void PlayerCharacter::recalculateStats() {
    // Calculate stats based on level, equipment, etc.
    maxHp_ = BASE_HP + (level_ * 10);
    // ...
}

} // namespace game
```

---

## Modern C++ Usage

### C++20 Features to Use

```cpp
// Concepts for template constraints
template<typename T>
concept Component = requires(T t) {
    { t.entityId } -> std::convertible_to<EntityId>;
    { T::componentTypeId() } -> std::same_as<ComponentTypeId>;
};

template<Component C>
void addComponent(Entity entity, C component);

// Ranges for cleaner algorithms
#include <ranges>

auto activeMonsters = entities
    | std::views::filter([](const Entity& e) { return e.isActive(); })
    | std::views::filter([](const Entity& e) { return e.type() == EntityType::Monster; })
    | std::views::take(100);

// Structured bindings
auto [success, message, data] = parsePacket(buffer);
if (success) {
    processData(data);
}

// Designated initializers
PlayerConfig config{
    .name = "Player1",
    .level = 50,
    .classId = ClassId::Warrior,
    .startPosition = {100.0f, 0.0f, 100.0f},
};

// std::span for non-owning views
void processPackets(std::span<const Packet> packets) {
    for (const auto& packet : packets) {
        handlePacket(packet);
    }
}

// std::expected for error handling (C++23, use custom or backport)
std::expected<Character, DatabaseError> loadCharacter(uint64_t id);

// Three-way comparison
auto operator<=>(const Vector3& other) const = default;

// constexpr for compile-time computation
constexpr uint32_t calculateExpForLevel(int level) {
    return 100 * level * level;
}

// Lambda improvements
auto handler = [this]<typename T>(T&& value) {
    process(std::forward<T>(value));
};

// Coroutines for async operations
Task<Character> loadCharacterAsync(uint64_t id) {
    auto data = co_await database_.queryAsync(id);
    co_return Character::fromData(data);
}
```

### Prefer Modern Alternatives

```cpp
// Use std::array instead of C arrays
std::array<int, 10> stats;        // NOT: int stats[10];

// Use std::string_view for non-owning strings
void logMessage(std::string_view message);  // NOT: const char*

// Use std::optional for nullable values
std::optional<Entity> findEntity(EntityId id);  // NOT: Entity* (nullable)

// Use std::variant for type-safe unions
using DamageValue = std::variant<int, float, DamageRange>;

// Use std::byte for raw bytes
std::vector<std::byte> buffer;  // NOT: std::vector<char>

// Use scoped enums
enum class Direction { North, South, East, West };  // NOT: enum Direction

// Use nullptr
Entity* entity = nullptr;  // NOT: NULL or 0

// Use auto for complex types
auto it = container.find(key);  // NOT: std::unordered_map<K,V>::iterator

// Use range-based for
for (const auto& entity : entities) { }  // NOT: for (int i = 0; ...)

// Use std::make_unique/make_shared
auto ptr = std::make_unique<Entity>();  // NOT: new Entity()
```

### Avoid These Patterns

```cpp
// Avoid raw new/delete
Entity* e = new Entity();  // BAD
delete e;                   // BAD

// Avoid C-style casts
int x = (int)floatValue;    // BAD
int x = static_cast<int>(floatValue);  // GOOD

// Avoid macros for constants
#define MAX_PLAYERS 5000    // BAD
constexpr int MAX_PLAYERS = 5000;  // GOOD

// Avoid using namespace in headers
using namespace std;        // NEVER in headers

// Avoid mutable global state
static int globalCounter;   // BAD (unless carefully controlled)

// Avoid raw arrays for dynamic data
int* data = new int[size];  // BAD
std::vector<int> data(size);  // GOOD
```

---

## Memory Management

### Ownership Guidelines

```cpp
// Use unique_ptr for exclusive ownership
class Player {
    std::unique_ptr<Inventory> inventory_;  // Player owns inventory
};

// Use shared_ptr for shared ownership (rare)
class World {
    std::shared_ptr<MapData> currentMap_;  // Multiple systems reference
};

// Use weak_ptr to break cycles
class Entity {
    std::weak_ptr<Entity> target_;  // Reference without ownership
};

// Use raw pointers for non-owning references
class MovementSystem {
    void update(World* world);  // Doesn't own world
};

// Use references when null is not valid
void applyDamage(Entity& target, int amount);  // target must exist
```

### Resource Management (RAII)

```cpp
// RAII for file handling
class ConfigFile {
public:
    explicit ConfigFile(const std::filesystem::path& path)
        : file_(path, std::ios::in)
    {
        if (!file_.is_open()) {
            throw ConfigError("Failed to open: " + path.string());
        }
    }

    // Destructor automatically closes file
    ~ConfigFile() = default;

    // Delete copy, allow move
    ConfigFile(const ConfigFile&) = delete;
    ConfigFile& operator=(const ConfigFile&) = delete;
    ConfigFile(ConfigFile&&) = default;
    ConfigFile& operator=(ConfigFile&&) = default;

private:
    std::ifstream file_;
};

// RAII for locks
class ThreadSafeCounter {
public:
    void increment() {
        std::lock_guard lock(mutex_);  // RAII lock
        ++count_;
    }  // Lock released here

    int get() const {
        std::shared_lock lock(mutex_);  // RAII shared lock
        return count_;
    }

private:
    mutable std::shared_mutex mutex_;
    int count_ = 0;
};

// RAII for database transactions
class Transaction {
public:
    explicit Transaction(Database& db) : db_(db) {
        db_.begin();
    }

    ~Transaction() {
        if (!committed_) {
            db_.rollback();
        }
    }

    void commit() {
        db_.commit();
        committed_ = true;
    }

private:
    Database& db_;
    bool committed_ = false;
};

// Usage
void transferItems(Database& db) {
    Transaction txn(db);  // Begin transaction

    db.removeItems(from);
    db.addItems(to);

    txn.commit();  // Commit on success
}  // Automatic rollback if exception thrown
```

### Object Pools

```cpp
// Object pool for frequently allocated objects
template<typename T, size_t BlockSize = 64>
class ObjectPool {
public:
    template<typename... Args>
    [[nodiscard]] T* allocate(Args&&... args) {
        if (freeList_.empty()) {
            expandPool();
        }

        T* obj = freeList_.back();
        freeList_.pop_back();

        return new(obj) T(std::forward<Args>(args)...);
    }

    void deallocate(T* obj) {
        obj->~T();
        freeList_.push_back(obj);
    }

private:
    void expandPool() {
        auto block = std::make_unique<std::array<std::byte, sizeof(T) * BlockSize>>();
        for (size_t i = 0; i < BlockSize; ++i) {
            freeList_.push_back(reinterpret_cast<T*>(block->data() + i * sizeof(T)));
        }
        blocks_.push_back(std::move(block));
    }

    std::vector<T*> freeList_;
    std::vector<std::unique_ptr<std::array<std::byte, sizeof(T) * BlockSize>>> blocks_;
};
```

---

## Error Handling

### Result Type Pattern

```cpp
// Result type for error handling
template<typename T, typename E = std::string>
class Result {
public:
    // Success constructors
    Result(T value) : data_(std::move(value)) {}

    // Error constructors
    static Result error(E err) {
        Result r;
        r.data_ = std::unexpected(std::move(err));
        return r;
    }

    // Accessors
    [[nodiscard]] bool isOk() const { return data_.has_value(); }
    [[nodiscard]] bool isError() const { return !data_.has_value(); }

    [[nodiscard]] T& value() & { return data_.value(); }
    [[nodiscard]] const T& value() const& { return data_.value(); }
    [[nodiscard]] T&& value() && { return std::move(data_.value()); }

    [[nodiscard]] const E& error() const { return data_.error(); }

    // Monadic operations
    template<typename F>
    auto andThen(F&& f) -> Result<std::invoke_result_t<F, T>, E> {
        if (isOk()) {
            return f(value());
        }
        return Result<std::invoke_result_t<F, T>, E>::error(error());
    }

    template<typename F>
    auto map(F&& f) -> Result<std::invoke_result_t<F, T>, E> {
        if (isOk()) {
            return Result<std::invoke_result_t<F, T>, E>(f(value()));
        }
        return Result<std::invoke_result_t<F, T>, E>::error(error());
    }

private:
    Result() = default;
    std::expected<T, E> data_;
};

// Usage
Result<Character, DatabaseError> loadCharacter(uint64_t id) {
    auto row = database_.queryById(id);
    if (!row) {
        return Result<Character, DatabaseError>::error(DatabaseError::NotFound);
    }

    return Character::fromRow(*row);
}

// Chaining
auto result = loadCharacter(123)
    .andThen([](Character& c) { return loadInventory(c.id()); })
    .andThen([](Inventory& inv) { return validateInventory(inv); });

if (result.isError()) {
    LOG_ERROR("Failed: {}", result.error());
}
```

### Exception Usage

```cpp
// Use exceptions only for exceptional conditions
class GameException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class DatabaseException : public GameException {
public:
    explicit DatabaseException(const std::string& msg, int errorCode)
        : GameException(msg)
        , errorCode_(errorCode)
    {}

    [[nodiscard]] int errorCode() const noexcept { return errorCode_; }

private:
    int errorCode_;
};

// Mark functions that don't throw
int getLevel() const noexcept;

// Use noexcept for move operations
PlayerCharacter(PlayerCharacter&&) noexcept = default;

// Document exceptions in function contracts
/**
 * @brief Load player data from database
 * @throws DatabaseException if database connection fails
 * @throws ValidationException if data is corrupted
 */
Player loadPlayer(uint64_t id);
```

### Assertions

```cpp
// Use assertions for programmer errors (invariants)
#include <cassert>

void setLevel(int level) {
    assert(level > 0 && level <= MAX_LEVEL);  // Debug only
    level_ = level;
}

// Custom assertion macro for more info
#define GAME_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            LOG_CRITICAL("Assertion failed: {} at {}:{}", \
                message, __FILE__, __LINE__); \
            std::abort(); \
        } \
    } while (0)

// Use for invariants
void processPacket(const Packet& packet) {
    GAME_ASSERT(packet.isValid(), "Invalid packet received");
    // ...
}
```

---

## Concurrency

### Thread Safety Guidelines

```cpp
// Document thread safety
/**
 * @brief Thread-safe player manager
 * @note All public methods are thread-safe
 */
class PlayerManager {
public:
    void addPlayer(std::unique_ptr<Player> player) {
        std::lock_guard lock(mutex_);
        players_.push_back(std::move(player));
    }

    // Return copy to avoid data races
    std::optional<PlayerInfo> getPlayerInfo(uint64_t id) const {
        std::shared_lock lock(mutex_);
        auto it = findPlayer(id);
        if (it == players_.end()) {
            return std::nullopt;
        }
        return (*it)->getInfo();  // Return copy
    }

private:
    mutable std::shared_mutex mutex_;
    std::vector<std::unique_ptr<Player>> players_;
};

// Use std::atomic for simple types
class GameServer {
    std::atomic<bool> running_{false};
    std::atomic<int> playerCount_{0};

public:
    void start() {
        running_.store(true, std::memory_order_release);
    }

    void addPlayer() {
        playerCount_.fetch_add(1, std::memory_order_relaxed);
    }

    bool isRunning() const {
        return running_.load(std::memory_order_acquire);
    }
};
```

### Lock-Free Patterns

```cpp
// Lock-free queue for inter-thread communication
template<typename T>
class LockFreeQueue {
public:
    void push(T value) {
        auto node = std::make_unique<Node>(std::move(value));
        Node* newNode = node.get();

        while (true) {
            Node* tail = tail_.load(std::memory_order_acquire);
            Node* next = tail->next.load(std::memory_order_acquire);

            if (tail == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (tail->next.compare_exchange_weak(
                            next, newNode,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                        tail_.compare_exchange_strong(
                            tail, newNode,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        node.release();
                        return;
                    }
                } else {
                    tail_.compare_exchange_weak(
                        tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                }
            }
        }
    }

    std::optional<T> pop();

private:
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};
        explicit Node(T d = T{}) : data(std::move(d)) {}
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};
```

### Async Patterns

```cpp
// Use std::async for parallel work
std::future<LoadResult> loadMapAsync(MapId mapId) {
    return std::async(std::launch::async, [mapId]() {
        return loadMapFromDisk(mapId);
    });
}

// Thread pool for task execution
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        auto future = task->get_future();

        {
            std::lock_guard lock(mutex_);
            tasks_.emplace([task]() { (*task)(); });
        }

        condition_.notify_one();
        return future;
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
};
```

---

## Performance Guidelines

### Data-Oriented Design

```cpp
// Prefer arrays of structs for cache efficiency
struct Positions {
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> z;
};

// Process in batches for SIMD
void updatePositions(Positions& pos, const Velocities& vel, float dt) {
    const size_t count = pos.x.size();

    // Loop can be auto-vectorized
    for (size_t i = 0; i < count; ++i) {
        pos.x[i] += vel.x[i] * dt;
        pos.y[i] += vel.y[i] * dt;
        pos.z[i] += vel.z[i] * dt;
    }
}
```

### Avoid Unnecessary Copies

```cpp
// Pass by const reference for read-only
void logPlayer(const Player& player);

// Pass by value and move for sink parameters
void addPlayer(Player player) {
    players_.push_back(std::move(player));
}

// Return by value (RVO/NRVO will optimize)
std::vector<Entity> getEntitiesInRange(const Vector3& center, float radius) {
    std::vector<Entity> result;
    // ... populate result
    return result;  // No copy, move or NRVO
}

// Use move semantics
class Entity {
public:
    Entity(Entity&& other) noexcept
        : id_(other.id_)
        , name_(std::move(other.name_))
        , components_(std::move(other.components_))
    {}
};
```

### Profiling and Optimization

```cpp
// Use [[likely]] and [[unlikely]] for branch hints
if (packet.type() == PacketType::PositionUpdate) [[likely]] {
    handlePositionUpdate(packet);
} else if (packet.type() == PacketType::Disconnect) [[unlikely]] {
    handleDisconnect(packet);
}

// Mark hot paths
[[gnu::hot]]
void GameLoop::tick(float deltaTime) {
    // Critical path
}

// Reserve container capacity
std::vector<Entity> entities;
entities.reserve(expectedCount);

// Use emplace for in-place construction
entities.emplace_back(id, name, position);  // NOT: push_back(Entity(...))

// Avoid string concatenation in loops
std::string result;
result.reserve(totalSize);
for (const auto& part : parts) {
    result += part;  // Now efficient
}
```

---

## Documentation

### Doxygen Comments

```cpp
/**
 * @file player_character.h
 * @brief Player character management
 * @author Game Team
 */

/**
 * @class PlayerCharacter
 * @brief Represents a player-controlled entity in the game world.
 *
 * PlayerCharacter handles all player-specific logic including:
 * - Character stats and progression
 * - Equipment and inventory management
 * - Skill usage and cooldowns
 *
 * @note Thread-safety: Public methods are NOT thread-safe unless noted.
 *
 * @par Example Usage:
 * @code
 * auto player = std::make_unique<PlayerCharacter>(characterId);
 * player->setLevel(50);
 * player->equipItem(sword);
 * @endcode
 *
 * @see Entity, CharacterStats, Inventory
 */
class PlayerCharacter {
public:
    /**
     * @brief Apply damage to this character.
     *
     * Reduces current HP by the specified amount after applying
     * defense calculations. Triggers death if HP reaches zero.
     *
     * @param amount Raw damage amount (before defense)
     * @param damageType Type of damage for resistance calculation
     * @param attacker Source of the damage (may be null for environmental)
     *
     * @return Actual damage dealt after calculations
     *
     * @pre amount >= 0
     * @post currentHp_ >= 0
     *
     * @throws std::invalid_argument if amount < 0
     */
    int takeDamage(int amount, DamageType damageType, Entity* attacker = nullptr);

    /**
     * @brief Get character's effective attack power.
     *
     * Calculates attack power including:
     * - Base stats
     * - Equipment bonuses
     * - Active buffs
     *
     * @return Current attack power value (always >= 0)
     *
     * @note This is a computed value, not cached.
     */
    [[nodiscard]] int getAttackPower() const noexcept;
};
```

### Code Comments

```cpp
// Good: Explains WHY, not WHAT
// Use weak_ptr to avoid circular reference with parent entity
std::weak_ptr<Entity> parent_;

// Good: Explains non-obvious algorithm
// Binary search won't work here because entities may not be sorted
// after removal. Using linear search with early exit instead.
for (const auto& entity : entities_) {
    if (entity.id() == targetId) {
        return &entity;
    }
}

// Good: Documents magic numbers
constexpr float GRAVITY = 9.81f;  // m/s^2, standard gravity
constexpr int TICK_RATE = 20;     // Server updates per second

// Bad: States the obvious
int count = 0;  // Initialize count to zero (BAD - obvious)
++count;        // Increment count (BAD - obvious)

// Bad: Outdated comment
// Calculate damage using old formula  (BAD - comment doesn't match code)
int damage = attack * 2;
```

---

## Code Review Checklist

### Correctness

- [ ] Logic is correct and handles edge cases
- [ ] No off-by-one errors
- [ ] Null/empty checks where needed
- [ ] Thread safety considerations addressed
- [ ] Resources properly managed (RAII)
- [ ] Error conditions handled appropriately

### Design

- [ ] Follows SOLID principles
- [ ] Appropriate abstraction level
- [ ] No premature optimization
- [ ] No premature generalization
- [ ] Dependencies clearly defined
- [ ] Interface is minimal and complete

### C++ Best Practices

- [ ] Uses modern C++ features appropriately
- [ ] No memory leaks possible
- [ ] Move semantics used where appropriate
- [ ] const correctness maintained
- [ ] noexcept specified where appropriate
- [ ] [[nodiscard]] on functions that shouldn't be ignored

### Performance

- [ ] No unnecessary copies
- [ ] Appropriate container choices
- [ ] No O(n²) or worse algorithms without justification
- [ ] Cache-friendly data access patterns
- [ ] Memory allocations minimized in hot paths

### Style

- [ ] Follows naming conventions
- [ ] Proper formatting (clang-format)
- [ ] Meaningful names
- [ ] Comments explain "why" not "what"
- [ ] No dead code
- [ ] No TODO/FIXME without issue link

### Testing

- [ ] Unit tests for new functionality
- [ ] Edge cases tested
- [ ] Error paths tested
- [ ] Tests are deterministic
- [ ] No flaky tests

---

## Appendix: Quick Reference

### Standard Library Cheat Sheet

```cpp
// Containers
std::vector<T>       // Dynamic array
std::array<T, N>     // Fixed array
std::unordered_map   // Hash map
std::map             // Ordered map (rare)
std::unordered_set   // Hash set
std::span<T>         // Non-owning view

// Smart Pointers
std::unique_ptr<T>   // Exclusive ownership
std::shared_ptr<T>   // Shared ownership
std::weak_ptr<T>     // Non-owning observer

// Utilities
std::optional<T>     // Maybe value
std::variant<Ts...>  // Type-safe union
std::expected<T, E>  // Result type (C++23)
std::string_view     // Non-owning string

// Concurrency
std::mutex           // Exclusive lock
std::shared_mutex    // Reader-writer lock
std::atomic<T>       // Lock-free atomic
std::future<T>       // Async result
std::thread          // Thread handle
```

### Compiler Attributes

```cpp
[[nodiscard]]        // Warn if return value ignored
[[maybe_unused]]     // Suppress unused warnings
[[likely]]           // Branch prediction hint
[[unlikely]]         // Branch prediction hint
[[fallthrough]]      // Intentional fallthrough in switch
[[deprecated("msg")]] // Mark as deprecated
```

---

*This document provides the complete coding standards for the unified game server project.*
