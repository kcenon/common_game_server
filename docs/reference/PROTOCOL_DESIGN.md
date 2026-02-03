# Protocol Design Reference

> Version: 0.1.0.0
> Last Updated: 2026-02-03
> Status: Foundation Reference

## Overview

This document defines the network protocol design for the unified game server, including packet formats, message types, serialization strategies, and communication patterns.

---

## Table of Contents

1. [Protocol Architecture](#protocol-architecture)
2. [Packet Format](#packet-format)
3. [Message Types](#message-types)
4. [Serialization](#serialization)
5. [Protocol Handlers](#protocol-handlers)
6. [Security](#security)
7. [Compression](#compression)
8. [Version Compatibility](#version-compatibility)

---

## Protocol Architecture

### Layer Design

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│              (Game Logic Messages)                          │
├─────────────────────────────────────────────────────────────┤
│                    Protocol Layer                           │
│         (Packet Framing, Serialization)                     │
├─────────────────────────────────────────────────────────────┤
│                    Security Layer                           │
│           (Encryption, Authentication)                      │
├─────────────────────────────────────────────────────────────┤
│                    Transport Layer                          │
│              (TCP/UDP, WebSocket)                           │
└─────────────────────────────────────────────────────────────┘
```

### Communication Patterns

| Pattern | Use Case | Transport |
|---------|----------|-----------|
| Request-Response | Login, Character Actions | TCP |
| Server Push | State Updates, Events | TCP |
| Broadcast | Area Events, Chat | TCP |
| Unreliable | Position Updates | UDP |
| Reliable Ordered | Combat Actions | TCP |

---

## Packet Format

### Base Packet Structure

```cpp
// packet_format.h
#pragma once

#include <cstdint>
#include <vector>
#include <span>

namespace protocol {

// Packet Header (16 bytes, fixed)
struct PacketHeader {
    uint32_t magic;          // Magic number: 0x47414D45 ('GAME')
    uint16_t version;        // Protocol version
    uint16_t type;           // Message type ID
    uint32_t length;         // Payload length (excluding header)
    uint32_t sequence;       // Sequence number for ordering
};

static_assert(sizeof(PacketHeader) == 16, "PacketHeader must be 16 bytes");

// Extended Header for encrypted packets (additional 8 bytes)
struct EncryptedPacketHeader {
    PacketHeader base;
    uint32_t checksum;       // CRC32 of encrypted payload
    uint32_t flags;          // Encryption flags
};

// Packet Constants
constexpr uint32_t PACKET_MAGIC = 0x47414D45;  // 'GAME'
constexpr uint16_t PROTOCOL_VERSION = 0x0100;   // v1.0
constexpr size_t MAX_PACKET_SIZE = 65536;       // 64KB max
constexpr size_t MIN_PACKET_SIZE = sizeof(PacketHeader);

// Packet Flags
enum class PacketFlags : uint32_t {
    None        = 0x00000000,
    Encrypted   = 0x00000001,
    Compressed  = 0x00000002,
    Fragmented  = 0x00000004,
    Priority    = 0x00000008,
    Reliable    = 0x00000010,
};

inline PacketFlags operator|(PacketFlags a, PacketFlags b) {
    return static_cast<PacketFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

inline bool hasFlag(PacketFlags flags, PacketFlags check) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(check)) != 0;
}

} // namespace protocol
```

### Packet Builder

```cpp
// packet_builder.h
#pragma once

#include "packet_format.h"
#include <memory>

namespace protocol {

class PacketBuilder {
public:
    PacketBuilder() = default;

    // Builder methods
    PacketBuilder& setType(uint16_t type) {
        header_.type = type;
        return *this;
    }

    PacketBuilder& setSequence(uint32_t seq) {
        header_.sequence = seq;
        return *this;
    }

    PacketBuilder& setPayload(std::span<const uint8_t> data) {
        payload_.assign(data.begin(), data.end());
        header_.length = static_cast<uint32_t>(payload_.size());
        return *this;
    }

    template<typename T>
    PacketBuilder& setPayload(const T& message) {
        // Serialize message to payload
        payload_ = serialize(message);
        header_.length = static_cast<uint32_t>(payload_.size());
        return *this;
    }

    PacketBuilder& encrypt(bool enable = true) {
        if (enable) {
            flags_ = flags_ | PacketFlags::Encrypted;
        }
        return *this;
    }

    PacketBuilder& compress(bool enable = true) {
        if (enable) {
            flags_ = flags_ | PacketFlags::Compressed;
        }
        return *this;
    }

    // Build final packet
    std::vector<uint8_t> build() const {
        std::vector<uint8_t> packet;

        // Prepare header
        PacketHeader header = header_;
        header.magic = PACKET_MAGIC;
        header.version = PROTOCOL_VERSION;

        // Reserve space
        packet.reserve(sizeof(PacketHeader) + payload_.size());

        // Copy header
        const uint8_t* headerBytes = reinterpret_cast<const uint8_t*>(&header);
        packet.insert(packet.end(), headerBytes, headerBytes + sizeof(PacketHeader));

        // Copy payload
        packet.insert(packet.end(), payload_.begin(), payload_.end());

        return packet;
    }

private:
    PacketHeader header_{};
    std::vector<uint8_t> payload_;
    PacketFlags flags_ = PacketFlags::None;
};

} // namespace protocol
```

### Packet Parser

```cpp
// packet_parser.h
#pragma once

#include "packet_format.h"
#include <optional>
#include <expected>

namespace protocol {

enum class ParseError {
    InsufficientData,
    InvalidMagic,
    InvalidVersion,
    InvalidLength,
    ChecksumMismatch,
    DecryptionFailed,
};

struct ParsedPacket {
    PacketHeader header;
    std::vector<uint8_t> payload;
    PacketFlags flags;
};

class PacketParser {
public:
    // Parse single packet from buffer
    static std::expected<ParsedPacket, ParseError> parse(
        std::span<const uint8_t> buffer
    ) {
        // Check minimum size
        if (buffer.size() < sizeof(PacketHeader)) {
            return std::unexpected(ParseError::InsufficientData);
        }

        // Read header
        PacketHeader header;
        std::memcpy(&header, buffer.data(), sizeof(PacketHeader));

        // Validate magic
        if (header.magic != PACKET_MAGIC) {
            return std::unexpected(ParseError::InvalidMagic);
        }

        // Validate version compatibility
        if ((header.version >> 8) != (PROTOCOL_VERSION >> 8)) {
            return std::unexpected(ParseError::InvalidVersion);
        }

        // Validate length
        size_t totalSize = sizeof(PacketHeader) + header.length;
        if (buffer.size() < totalSize) {
            return std::unexpected(ParseError::InsufficientData);
        }

        if (header.length > MAX_PACKET_SIZE - sizeof(PacketHeader)) {
            return std::unexpected(ParseError::InvalidLength);
        }

        // Extract payload
        ParsedPacket result;
        result.header = header;
        result.payload.assign(
            buffer.begin() + sizeof(PacketHeader),
            buffer.begin() + totalSize
        );
        result.flags = PacketFlags::None;

        return result;
    }

    // Get required bytes to complete packet
    static size_t requiredBytes(std::span<const uint8_t> buffer) {
        if (buffer.size() < sizeof(PacketHeader)) {
            return sizeof(PacketHeader) - buffer.size();
        }

        PacketHeader header;
        std::memcpy(&header, buffer.data(), sizeof(PacketHeader));

        size_t totalSize = sizeof(PacketHeader) + header.length;
        if (buffer.size() < totalSize) {
            return totalSize - buffer.size();
        }

        return 0;
    }
};

} // namespace protocol
```

---

## Message Types

### Message Type Registry

```cpp
// message_types.h
#pragma once

#include <cstdint>

namespace protocol {

// Message type categories (high byte)
enum class MessageCategory : uint8_t {
    System      = 0x00,  // System messages
    Auth        = 0x01,  // Authentication
    Character   = 0x02,  // Character management
    World       = 0x03,  // World/map
    Movement    = 0x04,  // Position/movement
    Combat      = 0x05,  // Combat actions
    Inventory   = 0x06,  // Items/inventory
    Social      = 0x07,  // Chat/guild/party
    Trade       = 0x08,  // Trading
    Quest       = 0x09,  // Quests
    Skill       = 0x0A,  // Skills/abilities
    Instance    = 0x0B,  // Dungeons/instances
};

// Create message type from category and id
constexpr uint16_t makeMessageType(MessageCategory category, uint8_t id) {
    return (static_cast<uint16_t>(category) << 8) | id;
}

// System Messages (0x00XX)
namespace SystemMessages {
    constexpr uint16_t Ping           = makeMessageType(MessageCategory::System, 0x01);
    constexpr uint16_t Pong           = makeMessageType(MessageCategory::System, 0x02);
    constexpr uint16_t Disconnect     = makeMessageType(MessageCategory::System, 0x03);
    constexpr uint16_t ServerInfo     = makeMessageType(MessageCategory::System, 0x04);
    constexpr uint16_t Error          = makeMessageType(MessageCategory::System, 0x05);
    constexpr uint16_t Heartbeat      = makeMessageType(MessageCategory::System, 0x06);
}

// Auth Messages (0x01XX)
namespace AuthMessages {
    constexpr uint16_t LoginRequest   = makeMessageType(MessageCategory::Auth, 0x01);
    constexpr uint16_t LoginResponse  = makeMessageType(MessageCategory::Auth, 0x02);
    constexpr uint16_t LogoutRequest  = makeMessageType(MessageCategory::Auth, 0x03);
    constexpr uint16_t LogoutResponse = makeMessageType(MessageCategory::Auth, 0x04);
    constexpr uint16_t TokenRefresh   = makeMessageType(MessageCategory::Auth, 0x05);
    constexpr uint16_t SessionExpired = makeMessageType(MessageCategory::Auth, 0x06);
}

// Character Messages (0x02XX)
namespace CharacterMessages {
    constexpr uint16_t CharacterList    = makeMessageType(MessageCategory::Character, 0x01);
    constexpr uint16_t CharacterCreate  = makeMessageType(MessageCategory::Character, 0x02);
    constexpr uint16_t CharacterDelete  = makeMessageType(MessageCategory::Character, 0x03);
    constexpr uint16_t CharacterSelect  = makeMessageType(MessageCategory::Character, 0x04);
    constexpr uint16_t CharacterInfo    = makeMessageType(MessageCategory::Character, 0x05);
    constexpr uint16_t CharacterUpdate  = makeMessageType(MessageCategory::Character, 0x06);
}

// Movement Messages (0x04XX)
namespace MovementMessages {
    constexpr uint16_t PositionUpdate   = makeMessageType(MessageCategory::Movement, 0x01);
    constexpr uint16_t MoveStart        = makeMessageType(MessageCategory::Movement, 0x02);
    constexpr uint16_t MoveStop         = makeMessageType(MessageCategory::Movement, 0x03);
    constexpr uint16_t Teleport         = makeMessageType(MessageCategory::Movement, 0x04);
    constexpr uint16_t Jump             = makeMessageType(MessageCategory::Movement, 0x05);
    constexpr uint16_t EntitySpawn      = makeMessageType(MessageCategory::Movement, 0x10);
    constexpr uint16_t EntityDespawn    = makeMessageType(MessageCategory::Movement, 0x11);
    constexpr uint16_t EntityPositions  = makeMessageType(MessageCategory::Movement, 0x12);
}

// Combat Messages (0x05XX)
namespace CombatMessages {
    constexpr uint16_t Attack           = makeMessageType(MessageCategory::Combat, 0x01);
    constexpr uint16_t Damage           = makeMessageType(MessageCategory::Combat, 0x02);
    constexpr uint16_t Death            = makeMessageType(MessageCategory::Combat, 0x03);
    constexpr uint16_t Respawn          = makeMessageType(MessageCategory::Combat, 0x04);
    constexpr uint16_t UseSkill         = makeMessageType(MessageCategory::Combat, 0x05);
    constexpr uint16_t SkillEffect      = makeMessageType(MessageCategory::Combat, 0x06);
    constexpr uint16_t BuffApply        = makeMessageType(MessageCategory::Combat, 0x10);
    constexpr uint16_t BuffRemove       = makeMessageType(MessageCategory::Combat, 0x11);
    constexpr uint16_t CombatStart      = makeMessageType(MessageCategory::Combat, 0x20);
    constexpr uint16_t CombatEnd        = makeMessageType(MessageCategory::Combat, 0x21);
}

// Social Messages (0x07XX)
namespace SocialMessages {
    constexpr uint16_t ChatMessage      = makeMessageType(MessageCategory::Social, 0x01);
    constexpr uint16_t ChatWhisper      = makeMessageType(MessageCategory::Social, 0x02);
    constexpr uint16_t ChatParty        = makeMessageType(MessageCategory::Social, 0x03);
    constexpr uint16_t ChatGuild        = makeMessageType(MessageCategory::Social, 0x04);
    constexpr uint16_t ChatWorld        = makeMessageType(MessageCategory::Social, 0x05);
    constexpr uint16_t PartyInvite      = makeMessageType(MessageCategory::Social, 0x10);
    constexpr uint16_t PartyJoin        = makeMessageType(MessageCategory::Social, 0x11);
    constexpr uint16_t PartyLeave       = makeMessageType(MessageCategory::Social, 0x12);
    constexpr uint16_t GuildInvite      = makeMessageType(MessageCategory::Social, 0x20);
    constexpr uint16_t GuildInfo        = makeMessageType(MessageCategory::Social, 0x21);
    constexpr uint16_t FriendList       = makeMessageType(MessageCategory::Social, 0x30);
    constexpr uint16_t FriendRequest    = makeMessageType(MessageCategory::Social, 0x31);
}

} // namespace protocol
```

### Message Structures

```cpp
// messages.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace protocol::messages {

// ============================================
// System Messages
// ============================================

struct PingMessage {
    int64_t clientTime;  // Client timestamp in milliseconds
};

struct PongMessage {
    int64_t clientTime;  // Echo back client timestamp
    int64_t serverTime;  // Server timestamp
};

struct ErrorMessage {
    uint32_t errorCode;
    std::string message;
};

// ============================================
// Auth Messages
// ============================================

struct LoginRequest {
    std::string username;
    std::string passwordHash;  // SHA-256 hash
    std::string clientVersion;
    std::string deviceId;
};

struct LoginResponse {
    bool success;
    uint32_t errorCode;
    std::string sessionToken;
    int64_t tokenExpiry;
    uint32_t accountId;
};

struct LogoutRequest {
    std::string sessionToken;
};

// ============================================
// Character Messages
// ============================================

struct CharacterSummary {
    uint64_t characterId;
    std::string name;
    uint8_t classId;
    uint16_t level;
    uint32_t mapId;
    int64_t lastLogin;
};

struct CharacterListResponse {
    std::vector<CharacterSummary> characters;
    uint8_t maxCharacters;
};

struct CharacterCreateRequest {
    std::string name;
    uint8_t classId;
    uint8_t gender;
    std::array<uint8_t, 32> appearance;  // Customization data
};

struct CharacterCreateResponse {
    bool success;
    uint32_t errorCode;
    uint64_t characterId;
};

struct CharacterSelectRequest {
    uint64_t characterId;
};

// ============================================
// Movement Messages
// ============================================

struct Vector3 {
    float x;
    float y;
    float z;
};

struct PositionUpdate {
    uint64_t entityId;
    Vector3 position;
    float rotation;      // Yaw in radians
    uint8_t moveFlags;   // Running, jumping, etc.
    uint32_t timestamp;  // Server timestamp for interpolation
};

struct MoveStartMessage {
    Vector3 destination;
    float speed;
    uint8_t moveType;    // Walk, run, mount
};

struct TeleportMessage {
    uint32_t mapId;
    Vector3 position;
    float rotation;
    uint8_t teleportType;  // Portal, skill, GM command
};

struct EntitySpawnMessage {
    uint64_t entityId;
    uint16_t entityType;   // Player, NPC, Monster, Item
    uint32_t templateId;   // Entity template/model
    Vector3 position;
    float rotation;
    std::string name;
    uint16_t level;
    uint32_t currentHp;
    uint32_t maxHp;
    std::vector<uint8_t> extraData;  // Entity-specific data
};

struct EntityDespawnMessage {
    uint64_t entityId;
    uint8_t reason;  // Out of range, death, teleport
};

struct EntityPositionsMessage {
    struct EntityPosition {
        uint64_t entityId;
        Vector3 position;
        float rotation;
        uint8_t moveFlags;
    };

    uint32_t timestamp;
    std::vector<EntityPosition> entities;
};

// ============================================
// Combat Messages
// ============================================

struct AttackMessage {
    uint64_t attackerId;
    uint64_t targetId;
    uint32_t skillId;      // 0 for basic attack
    Vector3 targetPos;     // For ground-targeted skills
};

struct DamageMessage {
    uint64_t attackerId;
    uint64_t targetId;
    uint32_t damage;
    uint8_t damageType;    // Physical, magical, true
    uint8_t flags;         // Critical, blocked, missed
    uint32_t resultHp;     // Target's HP after damage
};

struct DeathMessage {
    uint64_t entityId;
    uint64_t killerId;
    uint8_t deathType;
};

struct RespawnMessage {
    uint64_t entityId;
    Vector3 position;
    uint32_t currentHp;
    uint32_t maxHp;
};

struct UseSkillMessage {
    uint64_t casterId;
    uint32_t skillId;
    uint64_t targetId;
    Vector3 targetPos;
    float castTime;
};

struct SkillEffectMessage {
    uint64_t casterId;
    uint32_t skillId;
    std::vector<uint64_t> affectedEntities;
    Vector3 effectPos;
    uint8_t effectType;
};

struct BuffMessage {
    uint64_t targetId;
    uint32_t buffId;
    uint8_t stacks;
    float duration;
    uint64_t sourceId;
};

// ============================================
// Social Messages
// ============================================

struct ChatMessage {
    uint8_t channel;       // Say, yell, party, guild, world
    uint64_t senderId;
    std::string senderName;
    std::string message;
    int64_t timestamp;
};

struct WhisperMessage {
    std::string targetName;
    std::string message;
};

struct PartyInviteMessage {
    uint64_t inviterId;
    std::string inviterName;
    uint64_t partyId;
};

struct PartyInfoMessage {
    uint64_t partyId;
    uint64_t leaderId;
    struct Member {
        uint64_t characterId;
        std::string name;
        uint8_t classId;
        uint16_t level;
        uint32_t currentHp;
        uint32_t maxHp;
        uint32_t mapId;
        bool online;
    };
    std::vector<Member> members;
};

} // namespace protocol::messages
```

---

## Serialization

### Binary Serializer

```cpp
// binary_serializer.h
#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <span>
#include <bit>

namespace protocol {

class BinaryWriter {
public:
    BinaryWriter() = default;
    explicit BinaryWriter(size_t reserveSize) {
        buffer_.reserve(reserveSize);
    }

    // Primitive types
    void writeU8(uint8_t value) {
        buffer_.push_back(value);
    }

    void writeU16(uint16_t value) {
        if constexpr (std::endian::native == std::endian::big) {
            value = std::byteswap(value);
        }
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        buffer_.insert(buffer_.end(), bytes, bytes + sizeof(value));
    }

    void writeU32(uint32_t value) {
        if constexpr (std::endian::native == std::endian::big) {
            value = std::byteswap(value);
        }
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        buffer_.insert(buffer_.end(), bytes, bytes + sizeof(value));
    }

    void writeU64(uint64_t value) {
        if constexpr (std::endian::native == std::endian::big) {
            value = std::byteswap(value);
        }
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        buffer_.insert(buffer_.end(), bytes, bytes + sizeof(value));
    }

    void writeI8(int8_t value) { writeU8(static_cast<uint8_t>(value)); }
    void writeI16(int16_t value) { writeU16(static_cast<uint16_t>(value)); }
    void writeI32(int32_t value) { writeU32(static_cast<uint32_t>(value)); }
    void writeI64(int64_t value) { writeU64(static_cast<uint64_t>(value)); }

    void writeFloat(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        writeU32(bits);
    }

    void writeDouble(double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        writeU64(bits);
    }

    void writeBool(bool value) {
        writeU8(value ? 1 : 0);
    }

    // String (length-prefixed)
    void writeString(const std::string& str) {
        if (str.size() > 65535) {
            throw std::runtime_error("String too long");
        }
        writeU16(static_cast<uint16_t>(str.size()));
        buffer_.insert(buffer_.end(), str.begin(), str.end());
    }

    // Raw bytes
    void writeBytes(std::span<const uint8_t> data) {
        writeU32(static_cast<uint32_t>(data.size()));
        buffer_.insert(buffer_.end(), data.begin(), data.end());
    }

    // Vector3
    void writeVector3(float x, float y, float z) {
        writeFloat(x);
        writeFloat(y);
        writeFloat(z);
    }

    // Get result
    const std::vector<uint8_t>& data() const { return buffer_; }
    std::vector<uint8_t> release() { return std::move(buffer_); }

private:
    std::vector<uint8_t> buffer_;
};

class BinaryReader {
public:
    explicit BinaryReader(std::span<const uint8_t> data)
        : data_(data), pos_(0) {}

    // Primitive types
    uint8_t readU8() {
        checkRemaining(1);
        return data_[pos_++];
    }

    uint16_t readU16() {
        checkRemaining(2);
        uint16_t value;
        std::memcpy(&value, &data_[pos_], sizeof(value));
        pos_ += sizeof(value);
        if constexpr (std::endian::native == std::endian::big) {
            value = std::byteswap(value);
        }
        return value;
    }

    uint32_t readU32() {
        checkRemaining(4);
        uint32_t value;
        std::memcpy(&value, &data_[pos_], sizeof(value));
        pos_ += sizeof(value);
        if constexpr (std::endian::native == std::endian::big) {
            value = std::byteswap(value);
        }
        return value;
    }

    uint64_t readU64() {
        checkRemaining(8);
        uint64_t value;
        std::memcpy(&value, &data_[pos_], sizeof(value));
        pos_ += sizeof(value);
        if constexpr (std::endian::native == std::endian::big) {
            value = std::byteswap(value);
        }
        return value;
    }

    int8_t readI8() { return static_cast<int8_t>(readU8()); }
    int16_t readI16() { return static_cast<int16_t>(readU16()); }
    int32_t readI32() { return static_cast<int32_t>(readU32()); }
    int64_t readI64() { return static_cast<int64_t>(readU64()); }

    float readFloat() {
        uint32_t bits = readU32();
        float value;
        std::memcpy(&value, &bits, sizeof(float));
        return value;
    }

    double readDouble() {
        uint64_t bits = readU64();
        double value;
        std::memcpy(&value, &bits, sizeof(double));
        return value;
    }

    bool readBool() {
        return readU8() != 0;
    }

    std::string readString() {
        uint16_t length = readU16();
        checkRemaining(length);
        std::string str(reinterpret_cast<const char*>(&data_[pos_]), length);
        pos_ += length;
        return str;
    }

    std::vector<uint8_t> readBytes() {
        uint32_t length = readU32();
        checkRemaining(length);
        std::vector<uint8_t> bytes(&data_[pos_], &data_[pos_] + length);
        pos_ += length;
        return bytes;
    }

    struct Vec3Result { float x, y, z; };
    Vec3Result readVector3() {
        return { readFloat(), readFloat(), readFloat() };
    }

    // Position management
    size_t position() const { return pos_; }
    size_t remaining() const { return data_.size() - pos_; }
    bool eof() const { return pos_ >= data_.size(); }

private:
    void checkRemaining(size_t bytes) {
        if (pos_ + bytes > data_.size()) {
            throw std::runtime_error("Buffer underflow");
        }
    }

    std::span<const uint8_t> data_;
    size_t pos_;
};

} // namespace protocol
```

### Message Serialization

```cpp
// message_serializer.h
#pragma once

#include "binary_serializer.h"
#include "messages.h"

namespace protocol {

// Serialize trait
template<typename T>
struct Serializer;

// LoginRequest serialization
template<>
struct Serializer<messages::LoginRequest> {
    static void serialize(BinaryWriter& writer, const messages::LoginRequest& msg) {
        writer.writeString(msg.username);
        writer.writeString(msg.passwordHash);
        writer.writeString(msg.clientVersion);
        writer.writeString(msg.deviceId);
    }

    static messages::LoginRequest deserialize(BinaryReader& reader) {
        messages::LoginRequest msg;
        msg.username = reader.readString();
        msg.passwordHash = reader.readString();
        msg.clientVersion = reader.readString();
        msg.deviceId = reader.readString();
        return msg;
    }
};

// PositionUpdate serialization
template<>
struct Serializer<messages::PositionUpdate> {
    static void serialize(BinaryWriter& writer, const messages::PositionUpdate& msg) {
        writer.writeU64(msg.entityId);
        writer.writeVector3(msg.position.x, msg.position.y, msg.position.z);
        writer.writeFloat(msg.rotation);
        writer.writeU8(msg.moveFlags);
        writer.writeU32(msg.timestamp);
    }

    static messages::PositionUpdate deserialize(BinaryReader& reader) {
        messages::PositionUpdate msg;
        msg.entityId = reader.readU64();
        auto pos = reader.readVector3();
        msg.position = { pos.x, pos.y, pos.z };
        msg.rotation = reader.readFloat();
        msg.moveFlags = reader.readU8();
        msg.timestamp = reader.readU32();
        return msg;
    }
};

// EntitySpawn serialization
template<>
struct Serializer<messages::EntitySpawnMessage> {
    static void serialize(BinaryWriter& writer, const messages::EntitySpawnMessage& msg) {
        writer.writeU64(msg.entityId);
        writer.writeU16(msg.entityType);
        writer.writeU32(msg.templateId);
        writer.writeVector3(msg.position.x, msg.position.y, msg.position.z);
        writer.writeFloat(msg.rotation);
        writer.writeString(msg.name);
        writer.writeU16(msg.level);
        writer.writeU32(msg.currentHp);
        writer.writeU32(msg.maxHp);
        writer.writeBytes(msg.extraData);
    }

    static messages::EntitySpawnMessage deserialize(BinaryReader& reader) {
        messages::EntitySpawnMessage msg;
        msg.entityId = reader.readU64();
        msg.entityType = reader.readU16();
        msg.templateId = reader.readU32();
        auto pos = reader.readVector3();
        msg.position = { pos.x, pos.y, pos.z };
        msg.rotation = reader.readFloat();
        msg.name = reader.readString();
        msg.level = reader.readU16();
        msg.currentHp = reader.readU32();
        msg.maxHp = reader.readU32();
        msg.extraData = reader.readBytes();
        return msg;
    }
};

// DamageMessage serialization
template<>
struct Serializer<messages::DamageMessage> {
    static void serialize(BinaryWriter& writer, const messages::DamageMessage& msg) {
        writer.writeU64(msg.attackerId);
        writer.writeU64(msg.targetId);
        writer.writeU32(msg.damage);
        writer.writeU8(msg.damageType);
        writer.writeU8(msg.flags);
        writer.writeU32(msg.resultHp);
    }

    static messages::DamageMessage deserialize(BinaryReader& reader) {
        messages::DamageMessage msg;
        msg.attackerId = reader.readU64();
        msg.targetId = reader.readU64();
        msg.damage = reader.readU32();
        msg.damageType = reader.readU8();
        msg.flags = reader.readU8();
        msg.resultHp = reader.readU32();
        return msg;
    }
};

// ChatMessage serialization
template<>
struct Serializer<messages::ChatMessage> {
    static void serialize(BinaryWriter& writer, const messages::ChatMessage& msg) {
        writer.writeU8(msg.channel);
        writer.writeU64(msg.senderId);
        writer.writeString(msg.senderName);
        writer.writeString(msg.message);
        writer.writeI64(msg.timestamp);
    }

    static messages::ChatMessage deserialize(BinaryReader& reader) {
        messages::ChatMessage msg;
        msg.channel = reader.readU8();
        msg.senderId = reader.readU64();
        msg.senderName = reader.readString();
        msg.message = reader.readString();
        msg.timestamp = reader.readI64();
        return msg;
    }
};

// Helper functions
template<typename T>
std::vector<uint8_t> serializeMessage(const T& message) {
    BinaryWriter writer;
    Serializer<T>::serialize(writer, message);
    return writer.release();
}

template<typename T>
T deserializeMessage(std::span<const uint8_t> data) {
    BinaryReader reader(data);
    return Serializer<T>::deserialize(reader);
}

} // namespace protocol
```

---

## Protocol Handlers

### Message Handler Interface

```cpp
// message_handler.h
#pragma once

#include "packet_format.h"
#include "messages.h"
#include <functional>
#include <unordered_map>
#include <memory>

namespace protocol {

// Forward declarations
class Session;

// Handler result
struct HandleResult {
    bool success;
    std::optional<std::vector<uint8_t>> response;
    std::string error;

    static HandleResult ok() { return { true, std::nullopt, "" }; }
    static HandleResult ok(std::vector<uint8_t> response) {
        return { true, std::move(response), "" };
    }
    static HandleResult fail(std::string error) {
        return { false, std::nullopt, std::move(error) };
    }
};

// Base handler interface
class IMessageHandler {
public:
    virtual ~IMessageHandler() = default;
    virtual HandleResult handle(
        Session& session,
        std::span<const uint8_t> payload
    ) = 0;
};

// Type-safe handler wrapper
template<typename MessageT, typename HandlerFunc>
class TypedMessageHandler : public IMessageHandler {
public:
    explicit TypedMessageHandler(HandlerFunc handler)
        : handler_(std::move(handler)) {}

    HandleResult handle(Session& session, std::span<const uint8_t> payload) override {
        try {
            auto message = deserializeMessage<MessageT>(payload);
            return handler_(session, message);
        } catch (const std::exception& e) {
            return HandleResult::fail(e.what());
        }
    }

private:
    HandlerFunc handler_;
};

// Handler registry
class MessageHandlerRegistry {
public:
    template<typename MessageT, typename HandlerFunc>
    void registerHandler(uint16_t messageType, HandlerFunc handler) {
        handlers_[messageType] = std::make_unique<TypedMessageHandler<MessageT, HandlerFunc>>(
            std::move(handler)
        );
    }

    HandleResult dispatch(
        uint16_t messageType,
        Session& session,
        std::span<const uint8_t> payload
    ) {
        auto it = handlers_.find(messageType);
        if (it == handlers_.end()) {
            return HandleResult::fail("Unknown message type");
        }
        return it->second->handle(session, payload);
    }

    bool hasHandler(uint16_t messageType) const {
        return handlers_.contains(messageType);
    }

private:
    std::unordered_map<uint16_t, std::unique_ptr<IMessageHandler>> handlers_;
};

} // namespace protocol
```

### Protocol Handler Implementation

```cpp
// game_protocol_handler.h
#pragma once

#include "message_handler.h"
#include "message_types.h"

namespace game {

class GameProtocolHandler {
public:
    GameProtocolHandler() {
        registerHandlers();
    }

    protocol::HandleResult processPacket(
        protocol::Session& session,
        const protocol::ParsedPacket& packet
    ) {
        return registry_.dispatch(
            packet.header.type,
            session,
            packet.payload
        );
    }

private:
    void registerHandlers() {
        using namespace protocol;

        // System handlers
        registry_.registerHandler<messages::PingMessage>(
            SystemMessages::Ping,
            [](Session& session, const messages::PingMessage& msg) {
                messages::PongMessage response{
                    .clientTime = msg.clientTime,
                    .serverTime = getCurrentTimestamp()
                };
                return HandleResult::ok(serializeMessage(response));
            }
        );

        // Auth handlers
        registry_.registerHandler<messages::LoginRequest>(
            AuthMessages::LoginRequest,
            [this](Session& session, const messages::LoginRequest& msg) {
                return handleLogin(session, msg);
            }
        );

        // Movement handlers
        registry_.registerHandler<messages::PositionUpdate>(
            MovementMessages::PositionUpdate,
            [this](Session& session, const messages::PositionUpdate& msg) {
                return handlePositionUpdate(session, msg);
            }
        );

        registry_.registerHandler<messages::MoveStartMessage>(
            MovementMessages::MoveStart,
            [this](Session& session, const messages::MoveStartMessage& msg) {
                return handleMoveStart(session, msg);
            }
        );

        // Combat handlers
        registry_.registerHandler<messages::AttackMessage>(
            CombatMessages::Attack,
            [this](Session& session, const messages::AttackMessage& msg) {
                return handleAttack(session, msg);
            }
        );

        registry_.registerHandler<messages::UseSkillMessage>(
            CombatMessages::UseSkill,
            [this](Session& session, const messages::UseSkillMessage& msg) {
                return handleUseSkill(session, msg);
            }
        );

        // Chat handlers
        registry_.registerHandler<messages::ChatMessage>(
            SocialMessages::ChatMessage,
            [this](Session& session, const messages::ChatMessage& msg) {
                return handleChat(session, msg);
            }
        );
    }

    // Handler implementations
    protocol::HandleResult handleLogin(
        protocol::Session& session,
        const protocol::messages::LoginRequest& msg
    );

    protocol::HandleResult handlePositionUpdate(
        protocol::Session& session,
        const protocol::messages::PositionUpdate& msg
    );

    protocol::HandleResult handleMoveStart(
        protocol::Session& session,
        const protocol::messages::MoveStartMessage& msg
    );

    protocol::HandleResult handleAttack(
        protocol::Session& session,
        const protocol::messages::AttackMessage& msg
    );

    protocol::HandleResult handleUseSkill(
        protocol::Session& session,
        const protocol::messages::UseSkillMessage& msg
    );

    protocol::HandleResult handleChat(
        protocol::Session& session,
        const protocol::messages::ChatMessage& msg
    );

    protocol::MessageHandlerRegistry registry_;
};

} // namespace game
```

---

## Security

### Encryption Layer

```cpp
// encryption.h
#pragma once

#include <vector>
#include <cstdint>
#include <span>
#include <array>
#include <memory>

namespace protocol::security {

// AES-256-GCM encryption
class AESEncryption {
public:
    static constexpr size_t KEY_SIZE = 32;    // 256 bits
    static constexpr size_t IV_SIZE = 12;     // 96 bits (GCM)
    static constexpr size_t TAG_SIZE = 16;    // 128 bits

    using Key = std::array<uint8_t, KEY_SIZE>;
    using IV = std::array<uint8_t, IV_SIZE>;
    using Tag = std::array<uint8_t, TAG_SIZE>;

    explicit AESEncryption(const Key& key);
    ~AESEncryption();

    // Encrypt data with random IV
    struct EncryptedData {
        IV iv;
        std::vector<uint8_t> ciphertext;
        Tag tag;
    };

    EncryptedData encrypt(std::span<const uint8_t> plaintext);

    // Decrypt data
    std::optional<std::vector<uint8_t>> decrypt(
        const IV& iv,
        std::span<const uint8_t> ciphertext,
        const Tag& tag
    );

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Key exchange using ECDH
class KeyExchange {
public:
    static constexpr size_t PUBLIC_KEY_SIZE = 32;
    static constexpr size_t PRIVATE_KEY_SIZE = 32;
    static constexpr size_t SHARED_SECRET_SIZE = 32;

    using PublicKey = std::array<uint8_t, PUBLIC_KEY_SIZE>;
    using PrivateKey = std::array<uint8_t, PRIVATE_KEY_SIZE>;
    using SharedSecret = std::array<uint8_t, SHARED_SECRET_SIZE>;

    KeyExchange();
    ~KeyExchange();

    // Generate key pair
    void generateKeyPair();

    // Get public key to send to peer
    PublicKey getPublicKey() const;

    // Compute shared secret from peer's public key
    SharedSecret computeSharedSecret(const PublicKey& peerPublicKey);

    // Derive encryption key from shared secret
    static AESEncryption::Key deriveKey(
        const SharedSecret& secret,
        std::span<const uint8_t> info
    );

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Session encryption context
class SessionEncryption {
public:
    enum class State {
        Uninitialized,
        KeyExchangePending,
        Established,
    };

    SessionEncryption();

    // Start key exchange (client side)
    KeyExchange::PublicKey initiateKeyExchange();

    // Complete key exchange (server side receives client public key)
    KeyExchange::PublicKey completeKeyExchange(
        const KeyExchange::PublicKey& clientPublicKey
    );

    // Finalize on client (receives server public key)
    void finalizeKeyExchange(const KeyExchange::PublicKey& serverPublicKey);

    // Encrypt/decrypt after key exchange
    std::vector<uint8_t> encrypt(std::span<const uint8_t> plaintext);
    std::optional<std::vector<uint8_t>> decrypt(std::span<const uint8_t> data);

    State getState() const { return state_; }
    bool isEstablished() const { return state_ == State::Established; }

private:
    State state_ = State::Uninitialized;
    KeyExchange keyExchange_;
    std::unique_ptr<AESEncryption> encryption_;
};

} // namespace protocol::security
```

### Packet Signing

```cpp
// packet_signing.h
#pragma once

#include <array>
#include <span>
#include <cstdint>

namespace protocol::security {

// HMAC-SHA256 for packet integrity
class PacketSigner {
public:
    static constexpr size_t SIGNATURE_SIZE = 32;  // SHA-256
    using Signature = std::array<uint8_t, SIGNATURE_SIZE>;
    using Key = std::array<uint8_t, 32>;

    explicit PacketSigner(const Key& key);

    // Sign packet data
    Signature sign(std::span<const uint8_t> data) const;

    // Verify signature
    bool verify(
        std::span<const uint8_t> data,
        const Signature& signature
    ) const;

private:
    Key key_;
};

// Anti-replay with sequence numbers
class SequenceValidator {
public:
    explicit SequenceValidator(size_t windowSize = 1024);

    // Check if sequence number is valid (not replayed)
    bool validate(uint32_t sequence);

    // Get expected next sequence
    uint32_t getExpectedSequence() const;

private:
    uint32_t highestReceived_ = 0;
    std::vector<bool> window_;
    size_t windowSize_;
};

} // namespace protocol::security
```

---

## Compression

### Compression Layer

```cpp
// compression.h
#pragma once

#include <vector>
#include <span>
#include <cstdint>

namespace protocol {

// Compression algorithms
enum class CompressionAlgorithm {
    None,
    LZ4,      // Fast, good for real-time
    ZSTD,     // Better ratio, slightly slower
};

class Compressor {
public:
    explicit Compressor(CompressionAlgorithm algorithm = CompressionAlgorithm::LZ4);

    // Compress data (returns empty if compression not beneficial)
    std::vector<uint8_t> compress(std::span<const uint8_t> data);

    // Decompress data
    std::vector<uint8_t> decompress(std::span<const uint8_t> compressed);

    // Get minimum size for compression to be worthwhile
    static size_t minCompressionSize() { return 128; }

private:
    CompressionAlgorithm algorithm_;
};

// Adaptive compression based on data patterns
class AdaptiveCompressor {
public:
    AdaptiveCompressor();

    // Compress with automatic algorithm selection
    struct CompressedResult {
        CompressionAlgorithm algorithm;
        std::vector<uint8_t> data;
        float ratio;  // Original size / compressed size
    };

    CompressedResult compress(std::span<const uint8_t> data);

    // Decompress with algorithm hint
    std::vector<uint8_t> decompress(
        CompressionAlgorithm algorithm,
        std::span<const uint8_t> data
    );

private:
    // Track compression effectiveness per message type
    std::unordered_map<uint16_t, CompressionAlgorithm> typePreferences_;
};

} // namespace protocol
```

---

## Version Compatibility

### Protocol Versioning

```cpp
// protocol_version.h
#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace protocol {

// Version format: Major.Minor.Patch
// Major: Breaking changes
// Minor: New features, backward compatible
// Patch: Bug fixes

struct ProtocolVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;

    // Pack into uint16_t (major.minor only, patch for info)
    uint16_t pack() const {
        return (static_cast<uint16_t>(major) << 8) | minor;
    }

    static ProtocolVersion unpack(uint16_t packed) {
        return {
            .major = static_cast<uint8_t>(packed >> 8),
            .minor = static_cast<uint8_t>(packed & 0xFF),
            .patch = 0
        };
    }

    std::string toString() const {
        return std::to_string(major) + "." +
               std::to_string(minor) + "." +
               std::to_string(patch);
    }

    // Compatibility check
    bool isCompatibleWith(const ProtocolVersion& other) const {
        // Same major version = compatible
        return major == other.major;
    }

    bool operator==(const ProtocolVersion& other) const {
        return major == other.major &&
               minor == other.minor &&
               patch == other.patch;
    }

    bool operator<(const ProtocolVersion& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return patch < other.patch;
    }
};

// Current protocol version
constexpr ProtocolVersion CURRENT_VERSION = { 1, 0, 0 };

// Version negotiation
class VersionNegotiator {
public:
    struct NegotiationResult {
        bool success;
        ProtocolVersion agreedVersion;
        std::string error;
    };

    // Server: Check if client version is acceptable
    static NegotiationResult negotiate(
        ProtocolVersion clientVersion,
        ProtocolVersion minSupported = { 1, 0, 0 }
    ) {
        if (!clientVersion.isCompatibleWith(CURRENT_VERSION)) {
            return {
                false,
                {},
                "Incompatible protocol version: " + clientVersion.toString()
            };
        }

        if (clientVersion < minSupported) {
            return {
                false,
                {},
                "Protocol version too old: " + clientVersion.toString()
            };
        }

        // Use minimum of client and server version features
        ProtocolVersion agreed = {
            CURRENT_VERSION.major,
            std::min(clientVersion.minor, CURRENT_VERSION.minor),
            0
        };

        return { true, agreed, "" };
    }
};

} // namespace protocol
```

### Feature Flags

```cpp
// feature_flags.h
#pragma once

#include <bitset>
#include <cstdint>

namespace protocol {

// Protocol feature flags
enum class ProtocolFeature : uint32_t {
    Compression     = 1 << 0,   // LZ4/ZSTD compression
    Encryption      = 1 << 1,   // AES-256-GCM encryption
    BatchMessages   = 1 << 2,   // Multiple messages in one packet
    BinaryFormat    = 1 << 3,   // Binary serialization (vs JSON)
    DeltaEncoding   = 1 << 4,   // Delta compression for updates
    Fragmentation   = 1 << 5,   // Large packet fragmentation
    Priority        = 1 << 6,   // Message priority queuing
    ReliableUDP     = 1 << 7,   // Reliable UDP support
};

class FeatureSet {
public:
    FeatureSet() = default;
    explicit FeatureSet(uint32_t flags) : flags_(flags) {}

    bool has(ProtocolFeature feature) const {
        return (flags_ & static_cast<uint32_t>(feature)) != 0;
    }

    void enable(ProtocolFeature feature) {
        flags_ |= static_cast<uint32_t>(feature);
    }

    void disable(ProtocolFeature feature) {
        flags_ &= ~static_cast<uint32_t>(feature);
    }

    uint32_t toUint32() const { return flags_; }

    // Negotiate common features
    static FeatureSet negotiate(FeatureSet client, FeatureSet server) {
        return FeatureSet(client.flags_ & server.flags_);
    }

    // Default features
    static FeatureSet defaults() {
        FeatureSet fs;
        fs.enable(ProtocolFeature::Compression);
        fs.enable(ProtocolFeature::Encryption);
        fs.enable(ProtocolFeature::BinaryFormat);
        return fs;
    }

private:
    uint32_t flags_ = 0;
};

} // namespace protocol
```

---

## Appendix

### Packet Size Guidelines

| Message Type | Typical Size | Max Size | Frequency |
|-------------|--------------|----------|-----------|
| Position Update | 32 bytes | 64 bytes | 20-60/sec |
| Entity Spawn | 128 bytes | 1KB | On visibility |
| Chat Message | 64 bytes | 512 bytes | User input |
| Damage Event | 24 bytes | 48 bytes | Combat |
| Skill Effect | 48 bytes | 256 bytes | Combat |
| Batch Update | 1KB | 8KB | 10/sec |

### Bandwidth Estimation

```
Player bandwidth (typical):
- Position updates: 32 bytes × 30/sec = 960 bytes/sec
- Nearby entities (50): 32 bytes × 50 × 10/sec = 16 KB/sec
- Combat events: 48 bytes × 5/sec = 240 bytes/sec
- Other: ~2 KB/sec

Total per player: ~20 KB/sec = 160 Kbps

Server bandwidth (1000 players):
- Incoming: 20 MB/sec
- Outgoing: 20 MB/sec (with smart batching)
- Peak (combat): 50 MB/sec
```

### Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0x0000 | Success | Operation successful |
| 0x0001 | Unknown | Unknown error |
| 0x0002 | InvalidPacket | Malformed packet |
| 0x0003 | AuthRequired | Authentication required |
| 0x0004 | AuthFailed | Authentication failed |
| 0x0005 | SessionExpired | Session expired |
| 0x0006 | RateLimited | Too many requests |
| 0x0007 | ServerFull | Server at capacity |
| 0x0008 | Maintenance | Server maintenance |
| 0x0100 | InvalidCharacter | Character not found |
| 0x0101 | CharacterInUse | Character already logged in |
| 0x0200 | InvalidTarget | Invalid target |
| 0x0201 | OutOfRange | Target out of range |
| 0x0202 | OnCooldown | Skill on cooldown |
| 0x0203 | NotEnoughResource | Insufficient mana/stamina |

---

*This document provides the complete protocol design for the unified game server network layer.*
