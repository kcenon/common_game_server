# Plugin System Design

## Extensible Game Logic Architecture

**Version**: 0.1.0.0
**Last Updated**: 2026-02-03
---

## 1. Overview

### 1.1 Purpose

The plugin system enables:

- **Modularity**: Game-specific logic separated from core framework
- **Extensibility**: Add new game types without modifying core code
- **Reusability**: Share common game mechanics across projects
- **Hot-Reload**: Update game logic without server restart (dev mode)

### 1.2 Plugin Types

| Type | Description | Examples |
|------|-------------|----------|
| **Game Plugin** | Complete game implementation | MMORPG, Battle Royale, RTS |
| **Feature Plugin** | Adds specific features | Chat, Achievements, Leaderboard |
| **Integration Plugin** | External service integration | Discord, Analytics, Payment |

---

## 2. Plugin Interface

### 2.1 Core Interface

```cpp
namespace game::plugin {

// Forward declarations
class PluginContext;
class PluginManager;

// Plugin metadata
struct PluginInfo {
    std::string name;           // Unique identifier
    std::string version;        // Semantic version (e.g., "1.0.0")
    std::string description;    // Human-readable description
    std::string author;         // Plugin author

    std::vector<std::string> dependencies;          // Required plugins
    std::vector<std::string> optional_dependencies; // Optional plugins
    std::vector<std::string> conflicts;             // Incompatible plugins

    // Minimum framework version required
    std::string min_framework_version = "1.0.0";
};

// Plugin lifecycle interface
class GamePlugin {
public:
    virtual ~GamePlugin() = default;

    // ==================== Required Methods ====================

    // Called when plugin is loaded (before world initialization)
    // Use this to register components and setup resources
    virtual Result<void> on_load(PluginContext& ctx) = 0;

    // Called when plugin is unloaded
    // Cleanup resources, unregister components
    virtual Result<void> on_unload() = 0;

    // Called when plugin is enabled for a world
    // Register systems, subscribe to events
    virtual Result<void> on_enable(ecs::World& world) = 0;

    // Called when plugin is disabled for a world
    // Unregister systems, unsubscribe from events
    virtual Result<void> on_disable(ecs::World& world) = 0;

    // Return plugin metadata
    virtual PluginInfo info() const = 0;

    // ==================== Optional Hooks ====================

    // Called every world tick
    virtual void on_tick(ecs::World& world, float delta_time) {}

    // Called when a player joins
    virtual void on_player_join(ecs::World& world, EntityId player) {}

    // Called when a player leaves
    virtual void on_player_leave(ecs::World& world, EntityId player) {}

    // Called when a player sends a message
    virtual void on_player_message(ecs::World& world, EntityId player,
                                   const Message& msg) {}

    // Called when configuration changes
    virtual void on_config_reload(const PluginConfig& config) {}

    // ==================== State Management ====================

    bool is_loaded() const { return loaded_; }
    bool is_enabled() const { return enabled_; }

protected:
    friend class PluginManager;
    bool loaded_ = false;
    bool enabled_ = false;
};

}  // namespace game::plugin
```

### 2.2 Plugin Context

```cpp
namespace game::plugin {

// Context provided to plugins for framework access
class PluginContext {
public:
    explicit PluginContext(GameFoundation& foundation,
                          PluginManager& plugins,
                          EventBus& events)
        : foundation_(foundation)
        , plugins_(plugins)
        , events_(events) {}

    // ==================== Foundation Access ====================

    GameFoundation& foundation() { return foundation_; }
    GameLogger& logger() { return foundation_.logger(); }
    GameDatabase& database() { return foundation_.database(); }
    GameNetwork& network() { return foundation_.network(); }
    GameThreadPool& thread_pool() { return foundation_.thread_pool(); }
    GameMonitor& monitor() { return foundation_.monitor(); }

    // ==================== Plugin System ====================

    PluginManager& plugins() { return plugins_; }

    // Check if a plugin is available
    bool has_plugin(std::string_view name) const {
        return plugins_.get_plugin(name) != nullptr;
    }

    // Get another plugin (for inter-plugin communication)
    template<typename T>
    T* get_plugin(std::string_view name) {
        return dynamic_cast<T*>(plugins_.get_plugin(name));
    }

    // ==================== Component Registration ====================

    template<typename T>
    void register_component() {
        registered_components_.push_back(typeid(T));
        // Component will be registered when world is created
    }

    // ==================== System Registration ====================

    template<typename T, typename... Args>
    void register_system(Args&&... args) {
        system_factories_.push_back([args = std::make_tuple(std::forward<Args>(args)...)]
            (ecs::World& world) mutable {
                std::apply([&world](auto&&... a) {
                    world.add_system<T>(std::forward<decltype(a)>(a)...);
                }, std::move(args));
            });
    }

    // ==================== Event System ====================

    EventBus& events() { return events_; }

    template<typename E>
    void subscribe(std::function<void(const E&)> handler) {
        events_.subscribe<E>(std::move(handler));
    }

    template<typename E>
    void emit(const E& event) {
        events_.emit(event);
    }

    // ==================== Packet Handlers ====================

    void register_packet_handler(PacketType type, PacketHandler handler) {
        packet_handlers_[type] = std::move(handler);
    }

    // ==================== Commands ====================

    void register_command(const std::string& name,
                         const std::string& description,
                         CommandHandler handler) {
        commands_[name] = {description, std::move(handler)};
    }

    // ==================== Configuration ====================

    PluginConfig& config() { return config_; }

    template<typename T>
    T get_config(const std::string& key, T default_value = T{}) {
        return config_.get<T>(key, default_value);
    }

private:
    GameFoundation& foundation_;
    PluginManager& plugins_;
    EventBus& events_;
    PluginConfig config_;

    std::vector<std::type_index> registered_components_;
    std::vector<std::function<void(ecs::World&)>> system_factories_;
    std::unordered_map<PacketType, PacketHandler> packet_handlers_;
    std::unordered_map<std::string, CommandInfo> commands_;
};

}  // namespace game::plugin
```

---

## 3. Plugin Manager

### 3.1 Manager Implementation

```cpp
namespace game::plugin {

class PluginManager {
public:
    PluginManager(GameFoundation& foundation, EventBus& events)
        : foundation_(foundation)
        , events_(events)
        , context_(foundation, *this, events) {}

    // ==================== Plugin Loading ====================

    // Load plugin from shared library
    Result<void> load_plugin(const std::filesystem::path& path) {
        // 1. Load shared library
        auto lib = load_library(path);
        if (!lib) {
            return Result<void>::error(Error::PluginLoadFailed,
                fmt::format("Failed to load library: {}", path.string()));
        }

        // 2. Get plugin factory function
        auto factory = lib->get_function<GamePlugin*()>("create_plugin");
        if (!factory) {
            return Result<void>::error(Error::PluginLoadFailed,
                "Plugin missing create_plugin function");
        }

        // 3. Create plugin instance
        std::unique_ptr<GamePlugin> plugin(factory());
        if (!plugin) {
            return Result<void>::error(Error::PluginLoadFailed,
                "create_plugin returned null");
        }

        // 4. Check for conflicts
        auto info = plugin->info();
        for (const auto& conflict : info.conflicts) {
            if (has_plugin(conflict)) {
                return Result<void>::error(Error::PluginConflict,
                    fmt::format("Conflicts with plugin: {}", conflict));
            }
        }

        // 5. Check dependencies
        for (const auto& dep : info.dependencies) {
            if (!has_plugin(dep)) {
                return Result<void>::error(Error::MissingDependency,
                    fmt::format("Missing required plugin: {}", dep));
            }
        }

        // 6. Call on_load
        auto result = plugin->on_load(context_);
        if (!result) {
            return result;
        }

        // 7. Store plugin
        plugin->loaded_ = true;
        plugin_map_[info.name] = plugin.get();
        plugins_.push_back(std::move(plugin));
        libraries_.push_back(std::move(lib));

        logger_.info("Loaded plugin: {} v{}", info.name, info.version);
        return Result<void>::ok();
    }

    // Load plugin from built-in type
    template<typename T, typename... Args>
    Result<void> load_plugin(Args&&... args) {
        auto plugin = std::make_unique<T>(std::forward<Args>(args)...);

        auto info = plugin->info();

        // Check dependencies
        for (const auto& dep : info.dependencies) {
            if (!has_plugin(dep)) {
                return Result<void>::error(Error::MissingDependency,
                    fmt::format("Missing required plugin: {}", dep));
            }
        }

        auto result = plugin->on_load(context_);
        if (!result) {
            return result;
        }

        plugin->loaded_ = true;
        plugin_map_[info.name] = plugin.get();
        plugins_.push_back(std::move(plugin));

        logger_.info("Loaded plugin: {} v{}", info.name, info.version);
        return Result<void>::ok();
    }

    // Unload plugin
    Result<void> unload_plugin(std::string_view name) {
        auto it = plugin_map_.find(std::string(name));
        if (it == plugin_map_.end()) {
            return Result<void>::error(Error::PluginNotFound);
        }

        GamePlugin* plugin = it->second;

        // Check if other plugins depend on this
        for (const auto& other : plugins_) {
            if (other.get() == plugin) continue;

            for (const auto& dep : other->info().dependencies) {
                if (dep == name) {
                    return Result<void>::error(Error::PluginInUse,
                        fmt::format("{} is required by {}", name, other->info().name));
                }
            }
        }

        // Disable if enabled
        if (plugin->is_enabled()) {
            // Need to disable in all worlds
            return Result<void>::error(Error::PluginInUse,
                "Plugin is currently enabled");
        }

        // Call on_unload
        auto result = plugin->on_unload();
        if (!result) {
            return result;
        }

        plugin->loaded_ = false;
        plugin_map_.erase(it);

        // Remove from plugins_ vector
        plugins_.erase(
            std::remove_if(plugins_.begin(), plugins_.end(),
                [plugin](const auto& p) { return p.get() == plugin; }),
            plugins_.end());

        logger_.info("Unloaded plugin: {}", name);
        return Result<void>::ok();
    }

    // ==================== Plugin Access ====================

    GamePlugin* get_plugin(std::string_view name) {
        auto it = plugin_map_.find(std::string(name));
        return it != plugin_map_.end() ? it->second : nullptr;
    }

    bool has_plugin(std::string_view name) const {
        return plugin_map_.find(std::string(name)) != plugin_map_.end();
    }

    std::vector<GamePlugin*> get_all_plugins() {
        std::vector<GamePlugin*> result;
        for (auto& plugin : plugins_) {
            result.push_back(plugin.get());
        }
        return result;
    }

    // ==================== World Integration ====================

    // Enable all loaded plugins for a world
    Result<void> enable_all(ecs::World& world) {
        auto sorted = resolve_dependencies();

        for (auto* plugin : sorted) {
            if (!plugin->is_loaded()) continue;

            auto result = plugin->on_enable(world);
            if (!result) {
                // Rollback already enabled plugins
                rollback_enable(world, sorted, plugin);
                return result;
            }
            plugin->enabled_ = true;
        }

        return Result<void>::ok();
    }

    // Disable all plugins for a world
    Result<void> disable_all(ecs::World& world) {
        // Disable in reverse dependency order
        auto sorted = resolve_dependencies();
        std::reverse(sorted.begin(), sorted.end());

        for (auto* plugin : sorted) {
            if (!plugin->is_enabled()) continue;

            auto result = plugin->on_disable(world);
            if (!result) {
                logger_.error("Failed to disable plugin: {}", plugin->info().name);
                // Continue disabling others
            }
            plugin->enabled_ = false;
        }

        return Result<void>::ok();
    }

    // ==================== Hooks ====================

    void tick(ecs::World& world, float delta_time) {
        for (auto& plugin : plugins_) {
            if (plugin->is_enabled()) {
                plugin->on_tick(world, delta_time);
            }
        }
    }

    void player_join(ecs::World& world, EntityId player) {
        for (auto& plugin : plugins_) {
            if (plugin->is_enabled()) {
                plugin->on_player_join(world, player);
            }
        }
    }

    void player_leave(ecs::World& world, EntityId player) {
        for (auto& plugin : plugins_) {
            if (plugin->is_enabled()) {
                plugin->on_player_leave(world, player);
            }
        }
    }

private:
    // Topological sort by dependencies
    std::vector<GamePlugin*> resolve_dependencies() {
        std::vector<GamePlugin*> sorted;
        std::set<std::string> visited;
        std::set<std::string> visiting;

        std::function<bool(GamePlugin*)> visit = [&](GamePlugin* plugin) -> bool {
            auto name = plugin->info().name;

            if (visiting.count(name)) {
                logger_.error("Circular dependency detected: {}", name);
                return false;
            }

            if (visited.count(name)) {
                return true;
            }

            visiting.insert(name);

            for (const auto& dep : plugin->info().dependencies) {
                if (auto* dep_plugin = get_plugin(dep)) {
                    if (!visit(dep_plugin)) {
                        return false;
                    }
                }
            }

            visiting.erase(name);
            visited.insert(name);
            sorted.push_back(plugin);
            return true;
        };

        for (auto& plugin : plugins_) {
            visit(plugin.get());
        }

        return sorted;
    }

    void rollback_enable(ecs::World& world,
                        const std::vector<GamePlugin*>& sorted,
                        GamePlugin* failed) {
        for (auto* plugin : sorted) {
            if (plugin == failed) break;
            if (plugin->is_enabled()) {
                plugin->on_disable(world);
                plugin->enabled_ = false;
            }
        }
    }

    GameFoundation& foundation_;
    EventBus& events_;
    PluginContext context_;
    GameLogger logger_{"PluginManager"};

    std::vector<std::unique_ptr<GamePlugin>> plugins_;
    std::unordered_map<std::string, GamePlugin*> plugin_map_;
    std::vector<std::unique_ptr<SharedLibrary>> libraries_;
};

}  // namespace game::plugin
```

---

## 4. MMORPG Plugin Example

### 4.1 Plugin Implementation

```cpp
namespace game::plugins {

class MMORPGPlugin : public GamePlugin {
public:
    // ==================== Lifecycle ====================

    Result<void> on_load(PluginContext& ctx) override {
        ctx_ = &ctx;
        logger_ = GameLogger("MMORPG");

        // Register components
        ctx.register_component<CharacterComponent>();
        ctx.register_component<InventoryComponent>();
        ctx.register_component<EquipmentComponent>();
        ctx.register_component<QuestLogComponent>();
        ctx.register_component<SkillBarComponent>();
        ctx.register_component<GuildMemberComponent>();
        ctx.register_component<AchievementComponent>();
        ctx.register_component<ReputationComponent>();
        ctx.register_component<TalentComponent>();

        // Combat components
        ctx.register_component<SpellCastComponent>();
        ctx.register_component<AuraComponent>();
        ctx.register_component<CooldownComponent>();

        // Register packet handlers
        ctx.register_packet_handler(CMSG_CAST_SPELL,
            [this](auto& world, auto entity, auto& packet) {
                handle_cast_spell(world, entity, packet);
            });

        ctx.register_packet_handler(CMSG_USE_ITEM,
            [this](auto& world, auto entity, auto& packet) {
                handle_use_item(world, entity, packet);
            });

        ctx.register_packet_handler(CMSG_QUEST_ACCEPT,
            [this](auto& world, auto entity, auto& packet) {
                handle_quest_accept(world, entity, packet);
            });

        // Register commands
        ctx.register_command("additem", "Add item to inventory",
            [this](auto& world, auto player, auto& args) {
                return cmd_add_item(world, player, args);
            });

        ctx.register_command("levelup", "Level up character",
            [this](auto& world, auto player, auto& args) {
                return cmd_level_up(world, player, args);
            });

        // Load configuration
        load_config();

        logger_.info("MMORPG plugin loaded");
        return Result<void>::ok();
    }

    Result<void> on_unload() override {
        logger_.info("MMORPG plugin unloaded");
        return Result<void>::ok();
    }

    Result<void> on_enable(ecs::World& world) override {
        // Register systems
        world.add_system<CharacterProgressionSystem>();
        world.add_system<InventorySystem>();
        world.add_system<EquipmentSystem>();
        world.add_system<QuestSystem>();
        world.add_system<SkillSystem>();
        world.add_system<GuildSystem>();
        world.add_system<AchievementSystem>();

        // Combat systems
        world.add_system<SpellCastSystem>(spell_data_);
        world.add_system<AuraSystem>(aura_data_);
        world.add_system<CooldownSystem>();

        // Subscribe to events
        ctx_->subscribe<PlayerLoginEvent>([this, &world](const auto& e) {
            on_player_login(world, e);
        });

        ctx_->subscribe<PlayerLogoutEvent>([this, &world](const auto& e) {
            on_player_logout(world, e);
        });

        ctx_->subscribe<EntityDeathEvent>([this, &world](const auto& e) {
            on_entity_death(world, e);
        });

        logger_.info("MMORPG plugin enabled");
        return Result<void>::ok();
    }

    Result<void> on_disable(ecs::World& world) override {
        // Systems will be removed automatically
        logger_.info("MMORPG plugin disabled");
        return Result<void>::ok();
    }

    PluginInfo info() const override {
        return {
            .name = "mmorpg",
            .version = "1.0.0",
            .description = "MMORPG game mechanics including character progression, "
                          "inventory, quests, guilds, and combat systems",
            .author = "Common Game Server Team",
            .dependencies = {},  // No dependencies
            .optional_dependencies = {"analytics", "chat"},
            .conflicts = {"battle-royale"},  // Can't run both
            .min_framework_version = "1.0.0"
        };
    }

    // ==================== Hooks ====================

    void on_tick(ecs::World& world, float dt) override {
        // Per-tick processing if needed
        update_quest_timers(world, dt);
        update_buff_timers(world, dt);
    }

    void on_player_join(ecs::World& world, EntityId player) override {
        // Initialize player components
        initialize_player(world, player);
    }

    void on_player_leave(ecs::World& world, EntityId player) override {
        // Save player data
        save_player_data(world, player);
    }

private:
    // ==================== Packet Handlers ====================

    void handle_cast_spell(ecs::World& world, EntityId caster,
                          const Packet& packet) {
        auto spell_id = packet.read<SpellId>();
        auto target_id = packet.read<EntityId>();

        // Validate spell
        if (!can_cast_spell(world, caster, spell_id, target_id)) {
            send_cast_error(world, caster, spell_id, CastError::InvalidTarget);
            return;
        }

        // Start casting
        auto& cast = world.get_or_add<SpellCastComponent>(caster);
        cast.spell_id = spell_id;
        cast.target = target_id;
        cast.state = SpellCastComponent::State::Casting;
        cast.cast_time = spell_data_.get_cast_time(spell_id);
        cast.elapsed = 0;
    }

    void handle_use_item(ecs::World& world, EntityId player,
                        const Packet& packet) {
        auto slot = packet.read<InventorySlot>();

        auto& inventory = world.get<InventoryComponent>(player);
        auto& item = inventory.get_item(slot);

        if (item.is_empty()) {
            return;
        }

        use_item(world, player, item);
    }

    void handle_quest_accept(ecs::World& world, EntityId player,
                            const Packet& packet) {
        auto quest_id = packet.read<QuestId>();

        if (!can_accept_quest(world, player, quest_id)) {
            send_quest_error(world, player, quest_id, QuestError::NotAvailable);
            return;
        }

        accept_quest(world, player, quest_id);
    }

    // ==================== Event Handlers ====================

    void on_player_login(ecs::World& world, const PlayerLoginEvent& event) {
        // Load character data from database
        auto result = load_character(event.character_id);
        if (!result) {
            logger_.error("Failed to load character: {}", event.character_id);
            return;
        }

        // Apply to entity
        apply_character_data(world, event.entity, result.value());
    }

    void on_player_logout(ecs::World& world, const PlayerLogoutEvent& event) {
        save_player_data(world, event.entity);
    }

    void on_entity_death(ecs::World& world, const EntityDeathEvent& event) {
        // Award experience to killer
        if (event.killer != INVALID_ENTITY &&
            world.has<PlayerComponent>(event.killer)) {
            award_kill_experience(world, event.killer, event.entity);
        }

        // Handle loot
        if (world.has<CreatureComponent>(event.entity)) {
            spawn_loot(world, event.entity);
        }
    }

    // ==================== Commands ====================

    Result<void> cmd_add_item(ecs::World& world, EntityId player,
                              const std::vector<std::string>& args) {
        if (args.size() < 1) {
            return Result<void>::error(Error::InvalidInput, "Usage: /additem <item_id> [count]");
        }

        auto item_id = std::stoi(args[0]);
        int count = args.size() > 1 ? std::stoi(args[1]) : 1;

        auto& inventory = world.get<InventoryComponent>(player);
        inventory.add_item(ItemId(item_id), count);

        return Result<void>::ok();
    }

    Result<void> cmd_level_up(ecs::World& world, EntityId player,
                              const std::vector<std::string>& args) {
        auto& character = world.get<CharacterComponent>(player);
        character.level++;
        recalculate_stats(world, player);

        ctx_->emit(LevelUpEvent{player, character.level});

        return Result<void>::ok();
    }

    // ==================== Helpers ====================

    void initialize_player(ecs::World& world, EntityId player) {
        // Ensure all required components exist
        world.get_or_add<CharacterComponent>(player);
        world.get_or_add<InventoryComponent>(player);
        world.get_or_add<EquipmentComponent>(player);
        world.get_or_add<QuestLogComponent>(player);
        world.get_or_add<SkillBarComponent>(player);
        world.get_or_add<AchievementComponent>(player);
        world.get_or_add<CooldownComponent>(player);
    }

    void load_config() {
        // Load spell data, quest data, etc.
        spell_data_.load("data/spells.yaml");
        aura_data_.load("data/auras.yaml");
        quest_data_.load("data/quests.yaml");
    }

    PluginContext* ctx_ = nullptr;
    GameLogger logger_;

    SpellData spell_data_;
    AuraData aura_data_;
    QuestData quest_data_;
};

// Plugin factory function (for dynamic loading)
extern "C" GamePlugin* create_plugin() {
    return new MMORPGPlugin();
}

}  // namespace game::plugins
```

### 4.2 MMORPG Systems

```cpp
namespace game::plugins::mmorpg {

// Character progression system
class CharacterProgressionSystem : public ecs::System {
public:
    void update(ecs::World& world, float dt) override {
        for (auto entity : world.view<CharacterComponent, ExperienceComponent>()) {
            auto& character = world.get<CharacterComponent>(entity);
            auto& experience = world.get<ExperienceComponent>(entity);

            // Check for level up
            while (experience.current >= experience.required) {
                experience.current -= experience.required;
                character.level++;
                experience.required = calculate_required_exp(character.level);

                // Emit level up event
                world.emit<LevelUpEvent>({entity, character.level});
            }
        }
    }

    std::string_view name() const override { return "CharacterProgression"; }
};

// Inventory system
class InventorySystem : public ecs::System {
public:
    void update(ecs::World& world, float dt) override {
        // Process pending inventory operations
        for (auto entity : world.view<InventoryComponent, PendingInventoryOps>()) {
            auto& inventory = world.get<InventoryComponent>(entity);
            auto& pending = world.get<PendingInventoryOps>(entity);

            for (auto& op : pending.operations) {
                process_operation(world, entity, inventory, op);
            }

            pending.operations.clear();
        }
    }

    std::string_view name() const override { return "Inventory"; }

private:
    void process_operation(ecs::World& world, EntityId entity,
                          InventoryComponent& inventory,
                          const InventoryOperation& op) {
        switch (op.type) {
            case InventoryOpType::Add:
                inventory.add_item(op.item_id, op.count);
                break;
            case InventoryOpType::Remove:
                inventory.remove_item(op.slot, op.count);
                break;
            case InventoryOpType::Move:
                inventory.move_item(op.source_slot, op.dest_slot);
                break;
            case InventoryOpType::Split:
                inventory.split_stack(op.slot, op.count);
                break;
        }
    }
};

// Quest system
class QuestSystem : public ecs::System {
public:
    void update(ecs::World& world, float dt) override {
        for (auto entity : world.view<QuestLogComponent>()) {
            auto& quests = world.get<QuestLogComponent>(entity);

            for (auto& quest : quests.active_quests) {
                // Check completion
                if (check_quest_complete(world, entity, quest)) {
                    complete_quest(world, entity, quest);
                }

                // Update timed quests
                if (quest.has_timer) {
                    quest.time_remaining -= dt;
                    if (quest.time_remaining <= 0) {
                        fail_quest(world, entity, quest);
                    }
                }
            }
        }
    }

    std::string_view name() const override { return "Quest"; }
};

}  // namespace game::plugins::mmorpg
```

---

## 5. Plugin Configuration

### 5.1 Configuration Format

```yaml
# plugins/mmorpg/config.yaml

plugin:
  name: mmorpg
  enabled: true

character:
  max_level: 60
  base_health: 100
  health_per_level: 10
  base_mana: 50
  mana_per_level: 5

experience:
  base_required: 100
  growth_factor: 1.5

combat:
  global_cooldown: 1.5
  max_targets: 10
  leash_distance: 40.0

inventory:
  max_bag_slots: 4
  default_bag_size: 16
  bank_slots: 28

quests:
  max_active: 25
  max_completed_tracked: 100

guilds:
  max_members: 100
  create_cost: 1000
  bank_tabs: 6
```

### 5.2 Configuration API

```cpp
namespace game::plugin {

class PluginConfig {
public:
    void load(const std::filesystem::path& path) {
        // Load YAML configuration
        auto yaml = YAML::LoadFile(path.string());
        root_ = yaml;
    }

    template<typename T>
    T get(const std::string& key, T default_value = T{}) const {
        try {
            auto node = resolve_key(key);
            return node.as<T>();
        } catch (...) {
            return default_value;
        }
    }

    bool has(const std::string& key) const {
        try {
            resolve_key(key);
            return true;
        } catch (...) {
            return false;
        }
    }

private:
    YAML::Node resolve_key(const std::string& key) const {
        // Support dot notation: "combat.global_cooldown"
        auto parts = split(key, '.');
        YAML::Node node = root_;
        for (const auto& part : parts) {
            node = node[part];
        }
        return node;
    }

    YAML::Node root_;
};

}  // namespace game::plugin
```

---

## 6. Inter-Plugin Communication

### 6.1 Plugin Events

```cpp
namespace game::plugin {

// Plugins can define custom events
struct PluginEvent {
    std::string source_plugin;
    std::string event_type;
    std::any data;
};

// Chat plugin example
namespace chat {

struct ChatMessageEvent {
    EntityId sender;
    std::string channel;
    std::string message;
};

struct ChatMuteEvent {
    EntityId target;
    EntityId issuer;
    std::chrono::seconds duration;
    std::string reason;
};

}  // namespace chat

// Analytics plugin example
namespace analytics {

struct TrackEvent {
    std::string event_name;
    std::unordered_map<std::string, std::string> properties;
};

}  // namespace analytics

}  // namespace game::plugin
```

### 6.2 Plugin Services

```cpp
namespace game::plugin {

// Plugins can expose services for other plugins
class AnalyticsPlugin : public GamePlugin {
public:
    // Public API for other plugins
    void track_event(const std::string& name,
                    const std::unordered_map<std::string, std::string>& props) {
        events_.push_back({name, props, std::chrono::system_clock::now()});
    }

    void track_player_action(EntityId player, const std::string& action) {
        track_event("player_action", {
            {"player_id", std::to_string(player)},
            {"action", action}
        });
    }
};

// MMORPG plugin using analytics
class MMORPGPlugin : public GamePlugin {
    void on_player_level_up(ecs::World& world, EntityId player, int level) {
        // Track with analytics plugin if available
        if (auto* analytics = ctx_->get_plugin<AnalyticsPlugin>("analytics")) {
            analytics->track_event("level_up", {
                {"player_id", std::to_string(player)},
                {"level", std::to_string(level)}
            });
        }
    }
};

}  // namespace game::plugin
```

---

## 7. Plugin Development Guide

### 7.1 Creating a New Plugin

```cpp
// 1. Create plugin class
class MyGamePlugin : public game::plugin::GamePlugin {
public:
    Result<void> on_load(PluginContext& ctx) override {
        // Register components
        ctx.register_component<MyComponent>();

        // Register packet handlers
        ctx.register_packet_handler(MY_PACKET_TYPE, ...);

        return Result<void>::ok();
    }

    Result<void> on_unload() override {
        return Result<void>::ok();
    }

    Result<void> on_enable(ecs::World& world) override {
        // Register systems
        world.add_system<MySystem>();
        return Result<void>::ok();
    }

    Result<void> on_disable(ecs::World& world) override {
        return Result<void>::ok();
    }

    PluginInfo info() const override {
        return {
            .name = "my-game",
            .version = "1.0.0",
            .description = "My custom game plugin"
        };
    }
};

// 2. Export factory function (for dynamic loading)
extern "C" game::plugin::GamePlugin* create_plugin() {
    return new MyGamePlugin();
}
```

### 7.2 Best Practices

| Practice | Description |
|----------|-------------|
| **Single Responsibility** | Each plugin should do one thing well |
| **Loose Coupling** | Use events for cross-plugin communication |
| **Configuration** | Make behavior configurable, not hardcoded |
| **Error Handling** | Return Result<T> for all operations |
| **Logging** | Log important events for debugging |
| **Testing** | Write unit tests for plugin logic |

---

## 8. Appendices

### 8.1 Plugin Lifecycle

```
┌─────────────────────────────────────────────────────────────┐
│                     Plugin Lifecycle                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────┐     ┌─────────┐     ┌─────────┐               │
│  │ Loaded  │────>│ Enabled │────>│ Running │               │
│  │         │     │         │     │         │               │
│  │on_load()│     │on_enable│     │on_tick()│               │
│  └────┬────┘     └────┬────┘     └────┬────┘               │
│       │               │               │                     │
│       │               │               │                     │
│       │               │    ┌──────────┘                     │
│       │               │    │                                │
│       │               v    v                                │
│       │          ┌─────────────┐                            │
│       │          │  Disabled   │                            │
│       │          │             │                            │
│       │          │ on_disable()│                            │
│       │          └──────┬──────┘                            │
│       │                 │                                   │
│       v                 v                                   │
│  ┌─────────────────────────┐                               │
│  │       Unloaded          │                               │
│  │                         │                               │
│  │      on_unload()        │                               │
│  └─────────────────────────┘                               │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 8.2 Related Documents

- [ARCHITECTURE.md](../ARCHITECTURE.md) - System architecture
- [ECS_DESIGN.md](./ECS_DESIGN.md) - ECS integration

---

*Plugin System Version*: 1.0.0
*Plugin System Design for Common Game Server*
