/// @file game_logger.cpp
/// @brief GameLogger implementation wrapping kcenon logger_system.

#include "cgs/foundation/game_logger.hpp"

// kcenon logger headers (hidden behind PIMPL)
#include <kcenon/common/interfaces/global_logger_registry.h>
#include <kcenon/common/interfaces/logger_interface.h>

#include <array>
#include <atomic>
#include <mutex>
#include <sstream>
#include <string>

namespace cgs::foundation {

// ---------------------------------------------------------------------------
// Level mapping: CGS -> kcenon
// ---------------------------------------------------------------------------
static kcenon::common::interfaces::log_level mapLevel(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return kcenon::common::interfaces::log_level::trace;
        case LogLevel::Debug:    return kcenon::common::interfaces::log_level::debug;
        case LogLevel::Info:     return kcenon::common::interfaces::log_level::info;
        case LogLevel::Warning:  return kcenon::common::interfaces::log_level::warning;
        case LogLevel::Error:    return kcenon::common::interfaces::log_level::error;
        case LogLevel::Critical: return kcenon::common::interfaces::log_level::critical;
        case LogLevel::Off:      return kcenon::common::interfaces::log_level::off;
    }
    return kcenon::common::interfaces::log_level::info;
}

// ---------------------------------------------------------------------------
// Default log levels per category (from SDS-MOD-003)
// ---------------------------------------------------------------------------
static constexpr std::array<LogLevel, kLogCategoryCount> kDefaultCategoryLevels = {
    LogLevel::Info,   // Core
    LogLevel::Debug,  // ECS
    LogLevel::Info,   // Network
    LogLevel::Info,   // Database
    LogLevel::Info,   // Plugin
    LogLevel::Debug,  // Combat
    LogLevel::Info,   // World
    LogLevel::Debug   // AI
};

// ---------------------------------------------------------------------------
// Context serialization
// ---------------------------------------------------------------------------
static std::string formatContext(const LogContext& ctx) {
    std::ostringstream oss;
    bool first = true;

    auto append = [&](std::string_view key, std::string_view val) {
        if (!first) {
            oss << ", ";
        }
        oss << key << '=' << val;
        first = false;
    };

    if (ctx.entityId && ctx.entityId->isValid()) {
        append("entity_id", std::to_string(ctx.entityId->value()));
    }
    if (ctx.playerId && ctx.playerId->isValid()) {
        append("player_id", std::to_string(ctx.playerId->value()));
    }
    if (ctx.sessionId && ctx.sessionId->isValid()) {
        append("session_id", std::to_string(ctx.sessionId->value()));
    }
    if (ctx.traceId && !ctx.traceId->empty()) {
        append("trace_id", *ctx.traceId);
    }
    for (const auto& [key, val] : ctx.extra) {
        append(key, val);
    }

    return oss.str();
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct GameLogger::Impl {
    // Per-category log levels (atomic for lock-free reads on the hot path)
    std::array<std::atomic<LogLevel>, kLogCategoryCount> categoryLevels;

    // Named loggers registered in GlobalLoggerRegistry, one per category
    std::array<std::string, kLogCategoryCount> loggerNames;

    Impl() {
        for (std::size_t i = 0; i < kLogCategoryCount; ++i) {
            categoryLevels[i].store(kDefaultCategoryLevels[i],
                                    std::memory_order_relaxed);
            loggerNames[i] = std::string("cgs.") +
                std::string(logCategoryName(static_cast<LogCategory>(i)));
        }
    }

    std::shared_ptr<kcenon::common::interfaces::ILogger> getLogger(
        LogCategory cat) const {
        auto idx = static_cast<std::size_t>(cat);
        if (idx >= kLogCategoryCount) {
            return kcenon::common::interfaces::GlobalLoggerRegistry::null_logger();
        }
        // Try named logger first, fall back to default
        auto& registry = kcenon::common::interfaces::GlobalLoggerRegistry::instance();
        auto logger = registry.get_logger(loggerNames[idx]);
        // If we get a NullLogger and there's a default logger, use the default
        if (!logger->is_enabled(kcenon::common::interfaces::log_level::off)) {
            auto defaultLogger = registry.get_default_logger();
            if (defaultLogger->is_enabled(kcenon::common::interfaces::log_level::off)
                || defaultLogger != kcenon::common::interfaces::GlobalLoggerRegistry::null_logger()) {
                return defaultLogger;
            }
        }
        return logger;
    }
};

// ---------------------------------------------------------------------------
// Construction / Destruction / Move
// ---------------------------------------------------------------------------
GameLogger::GameLogger() : impl_(std::make_unique<Impl>()) {}

GameLogger::~GameLogger() = default;

GameLogger::GameLogger(GameLogger&&) noexcept = default;
GameLogger& GameLogger::operator=(GameLogger&&) noexcept = default;

// ---------------------------------------------------------------------------
// log()
// ---------------------------------------------------------------------------
void GameLogger::log(LogLevel level, LogCategory cat, std::string_view msg) {
    if (!isEnabled(level, cat)) {
        return;
    }

    auto logger = impl_->getLogger(cat);
    auto kcLevel = mapLevel(level);

    // Format: [Category] message
    std::string formatted;
    formatted.reserve(msg.size() + 16);
    formatted += '[';
    formatted += logCategoryName(cat);
    formatted += "] ";
    formatted += msg;

    logger->log(kcLevel, formatted);
}

// ---------------------------------------------------------------------------
// logWithContext()
// ---------------------------------------------------------------------------
void GameLogger::logWithContext(LogLevel level, LogCategory cat,
                                std::string_view msg, const LogContext& ctx) {
    if (!isEnabled(level, cat)) {
        return;
    }

    auto logger = impl_->getLogger(cat);
    auto kcLevel = mapLevel(level);

    std::string ctxStr = formatContext(ctx);

    // Format: [Category] message {key=val, ...}
    std::string formatted;
    formatted.reserve(msg.size() + ctxStr.size() + 20);
    formatted += '[';
    formatted += logCategoryName(cat);
    formatted += "] ";
    formatted += msg;
    if (!ctxStr.empty()) {
        formatted += " {";
        formatted += ctxStr;
        formatted += '}';
    }

    logger->log(kcLevel, formatted);
}

// ---------------------------------------------------------------------------
// Category level control
// ---------------------------------------------------------------------------
void GameLogger::setCategoryLevel(LogCategory cat, LogLevel minLevel) {
    auto idx = static_cast<std::size_t>(cat);
    if (idx < kLogCategoryCount) {
        impl_->categoryLevels[idx].store(minLevel, std::memory_order_release);
    }
}

LogLevel GameLogger::getCategoryLevel(LogCategory cat) const {
    auto idx = static_cast<std::size_t>(cat);
    if (idx < kLogCategoryCount) {
        return impl_->categoryLevels[idx].load(std::memory_order_acquire);
    }
    return LogLevel::Off;
}

bool GameLogger::isEnabled(LogLevel level, LogCategory cat) const {
    auto idx = static_cast<std::size_t>(cat);
    if (idx >= kLogCategoryCount) {
        return false;
    }
    auto minLevel = impl_->categoryLevels[idx].load(std::memory_order_acquire);
    return static_cast<uint8_t>(level) >= static_cast<uint8_t>(minLevel);
}

// ---------------------------------------------------------------------------
// flush()
// ---------------------------------------------------------------------------
GameResult<void> GameLogger::flush() {
    auto& registry = kcenon::common::interfaces::GlobalLoggerRegistry::instance();
    auto logger = registry.get_default_logger();
    auto result = logger->flush();
    if (result.is_err()) {
        return GameResult<void>::err(
            GameError(ErrorCode::LoggerFlushFailed, "failed to flush logger"));
    }
    return GameResult<void>::ok();
}

// ---------------------------------------------------------------------------
// instance()
// ---------------------------------------------------------------------------
GameLogger& GameLogger::instance() {
    static GameLogger inst;
    return inst;
}

} // namespace cgs::foundation
