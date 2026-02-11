#pragma once

/// @file mmorpg_types.hpp
/// @brief MMORPG-specific data types for the reference plugin.
///
/// Defines character classes, guild structures, and chat system types
/// used by the MMORPGPlugin to provide a complete game server reference
/// implementation.
///
/// @see SDS-MOD-023
/// @see Issue #29

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "cgs/ecs/entity.hpp"

namespace cgs::plugin {

// ============================================================================
// Character Classes
// ============================================================================

/// Playable character class archetypes.
enum class CharacterClass : uint8_t {
    Warrior,
    Mage,
    Priest,
    Rogue,
    Ranger,
    Warlock,
    COUNT
};

/// Number of character classes (excluding COUNT sentinel).
constexpr std::size_t kCharacterClassCount =
    static_cast<std::size_t>(CharacterClass::COUNT);

/// Base stats template for a character class.
///
/// Defines the starting attributes when a new character of this class
/// is created.  Used by MMORPGPlugin::initializeClassTemplates().
struct ClassTemplate {
    CharacterClass characterClass = CharacterClass::Warrior;
    int32_t baseHealth = 100;
    int32_t baseMana = 0;
    float baseSpeed = 5.0f;
};

/// Per-player character metadata, stored alongside the ECS entity.
///
/// The MMORPG plugin maintains a map<Entity, CharacterData> to track
/// these values outside the ECS component storages.
struct CharacterData {
    CharacterClass characterClass = CharacterClass::Warrior;
    uint32_t level = 1;
    uint64_t experience = 0;
    uint32_t guildId = 0;  ///< 0 = not in a guild.
    std::string name;
};

// ============================================================================
// Guild System
// ============================================================================

/// Guild member rank hierarchy.
enum class GuildRank : uint8_t {
    Leader,
    Officer,
    Member
};

/// A single member entry within a guild.
struct GuildMember {
    cgs::ecs::Entity entity;
    std::string name;
    GuildRank rank = GuildRank::Member;
};

/// Default maximum number of members per guild.
constexpr uint32_t kDefaultMaxGuildMembers = 50;

/// Complete guild data including roster.
struct GuildData {
    uint32_t id = 0;
    std::string name;
    cgs::ecs::Entity leader;
    std::vector<GuildMember> members;
    uint32_t maxMembers = kDefaultMaxGuildMembers;
};

// ============================================================================
// Chat System
// ============================================================================

/// Chat channel classification.
enum class ChatChannel : uint8_t {
    System,
    Global,
    Party,
    Guild,
    Whisper,
    Trade,
    COUNT
};

/// Number of chat channels (excluding COUNT sentinel).
constexpr std::size_t kChatChannelCount =
    static_cast<std::size_t>(ChatChannel::COUNT);

/// Maximum number of messages retained per channel.
constexpr std::size_t kMaxChatHistoryPerChannel = 100;

/// A single chat message.
struct ChatMessage {
    cgs::ecs::Entity sender;
    ChatChannel channel = ChatChannel::Global;
    std::string senderName;
    std::string content;
    std::chrono::steady_clock::time_point timestamp;
};

} // namespace cgs::plugin
