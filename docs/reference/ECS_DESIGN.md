# Entity-Component System Design

## ECS Architecture for Common Game Server

**Version**: 0.1.0.0
**Last Updated**: 2026-02-03
**Source**: unified_game_server (UGS)

---

## 1. Overview

### 1.1 What is ECS?

Entity-Component System (ECS) is a data-oriented design pattern that separates:

- **Entity**: A unique identifier (just an ID)
- **Component**: Pure data (no logic)
- **System**: Logic that operates on components

### 1.2 Why ECS for Game Servers?

| Benefit | Description |
|---------|-------------|
| **Cache Efficiency** | Components stored contiguously in memory |
| **Parallelization** | Systems can run in parallel safely |
| **Flexibility** | Entities composed dynamically at runtime |
| **Scalability** | Handles 10K+ entities efficiently |
| **Testability** | Systems are pure functions on data |

### 1.3 ECS vs Traditional OOP

```cpp
// Traditional OOP (game_server style)
class Player : public Unit {
    void Update(float dt) override {
        UpdateMovement(dt);      // Cache miss
        UpdateCombat(dt);        // Cache miss
        UpdateSpells(dt);        // Cache miss
    }
};

// ECS Approach
// All positions updated together (cache friendly)
for (auto& pos : positions) {
    pos.x += velocity.x * dt;
}
// All combat updated together
for (auto& combat : combats) {
    combat.update(dt);
}
```

---

## 2. Core Components

### 2.1 Entity

An entity is simply a unique identifier:

```cpp
namespace ecs {

// Entity is just an ID
using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY = 0;

// Generation for safe entity recycling
struct EntityHandle {
    EntityId id;
    uint32_t generation;

    bool operator==(const EntityHandle& other) const {
        return id == other.id && generation == other.generation;
    }
};

class EntityManager {
public:
    // Create new entity
    EntityId create() {
        if (!free_list_.empty()) {
            EntityId id = free_list_.back();
            free_list_.pop_back();
            alive_[id] = true;
            return id;
        }

        EntityId id = next_id_++;
        if (id >= alive_.size()) {
            alive_.resize(id + 1, false);
            generations_.resize(id + 1, 0);
        }
        alive_[id] = true;
        return id;
    }

    // Destroy entity
    void destroy(EntityId id) {
        if (is_valid(id)) {
            alive_[id] = false;
            generations_[id]++;
            free_list_.push_back(id);
        }
    }

    // Check if entity is valid
    bool is_valid(EntityId id) const {
        return id != INVALID_ENTITY &&
               id < alive_.size() &&
               alive_[id];
    }

    // Get handle for safe reference
    EntityHandle get_handle(EntityId id) const {
        return EntityHandle{id, generations_[id]};
    }

    // Validate handle
    bool is_valid_handle(EntityHandle handle) const {
        return is_valid(handle.id) &&
               generations_[handle.id] == handle.generation;
    }

private:
    EntityId next_id_ = 1;  // 0 is INVALID_ENTITY
    std::vector<bool> alive_;
    std::vector<uint32_t> generations_;
    std::vector<EntityId> free_list_;
};

}  // namespace ecs
```

### 2.2 Component

Components are pure data structures:

```cpp
namespace ecs {

// Base component marker (for type introspection)
struct ComponentBase {
    static constexpr bool is_component = true;
};

// Example components

// Transform component - position and rotation
struct TransformComponent : ComponentBase {
    Vector3 position{0, 0, 0};
    Quaternion rotation{0, 0, 0, 1};
    Vector3 scale{1, 1, 1};

    Matrix4 to_matrix() const;
    Vector3 forward() const;
    Vector3 right() const;
    Vector3 up() const;
};

// Velocity component - movement speed
struct VelocityComponent : ComponentBase {
    Vector3 linear{0, 0, 0};
    Vector3 angular{0, 0, 0};
};

// Health component - entity health
struct HealthComponent : ComponentBase {
    int32_t max_health = 100;
    int32_t current_health = 100;

    bool is_dead() const { return current_health <= 0; }
    float percentage() const {
        return static_cast<float>(current_health) / max_health;
    }
};

// Network component - for replicated entities
struct NetworkComponent : ComponentBase {
    uint64_t network_id;
    SessionId owner_session;
    ReplicationFlags flags;
    uint32_t last_update_tick;
};

// Tag components (zero-size, just for marking)
struct PlayerTag : ComponentBase {};
struct CreatureTag : ComponentBase {};
struct DeadTag : ComponentBase {};

}  // namespace ecs
```

### 2.3 Component Storage (Sparse Set)

Sparse set provides O(1) operations with good cache locality:

```cpp
namespace ecs {

constexpr size_t INVALID_INDEX = std::numeric_limits<size_t>::max();

template<typename T>
class ComponentPool {
public:
    // Add component to entity
    T& add(EntityId entity) {
        assert(!has(entity) && "Entity already has component");

        size_t index = dense_.size();
        dense_.push_back(T{});
        dense_to_entity_.push_back(entity);

        ensure_sparse_size(entity);
        sparse_[entity] = index;

        return dense_.back();
    }

    // Add component with initial value
    T& add(EntityId entity, const T& value) {
        T& comp = add(entity);
        comp = value;
        return comp;
    }

    // Remove component from entity
    void remove(EntityId entity) {
        if (!has(entity)) return;

        size_t index = sparse_[entity];
        size_t last_index = dense_.size() - 1;

        if (index != last_index) {
            // Swap with last element
            dense_[index] = std::move(dense_[last_index]);
            dense_to_entity_[index] = dense_to_entity_[last_index];
            sparse_[dense_to_entity_[index]] = index;
        }

        dense_.pop_back();
        dense_to_entity_.pop_back();
        sparse_[entity] = INVALID_INDEX;
    }

    // Get component for entity
    T& get(EntityId entity) {
        assert(has(entity) && "Entity doesn't have component");
        return dense_[sparse_[entity]];
    }

    const T& get(EntityId entity) const {
        assert(has(entity) && "Entity doesn't have component");
        return dense_[sparse_[entity]];
    }

    // Check if entity has component
    bool has(EntityId entity) const {
        return entity < sparse_.size() &&
               sparse_[entity] != INVALID_INDEX;
    }

    // Get or add component
    T& get_or_add(EntityId entity) {
        if (has(entity)) {
            return get(entity);
        }
        return add(entity);
    }

    // Iteration support
    size_t size() const { return dense_.size(); }
    bool empty() const { return dense_.empty(); }

    auto begin() { return dense_.begin(); }
    auto end() { return dense_.end(); }
    auto begin() const { return dense_.begin(); }
    auto end() const { return dense_.end(); }

    // Get entity for component at index
    EntityId entity_at(size_t index) const {
        return dense_to_entity_[index];
    }

    // Clear all components
    void clear() {
        dense_.clear();
        dense_to_entity_.clear();
        std::fill(sparse_.begin(), sparse_.end(), INVALID_INDEX);
    }

private:
    void ensure_sparse_size(EntityId entity) {
        if (entity >= sparse_.size()) {
            sparse_.resize(entity + 1, INVALID_INDEX);
        }
    }

    std::vector<T> dense_;                    // Actual component data
    std::vector<EntityId> dense_to_entity_;   // Entity ID for each component
    std::vector<size_t> sparse_;              // Entity ID -> dense index
};

}  // namespace ecs
```

### 2.4 Component Registry

Type-erased storage for all component types:

```cpp
namespace ecs {

// Type-erased base for component pools
class ComponentPoolBase {
public:
    virtual ~ComponentPoolBase() = default;
    virtual void remove(EntityId entity) = 0;
    virtual bool has(EntityId entity) const = 0;
    virtual void clear() = 0;
};

template<typename T>
class TypedComponentPool : public ComponentPoolBase {
public:
    ComponentPool<T>& pool() { return pool_; }
    const ComponentPool<T>& pool() const { return pool_; }

    void remove(EntityId entity) override { pool_.remove(entity); }
    bool has(EntityId entity) const override { return pool_.has(entity); }
    void clear() override { pool_.clear(); }

private:
    ComponentPool<T> pool_;
};

class ComponentRegistry {
public:
    template<typename T>
    void register_component() {
        auto type_id = std::type_index(typeid(T));
        if (pools_.find(type_id) == pools_.end()) {
            pools_[type_id] = std::make_unique<TypedComponentPool<T>>();
        }
    }

    template<typename T>
    ComponentPool<T>& get_pool() {
        auto type_id = std::type_index(typeid(T));
        auto it = pools_.find(type_id);
        if (it == pools_.end()) {
            register_component<T>();
            it = pools_.find(type_id);
        }
        return static_cast<TypedComponentPool<T>*>(it->second.get())->pool();
    }

    void remove_entity(EntityId entity) {
        for (auto& [type_id, pool] : pools_) {
            pool->remove(entity);
        }
    }

    void clear() {
        for (auto& [type_id, pool] : pools_) {
            pool->clear();
        }
    }

private:
    std::unordered_map<std::type_index,
                       std::unique_ptr<ComponentPoolBase>> pools_;
};

}  // namespace ecs
```

---

## 3. Systems

### 3.1 System Interface

```cpp
namespace ecs {

class World;  // Forward declaration

class System {
public:
    virtual ~System() = default;

    // Core update function
    virtual void update(World& world, float delta_time) = 0;

    // System identification
    virtual std::string_view name() const = 0;

    // Lifecycle hooks (optional)
    virtual void on_init(World& world) {}
    virtual void on_shutdown(World& world) {}

    // Component dependencies (for parallel execution)
    virtual std::vector<std::type_index> reads() const { return {}; }
    virtual std::vector<std::type_index> writes() const { return {}; }

    // Enable/disable
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

protected:
    bool enabled_ = true;
};

}  // namespace ecs
```

### 3.2 Example Systems

```cpp
namespace game::systems {

// Movement system - updates positions based on velocity
class MovementSystem : public ecs::System {
public:
    void update(ecs::World& world, float dt) override {
        auto& transforms = world.get_pool<TransformComponent>();
        auto& velocities = world.get_pool<VelocityComponent>();

        // Iterate entities with both components
        for (auto entity : world.view<TransformComponent, VelocityComponent>()) {
            auto& transform = transforms.get(entity);
            auto& velocity = velocities.get(entity);

            transform.position += velocity.linear * dt;
            transform.rotation = transform.rotation *
                Quaternion::from_euler(velocity.angular * dt);
        }
    }

    std::string_view name() const override { return "MovementSystem"; }

    std::vector<std::type_index> reads() const override {
        return {typeid(VelocityComponent)};
    }

    std::vector<std::type_index> writes() const override {
        return {typeid(TransformComponent)};
    }
};

// Health regeneration system
class HealthRegenSystem : public ecs::System {
public:
    void update(ecs::World& world, float dt) override {
        for (auto entity : world.view<HealthComponent>()) {
            auto& health = world.get<HealthComponent>(entity);

            // Skip dead entities
            if (health.is_dead()) continue;

            // Regenerate 1% per second
            int regen = static_cast<int>(health.max_health * 0.01f * dt);
            health.current_health = std::min(
                health.current_health + regen,
                health.max_health
            );
        }
    }

    std::string_view name() const override { return "HealthRegenSystem"; }

    std::vector<std::type_index> writes() const override {
        return {typeid(HealthComponent)};
    }
};

// Damage processing system
class DamageSystem : public ecs::System {
public:
    void update(ecs::World& world, float dt) override {
        auto& damages = world.get_pool<PendingDamageComponent>();
        auto& healths = world.get_pool<HealthComponent>();

        for (size_t i = 0; i < damages.size(); ++i) {
            auto entity = damages.entity_at(i);
            auto& damage = damages.get(entity);

            if (!healths.has(entity)) continue;

            auto& health = healths.get(entity);

            for (const auto& pending : damage.pending) {
                health.current_health -= pending.amount;

                // Emit damage event
                world.emit<DamageEvent>({
                    .target = entity,
                    .source = pending.source,
                    .amount = pending.amount,
                    .type = pending.type
                });
            }

            damage.pending.clear();

            // Add dead tag if health depleted
            if (health.is_dead() && !world.has<DeadTag>(entity)) {
                world.add<DeadTag>(entity);
                world.emit<DeathEvent>({.entity = entity});
            }
        }
    }

    std::string_view name() const override { return "DamageSystem"; }

    std::vector<std::type_index> writes() const override {
        return {typeid(HealthComponent), typeid(PendingDamageComponent)};
    }
};

}  // namespace game::systems
```

### 3.3 System Runner

```cpp
namespace ecs {

class SystemRunner {
public:
    void add_system(std::unique_ptr<System> system) {
        systems_.push_back(std::move(system));
    }

    template<typename T, typename... Args>
    T& add_system(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *system;
        systems_.push_back(std::move(system));
        return ref;
    }

    void remove_system(std::string_view name) {
        systems_.erase(
            std::remove_if(systems_.begin(), systems_.end(),
                [name](const auto& s) { return s->name() == name; }),
            systems_.end()
        );
    }

    System* get_system(std::string_view name) {
        auto it = std::find_if(systems_.begin(), systems_.end(),
            [name](const auto& s) { return s->name() == name; });
        return it != systems_.end() ? it->get() : nullptr;
    }

    void init(World& world) {
        for (auto& system : systems_) {
            system->on_init(world);
        }
    }

    void shutdown(World& world) {
        for (auto& system : systems_) {
            system->on_shutdown(world);
        }
    }

    void update(World& world, float delta_time) {
        if (parallel_enabled_) {
            update_parallel(world, delta_time);
        } else {
            update_sequential(world, delta_time);
        }
    }

    void set_parallel_execution(bool enabled) {
        parallel_enabled_ = enabled;
    }

private:
    void update_sequential(World& world, float delta_time) {
        for (auto& system : systems_) {
            if (system->is_enabled()) {
                system->update(world, delta_time);
            }
        }
    }

    void update_parallel(World& world, float delta_time) {
        // Build dependency graph and execute in parallel
        // Systems that don't share write components can run concurrently
        auto groups = build_parallel_groups();

        for (const auto& group : groups) {
            std::vector<std::future<void>> futures;
            for (auto* system : group) {
                futures.push_back(std::async(std::launch::async,
                    [system, &world, delta_time]() {
                        system->update(world, delta_time);
                    }
                ));
            }
            for (auto& future : futures) {
                future.wait();
            }
        }
    }

    std::vector<std::vector<System*>> build_parallel_groups() {
        // Topological sort based on read/write dependencies
        // Systems in same group don't conflict
        std::vector<std::vector<System*>> groups;
        // ... implementation ...
        return groups;
    }

    std::vector<std::unique_ptr<System>> systems_;
    bool parallel_enabled_ = false;
};

}  // namespace ecs
```

---

## 4. World

### 4.1 World Container

```cpp
namespace ecs {

class World {
public:
    World() = default;
    ~World() = default;

    // Non-copyable, movable
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = default;
    World& operator=(World&&) = default;

    // Entity management
    EntityId create_entity() {
        return entities_.create();
    }

    void destroy_entity(EntityId entity) {
        components_.remove_entity(entity);
        entities_.destroy(entity);
    }

    bool is_valid(EntityId entity) const {
        return entities_.is_valid(entity);
    }

    // Component management
    template<typename T>
    T& add(EntityId entity) {
        return components_.get_pool<T>().add(entity);
    }

    template<typename T>
    T& add(EntityId entity, const T& value) {
        return components_.get_pool<T>().add(entity, value);
    }

    template<typename T>
    void remove(EntityId entity) {
        components_.get_pool<T>().remove(entity);
    }

    template<typename T>
    T& get(EntityId entity) {
        return components_.get_pool<T>().get(entity);
    }

    template<typename T>
    const T& get(EntityId entity) const {
        return components_.get_pool<T>().get(entity);
    }

    template<typename T>
    bool has(EntityId entity) const {
        return components_.get_pool<T>().has(entity);
    }

    template<typename T>
    T& get_or_add(EntityId entity) {
        return components_.get_pool<T>().get_or_add(entity);
    }

    // Direct pool access
    template<typename T>
    ComponentPool<T>& get_pool() {
        return components_.get_pool<T>();
    }

    // View for iterating entities with specific components
    template<typename... Components>
    View<Components...> view() {
        return View<Components...>(*this);
    }

    // System management
    template<typename T, typename... Args>
    T& add_system(Args&&... args) {
        return systems_.add_system<T>(std::forward<Args>(args)...);
    }

    void remove_system(std::string_view name) {
        systems_.remove_system(name);
    }

    // World update
    void update(float delta_time) {
        systems_.update(*this, delta_time);
    }

    // Event system
    template<typename E>
    void emit(const E& event) {
        events_.emit(event);
    }

    template<typename E>
    void subscribe(std::function<void(const E&)> handler) {
        events_.subscribe<E>(std::move(handler));
    }

private:
    EntityManager entities_;
    ComponentRegistry components_;
    SystemRunner systems_;
    EventBus events_;
};

}  // namespace ecs
```

### 4.2 View (Query)

```cpp
namespace ecs {

template<typename... Components>
class View {
public:
    explicit View(World& world) : world_(world) {}

    class Iterator {
    public:
        Iterator(View& view, size_t index)
            : view_(view), index_(index) {
            advance_to_valid();
        }

        EntityId operator*() const {
            return view_.smallest_pool().entity_at(index_);
        }

        Iterator& operator++() {
            ++index_;
            advance_to_valid();
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return index_ != other.index_;
        }

    private:
        void advance_to_valid() {
            auto& smallest = view_.smallest_pool();
            while (index_ < smallest.size()) {
                EntityId entity = smallest.entity_at(index_);
                if (view_.has_all_components(entity)) {
                    break;
                }
                ++index_;
            }
        }

        View& view_;
        size_t index_;
    };

    Iterator begin() { return Iterator(*this, 0); }
    Iterator end() { return Iterator(*this, smallest_pool().size()); }

    // Get all components for an entity
    std::tuple<Components&...> get(EntityId entity) {
        return std::tie(world_.get<Components>(entity)...);
    }

private:
    // Find smallest pool to iterate (optimization)
    auto& smallest_pool() {
        // Return pool with fewest entities
        return find_smallest(world_.get_pool<Components>()...);
    }

    bool has_all_components(EntityId entity) {
        return (world_.has<Components>(entity) && ...);
    }

    World& world_;
};

}  // namespace ecs
```

---

## 5. Game Logic Components

### 5.1 Object System Components (from game_server)

```cpp
namespace game::components {

// Base object component
struct ObjectComponent {
    ObjectGUID guid;
    ObjectType type;
    std::string name;
    uint32_t entry;  // Template ID

    static constexpr bool is_component = true;
};

// Unit component (for combat-capable entities)
struct UnitComponent {
    // Base stats
    int32_t level = 1;
    int32_t base_health = 100;
    int32_t base_mana = 100;

    // Current stats
    int32_t health = 100;
    int32_t mana = 100;

    // Attributes
    int32_t strength = 10;
    int32_t agility = 10;
    int32_t stamina = 10;
    int32_t intellect = 10;
    int32_t spirit = 10;

    // Combat modifiers
    float damage_modifier = 1.0f;
    float haste_modifier = 1.0f;
    float crit_chance = 0.05f;

    // Faction
    FactionId faction;
    UnitFlags flags;

    static constexpr bool is_component = true;
};

// Player-specific component
struct PlayerComponent {
    AccountId account_id;
    CharacterId character_id;
    SessionId session_id;

    // Character info
    Race race;
    Class player_class;
    Gender gender;

    // Experience
    int32_t experience = 0;
    int32_t rested_experience = 0;

    // Binding
    BindLocation bind_location;

    // Flags
    PlayerFlags flags;

    static constexpr bool is_component = true;
};

// Creature-specific component
struct CreatureComponent {
    CreatureTemplateId template_id;
    SpawnId spawn_id;

    // AI
    AIType ai_type;
    uint32_t script_id;

    // Respawn
    float respawn_time = 60.0f;
    float death_timer = 0.0f;

    // Loot
    LootTableId loot_table;

    static constexpr bool is_component = true;
};

}  // namespace game::components
```

### 5.2 Combat System Components

```cpp
namespace game::components {

// Combat state
struct CombatComponent {
    bool in_combat = false;
    EntityId current_target = INVALID_ENTITY;
    std::chrono::steady_clock::time_point combat_start;
    std::chrono::steady_clock::time_point last_attack;

    static constexpr bool is_component = true;
};

// Threat management
struct ThreatComponent {
    struct ThreatEntry {
        EntityId entity;
        float threat;
        float taunt_multiplier = 1.0f;
    };

    std::vector<ThreatEntry> threat_list;
    EntityId highest_threat = INVALID_ENTITY;

    void add_threat(EntityId entity, float amount) {
        auto it = std::find_if(threat_list.begin(), threat_list.end(),
            [entity](const ThreatEntry& e) { return e.entity == entity; });

        if (it != threat_list.end()) {
            it->threat += amount;
        } else {
            threat_list.push_back({entity, amount, 1.0f});
        }

        update_highest();
    }

    void update_highest() {
        if (threat_list.empty()) {
            highest_threat = INVALID_ENTITY;
            return;
        }

        auto max_it = std::max_element(threat_list.begin(), threat_list.end(),
            [](const ThreatEntry& a, const ThreatEntry& b) {
                return (a.threat * a.taunt_multiplier) <
                       (b.threat * b.taunt_multiplier);
            });

        highest_threat = max_it->entity;
    }

    static constexpr bool is_component = true;
};

// Pending damage (processed by DamageSystem)
struct PendingDamageComponent {
    struct DamageEntry {
        EntityId source;
        int32_t amount;
        DamageType type;
        SpellId spell_id;  // 0 for melee
        bool is_critical = false;
    };

    std::vector<DamageEntry> pending;

    void add_damage(EntityId source, int32_t amount,
                    DamageType type, SpellId spell = 0) {
        pending.push_back({source, amount, type, spell});
    }

    static constexpr bool is_component = true;
};

// Spell casting
struct SpellCastComponent {
    SpellId spell_id = 0;
    EntityId target = INVALID_ENTITY;
    Vector3 target_position;

    enum class State {
        Idle,
        Preparing,
        Casting,
        Channeling,
        Finished,
        Interrupted
    };
    State state = State::Idle;

    float cast_time = 0.0f;
    float elapsed = 0.0f;

    bool is_casting() const {
        return state == State::Casting || state == State::Channeling;
    }

    static constexpr bool is_component = true;
};

// Active auras
struct AuraComponent {
    struct ActiveAura {
        AuraId id;
        EntityId caster;
        int32_t stacks = 1;
        int32_t max_stacks = 1;
        float duration;
        float remaining;
        std::vector<AuraEffect> effects;
    };

    std::vector<ActiveAura> auras;

    void add_aura(const ActiveAura& aura) {
        // Check for existing aura from same caster
        auto it = std::find_if(auras.begin(), auras.end(),
            [&](const ActiveAura& a) {
                return a.id == aura.id && a.caster == aura.caster;
            });

        if (it != auras.end()) {
            // Refresh duration and add stack
            it->remaining = aura.duration;
            it->stacks = std::min(it->stacks + 1, it->max_stacks);
        } else {
            auras.push_back(aura);
        }
    }

    void remove_aura(AuraId id, EntityId caster = INVALID_ENTITY) {
        auras.erase(std::remove_if(auras.begin(), auras.end(),
            [id, caster](const ActiveAura& a) {
                return a.id == id &&
                       (caster == INVALID_ENTITY || a.caster == caster);
            }), auras.end());
    }

    static constexpr bool is_component = true;
};

}  // namespace game::components
```

### 5.3 World System Components

```cpp
namespace game::components {

// Position in world
struct PositionComponent {
    MapId map_id;
    ZoneId zone_id;
    AreaId area_id;

    Vector3 position;
    float orientation;

    // Grid coordinates (for spatial queries)
    GridCoord grid;

    static constexpr bool is_component = true;
};

// Movement
struct MovementComponent {
    Vector3 velocity;
    float speed = 7.0f;  // Base run speed

    // Movement flags
    bool is_moving = false;
    bool is_falling = false;
    bool is_swimming = false;
    bool is_flying = false;

    // Speed modifiers
    float speed_modifier = 1.0f;
    float slow_modifier = 1.0f;

    float get_effective_speed() const {
        return speed * speed_modifier * slow_modifier;
    }

    static constexpr bool is_component = true;
};

// Visibility
struct VisibilityComponent {
    float visibility_range = 100.0f;

    std::set<EntityId> visible_entities;
    std::set<EntityId> known_entities;  // Were visible, might need update

    static constexpr bool is_component = true;
};

// Grid cell (for spatial partitioning)
struct GridCellComponent {
    GridCoord coord;
    std::set<EntityId> entities;

    static constexpr bool is_component = true;
};

}  // namespace game::components
```

---

## 6. Performance Optimization

### 6.1 Memory Layout

```cpp
// Components are stored contiguously in memory
// This provides excellent cache locality

// Memory layout example for 1000 entities with TransformComponent:
// [Transform0][Transform1][Transform2]...[Transform999]
//     ^           ^           ^
//     |           |           |
// Sequential memory access - cache friendly!

// Compare to OOP where transforms are scattered:
// [Player0->transform][...other data...][Player1->transform][...]
//     ^                                      ^
//     |                                      |
// Cache misses on every access
```

### 6.2 Component Access Patterns

```cpp
// Best practice: Access one component type at a time
void MovementSystem::update(World& world, float dt) {
    // Good: Sequential access to transforms
    for (auto& transform : world.get_pool<TransformComponent>()) {
        // Process transform
    }

    // Good: Sequential access to velocities
    for (auto& velocity : world.get_pool<VelocityComponent>()) {
        // Process velocity
    }
}

// Avoid: Random access patterns
void BadUpdate(World& world, float dt) {
    for (auto entity : all_entities) {
        // Bad: Jumps between different component arrays
        auto& transform = world.get<TransformComponent>(entity);  // Cache miss
        auto& velocity = world.get<VelocityComponent>(entity);     // Cache miss
        auto& health = world.get<HealthComponent>(entity);         // Cache miss
    }
}
```

### 6.3 Parallel Execution

```cpp
// Systems that don't share write components can run in parallel

// Group 1 (parallel):
// - MovementSystem (writes: Transform)
// - HealthRegenSystem (writes: Health)
// - CooldownSystem (writes: Cooldown)

// Group 2 (after Group 1):
// - DamageSystem (writes: Health, reads: Position)

// Group 3 (after Group 2):
// - DeathSystem (reads: Health, writes: Dead tag)
```

---

## 7. Integration with game_server

### 7.1 Hybrid Bridge

```cpp
namespace game::bridge {

// Allow gradual migration from OOP to ECS
class HybridWorld {
public:
    // Create ECS entity from legacy object
    EntityId import_object(legacy::Object* obj) {
        EntityId entity = world_.create_entity();

        // Add base components
        auto& object = world_.add<ObjectComponent>(entity);
        object.guid = obj->GetGUID();
        object.type = obj->GetType();

        auto& transform = world_.add<TransformComponent>(entity);
        transform.position = obj->GetPosition();

        // Keep reference to legacy object
        auto& legacy = world_.add<LegacyObjectComponent>(entity);
        legacy.object = obj->shared_from_this();

        return entity;
    }

    // Sync ECS back to legacy object
    void sync_to_legacy(EntityId entity) {
        if (!world_.has<LegacyObjectComponent>(entity)) return;

        auto& legacy = world_.get<LegacyObjectComponent>(entity);
        auto& transform = world_.get<TransformComponent>(entity);

        legacy.object->SetPosition(transform.position);
    }

private:
    ecs::World world_;
};

}  // namespace game::bridge
```

---

## 8. Appendices

### 8.1 Performance Benchmarks

| Operation | Traditional OOP | ECS |
|-----------|-----------------|-----|
| Update 10K entities | ~15ms | ~3ms |
| Create entity | ~500ns | ~50ns |
| Add component | ~200ns | ~100ns |
| Query entities | ~5ms | ~0.5ms |
| Memory per entity | ~2KB | ~200B |

### 8.2 Related Documents

- [ARCHITECTURE.md](../ARCHITECTURE.md) - System architecture
- [PROJECT_ANALYSIS.md](./PROJECT_ANALYSIS.md) - Source analysis
- [PLUGIN_SYSTEM.md](./PLUGIN_SYSTEM.md) - Plugin integration

---

*ECS Design Version*: 1.0.0
*Source*: unified_game_server
