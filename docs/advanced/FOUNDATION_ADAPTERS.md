# Foundation Adapter Patterns

## Game-Specific Wrappers for Foundation Systems

**Version**: 0.1.0.0
**Last Updated**: 2026-02-03
---

## 1. Overview

### 1.1 Purpose

Foundation Adapters provide:

- **Abstraction**: Game logic doesn't depend directly on foundation systems
- **Game Context**: APIs designed for game-specific operations
- **Testability**: Easy to mock for unit testing
- **Future-Proofing**: Foundation changes don't affect game code

### 1.2 Adapter Pattern

```
┌─────────────────────────────────────────────────────────────┐
│                      Game Logic Layer                        │
│                                                              │
│   [Combat System]  [World System]  [Quest System]           │
│         │               │               │                    │
│         └───────────────┼───────────────┘                    │
│                         │                                    │
│                         v                                    │
├─────────────────────────────────────────────────────────────┤
│                  Foundation Adapter Layer                    │
│                                                              │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │  Game    │ │  Game    │ │  Game    │ │  Game    │       │
│  │  Logger  │ │ Network  │ │ Database │ │  Thread  │       │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘       │
│       │            │            │            │              │
├───────┼────────────┼────────────┼────────────┼──────────────┤
│       │            │            │            │              │
│       v            v            v            v              │
│                  Foundation Layer                            │
│                                                              │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │ logger_  │ │ network_ │ │database_ │ │ thread_  │       │
│  │ system   │ │ system   │ │ system   │ │ system   │       │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘       │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 Design Principles

| Principle | Description |
|-----------|-------------|
| **Single Responsibility** | Each adapter wraps exactly one foundation system |
| **Domain Language** | APIs use game terminology (Player, Spell, Zone) |
| **Error Handling** | All methods return Result<T, E> |
| **Testability** | Interface-based for easy mocking |
| **Performance** | Minimal overhead over raw foundation calls |

---

## 2. GameLogger Adapter

### 2.1 Interface

```cpp
namespace game::foundation {

// Log levels with game context
enum class GameLogLevel {
    Trace,      // Detailed debugging
    Debug,      // Development info
    Info,       // Normal operations
    Warning,    // Potential issues
    Error,      // Errors (recoverable)
    Critical,   // Critical errors (may crash)
    Security    // Security-related events
};

// Log categories for filtering
enum class GameLogCategory {
    General,
    Network,
    Database,
    Combat,
    Movement,
    Quest,
    Inventory,
    Guild,
    Chat,
    AI,
    Spawn,
    Loot,
    Security,
    Performance
};

class IGameLogger {
public:
    virtual ~IGameLogger() = default;

    // Basic logging
    virtual void log(GameLogLevel level, GameLogCategory category,
                    std::string_view message) = 0;

    // Structured logging with context
    virtual void log(GameLogLevel level, GameLogCategory category,
                    std::string_view message,
                    const std::unordered_map<std::string, std::string>& context) = 0;

    // Game-specific logging
    virtual void log_player_action(PlayerId player, std::string_view action,
                                   const std::string& details = "") = 0;
    virtual void log_combat_event(EntityId source, EntityId target,
                                  const CombatEventInfo& info) = 0;
    virtual void log_world_event(WorldEventType type,
                                 const WorldEventInfo& info) = 0;
    virtual void log_security_event(SecurityEventType type,
                                    const SecurityEventInfo& info) = 0;
};

}  // namespace game::foundation
```

### 2.2 Implementation

```cpp
namespace game::foundation {

class GameLogger : public IGameLogger {
public:
    explicit GameLogger(const std::string& name)
        : name_(name)
        , logger_(kcenon::logger::LoggerFactory::get(name)) {}

    // Basic logging
    void log(GameLogLevel level, GameLogCategory category,
             std::string_view message) override {
        auto kcenon_level = convert_level(level);
        logger_.log(kcenon_level, "[{}] {}", category_name(category), message);
    }

    // Structured logging
    void log(GameLogLevel level, GameLogCategory category,
             std::string_view message,
             const std::unordered_map<std::string, std::string>& context) override {
        auto kcenon_level = convert_level(level);

        // Build context string
        std::string ctx_str;
        for (const auto& [key, value] : context) {
            if (!ctx_str.empty()) ctx_str += ", ";
            ctx_str += fmt::format("{}={}", key, value);
        }

        logger_.log(kcenon_level, "[{}] {} | {}", category_name(category),
                   message, ctx_str);
    }

    // Player action logging
    void log_player_action(PlayerId player, std::string_view action,
                           const std::string& details) override {
        log(GameLogLevel::Info, GameLogCategory::General,
            fmt::format("Player {} performed: {}", player, action),
            {{"player_id", std::to_string(player)},
             {"action", std::string(action)},
             {"details", details}});
    }

    // Combat event logging
    void log_combat_event(EntityId source, EntityId target,
                          const CombatEventInfo& info) override {
        log(GameLogLevel::Debug, GameLogCategory::Combat,
            fmt::format("{} -> {} : {} {} ({})",
                       source, target, info.type_name(), info.amount,
                       info.is_critical ? "CRIT" : "normal"),
            {{"source", std::to_string(source)},
             {"target", std::to_string(target)},
             {"type", info.type_name()},
             {"amount", std::to_string(info.amount)},
             {"spell_id", std::to_string(info.spell_id)}});
    }

    // World event logging
    void log_world_event(WorldEventType type,
                         const WorldEventInfo& info) override {
        log(GameLogLevel::Info, GameLogCategory::Spawn,
            fmt::format("World event: {} at ({}, {}, {})",
                       world_event_name(type),
                       info.position.x, info.position.y, info.position.z),
            {{"event_type", world_event_name(type)},
             {"map_id", std::to_string(info.map_id)},
             {"zone_id", std::to_string(info.zone_id)}});
    }

    // Security event logging
    void log_security_event(SecurityEventType type,
                            const SecurityEventInfo& info) override {
        log(GameLogLevel::Security, GameLogCategory::Security,
            fmt::format("Security: {} - {}", security_event_name(type), info.description),
            {{"event_type", security_event_name(type)},
             {"ip_address", info.ip_address},
             {"account_id", std::to_string(info.account_id)},
             {"severity", std::to_string(static_cast<int>(info.severity))}});
    }

    // Convenience methods
    template<typename... Args>
    void trace(GameLogCategory cat, fmt::format_string<Args...> fmt, Args&&... args) {
        log(GameLogLevel::Trace, cat, fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void debug(GameLogCategory cat, fmt::format_string<Args...> fmt, Args&&... args) {
        log(GameLogLevel::Debug, cat, fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void info(GameLogCategory cat, fmt::format_string<Args...> fmt, Args&&... args) {
        log(GameLogLevel::Info, cat, fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void warn(GameLogCategory cat, fmt::format_string<Args...> fmt, Args&&... args) {
        log(GameLogLevel::Warning, cat, fmt::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void error(GameLogCategory cat, fmt::format_string<Args...> fmt, Args&&... args) {
        log(GameLogLevel::Error, cat, fmt::format(fmt, std::forward<Args>(args)...));
    }

private:
    static kcenon::logger::LogLevel convert_level(GameLogLevel level) {
        switch (level) {
            case GameLogLevel::Trace: return kcenon::logger::LogLevel::Trace;
            case GameLogLevel::Debug: return kcenon::logger::LogLevel::Debug;
            case GameLogLevel::Info: return kcenon::logger::LogLevel::Info;
            case GameLogLevel::Warning: return kcenon::logger::LogLevel::Warning;
            case GameLogLevel::Error: return kcenon::logger::LogLevel::Error;
            case GameLogLevel::Critical: return kcenon::logger::LogLevel::Critical;
            case GameLogLevel::Security: return kcenon::logger::LogLevel::Warning;
            default: return kcenon::logger::LogLevel::Info;
        }
    }

    static const char* category_name(GameLogCategory cat) {
        switch (cat) {
            case GameLogCategory::General: return "General";
            case GameLogCategory::Network: return "Network";
            case GameLogCategory::Database: return "Database";
            case GameLogCategory::Combat: return "Combat";
            case GameLogCategory::Movement: return "Movement";
            case GameLogCategory::Quest: return "Quest";
            case GameLogCategory::Inventory: return "Inventory";
            case GameLogCategory::Guild: return "Guild";
            case GameLogCategory::Chat: return "Chat";
            case GameLogCategory::AI: return "AI";
            case GameLogCategory::Spawn: return "Spawn";
            case GameLogCategory::Loot: return "Loot";
            case GameLogCategory::Security: return "Security";
            case GameLogCategory::Performance: return "Performance";
            default: return "Unknown";
        }
    }

    std::string name_;
    kcenon::logger::Logger& logger_;
};

}  // namespace game::foundation
```

---

## 3. GameNetwork Adapter

### 3.1 Interface

```cpp
namespace game::foundation {

class IGameNetwork {
public:
    virtual ~IGameNetwork() = default;

    // Session management
    virtual Result<SessionId> create_session(PlayerId player,
                                              ConnectionId conn) = 0;
    virtual Result<void> destroy_session(SessionId session) = 0;
    virtual std::optional<SessionInfo> get_session(SessionId session) const = 0;
    virtual std::optional<SessionId> get_session_by_player(PlayerId player) const = 0;

    // Packet sending
    virtual Result<void> send_packet(SessionId session,
                                     const GamePacket& packet) = 0;
    virtual Result<void> send_packet(PlayerId player,
                                     const GamePacket& packet) = 0;

    // Broadcast
    virtual Result<void> broadcast_to_map(MapId map,
                                          const GamePacket& packet) = 0;
    virtual Result<void> broadcast_to_zone(ZoneId zone,
                                           const GamePacket& packet) = 0;
    virtual Result<void> broadcast_to_range(const Vector3& center, float range,
                                            MapId map, const GamePacket& packet) = 0;

    // Multicast
    virtual Result<void> multicast(const std::vector<SessionId>& sessions,
                                   const GamePacket& packet) = 0;
    virtual Result<void> multicast(const std::vector<PlayerId>& players,
                                   const GamePacket& packet) = 0;

    // Connection management
    virtual void disconnect_session(SessionId session, DisconnectReason reason) = 0;
    virtual ConnectionStats get_stats() const = 0;
};

}  // namespace game::foundation
```

### 3.2 Implementation

```cpp
namespace game::foundation {

class GameNetwork : public IGameNetwork {
public:
    GameNetwork(kcenon::network::TcpServer& server)
        : server_(server)
        , logger_("Network") {}

    // Session management
    Result<SessionId> create_session(PlayerId player, ConnectionId conn) override {
        std::lock_guard lock(mutex_);

        SessionId session_id = next_session_id_++;

        SessionInfo info{
            .session_id = session_id,
            .player_id = player,
            .connection_id = conn,
            .created_at = std::chrono::steady_clock::now(),
            .last_activity = std::chrono::steady_clock::now()
        };

        sessions_[session_id] = info;
        player_to_session_[player] = session_id;
        connection_to_session_[conn] = session_id;

        logger_.info(GameLogCategory::Network,
                    "Created session {} for player {}", session_id, player);

        return Result<SessionId>::ok(session_id);
    }

    Result<void> destroy_session(SessionId session) override {
        std::lock_guard lock(mutex_);

        auto it = sessions_.find(session);
        if (it == sessions_.end()) {
            return Result<void>::error(Error::SessionNotFound);
        }

        player_to_session_.erase(it->second.player_id);
        connection_to_session_.erase(it->second.connection_id);
        sessions_.erase(it);

        logger_.info(GameLogCategory::Network, "Destroyed session {}", session);

        return Result<void>::ok();
    }

    std::optional<SessionInfo> get_session(SessionId session) const override {
        std::lock_guard lock(mutex_);

        auto it = sessions_.find(session);
        if (it == sessions_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::optional<SessionId> get_session_by_player(PlayerId player) const override {
        std::lock_guard lock(mutex_);

        auto it = player_to_session_.find(player);
        if (it == player_to_session_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    // Packet sending
    Result<void> send_packet(SessionId session, const GamePacket& packet) override {
        auto info = get_session(session);
        if (!info) {
            return Result<void>::error(Error::SessionNotFound);
        }

        return send_to_connection(info->connection_id, packet);
    }

    Result<void> send_packet(PlayerId player, const GamePacket& packet) override {
        auto session = get_session_by_player(player);
        if (!session) {
            return Result<void>::error(Error::PlayerNotConnected);
        }
        return send_packet(*session, packet);
    }

    // Broadcast
    Result<void> broadcast_to_map(MapId map, const GamePacket& packet) override {
        std::lock_guard lock(mutex_);

        auto it = map_sessions_.find(map);
        if (it == map_sessions_.end()) {
            return Result<void>::ok();  // No sessions in map
        }

        for (SessionId session : it->second) {
            send_packet(session, packet);  // Ignore individual failures
        }

        return Result<void>::ok();
    }

    Result<void> broadcast_to_zone(ZoneId zone, const GamePacket& packet) override {
        std::lock_guard lock(mutex_);

        auto it = zone_sessions_.find(zone);
        if (it == zone_sessions_.end()) {
            return Result<void>::ok();
        }

        for (SessionId session : it->second) {
            send_packet(session, packet);
        }

        return Result<void>::ok();
    }

    Result<void> broadcast_to_range(const Vector3& center, float range,
                                    MapId map, const GamePacket& packet) override {
        // Get sessions in range (uses spatial index if available)
        auto sessions = get_sessions_in_range(center, range, map);

        for (SessionId session : sessions) {
            send_packet(session, packet);
        }

        return Result<void>::ok();
    }

    // Multicast
    Result<void> multicast(const std::vector<SessionId>& sessions,
                          const GamePacket& packet) override {
        for (SessionId session : sessions) {
            send_packet(session, packet);  // Ignore individual failures
        }
        return Result<void>::ok();
    }

    Result<void> multicast(const std::vector<PlayerId>& players,
                          const GamePacket& packet) override {
        for (PlayerId player : players) {
            send_packet(player, packet);
        }
        return Result<void>::ok();
    }

    // Connection management
    void disconnect_session(SessionId session, DisconnectReason reason) override {
        auto info = get_session(session);
        if (!info) return;

        logger_.info(GameLogCategory::Network,
                    "Disconnecting session {} (reason: {})",
                    session, static_cast<int>(reason));

        server_.disconnect(info->connection_id);
        destroy_session(session);
    }

    ConnectionStats get_stats() const override {
        std::lock_guard lock(mutex_);

        return ConnectionStats{
            .total_sessions = sessions_.size(),
            .bytes_sent = bytes_sent_,
            .bytes_received = bytes_received_,
            .packets_sent = packets_sent_,
            .packets_received = packets_received_
        };
    }

    // Session location tracking (for efficient broadcasts)
    void update_session_location(SessionId session, MapId map, ZoneId zone) {
        std::lock_guard lock(mutex_);

        auto it = sessions_.find(session);
        if (it == sessions_.end()) return;

        // Remove from old location
        if (it->second.current_map != MapId{0}) {
            map_sessions_[it->second.current_map].erase(session);
            zone_sessions_[it->second.current_zone].erase(session);
        }

        // Add to new location
        it->second.current_map = map;
        it->second.current_zone = zone;
        map_sessions_[map].insert(session);
        zone_sessions_[zone].insert(session);
    }

private:
    Result<void> send_to_connection(ConnectionId conn, const GamePacket& packet) {
        // Serialize packet
        auto data = packet.serialize();

        // Send via foundation network system
        auto result = server_.send(conn, data.data(), data.size());
        if (!result) {
            return Result<void>::error(Error::SendFailed);
        }

        bytes_sent_ += data.size();
        packets_sent_++;

        return Result<void>::ok();
    }

    std::vector<SessionId> get_sessions_in_range(const Vector3& center,
                                                  float range, MapId map) {
        std::vector<SessionId> result;

        auto it = map_sessions_.find(map);
        if (it == map_sessions_.end()) {
            return result;
        }

        // TODO: Use spatial index for better performance
        for (SessionId session : it->second) {
            auto info = get_session(session);
            if (info && info->position.has_value()) {
                float dist = (info->position.value() - center).length();
                if (dist <= range) {
                    result.push_back(session);
                }
            }
        }

        return result;
    }

    kcenon::network::TcpServer& server_;
    GameLogger logger_;

    mutable std::mutex mutex_;
    SessionId next_session_id_ = 1;

    std::unordered_map<SessionId, SessionInfo> sessions_;
    std::unordered_map<PlayerId, SessionId> player_to_session_;
    std::unordered_map<ConnectionId, SessionId> connection_to_session_;

    std::unordered_map<MapId, std::set<SessionId>> map_sessions_;
    std::unordered_map<ZoneId, std::set<SessionId>> zone_sessions_;

    std::atomic<uint64_t> bytes_sent_{0};
    std::atomic<uint64_t> bytes_received_{0};
    std::atomic<uint64_t> packets_sent_{0};
    std::atomic<uint64_t> packets_received_{0};
};

}  // namespace game::foundation
```

---

## 4. GameDatabase Adapter

### 4.1 Interface

```cpp
namespace game::foundation {

class IGameDatabase {
public:
    virtual ~IGameDatabase() = default;

    // Player data
    virtual Result<PlayerData> load_player(CharacterId character) = 0;
    virtual Result<void> save_player(const PlayerData& data) = 0;
    virtual Result<std::vector<CharacterSummary>> get_character_list(
        AccountId account) = 0;

    // Inventory
    virtual Result<Inventory> load_inventory(CharacterId character) = 0;
    virtual Result<void> save_inventory(CharacterId character,
                                        const Inventory& inventory) = 0;

    // Quests
    virtual Result<QuestLog> load_quests(CharacterId character) = 0;
    virtual Result<void> save_quests(CharacterId character,
                                     const QuestLog& quests) = 0;

    // Guild
    virtual Result<GuildData> load_guild(GuildId guild) = 0;
    virtual Result<void> save_guild(const GuildData& guild) = 0;
    virtual Result<std::vector<GuildMember>> load_guild_members(GuildId guild) = 0;

    // World data
    virtual Result<MapData> load_map(MapId map) = 0;
    virtual Result<std::vector<SpawnPoint>> load_spawn_points(MapId map) = 0;
    virtual Result<std::vector<GameObjectSpawn>> load_gameobject_spawns(
        MapId map) = 0;

    // Transactions
    virtual Result<void> begin_transaction() = 0;
    virtual Result<void> commit() = 0;
    virtual Result<void> rollback() = 0;

    // Raw query (use sparingly)
    virtual Result<QueryResult> execute(const std::string& query) = 0;
    virtual Result<QueryResult> execute(const std::string& query,
                                        const std::vector<QueryParam>& params) = 0;
};

}  // namespace game::foundation
```

### 4.2 Implementation

```cpp
namespace game::foundation {

class GameDatabase : public IGameDatabase {
public:
    GameDatabase(kcenon::database::Database& db)
        : db_(db)
        , logger_("Database") {}

    // Player data
    Result<PlayerData> load_player(CharacterId character) override {
        auto result = db_.execute(
            "SELECT * FROM characters WHERE id = ?",
            {character}
        );

        if (!result) {
            return Result<PlayerData>::error(Error::DatabaseError,
                "Failed to load player");
        }

        if (result->empty()) {
            return Result<PlayerData>::error(Error::NotFound,
                "Character not found");
        }

        return Result<PlayerData>::ok(PlayerData::from_row(result->front()));
    }

    Result<void> save_player(const PlayerData& data) override {
        auto result = db_.execute(
            "UPDATE characters SET "
            "level = ?, experience = ?, health = ?, mana = ?, "
            "position_x = ?, position_y = ?, position_z = ?, "
            "map_id = ?, zone_id = ?, orientation = ?, "
            "played_time = ?, updated_at = NOW() "
            "WHERE id = ?",
            {data.level, data.experience, data.health, data.mana,
             data.position.x, data.position.y, data.position.z,
             data.map_id, data.zone_id, data.orientation,
             data.played_time, data.character_id}
        );

        if (!result) {
            return Result<void>::error(Error::DatabaseError,
                "Failed to save player");
        }

        return Result<void>::ok();
    }

    Result<std::vector<CharacterSummary>> get_character_list(
        AccountId account) override {
        auto result = db_.execute(
            "SELECT id, name, race, class, level, zone_id "
            "FROM characters WHERE account_id = ? AND deleted = false "
            "ORDER BY last_login DESC",
            {account}
        );

        if (!result) {
            return Result<std::vector<CharacterSummary>>::error(
                Error::DatabaseError);
        }

        std::vector<CharacterSummary> characters;
        for (const auto& row : *result) {
            characters.push_back(CharacterSummary::from_row(row));
        }

        return Result<std::vector<CharacterSummary>>::ok(std::move(characters));
    }

    // Inventory
    Result<Inventory> load_inventory(CharacterId character) override {
        auto result = db_.execute(
            "SELECT slot, item_id, count, durability, enchants "
            "FROM character_inventory WHERE character_id = ?",
            {character}
        );

        if (!result) {
            return Result<Inventory>::error(Error::DatabaseError);
        }

        Inventory inventory;
        for (const auto& row : *result) {
            inventory.set_item(
                row.get<int>("slot"),
                ItemStack::from_row(row)
            );
        }

        return Result<Inventory>::ok(std::move(inventory));
    }

    Result<void> save_inventory(CharacterId character,
                                const Inventory& inventory) override {
        // Begin transaction
        auto tx_result = begin_transaction();
        if (!tx_result) return tx_result;

        // Delete old inventory
        auto delete_result = db_.execute(
            "DELETE FROM character_inventory WHERE character_id = ?",
            {character}
        );

        if (!delete_result) {
            rollback();
            return Result<void>::error(Error::DatabaseError);
        }

        // Insert new inventory
        for (const auto& [slot, item] : inventory.items()) {
            if (item.is_empty()) continue;

            auto insert_result = db_.execute(
                "INSERT INTO character_inventory "
                "(character_id, slot, item_id, count, durability, enchants) "
                "VALUES (?, ?, ?, ?, ?, ?)",
                {character, slot, item.item_id, item.count,
                 item.durability, item.enchants_json()}
            );

            if (!insert_result) {
                rollback();
                return Result<void>::error(Error::DatabaseError);
            }
        }

        return commit();
    }

    // Quests
    Result<QuestLog> load_quests(CharacterId character) override {
        // Active quests
        auto active_result = db_.execute(
            "SELECT quest_id, status, objectives_progress, started_at "
            "FROM character_quests WHERE character_id = ? AND completed = false",
            {character}
        );

        if (!active_result) {
            return Result<QuestLog>::error(Error::DatabaseError);
        }

        QuestLog quests;
        for (const auto& row : *active_result) {
            quests.add_active(QuestProgress::from_row(row));
        }

        // Completed quests
        auto completed_result = db_.execute(
            "SELECT quest_id, completed_at "
            "FROM character_quests WHERE character_id = ? AND completed = true",
            {character}
        );

        if (completed_result) {
            for (const auto& row : *completed_result) {
                quests.add_completed(row.get<QuestId>("quest_id"));
            }
        }

        return Result<QuestLog>::ok(std::move(quests));
    }

    // World data
    Result<std::vector<SpawnPoint>> load_spawn_points(MapId map) override {
        auto result = db_.execute(
            "SELECT id, creature_id, position_x, position_y, position_z, "
            "orientation, spawn_time, movement_type "
            "FROM creature_spawns WHERE map_id = ?",
            {map}
        );

        if (!result) {
            return Result<std::vector<SpawnPoint>>::error(Error::DatabaseError);
        }

        std::vector<SpawnPoint> spawns;
        for (const auto& row : *result) {
            spawns.push_back(SpawnPoint::from_row(row));
        }

        return Result<std::vector<SpawnPoint>>::ok(std::move(spawns));
    }

    // Transactions
    Result<void> begin_transaction() override {
        auto result = db_.begin_transaction();
        if (!result) {
            return Result<void>::error(Error::DatabaseError,
                "Failed to begin transaction");
        }
        return Result<void>::ok();
    }

    Result<void> commit() override {
        auto result = db_.commit();
        if (!result) {
            return Result<void>::error(Error::DatabaseError,
                "Failed to commit transaction");
        }
        return Result<void>::ok();
    }

    Result<void> rollback() override {
        auto result = db_.rollback();
        if (!result) {
            return Result<void>::error(Error::DatabaseError,
                "Failed to rollback transaction");
        }
        return Result<void>::ok();
    }

    // Raw query
    Result<QueryResult> execute(const std::string& query) override {
        return db_.execute(query);
    }

    Result<QueryResult> execute(const std::string& query,
                                const std::vector<QueryParam>& params) override {
        return db_.execute(query, params);
    }

private:
    kcenon::database::Database& db_;
    GameLogger logger_;
};

}  // namespace game::foundation
```

---

## 5. GameThreadPool Adapter

### 5.1 Interface

```cpp
namespace game::foundation {

// Job priorities for game operations
enum class GameJobPriority {
    Critical,   // Network packets, player actions
    High,       // World ticks, combat updates
    Normal,     // AI updates, spawn checks
    Low,        // Analytics, non-essential
    Background  // Cleanup, optimization
};

class IGameThreadPool {
public:
    virtual ~IGameThreadPool() = default;

    // Schedule jobs
    virtual JobHandle schedule(GameJobPriority priority,
                               std::function<void()> job) = 0;

    // Game-specific scheduling
    virtual JobHandle schedule_tick(WorldId world,
                                    std::function<void()> tick) = 0;
    virtual JobHandle schedule_player_action(PlayerId player,
                                             std::function<void()> action) = 0;
    virtual JobHandle schedule_ai_update(EntityId entity,
                                         std::function<void()> update) = 0;

    // Delayed execution
    virtual JobHandle schedule_delayed(GameJobPriority priority,
                                       std::chrono::milliseconds delay,
                                       std::function<void()> job) = 0;

    // Periodic execution
    virtual JobHandle schedule_periodic(GameJobPriority priority,
                                        std::chrono::milliseconds interval,
                                        std::function<void()> job) = 0;

    // Job management
    virtual bool cancel(JobHandle handle) = 0;
    virtual bool is_running(JobHandle handle) const = 0;

    // Stats
    virtual ThreadPoolStats get_stats() const = 0;
};

}  // namespace game::foundation
```

### 5.2 Implementation

```cpp
namespace game::foundation {

class GameThreadPool : public IGameThreadPool {
public:
    GameThreadPool(kcenon::thread::TypedThreadPool& pool)
        : pool_(pool)
        , logger_("ThreadPool") {}

    JobHandle schedule(GameJobPriority priority,
                       std::function<void()> job) override {
        auto kcenon_priority = convert_priority(priority);
        return pool_.schedule(kcenon_priority, std::move(job));
    }

    JobHandle schedule_tick(WorldId world,
                            std::function<void()> tick) override {
        // World ticks are high priority
        return schedule(GameJobPriority::High, [this, world, tick = std::move(tick)]() {
            auto start = std::chrono::steady_clock::now();

            tick();

            auto elapsed = std::chrono::steady_clock::now() - start;
            auto ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);

            if (ms > std::chrono::milliseconds(50)) {
                logger_.warn(GameLogCategory::Performance,
                            "World {} tick took {}us (target: 50000us)",
                            world, ms.count());
            }
        });
    }

    JobHandle schedule_player_action(PlayerId player,
                                     std::function<void()> action) override {
        // Player actions are critical priority
        return schedule(GameJobPriority::Critical, std::move(action));
    }

    JobHandle schedule_ai_update(EntityId entity,
                                 std::function<void()> update) override {
        // AI updates are normal priority
        return schedule(GameJobPriority::Normal, std::move(update));
    }

    JobHandle schedule_delayed(GameJobPriority priority,
                               std::chrono::milliseconds delay,
                               std::function<void()> job) override {
        auto kcenon_priority = convert_priority(priority);
        return pool_.schedule_delayed(kcenon_priority, delay, std::move(job));
    }

    JobHandle schedule_periodic(GameJobPriority priority,
                                std::chrono::milliseconds interval,
                                std::function<void()> job) override {
        auto kcenon_priority = convert_priority(priority);
        return pool_.schedule_periodic(kcenon_priority, interval, std::move(job));
    }

    bool cancel(JobHandle handle) override {
        return pool_.cancel(handle);
    }

    bool is_running(JobHandle handle) const override {
        return pool_.is_running(handle);
    }

    ThreadPoolStats get_stats() const override {
        auto kcenon_stats = pool_.get_stats();
        return ThreadPoolStats{
            .total_jobs_processed = kcenon_stats.total_jobs,
            .jobs_per_second = kcenon_stats.jobs_per_second,
            .average_job_time_us = kcenon_stats.avg_job_time_us,
            .queue_size = kcenon_stats.queue_size,
            .active_workers = kcenon_stats.active_workers
        };
    }

private:
    static kcenon::thread::Priority convert_priority(GameJobPriority priority) {
        switch (priority) {
            case GameJobPriority::Critical: return kcenon::thread::Priority::Critical;
            case GameJobPriority::High: return kcenon::thread::Priority::High;
            case GameJobPriority::Normal: return kcenon::thread::Priority::Normal;
            case GameJobPriority::Low: return kcenon::thread::Priority::Low;
            case GameJobPriority::Background: return kcenon::thread::Priority::Background;
            default: return kcenon::thread::Priority::Normal;
        }
    }

    kcenon::thread::TypedThreadPool& pool_;
    GameLogger logger_;
};

}  // namespace game::foundation
```

---

## 6. GameMonitor Adapter

### 6.1 Interface

```cpp
namespace game::foundation {

class IGameMonitor {
public:
    virtual ~IGameMonitor() = default;

    // Player metrics
    virtual void record_online_players(int count) = 0;
    virtual void record_player_login() = 0;
    virtual void record_player_logout() = 0;

    // Performance metrics
    virtual void record_tick_duration(WorldId world,
                                      std::chrono::microseconds duration) = 0;
    virtual void record_message_latency(std::chrono::microseconds latency) = 0;
    virtual void record_database_query(std::chrono::microseconds duration,
                                       bool success) = 0;

    // Game metrics
    virtual void record_combat_action(CombatActionType type) = 0;
    virtual void record_item_transaction(ItemTransactionType type) = 0;
    virtual void record_quest_completion(QuestId quest) = 0;
    virtual void record_gold_transaction(int64_t amount,
                                         GoldTransactionType type) = 0;

    // Tracing
    virtual TraceContext start_trace(const std::string& operation) = 0;
    virtual void end_trace(TraceContext& ctx) = 0;

    // Health checks
    virtual HealthStatus get_health() const = 0;
};

}  // namespace game::foundation
```

### 6.2 Implementation

```cpp
namespace game::foundation {

class GameMonitor : public IGameMonitor {
public:
    GameMonitor(kcenon::monitoring::Metrics& metrics,
                kcenon::monitoring::Tracer& tracer)
        : metrics_(metrics)
        , tracer_(tracer) {
        register_metrics();
    }

    // Player metrics
    void record_online_players(int count) override {
        metrics_.set_gauge("game_online_players", count);
    }

    void record_player_login() override {
        metrics_.increment("game_player_logins_total");
    }

    void record_player_logout() override {
        metrics_.increment("game_player_logouts_total");
    }

    // Performance metrics
    void record_tick_duration(WorldId world,
                              std::chrono::microseconds duration) override {
        metrics_.observe_histogram(
            "game_tick_duration_us",
            duration.count(),
            {{"world_id", std::to_string(world)}}
        );
    }

    void record_message_latency(std::chrono::microseconds latency) override {
        metrics_.observe_histogram("game_message_latency_us", latency.count());
    }

    void record_database_query(std::chrono::microseconds duration,
                               bool success) override {
        metrics_.observe_histogram(
            "game_db_query_duration_us",
            duration.count(),
            {{"success", success ? "true" : "false"}}
        );

        if (!success) {
            metrics_.increment("game_db_query_errors_total");
        }
    }

    // Game metrics
    void record_combat_action(CombatActionType type) override {
        metrics_.increment(
            "game_combat_actions_total",
            {{"type", combat_action_name(type)}}
        );
    }

    void record_item_transaction(ItemTransactionType type) override {
        metrics_.increment(
            "game_item_transactions_total",
            {{"type", item_transaction_name(type)}}
        );
    }

    void record_quest_completion(QuestId quest) override {
        metrics_.increment(
            "game_quests_completed_total",
            {{"quest_id", std::to_string(quest)}}
        );
    }

    void record_gold_transaction(int64_t amount,
                                 GoldTransactionType type) override {
        metrics_.observe_histogram(
            "game_gold_transaction_amount",
            static_cast<double>(amount),
            {{"type", gold_transaction_name(type)}}
        );
    }

    // Tracing
    TraceContext start_trace(const std::string& operation) override {
        auto span = tracer_.start_span(operation);
        return TraceContext{
            .span_id = span.id(),
            .operation = operation,
            .start_time = std::chrono::steady_clock::now()
        };
    }

    void end_trace(TraceContext& ctx) override {
        auto elapsed = std::chrono::steady_clock::now() - ctx.start_time;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

        tracer_.end_span(ctx.span_id, ms);
    }

    // Health checks
    HealthStatus get_health() const override {
        // Check various system health indicators
        HealthStatus status;
        status.healthy = true;
        status.components["database"] = check_database_health();
        status.components["network"] = check_network_health();
        status.components["thread_pool"] = check_thread_pool_health();

        for (const auto& [name, healthy] : status.components) {
            if (!healthy) {
                status.healthy = false;
                break;
            }
        }

        return status;
    }

private:
    void register_metrics() {
        // Register all game metrics with Prometheus-compatible names

        // Gauges
        metrics_.register_gauge("game_online_players",
            "Current number of online players");

        // Counters
        metrics_.register_counter("game_player_logins_total",
            "Total player logins");
        metrics_.register_counter("game_player_logouts_total",
            "Total player logouts");
        metrics_.register_counter("game_combat_actions_total",
            "Total combat actions by type");
        metrics_.register_counter("game_item_transactions_total",
            "Total item transactions by type");
        metrics_.register_counter("game_quests_completed_total",
            "Total quests completed");
        metrics_.register_counter("game_db_query_errors_total",
            "Total database query errors");

        // Histograms
        metrics_.register_histogram("game_tick_duration_us",
            "World tick duration in microseconds",
            {1000, 5000, 10000, 25000, 50000, 100000});
        metrics_.register_histogram("game_message_latency_us",
            "Message processing latency in microseconds",
            {100, 500, 1000, 5000, 10000});
        metrics_.register_histogram("game_db_query_duration_us",
            "Database query duration in microseconds",
            {1000, 5000, 10000, 25000, 50000, 100000});
        metrics_.register_histogram("game_gold_transaction_amount",
            "Gold transaction amounts",
            {100, 1000, 10000, 100000, 1000000});
    }

    bool check_database_health() const {
        // Simple ping query
        return true;  // Implement actual check
    }

    bool check_network_health() const {
        // Check network subsystem
        return true;  // Implement actual check
    }

    bool check_thread_pool_health() const {
        // Check thread pool queue size
        return true;  // Implement actual check
    }

    kcenon::monitoring::Metrics& metrics_;
    kcenon::monitoring::Tracer& tracer_;
};

}  // namespace game::foundation
```

---

## 7. GameFoundation Facade

### 7.1 Unified Access Point

```cpp
namespace game::foundation {

// Single entry point for all foundation adapters
class GameFoundation {
public:
    GameFoundation(kcenon::common::ServiceContainer& container)
        : container_(container)
        , logger_(std::make_unique<GameLogger>("Game"))
        , network_(std::make_unique<GameNetwork>(
            container.get<kcenon::network::TcpServer>()))
        , database_(std::make_unique<GameDatabase>(
            container.get<kcenon::database::Database>()))
        , thread_pool_(std::make_unique<GameThreadPool>(
            container.get<kcenon::thread::TypedThreadPool>()))
        , monitor_(std::make_unique<GameMonitor>(
            container.get<kcenon::monitoring::Metrics>(),
            container.get<kcenon::monitoring::Tracer>())) {}

    // Adapter access
    GameLogger& logger() { return *logger_; }
    IGameNetwork& network() { return *network_; }
    IGameDatabase& database() { return *database_; }
    IGameThreadPool& thread_pool() { return *thread_pool_; }
    IGameMonitor& monitor() { return *monitor_; }

    // Named loggers
    GameLogger get_logger(const std::string& name) {
        return GameLogger(name);
    }

private:
    kcenon::common::ServiceContainer& container_;

    std::unique_ptr<GameLogger> logger_;
    std::unique_ptr<IGameNetwork> network_;
    std::unique_ptr<IGameDatabase> database_;
    std::unique_ptr<IGameThreadPool> thread_pool_;
    std::unique_ptr<IGameMonitor> monitor_;
};

}  // namespace game::foundation
```

---

## 8. Testing Adapters

### 8.1 Mock Implementations

```cpp
namespace game::foundation::testing {

// Mock logger for testing
class MockGameLogger : public GameLogger {
public:
    MockGameLogger() : GameLogger("Mock") {}

    void log(GameLogLevel level, GameLogCategory category,
             std::string_view message) override {
        logs_.push_back({level, category, std::string(message)});
    }

    const std::vector<LogEntry>& logs() const { return logs_; }
    void clear() { logs_.clear(); }

private:
    struct LogEntry {
        GameLogLevel level;
        GameLogCategory category;
        std::string message;
    };
    std::vector<LogEntry> logs_;
};

// Mock network for testing
class MockGameNetwork : public IGameNetwork {
public:
    Result<void> send_packet(SessionId session,
                             const GamePacket& packet) override {
        sent_packets_.push_back({session, packet});
        return Result<void>::ok();
    }

    const std::vector<std::pair<SessionId, GamePacket>>& sent_packets() const {
        return sent_packets_;
    }

private:
    std::vector<std::pair<SessionId, GamePacket>> sent_packets_;
};

// Mock database for testing
class MockGameDatabase : public IGameDatabase {
public:
    void set_player_data(CharacterId id, const PlayerData& data) {
        players_[id] = data;
    }

    Result<PlayerData> load_player(CharacterId character) override {
        auto it = players_.find(character);
        if (it == players_.end()) {
            return Result<PlayerData>::error(Error::NotFound);
        }
        return Result<PlayerData>::ok(it->second);
    }

private:
    std::unordered_map<CharacterId, PlayerData> players_;
};

}  // namespace game::foundation::testing
```

---

## 9. Appendices

### 9.1 Error Codes

| Code Range | Category | Description |
|------------|----------|-------------|
| 0x0001-0x0FFF | Common | General errors |
| 0x1000-0x1FFF | Network | Network-related errors |
| 0x2000-0x2FFF | Database | Database-related errors |
| 0x3000-0x3FFF | Thread | Thread pool errors |
| 0x4000-0x4FFF | Monitor | Monitoring errors |

### 9.2 Related Documents

- [ARCHITECTURE.md](../ARCHITECTURE.md) - System architecture
- [ECS_DESIGN.md](./ECS_DESIGN.md) - ECS integration

---

*Foundation Adapters Version*: 1.0.0
*Foundation Adapter Patterns for Common Game Server*
