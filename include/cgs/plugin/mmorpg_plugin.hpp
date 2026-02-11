#pragma once

/// @file mmorpg_plugin.hpp
/// @brief Reference MMORPG plugin integrating all six game systems.
///
/// MMORPGPlugin is a complete reference implementation that demonstrates
/// how to compose the game server's ECS, game logic systems, and plugin
/// infrastructure into a working MMORPG server module.
///
/// Owns all ECS infrastructure:
///   - EntityManager (entity lifecycle)
///   - 18 ComponentStorages (all game component types)
///   - SystemScheduler (staged execution of 6 game systems)
///
/// Provides high-level game operations:
///   - Character lifecycle (create, remove, query)
///   - NPC/Creature spawning
///   - Guild management (create, join, leave, disband)
///   - Chat system (multi-channel messaging)
///   - Map instance management
///
/// @see SDS-MOD-023
/// @see Issue #29

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/ai_components.hpp"
#include "cgs/game/combat_components.hpp"
#include "cgs/game/components.hpp"
#include "cgs/game/inventory_components.hpp"
#include "cgs/game/math_types.hpp"
#include "cgs/game/quest_components.hpp"
#include "cgs/game/world_components.hpp"
#include "cgs/plugin/iplugin.hpp"
#include "cgs/plugin/mmorpg_types.hpp"

namespace cgs::plugin {

/// Reference MMORPG plugin that integrates all game systems.
///
/// System execution order per tick (managed by SystemScheduler):
///   PreUpdate:  WorldSystem (spatial index synchronization)
///   Update:     ObjectUpdateSystem, CombatSystem, AISystem
///   PostUpdate: QuestSystem, InventorySystem
class MMORPGPlugin : public IPlugin {
public:
    MMORPGPlugin();
    ~MMORPGPlugin() override;

    // ── IPlugin interface ──────────────────────────────────────────────

    [[nodiscard]] const PluginInfo& GetInfo() const override;
    bool OnLoad(PluginContext& ctx) override;
    bool OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    void OnUnload() override;

    // ── Character management ───────────────────────────────────────────

    /// Create a player character entity with all required components.
    ///
    /// Initializes: Identity, Transform, Stats, Movement, QuestLog,
    /// Inventory, Equipment, SpellCast, AuraHolder, ThreatList,
    /// VisibilityRange.
    ///
    /// @param name  Character display name.
    /// @param cls   Character class (determines starting stats).
    /// @param position  Spawn position in world space.
    /// @param mapEntity Map instance to place the character in.
    /// @return The created entity handle.
    [[nodiscard]] cgs::ecs::Entity CreateCharacter(
        const std::string& name,
        CharacterClass cls,
        const cgs::game::Vector3& position,
        cgs::ecs::Entity mapEntity);

    /// Remove a player character and clean up all components.
    void RemoveCharacter(cgs::ecs::Entity entity);

    /// Get character metadata for an entity (nullptr if not a character).
    [[nodiscard]] const CharacterData*
    GetCharacterData(cgs::ecs::Entity entity) const;

    /// Number of active player characters.
    [[nodiscard]] std::size_t PlayerCount() const noexcept;

    // ── NPC / Creature spawning ────────────────────────────────────────

    /// Spawn an AI-controlled creature.
    ///
    /// Initializes: Identity, Transform, Stats, Movement, AIBrain,
    /// SpellCast, AuraHolder, ThreatList, MapMembership.
    ///
    /// @param entry     Creature template/entry ID.
    /// @param name      Display name.
    /// @param position  Spawn position.
    /// @param mapEntity Map instance to place the creature in.
    /// @return The created entity handle.
    [[nodiscard]] cgs::ecs::Entity SpawnCreature(
        uint32_t entry,
        const std::string& name,
        const cgs::game::Vector3& position,
        cgs::ecs::Entity mapEntity);

    /// Remove a creature entity.
    void RemoveCreature(cgs::ecs::Entity entity);

    // ── Guild management ───────────────────────────────────────────────

    /// Create a new guild with the specified leader.
    ///
    /// @return Guild ID (0 on failure).
    [[nodiscard]] uint32_t CreateGuild(
        const std::string& name, cgs::ecs::Entity leader);

    /// Disband a guild and clear all member associations.
    bool DisbandGuild(uint32_t guildId);

    /// Add a player to a guild.
    bool JoinGuild(cgs::ecs::Entity entity, uint32_t guildId);

    /// Remove a player from their current guild.
    bool LeaveGuild(cgs::ecs::Entity entity);

    /// Look up guild data by ID (nullptr if not found).
    [[nodiscard]] const GuildData* GetGuild(uint32_t guildId) const;

    /// Number of active guilds.
    [[nodiscard]] std::size_t GuildCount() const noexcept;

    // ── Chat system ────────────────────────────────────────────────────

    /// Send a chat message to the specified channel.
    void SendChat(cgs::ecs::Entity sender, ChatChannel channel,
                  const std::string& message);

    /// Retrieve the most recent messages from a channel.
    [[nodiscard]] std::vector<ChatMessage>
    GetChatHistory(ChatChannel channel, std::size_t count) const;

    // ── Map management ─────────────────────────────────────────────────

    /// Create a new map instance entity.
    [[nodiscard]] cgs::ecs::Entity CreateMapInstance(
        uint32_t mapId, cgs::game::MapType type);

    // ── ECS access (for testing and advanced usage) ────────────────────

    [[nodiscard]] cgs::ecs::EntityManager& GetEntityManager() noexcept;
    [[nodiscard]] cgs::ecs::SystemScheduler& GetScheduler() noexcept;

private:
    /// Initialize starting stats for each character class.
    void initializeClassTemplates();

    /// Register all component storages with the entity manager.
    void registerComponentStorages();

    /// Get class template for the given character class.
    [[nodiscard]] const ClassTemplate&
    getClassTemplate(CharacterClass cls) const;

    PluginInfo info_;
    PluginContext* ctx_ = nullptr;

    // ── ECS core infrastructure ────────────────────────────────────────

    cgs::ecs::EntityManager entityManager_;
    cgs::ecs::SystemScheduler scheduler_;

    // ── Component storages (18 types) ──────────────────────────────────

    // Object system components
    cgs::ecs::ComponentStorage<cgs::game::Transform> transforms_;
    cgs::ecs::ComponentStorage<cgs::game::Identity> identities_;
    cgs::ecs::ComponentStorage<cgs::game::Stats> stats_;
    cgs::ecs::ComponentStorage<cgs::game::Movement> movements_;

    // Combat system components
    cgs::ecs::ComponentStorage<cgs::game::SpellCast> spellCasts_;
    cgs::ecs::ComponentStorage<cgs::game::AuraHolder> auraHolders_;
    cgs::ecs::ComponentStorage<cgs::game::DamageEvent> damageEvents_;
    cgs::ecs::ComponentStorage<cgs::game::ThreatList> threatLists_;

    // World system components
    cgs::ecs::ComponentStorage<cgs::game::MapMembership> mapMemberships_;
    cgs::ecs::ComponentStorage<cgs::game::MapInstance> mapInstances_;
    cgs::ecs::ComponentStorage<cgs::game::VisibilityRange> visibilityRanges_;
    cgs::ecs::ComponentStorage<cgs::game::Zone> zones_;

    // AI system components
    cgs::ecs::ComponentStorage<cgs::game::AIBrain> aiBrains_;

    // Quest system components
    cgs::ecs::ComponentStorage<cgs::game::QuestLog> questLogs_;
    cgs::ecs::ComponentStorage<cgs::game::QuestEvent> questEvents_;

    // Inventory system components
    cgs::ecs::ComponentStorage<cgs::game::Inventory> inventories_;
    cgs::ecs::ComponentStorage<cgs::game::Equipment> equipment_;
    cgs::ecs::ComponentStorage<cgs::game::DurabilityEvent> durabilityEvents_;

    // ── MMORPG-level data ──────────────────────────────────────────────

    std::unordered_map<cgs::ecs::Entity, CharacterData> characters_;
    std::unordered_map<uint32_t, GuildData> guilds_;
    std::array<std::vector<ChatMessage>, kChatChannelCount> chatHistory_;
    std::array<ClassTemplate, kCharacterClassCount> classTemplates_;
    uint32_t nextGuildId_ = 1;
    uint32_t nextInstanceId_ = 1;
};

} // namespace cgs::plugin
