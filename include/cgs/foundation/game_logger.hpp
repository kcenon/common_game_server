#pragma once

/// @file game_logger.hpp
/// @brief GameLogger wrapping kcenon logger_system for game-specific structured logging.
///
/// Provides category-based filtering, structured logging with context,
/// and per-category runtime log level control. Part of the Logger System
/// Adapter (SDS-MOD-003).

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "cgs/foundation/game_result.hpp"
#include "cgs/foundation/types.hpp"

namespace cgs::foundation {

/// Log severity levels for the game framework.
///
/// Maps to kcenon::common::interfaces::log_level internally:
///   Trace -> trace, Debug -> debug, Info -> info, Warning -> warning,
///   Error -> error, Critical -> critical, Off -> off
enum class LogLevel : uint8_t {
    Trace    = 0,
    Debug    = 1,
    Info     = 2,
    Warning  = 3,
    Error    = 4,
    Critical = 5,
    Off      = 6
};

/// Game-specific log categories for structured filtering.
///
/// Each category can have its own minimum log level, enabling
/// fine-grained control over logging verbosity per subsystem.
enum class LogCategory : uint8_t {
    Core     = 0, ///< Core framework operations
    ECS      = 1, ///< Entity-Component-System
    Network  = 2, ///< Network communication
    Database = 3, ///< Database operations
    Plugin   = 4, ///< Plugin lifecycle
    Combat   = 5, ///< Combat calculations
    World    = 6, ///< World/map operations
    AI       = 7  ///< AI behavior
};

/// Total number of log categories.
inline constexpr std::size_t kLogCategoryCount = 8;

/// Return the string name for a log category.
constexpr std::string_view logCategoryName(LogCategory cat) {
    constexpr std::array<std::string_view, kLogCategoryCount> names = {
        "Core", "ECS", "Network", "Database", "Plugin", "Combat", "World", "AI"
    };
    auto idx = static_cast<std::size_t>(cat);
    return idx < kLogCategoryCount ? names[idx] : "Unknown";
}

/// Return the string name for a log level.
constexpr std::string_view logLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return "TRACE";
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warning:  return "WARNING";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        case LogLevel::Off:      return "OFF";
    }
    return "UNKNOWN";
}

/// Structured context data attached to log entries.
///
/// Provides optional game-specific identifiers and arbitrary key-value pairs
/// for rich, searchable log output.
///
/// Example:
/// @code
///   LogContext ctx;
///   ctx.playerId = PlayerId(42);
///   ctx.extra["damage"] = "150";
///   logger.logWithContext(LogLevel::Debug, LogCategory::Combat,
///                         "Damage applied", ctx);
/// @endcode
struct LogContext {
    std::optional<EntityId> entityId;
    std::optional<PlayerId> playerId;
    std::optional<SessionId> sessionId;
    std::optional<std::string> traceId;
    std::unordered_map<std::string, std::string> extra;
};

/// Game-specific logger wrapping kcenon's logging system.
///
/// Provides category-based filtering, structured logging with context,
/// and per-category runtime log level control. Uses PIMPL to hide
/// kcenon implementation details from the public API.
///
/// Default log levels per category:
/// | Category | Default Level |
/// |----------|---------------|
/// | Core     | Info          |
/// | ECS      | Debug         |
/// | Network  | Info          |
/// | Database | Info          |
/// | Plugin   | Info          |
/// | Combat   | Debug         |
/// | World    | Info          |
/// | AI       | Debug         |
///
/// Example:
/// @code
///   GameLogger logger;
///   logger.log(LogLevel::Info, LogCategory::Core, "Server starting");
///
///   LogContext ctx;
///   ctx.playerId = PlayerId(42);
///   logger.logWithContext(LogLevel::Debug, LogCategory::Combat,
///                         "Damage calculated", ctx);
///
///   logger.setCategoryLevel(LogCategory::AI, LogLevel::Trace);
/// @endcode
class GameLogger {
public:
    GameLogger();
    ~GameLogger();

    // Non-copyable, movable.
    GameLogger(const GameLogger&) = delete;
    GameLogger& operator=(const GameLogger&) = delete;
    GameLogger(GameLogger&&) noexcept;
    GameLogger& operator=(GameLogger&&) noexcept;

    /// Log a message under the given category.
    /// No-op if the level is below the category's minimum level.
    void log(LogLevel level, LogCategory cat, std::string_view msg);

    /// Log a message with structured context data.
    /// Context fields are appended as key-value pairs to the log message.
    void logWithContext(LogLevel level, LogCategory cat,
                        std::string_view msg, const LogContext& ctx);

    /// Set the minimum log level for a category at runtime.
    void setCategoryLevel(LogCategory cat, LogLevel minLevel);

    /// Get the current minimum log level for a category.
    [[nodiscard]] LogLevel getCategoryLevel(LogCategory cat) const;

    /// Check if logging is enabled for the given level and category.
    [[nodiscard]] bool isEnabled(LogLevel level, LogCategory cat) const;

    /// Flush all buffered log messages.
    GameResult<void> flush();

    /// Get the global GameLogger singleton instance.
    static GameLogger& instance();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cgs::foundation

// ---------------------------------------------------------------------------
// Convenience macros (must be outside namespace — macros are global)
// ---------------------------------------------------------------------------

/// @name CGS_LOG Macros
/// @brief Zero-cost logging macros with compile-time and runtime level checks.
///
/// CGS_MIN_LOG_LEVEL can be defined before including this header to
/// eliminate logging calls below the threshold at compile time.
/// Values: 0=Trace, 1=Debug, 2=Info, 3=Warning, 4=Error, 5=Critical, 6=Off
/// @{

#ifndef CGS_MIN_LOG_LEVEL
    #define CGS_MIN_LOG_LEVEL 0
#endif

#define CGS_LOG(level, cat, msg)                                                 \
    do {                                                                         \
        _Pragma("GCC diagnostic push")                                           \
        _Pragma("GCC diagnostic ignored \"-Wtype-limits\"")                      \
        if (static_cast<int>(level) >= CGS_MIN_LOG_LEVEL &&                      \
            ::cgs::foundation::GameLogger::instance().isEnabled((level), (cat)))  \
        {                                                                        \
            ::cgs::foundation::GameLogger::instance().log((level), (cat), (msg)); \
        }                                                                        \
        _Pragma("GCC diagnostic pop")                                            \
    } while (0)

#define CGS_LOG_DEBUG(cat, msg) \
    CGS_LOG(::cgs::foundation::LogLevel::Debug, (cat), (msg))

#define CGS_LOG_INFO(cat, msg) \
    CGS_LOG(::cgs::foundation::LogLevel::Info, (cat), (msg))

#define CGS_LOG_WARN(cat, msg) \
    CGS_LOG(::cgs::foundation::LogLevel::Warning, (cat), (msg))

#define CGS_LOG_ERROR(cat, msg) \
    CGS_LOG(::cgs::foundation::LogLevel::Error, (cat), (msg))

/// @}
