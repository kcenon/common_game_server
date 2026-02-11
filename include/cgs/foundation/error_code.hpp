#pragma once

/// @file error_code.hpp
/// @brief Categorized error codes for the game server framework.

#include <cstdint>
#include <string_view>

namespace cgs::foundation {

/// Error codes categorized by subsystem using hex ranges.
///
/// Each subsystem occupies a 256-value range (0x100), making it possible
/// to determine the error source from the code value alone.
enum class ErrorCode : uint32_t {
    // General (0x0000 - 0x00FF)
    Success = 0x0000,
    Unknown = 0x0001,
    InvalidArgument = 0x0002,
    NotFound = 0x0003,
    AlreadyExists = 0x0004,
    NotImplemented = 0x0005,

    // Network (0x0100 - 0x01FF)
    NetworkError = 0x0100,
    ConnectionFailed = 0x0101,
    ConnectionLost = 0x0102,
    Timeout = 0x0103,
    SendFailed = 0x0104,
    ListenFailed = 0x0105,
    SessionNotFound = 0x0106,
    InvalidMessage = 0x0107,

    // Database (0x0200 - 0x02FF)
    DatabaseError = 0x0200,
    QueryFailed = 0x0201,
    TransactionFailed = 0x0202,
    ConnectionPoolExhausted = 0x0203,
    ConnectionPoolTimeout = 0x0204,
    NotConnected = 0x0205,
    PreparedStatementFailed = 0x0206,

    // ECS (0x0300 - 0x03FF)
    EntityNotFound = 0x0300,
    ComponentNotFound = 0x0301,
    SystemError = 0x0302,

    // Plugin (0x0400 - 0x04FF)
    PluginLoadFailed = 0x0400,
    PluginNotFound = 0x0401,
    DependencyError = 0x0402,
    PluginAlreadyLoaded = 0x0403,
    PluginInvalidState = 0x0404,
    PluginVersionMismatch = 0x0405,
    PluginInitFailed = 0x0406,
    HotReloadFailed = 0x0407,
    HotReloadDisabled = 0x0408,
    StateSerializationFailed = 0x0409,
    StateDeserializationFailed = 0x040A,

    // Auth (0x0500 - 0x05FF)
    AuthenticationFailed = 0x0500,
    TokenExpired = 0x0501,
    InvalidToken = 0x0502,
    PermissionDenied = 0x0503,
    UserAlreadyExists = 0x0504,
    InvalidEmail = 0x0505,
    WeakPassword = 0x0506,
    RateLimitExceeded = 0x0507,
    TokenRevoked = 0x0508,
    InvalidCredentials = 0x0509,
    RefreshTokenExpired = 0x050A,

    // Config (0x0600 - 0x06FF)
    ConfigLoadFailed = 0x0600,
    ConfigKeyNotFound = 0x0601,
    ConfigTypeMismatch = 0x0602,

    // Thread (0x0700 - 0x07FF)
    ThreadError = 0x0700,
    JobScheduleFailed = 0x0701,
    JobNotFound = 0x0702,
    JobCancelled = 0x0703,
    JobTimeout = 0x0704,
    JobDependencyFailed = 0x0705,

    // Logger (0x0800 - 0x08FF)
    LoggerError = 0x0800,
    LoggerNotInitialized = 0x0801,
    LoggerFlushFailed = 0x0802,

    // Monitoring (0x0900 - 0x09FF)
    MonitoringError = 0x0900,
    MetricNotFound = 0x0901,
    InvalidMetricType = 0x0902,
    HistogramNotRegistered = 0x0903,

    // Serialization (0x0A00 - 0x0AFF)
    SerializationError = 0x0A00,
    InvalidBinaryData = 0x0A01,
    InvalidJsonData = 0x0A02,

    // GameServer (0x0B00 - 0x0BFF)
    GameServerError = 0x0B00,
    MapInstanceNotFound = 0x0B01,
    MapInstanceLimitReached = 0x0B02,
    MapInstanceInvalidState = 0x0B03,
    GameLoopAlreadyRunning = 0x0B04,
    GameLoopNotRunning = 0x0B05,
    PlayerAlreadyInWorld = 0x0B06,
    PlayerNotInWorld = 0x0B07,
    InstanceFull = 0x0B08,
    SystemSchedulerBuildFailed = 0x0B09,

    // Lobby (0x0C00 - 0x0CFF)
    LobbyError = 0x0C00,
    QueueFull = 0x0C01,
    AlreadyInQueue = 0x0C02,
    NotInQueue = 0x0C03,
    InvalidRating = 0x0C04,
    PartyNotFound = 0x0C05,
    PartyFull = 0x0C06,
    PlayerAlreadyInParty = 0x0C07,
    NotPartyLeader = 0x0C08,
    LobbyNotStarted = 0x0C09,
    LobbyAlreadyStarted = 0x0C0A,
    NoServerAvailable = 0x0C0B,
    PlayerNotInParty = 0x0C0C,

    // DBProxy (0x0D00 - 0x0DFF)
    DBProxyError = 0x0D00,
    CacheMiss = 0x0D01,
    CacheInvalidation = 0x0D02,
    ReplicaUnavailable = 0x0D03,
    PrimaryUnavailable = 0x0D04,
    QueryRoutingFailed = 0x0D05,
    DBProxyNotStarted = 0x0D06,

    // Gateway (0x0E00 - 0x0EFF)
    GatewayError = 0x0E00,
    GatewayNotStarted = 0x0E01,
    GatewayAlreadyStarted = 0x0E02,
    ConnectionLimitReached = 0x0E03,
    ClientNotAuthenticated = 0x0E04,
    RouteNotFound = 0x0E05,
    MigrationFailed = 0x0E06,
    GatewayRateLimited = 0x0E07,
    AuthTimeoutExpired = 0x0E08,

    // Persistence (0x0F00 - 0x0FFF)
    PersistenceError = 0x0F00,
    WalWriteFailed = 0x0F01,
    WalReadFailed = 0x0F02,
    WalCorrupted = 0x0F03,
    WalTruncateFailed = 0x0F04,
    SnapshotWriteFailed = 0x0F05,
    SnapshotReadFailed = 0x0F06,
    SnapshotCorrupted = 0x0F07,
    RecoveryFailed = 0x0F08,
    PersistenceNotStarted = 0x0F09,
    PersistenceAlreadyStarted = 0x0F0A,
};

/// Return the subsystem name for a given error code.
constexpr std::string_view errorSubsystem(ErrorCode code) {
    auto value = static_cast<uint32_t>(code);
    auto category = value & 0xFF00;
    switch (category) {
        case 0x0000: return "General";
        case 0x0100: return "Network";
        case 0x0200: return "Database";
        case 0x0300: return "ECS";
        case 0x0400: return "Plugin";
        case 0x0500: return "Auth";
        case 0x0600: return "Config";
        case 0x0700: return "Thread";
        case 0x0800: return "Logger";
        case 0x0900: return "Monitoring";
        case 0x0A00: return "Serialization";
        case 0x0B00: return "GameServer";
        case 0x0C00: return "Lobby";
        case 0x0D00: return "DBProxy";
        case 0x0E00: return "Gateway";
        case 0x0F00: return "Persistence";
        default: return "Unknown";
    }
}

} // namespace cgs::foundation
