/// @file mmorpg_plugin.cpp
/// @brief Implementation of the reference MMORPG plugin.
///
/// @see mmorpg_plugin.hpp

#include "cgs/plugin/mmorpg_plugin.hpp"

#include <algorithm>
#include <cstring>

#include "cgs/game/ai_system.hpp"
#include "cgs/game/combat_system.hpp"
#include "cgs/game/inventory_system.hpp"
#include "cgs/game/object_system.hpp"
#include "cgs/game/quest_system.hpp"
#include "cgs/game/world_system.hpp"

namespace cgs::plugin {

// ============================================================================
// Construction / Destruction
// ============================================================================

MMORPGPlugin::MMORPGPlugin()
    : info_{"MMORPGPlugin",
            "Reference MMORPG plugin integrating all game systems",
            {1, 0, 0},
            {},
            kPluginApiVersion} {
    initializeClassTemplates();
}

MMORPGPlugin::~MMORPGPlugin() = default;

// ============================================================================
// IPlugin Interface
// ============================================================================

const PluginInfo& MMORPGPlugin::GetInfo() const {
    return info_;
}

bool MMORPGPlugin::OnLoad(PluginContext& ctx) {
    ctx_ = &ctx;
    registerComponentStorages();
    return true;
}

bool MMORPGPlugin::OnInit() {
    // Register all six game systems in the scheduler.
    // The scheduler groups them by stage automatically:
    //   PreUpdate:  WorldSystem
    //   Update:     ObjectUpdateSystem, CombatSystem, AISystem
    //   PostUpdate: QuestSystem, InventorySystem

    scheduler_.Register<cgs::game::WorldSystem>(
        transforms_, mapMemberships_, mapInstances_,
        visibilityRanges_, zones_);

    scheduler_.Register<cgs::game::ObjectUpdateSystem>(
        transforms_, movements_);

    scheduler_.Register<cgs::game::CombatSystem>(
        spellCasts_, auraHolders_, damageEvents_, stats_, threatLists_);

    scheduler_.Register<cgs::game::AISystem>(
        aiBrains_, transforms_, movements_, stats_, threatLists_);

    scheduler_.Register<cgs::game::QuestSystem>(
        questLogs_, questEvents_);

    scheduler_.Register<cgs::game::InventorySystem>(
        inventories_, equipment_, durabilityEvents_);

    // Build the execution plan (topological sort per stage).
    if (!scheduler_.Build()) {
        return false;
    }

    return true;
}

void MMORPGPlugin::OnUpdate(float deltaTime) {
    scheduler_.Execute(deltaTime);
    entityManager_.FlushDeferred();
}

void MMORPGPlugin::OnShutdown() {
    characters_.clear();
    guilds_.clear();
    for (auto& history : chatHistory_) {
        history.clear();
    }
}

void MMORPGPlugin::OnUnload() {
    ctx_ = nullptr;
}

// ============================================================================
// Character Management
// ============================================================================

cgs::ecs::Entity MMORPGPlugin::CreateCharacter(
    const std::string& name,
    CharacterClass cls,
    const cgs::game::Vector3& position,
    cgs::ecs::Entity mapEntity) {

    auto entity = entityManager_.Create();
    const auto& tmpl = getClassTemplate(cls);

    // Identity
    identities_.Add(entity, cgs::game::Identity{
        cgs::game::GenerateGUID(),
        name,
        cgs::game::ObjectType::Player,
        0});

    // Transform
    transforms_.Add(entity, cgs::game::Transform{position, {}, {}});

    // Stats
    cgs::game::Stats charStats;
    charStats.health = tmpl.baseHealth;
    charStats.maxHealth = tmpl.baseHealth;
    charStats.mana = tmpl.baseMana;
    charStats.maxMana = tmpl.baseMana;
    stats_.Add(entity, std::move(charStats));

    // Movement
    cgs::game::Movement mov;
    mov.speed = tmpl.baseSpeed;
    mov.baseSpeed = tmpl.baseSpeed;
    movements_.Add(entity, std::move(mov));

    // Combat components
    spellCasts_.Add(entity, cgs::game::SpellCast{});
    auraHolders_.Add(entity, cgs::game::AuraHolder{});
    threatLists_.Add(entity, cgs::game::ThreatList{});

    // World membership
    mapMemberships_.Add(entity, cgs::game::MapMembership{mapEntity, 0});
    visibilityRanges_.Add(entity, cgs::game::VisibilityRange{});

    // Quest log
    questLogs_.Add(entity, cgs::game::QuestLog{});

    // Inventory and equipment
    cgs::game::Inventory inv;
    inv.Initialize();
    inventories_.Add(entity, std::move(inv));
    equipment_.Add(entity, cgs::game::Equipment{});

    // Track character data
    CharacterData data;
    data.characterClass = cls;
    data.level = 1;
    data.experience = 0;
    data.guildId = 0;
    data.name = name;
    characters_.insert_or_assign(entity, std::move(data));

    return entity;
}

void MMORPGPlugin::RemoveCharacter(cgs::ecs::Entity entity) {
    // Remove from guild if in one.
    auto it = characters_.find(entity);
    if (it != characters_.end()) {
        if (it->second.guildId != 0) {
            LeaveGuild(entity);
        }
        characters_.erase(it);
    }

    entityManager_.Destroy(entity);
}

const CharacterData*
MMORPGPlugin::GetCharacterData(cgs::ecs::Entity entity) const {
    auto it = characters_.find(entity);
    if (it == characters_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::size_t MMORPGPlugin::PlayerCount() const noexcept {
    return characters_.size();
}

// ============================================================================
// NPC / Creature Spawning
// ============================================================================

cgs::ecs::Entity MMORPGPlugin::SpawnCreature(
    uint32_t entry,
    const std::string& name,
    const cgs::game::Vector3& position,
    cgs::ecs::Entity mapEntity) {

    auto entity = entityManager_.Create();

    // Identity
    identities_.Add(entity, cgs::game::Identity{
        cgs::game::GenerateGUID(),
        name,
        cgs::game::ObjectType::Creature,
        entry});

    // Transform
    transforms_.Add(entity, cgs::game::Transform{position, {}, {}});

    // Stats (basic creature stats)
    cgs::game::Stats creatureStats;
    creatureStats.health = 100;
    creatureStats.maxHealth = 100;
    creatureStats.mana = 50;
    creatureStats.maxMana = 50;
    stats_.Add(entity, std::move(creatureStats));

    // Movement
    cgs::game::Movement mov;
    mov.speed = 3.0f;
    mov.baseSpeed = 3.0f;
    movements_.Add(entity, std::move(mov));

    // Combat components
    spellCasts_.Add(entity, cgs::game::SpellCast{});
    auraHolders_.Add(entity, cgs::game::AuraHolder{});
    threatLists_.Add(entity, cgs::game::ThreatList{});

    // AI brain (default idle behavior)
    cgs::game::AIBrain brain;
    brain.state = cgs::game::AIState::Idle;
    brain.homePosition = position;
    aiBrains_.Add(entity, std::move(brain));

    // World membership
    mapMemberships_.Add(entity, cgs::game::MapMembership{mapEntity, 0});

    return entity;
}

void MMORPGPlugin::RemoveCreature(cgs::ecs::Entity entity) {
    entityManager_.Destroy(entity);
}

// ============================================================================
// Guild Management
// ============================================================================

uint32_t MMORPGPlugin::CreateGuild(
    const std::string& name, cgs::ecs::Entity leader) {

    // Verify the leader is a known character.
    auto* charData = GetCharacterData(leader);
    if (charData == nullptr) {
        return 0;
    }

    // Leader must not already be in a guild.
    if (charData->guildId != 0) {
        return 0;
    }

    uint32_t guildId = nextGuildId_++;

    GuildData guild;
    guild.id = guildId;
    guild.name = name;
    guild.leader = leader;
    guild.members.push_back(
        GuildMember{leader, charData->name, GuildRank::Leader});

    guilds_.insert_or_assign(guildId, std::move(guild));

    // Update character data.
    characters_[leader].guildId = guildId;

    return guildId;
}

bool MMORPGPlugin::DisbandGuild(uint32_t guildId) {
    auto it = guilds_.find(guildId);
    if (it == guilds_.end()) {
        return false;
    }

    // Clear guild association for all members.
    for (const auto& member : it->second.members) {
        auto charIt = characters_.find(member.entity);
        if (charIt != characters_.end()) {
            charIt->second.guildId = 0;
        }
    }

    guilds_.erase(it);
    return true;
}

bool MMORPGPlugin::JoinGuild(cgs::ecs::Entity entity, uint32_t guildId) {
    auto* charData = GetCharacterData(entity);
    if (charData == nullptr || charData->guildId != 0) {
        return false;
    }

    auto guildIt = guilds_.find(guildId);
    if (guildIt == guilds_.end()) {
        return false;
    }

    auto& guild = guildIt->second;
    if (guild.members.size() >= guild.maxMembers) {
        return false;
    }

    guild.members.push_back(
        GuildMember{entity, charData->name, GuildRank::Member});
    characters_[entity].guildId = guildId;

    return true;
}

bool MMORPGPlugin::LeaveGuild(cgs::ecs::Entity entity) {
    auto charIt = characters_.find(entity);
    if (charIt == characters_.end() || charIt->second.guildId == 0) {
        return false;
    }

    uint32_t guildId = charIt->second.guildId;
    auto guildIt = guilds_.find(guildId);
    if (guildIt != guilds_.end()) {
        auto& members = guildIt->second.members;
        std::erase_if(members, [entity](const GuildMember& m) {
            return m.entity == entity;
        });

        // If the guild is now empty, disband it.
        if (members.empty()) {
            guilds_.erase(guildIt);
        }
    }

    charIt->second.guildId = 0;
    return true;
}

const GuildData* MMORPGPlugin::GetGuild(uint32_t guildId) const {
    auto it = guilds_.find(guildId);
    if (it == guilds_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::size_t MMORPGPlugin::GuildCount() const noexcept {
    return guilds_.size();
}

// ============================================================================
// Chat System
// ============================================================================

void MMORPGPlugin::SendChat(
    cgs::ecs::Entity sender,
    ChatChannel channel,
    const std::string& message) {

    auto idx = static_cast<std::size_t>(channel);
    if (idx >= kChatChannelCount) {
        return;
    }

    std::string senderName;
    auto* charData = GetCharacterData(sender);
    if (charData != nullptr) {
        senderName = charData->name;
    }

    auto& history = chatHistory_[idx];
    history.push_back(ChatMessage{
        sender,
        channel,
        std::move(senderName),
        message,
        std::chrono::steady_clock::now()});

    // Trim to max history size.
    if (history.size() > kMaxChatHistoryPerChannel) {
        history.erase(
            history.begin(),
            history.begin()
                + static_cast<std::ptrdiff_t>(
                    history.size() - kMaxChatHistoryPerChannel));
    }
}

std::vector<ChatMessage>
MMORPGPlugin::GetChatHistory(ChatChannel channel, std::size_t count) const {
    auto idx = static_cast<std::size_t>(channel);
    if (idx >= kChatChannelCount) {
        return {};
    }

    const auto& history = chatHistory_[idx];
    if (count >= history.size()) {
        return history;
    }

    return {history.end() - static_cast<std::ptrdiff_t>(count),
            history.end()};
}

// ============================================================================
// Map Management
// ============================================================================

cgs::ecs::Entity MMORPGPlugin::CreateMapInstance(
    uint32_t mapId, cgs::game::MapType type) {

    auto entity = entityManager_.Create();

    cgs::game::MapInstance mapInst;
    mapInst.mapId = mapId;
    mapInst.instanceId = nextInstanceId_++;
    mapInst.type = type;
    mapInstances_.Add(entity, std::move(mapInst));

    return entity;
}

// ============================================================================
// ECS Access
// ============================================================================

cgs::ecs::EntityManager& MMORPGPlugin::GetEntityManager() noexcept {
    return entityManager_;
}

cgs::ecs::SystemScheduler& MMORPGPlugin::GetScheduler() noexcept {
    return scheduler_;
}

// ============================================================================
// Private Helpers
// ============================================================================

void MMORPGPlugin::initializeClassTemplates() {
    classTemplates_[static_cast<std::size_t>(CharacterClass::Warrior)] =
        {CharacterClass::Warrior, 200, 50, 5.0f};
    classTemplates_[static_cast<std::size_t>(CharacterClass::Mage)] =
        {CharacterClass::Mage, 80, 250, 4.5f};
    classTemplates_[static_cast<std::size_t>(CharacterClass::Priest)] =
        {CharacterClass::Priest, 100, 200, 4.5f};
    classTemplates_[static_cast<std::size_t>(CharacterClass::Rogue)] =
        {CharacterClass::Rogue, 120, 80, 6.0f};
    classTemplates_[static_cast<std::size_t>(CharacterClass::Ranger)] =
        {CharacterClass::Ranger, 110, 100, 5.5f};
    classTemplates_[static_cast<std::size_t>(CharacterClass::Warlock)] =
        {CharacterClass::Warlock, 90, 220, 4.5f};
}

void MMORPGPlugin::registerComponentStorages() {
    entityManager_.RegisterStorage(&transforms_);
    entityManager_.RegisterStorage(&identities_);
    entityManager_.RegisterStorage(&stats_);
    entityManager_.RegisterStorage(&movements_);
    entityManager_.RegisterStorage(&spellCasts_);
    entityManager_.RegisterStorage(&auraHolders_);
    entityManager_.RegisterStorage(&damageEvents_);
    entityManager_.RegisterStorage(&threatLists_);
    entityManager_.RegisterStorage(&mapMemberships_);
    entityManager_.RegisterStorage(&mapInstances_);
    entityManager_.RegisterStorage(&visibilityRanges_);
    entityManager_.RegisterStorage(&zones_);
    entityManager_.RegisterStorage(&aiBrains_);
    entityManager_.RegisterStorage(&questLogs_);
    entityManager_.RegisterStorage(&questEvents_);
    entityManager_.RegisterStorage(&inventories_);
    entityManager_.RegisterStorage(&equipment_);
    entityManager_.RegisterStorage(&durabilityEvents_);
}

const ClassTemplate&
MMORPGPlugin::getClassTemplate(CharacterClass cls) const {
    auto idx = static_cast<std::size_t>(cls);
    if (idx >= kCharacterClassCount) {
        // Fallback to Warrior.
        return classTemplates_[0];
    }
    return classTemplates_[idx];
}

} // namespace cgs::plugin
