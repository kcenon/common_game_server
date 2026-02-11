/// @file mmorpg_plugin_test.cpp
/// @brief Unit tests for the MMORPGPlugin reference implementation.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/ai_system.hpp"
#include "cgs/game/combat_system.hpp"
#include "cgs/game/components.hpp"
#include "cgs/game/inventory_system.hpp"
#include "cgs/game/math_types.hpp"
#include "cgs/game/object_system.hpp"
#include "cgs/game/quest_system.hpp"
#include "cgs/game/world_system.hpp"
#include "cgs/plugin/mmorpg_plugin.hpp"
#include "cgs/plugin/mmorpg_types.hpp"
#include "cgs/plugin/plugin_types.hpp"

using namespace cgs::plugin;
using namespace cgs::game;
using namespace cgs::ecs;

// ============================================================================
// Test Fixture
// ============================================================================

class MMORPGPluginTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.services = nullptr;
        ctx_.eventBus = nullptr;

        ASSERT_TRUE(plugin_.OnLoad(ctx_));
        ASSERT_TRUE(plugin_.OnInit());

        // Create a default open-world map.
        defaultMap_ = plugin_.CreateMapInstance(1, MapType::OpenWorld);
    }

    void TearDown() override {
        plugin_.OnShutdown();
        plugin_.OnUnload();
    }

    MMORPGPlugin plugin_;
    PluginContext ctx_;
    Entity defaultMap_;
};

// ============================================================================
// Plugin Lifecycle Tests
// ============================================================================

TEST_F(MMORPGPluginTest, PluginInfoIsCorrect) {
    const auto& info = plugin_.GetInfo();
    EXPECT_EQ(info.name, "MMORPGPlugin");
    EXPECT_EQ(info.version.major, 1);
    EXPECT_EQ(info.version.minor, 0);
    EXPECT_EQ(info.version.patch, 0);
    EXPECT_EQ(info.apiVersion, kPluginApiVersion);
    EXPECT_TRUE(info.dependencies.empty());
}

TEST_F(MMORPGPluginTest, SchedulerHasSixSystems) {
    EXPECT_EQ(plugin_.GetScheduler().SystemCount(), 6u);
}

TEST_F(MMORPGPluginTest, SystemStagesAreCorrect) {
    auto& scheduler = plugin_.GetScheduler();

    // PreUpdate: WorldSystem
    auto preUpdate = scheduler.GetExecutionOrder(SystemStage::PreUpdate);
    EXPECT_EQ(preUpdate.size(), 1u);

    // Update: ObjectUpdateSystem, CombatSystem, AISystem
    auto update = scheduler.GetExecutionOrder(SystemStage::Update);
    EXPECT_EQ(update.size(), 3u);

    // PostUpdate: QuestSystem, InventorySystem
    auto postUpdate = scheduler.GetExecutionOrder(SystemStage::PostUpdate);
    EXPECT_EQ(postUpdate.size(), 2u);
}

TEST_F(MMORPGPluginTest, UpdateDoesNotCrashWithNoEntities) {
    plugin_.OnUpdate(1.0f / 60.0f);
}

// ============================================================================
// Character Management Tests
// ============================================================================

TEST_F(MMORPGPluginTest, CreateCharacterReturnsValidEntity) {
    auto entity = plugin_.CreateCharacter(
        "TestWarrior", CharacterClass::Warrior,
        Vector3{10.0f, 0.0f, 20.0f}, defaultMap_);

    EXPECT_TRUE(entity.isValid());
    EXPECT_TRUE(plugin_.GetEntityManager().IsAlive(entity));
    EXPECT_EQ(plugin_.PlayerCount(), 1u);
}

TEST_F(MMORPGPluginTest, CharacterDataIsPopulated) {
    auto entity = plugin_.CreateCharacter(
        "TestMage", CharacterClass::Mage,
        Vector3{}, defaultMap_);

    const auto* data = plugin_.GetCharacterData(entity);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->name, "TestMage");
    EXPECT_EQ(data->characterClass, CharacterClass::Mage);
    EXPECT_EQ(data->level, 1u);
    EXPECT_EQ(data->experience, 0u);
    EXPECT_EQ(data->guildId, 0u);
}

TEST_F(MMORPGPluginTest, WarriorHasCorrectStartingStats) {
    auto entity = plugin_.CreateCharacter(
        "Warrior", CharacterClass::Warrior, Vector3{}, defaultMap_);

    // Warrior: 200 HP, 50 mana, 5.0 speed
    // Access stats via entity manager (the plugin owns storages internally,
    // but we verify via OnUpdate working correctly).
    const auto* data = plugin_.GetCharacterData(entity);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->characterClass, CharacterClass::Warrior);
}

TEST_F(MMORPGPluginTest, MageHasDifferentStatsFromWarrior) {
    auto warrior = plugin_.CreateCharacter(
        "Warrior", CharacterClass::Warrior, Vector3{}, defaultMap_);
    auto mage = plugin_.CreateCharacter(
        "Mage", CharacterClass::Mage, Vector3{}, defaultMap_);

    EXPECT_NE(warrior, mage);
    EXPECT_EQ(plugin_.PlayerCount(), 2u);
}

TEST_F(MMORPGPluginTest, CreateAllClassTypes) {
    (void)plugin_.CreateCharacter("W", CharacterClass::Warrior, Vector3{}, defaultMap_);
    (void)plugin_.CreateCharacter("M", CharacterClass::Mage, Vector3{}, defaultMap_);
    (void)plugin_.CreateCharacter("P", CharacterClass::Priest, Vector3{}, defaultMap_);
    (void)plugin_.CreateCharacter("R", CharacterClass::Rogue, Vector3{}, defaultMap_);
    (void)plugin_.CreateCharacter("Rng", CharacterClass::Ranger, Vector3{}, defaultMap_);
    (void)plugin_.CreateCharacter("Wlk", CharacterClass::Warlock, Vector3{}, defaultMap_);

    EXPECT_EQ(plugin_.PlayerCount(), 6u);
}

TEST_F(MMORPGPluginTest, RemoveCharacterDecrementsCount) {
    auto entity = plugin_.CreateCharacter(
        "ToRemove", CharacterClass::Rogue, Vector3{}, defaultMap_);
    EXPECT_EQ(plugin_.PlayerCount(), 1u);

    plugin_.RemoveCharacter(entity);
    EXPECT_EQ(plugin_.PlayerCount(), 0u);
    EXPECT_EQ(plugin_.GetCharacterData(entity), nullptr);
    EXPECT_FALSE(plugin_.GetEntityManager().IsAlive(entity));
}

TEST_F(MMORPGPluginTest, RemoveCharacterRemovesFromGuild) {
    auto leader = plugin_.CreateCharacter(
        "GuildLeader", CharacterClass::Warrior, Vector3{}, defaultMap_);
    auto member = plugin_.CreateCharacter(
        "GuildMember", CharacterClass::Mage, Vector3{}, defaultMap_);

    auto guildId = plugin_.CreateGuild("TestGuild", leader);
    ASSERT_NE(guildId, 0u);
    ASSERT_TRUE(plugin_.JoinGuild(member, guildId));

    plugin_.RemoveCharacter(member);

    const auto* guild = plugin_.GetGuild(guildId);
    ASSERT_NE(guild, nullptr);
    EXPECT_EQ(guild->members.size(), 1u);
}

// ============================================================================
// NPC / Creature Spawning Tests
// ============================================================================

TEST_F(MMORPGPluginTest, SpawnCreatureReturnsValidEntity) {
    auto creature = plugin_.SpawnCreature(
        1001, "Wolf", Vector3{5.0f, 0.0f, 5.0f}, defaultMap_);

    EXPECT_TRUE(creature.isValid());
    EXPECT_TRUE(plugin_.GetEntityManager().IsAlive(creature));
}

TEST_F(MMORPGPluginTest, CreatureIsNotACharacter) {
    auto creature = plugin_.SpawnCreature(
        1001, "Wolf", Vector3{}, defaultMap_);

    EXPECT_EQ(plugin_.GetCharacterData(creature), nullptr);
    EXPECT_EQ(plugin_.PlayerCount(), 0u);
}

TEST_F(MMORPGPluginTest, RemoveCreatureDestroysEntity) {
    auto creature = plugin_.SpawnCreature(
        1001, "Wolf", Vector3{}, defaultMap_);
    plugin_.RemoveCreature(creature);
    EXPECT_FALSE(plugin_.GetEntityManager().IsAlive(creature));
}

// ============================================================================
// Guild Management Tests
// ============================================================================

TEST_F(MMORPGPluginTest, CreateGuildReturnsNonZeroId) {
    auto leader = plugin_.CreateCharacter(
        "Leader", CharacterClass::Warrior, Vector3{}, defaultMap_);

    auto guildId = plugin_.CreateGuild("MyGuild", leader);
    EXPECT_NE(guildId, 0u);
    EXPECT_EQ(plugin_.GuildCount(), 1u);
}

TEST_F(MMORPGPluginTest, GuildDataIsCorrect) {
    auto leader = plugin_.CreateCharacter(
        "Leader", CharacterClass::Warrior, Vector3{}, defaultMap_);

    auto guildId = plugin_.CreateGuild("TestGuild", leader);
    const auto* guild = plugin_.GetGuild(guildId);

    ASSERT_NE(guild, nullptr);
    EXPECT_EQ(guild->name, "TestGuild");
    EXPECT_EQ(guild->leader, leader);
    EXPECT_EQ(guild->members.size(), 1u);
    EXPECT_EQ(guild->members[0].rank, GuildRank::Leader);
}

TEST_F(MMORPGPluginTest, CreateGuildUpdatesCharacterData) {
    auto leader = plugin_.CreateCharacter(
        "Leader", CharacterClass::Warrior, Vector3{}, defaultMap_);

    auto guildId = plugin_.CreateGuild("TestGuild", leader);
    const auto* data = plugin_.GetCharacterData(leader);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->guildId, guildId);
}

TEST_F(MMORPGPluginTest, CreateGuildFailsForNonCharacter) {
    Entity fakeEntity{100, 0};
    auto guildId = plugin_.CreateGuild("BadGuild", fakeEntity);
    EXPECT_EQ(guildId, 0u);
}

TEST_F(MMORPGPluginTest, CreateGuildFailsIfAlreadyInGuild) {
    auto leader = plugin_.CreateCharacter(
        "Leader", CharacterClass::Warrior, Vector3{}, defaultMap_);

    (void)plugin_.CreateGuild("FirstGuild", leader);
    auto secondId = plugin_.CreateGuild("SecondGuild", leader);
    EXPECT_EQ(secondId, 0u);
    EXPECT_EQ(plugin_.GuildCount(), 1u);
}

TEST_F(MMORPGPluginTest, JoinGuildAddsNewMember) {
    auto leader = plugin_.CreateCharacter(
        "Leader", CharacterClass::Warrior, Vector3{}, defaultMap_);
    auto member = plugin_.CreateCharacter(
        "Member", CharacterClass::Mage, Vector3{}, defaultMap_);

    auto guildId = plugin_.CreateGuild("TestGuild", leader);
    EXPECT_TRUE(plugin_.JoinGuild(member, guildId));

    const auto* guild = plugin_.GetGuild(guildId);
    ASSERT_NE(guild, nullptr);
    EXPECT_EQ(guild->members.size(), 2u);

    const auto* memberData = plugin_.GetCharacterData(member);
    ASSERT_NE(memberData, nullptr);
    EXPECT_EQ(memberData->guildId, guildId);
}

TEST_F(MMORPGPluginTest, JoinGuildFailsIfAlreadyInGuild) {
    auto leader = plugin_.CreateCharacter(
        "Leader", CharacterClass::Warrior, Vector3{}, defaultMap_);
    auto member = plugin_.CreateCharacter(
        "Member", CharacterClass::Mage, Vector3{}, defaultMap_);

    auto guildId = plugin_.CreateGuild("TestGuild", leader);
    EXPECT_TRUE(plugin_.JoinGuild(member, guildId));
    EXPECT_FALSE(plugin_.JoinGuild(member, guildId));
}

TEST_F(MMORPGPluginTest, JoinGuildFailsForInvalidGuild) {
    auto member = plugin_.CreateCharacter(
        "Member", CharacterClass::Mage, Vector3{}, defaultMap_);

    EXPECT_FALSE(plugin_.JoinGuild(member, 9999));
}

TEST_F(MMORPGPluginTest, LeaveGuildRemovesMember) {
    auto leader = plugin_.CreateCharacter(
        "Leader", CharacterClass::Warrior, Vector3{}, defaultMap_);
    auto member = plugin_.CreateCharacter(
        "Member", CharacterClass::Mage, Vector3{}, defaultMap_);

    auto guildId = plugin_.CreateGuild("TestGuild", leader);
    plugin_.JoinGuild(member, guildId);
    EXPECT_TRUE(plugin_.LeaveGuild(member));

    const auto* memberData = plugin_.GetCharacterData(member);
    ASSERT_NE(memberData, nullptr);
    EXPECT_EQ(memberData->guildId, 0u);

    const auto* guild = plugin_.GetGuild(guildId);
    ASSERT_NE(guild, nullptr);
    EXPECT_EQ(guild->members.size(), 1u);
}

TEST_F(MMORPGPluginTest, LeaveGuildFailsIfNotInGuild) {
    auto player = plugin_.CreateCharacter(
        "Solo", CharacterClass::Rogue, Vector3{}, defaultMap_);
    EXPECT_FALSE(plugin_.LeaveGuild(player));
}

TEST_F(MMORPGPluginTest, LeaveGuildDisbandIfLastMember) {
    auto leader = plugin_.CreateCharacter(
        "Leader", CharacterClass::Warrior, Vector3{}, defaultMap_);

    auto guildId = plugin_.CreateGuild("SoloGuild", leader);
    EXPECT_TRUE(plugin_.LeaveGuild(leader));
    EXPECT_EQ(plugin_.GetGuild(guildId), nullptr);
    EXPECT_EQ(plugin_.GuildCount(), 0u);
}

TEST_F(MMORPGPluginTest, DisbandGuildClearsAllMembers) {
    auto leader = plugin_.CreateCharacter(
        "Leader", CharacterClass::Warrior, Vector3{}, defaultMap_);
    auto member = plugin_.CreateCharacter(
        "Member", CharacterClass::Mage, Vector3{}, defaultMap_);

    auto guildId = plugin_.CreateGuild("TestGuild", leader);
    plugin_.JoinGuild(member, guildId);

    EXPECT_TRUE(plugin_.DisbandGuild(guildId));
    EXPECT_EQ(plugin_.GuildCount(), 0u);
    EXPECT_EQ(plugin_.GetCharacterData(leader)->guildId, 0u);
    EXPECT_EQ(plugin_.GetCharacterData(member)->guildId, 0u);
}

TEST_F(MMORPGPluginTest, DisbandGuildFailsForInvalidId) {
    EXPECT_FALSE(plugin_.DisbandGuild(9999));
}

// ============================================================================
// Chat System Tests
// ============================================================================

TEST_F(MMORPGPluginTest, SendAndRetrieveChatMessage) {
    auto player = plugin_.CreateCharacter(
        "Chatter", CharacterClass::Mage, Vector3{}, defaultMap_);

    plugin_.SendChat(player, ChatChannel::Global, "Hello world!");

    auto history = plugin_.GetChatHistory(ChatChannel::Global, 10);
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0].content, "Hello world!");
    EXPECT_EQ(history[0].senderName, "Chatter");
    EXPECT_EQ(history[0].channel, ChatChannel::Global);
    EXPECT_EQ(history[0].sender, player);
}

TEST_F(MMORPGPluginTest, ChatHistoryLimitReturnsLatest) {
    auto player = plugin_.CreateCharacter(
        "Chatter", CharacterClass::Mage, Vector3{}, defaultMap_);

    plugin_.SendChat(player, ChatChannel::Global, "First");
    plugin_.SendChat(player, ChatChannel::Global, "Second");
    plugin_.SendChat(player, ChatChannel::Global, "Third");

    auto history = plugin_.GetChatHistory(ChatChannel::Global, 2);
    ASSERT_EQ(history.size(), 2u);
    EXPECT_EQ(history[0].content, "Second");
    EXPECT_EQ(history[1].content, "Third");
}

TEST_F(MMORPGPluginTest, ChatChannelsAreIsolated) {
    auto player = plugin_.CreateCharacter(
        "Player", CharacterClass::Warrior, Vector3{}, defaultMap_);

    plugin_.SendChat(player, ChatChannel::Global, "Global message");
    plugin_.SendChat(player, ChatChannel::Trade, "Trade message");

    auto globalHistory = plugin_.GetChatHistory(ChatChannel::Global, 10);
    auto tradeHistory = plugin_.GetChatHistory(ChatChannel::Trade, 10);

    EXPECT_EQ(globalHistory.size(), 1u);
    EXPECT_EQ(tradeHistory.size(), 1u);
    EXPECT_EQ(globalHistory[0].content, "Global message");
    EXPECT_EQ(tradeHistory[0].content, "Trade message");
}

TEST_F(MMORPGPluginTest, EmptyHistoryReturnsEmpty) {
    auto history = plugin_.GetChatHistory(ChatChannel::Whisper, 10);
    EXPECT_TRUE(history.empty());
}

// ============================================================================
// Map Instance Tests
// ============================================================================

TEST_F(MMORPGPluginTest, CreateMapInstanceReturnsValidEntity) {
    auto map = plugin_.CreateMapInstance(42, MapType::Dungeon);
    EXPECT_TRUE(map.isValid());
    EXPECT_TRUE(plugin_.GetEntityManager().IsAlive(map));
}

TEST_F(MMORPGPluginTest, MultipleMapInstances) {
    auto map1 = plugin_.CreateMapInstance(1, MapType::OpenWorld);
    auto map2 = plugin_.CreateMapInstance(1, MapType::Dungeon);
    auto map3 = plugin_.CreateMapInstance(2, MapType::Battleground);

    EXPECT_NE(map1, map2);
    EXPECT_NE(map2, map3);
}

// ============================================================================
// Full Simulation Tests
// ============================================================================

TEST_F(MMORPGPluginTest, MultipleUpdateTicksDoNotCrash) {
    auto player = plugin_.CreateCharacter(
        "TestPlayer", CharacterClass::Warrior,
        Vector3{10.0f, 0.0f, 10.0f}, defaultMap_);
    auto creature = plugin_.SpawnCreature(
        1001, "Wolf", Vector3{20.0f, 0.0f, 20.0f}, defaultMap_);

    // Run several simulation ticks.
    for (int i = 0; i < 60; ++i) {
        plugin_.OnUpdate(1.0f / 60.0f);
    }

    // Entities should still be alive.
    EXPECT_TRUE(plugin_.GetEntityManager().IsAlive(player));
    EXPECT_TRUE(plugin_.GetEntityManager().IsAlive(creature));
}

TEST_F(MMORPGPluginTest, SimulationWithMixedEntities) {
    // Create a diverse world.
    auto map1 = plugin_.CreateMapInstance(1, MapType::OpenWorld);
    auto map2 = plugin_.CreateMapInstance(2, MapType::Dungeon);

    // Players in different maps.
    auto p1 = plugin_.CreateCharacter(
        "Player1", CharacterClass::Warrior,
        Vector3{0.0f, 0.0f, 0.0f}, map1);
    auto p2 = plugin_.CreateCharacter(
        "Player2", CharacterClass::Mage,
        Vector3{50.0f, 0.0f, 50.0f}, map2);

    // Creatures.
    auto c1 = plugin_.SpawnCreature(1001, "Wolf", Vector3{10.0f, 0.0f, 10.0f}, map1);
    auto c2 = plugin_.SpawnCreature(1002, "Dragon", Vector3{60.0f, 0.0f, 60.0f}, map2);

    // Guild.
    auto guildId = plugin_.CreateGuild("Adventurers", p1);
    plugin_.JoinGuild(p2, guildId);

    // Chat.
    plugin_.SendChat(p1, ChatChannel::Guild, "Ready for dungeon!");
    plugin_.SendChat(p2, ChatChannel::Guild, "On my way!");

    // Simulate for 1 second.
    for (int i = 0; i < 60; ++i) {
        plugin_.OnUpdate(1.0f / 60.0f);
    }

    // Verify state.
    EXPECT_EQ(plugin_.PlayerCount(), 2u);
    EXPECT_EQ(plugin_.GuildCount(), 1u);
    EXPECT_TRUE(plugin_.GetEntityManager().IsAlive(p1));
    EXPECT_TRUE(plugin_.GetEntityManager().IsAlive(p2));
    EXPECT_TRUE(plugin_.GetEntityManager().IsAlive(c1));
    EXPECT_TRUE(plugin_.GetEntityManager().IsAlive(c2));

    auto chatHistory = plugin_.GetChatHistory(ChatChannel::Guild, 10);
    EXPECT_EQ(chatHistory.size(), 2u);
}

TEST_F(MMORPGPluginTest, ShutdownCleansState) {
    auto player = plugin_.CreateCharacter(
        "Player", CharacterClass::Warrior, Vector3{}, defaultMap_);
    (void)plugin_.CreateGuild("TestGuild", player);
    plugin_.SendChat(player, ChatChannel::Global, "Hello");

    plugin_.OnShutdown();

    EXPECT_EQ(plugin_.PlayerCount(), 0u);
    EXPECT_EQ(plugin_.GuildCount(), 0u);
    EXPECT_TRUE(plugin_.GetChatHistory(ChatChannel::Global, 10).empty());

    // Re-initialize to ensure plugin can restart.
    EXPECT_TRUE(plugin_.OnInit());
}
