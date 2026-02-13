/// @file json_log_formatter.cpp
/// @brief Structured JSON log formatter implementation (SRS-NFR-019).

#include "cgs/foundation/json_log_formatter.hpp"

#include <chrono>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

namespace cgs::foundation {

// ---------------------------------------------------------------------------
// JSON string escaping (handles control chars, quotes, backslashes)
// ---------------------------------------------------------------------------
static void appendJsonString(std::string& out, std::string_view value) {
    out += '"';
    for (char c : value) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control characters: \u00XX
                    char buf[8];
                    std::snprintf(buf,
                                  sizeof(buf),
                                  "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    out += '"';
}

// ---------------------------------------------------------------------------
// ISO 8601 timestamp with millisecond precision (UTC)
// ---------------------------------------------------------------------------
static std::string formatTimestamp() {
    using Clock = std::chrono::system_clock;
    auto now = Clock::now();
    auto epoch = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch);
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(epoch) -
                  std::chrono::duration_cast<std::chrono::milliseconds>(seconds);

    std::time_t tt = Clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif

    char buf[96];  // Oversized to satisfy GCC -Wformat-truncation (int range analysis)
    std::snprintf(buf,
                  sizeof(buf),
                  "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                  utc.tm_year + 1900,
                  utc.tm_mon + 1,
                  utc.tm_mday,
                  utc.tm_hour,
                  utc.tm_min,
                  utc.tm_sec,
                  static_cast<int>(millis.count()));
    return buf;
}

// ---------------------------------------------------------------------------
// UUID v4 generation (thread-local PRNG for zero-contention)
// ---------------------------------------------------------------------------
std::string generateCorrelationId() {
    thread_local std::mt19937_64 gen(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;

    uint64_t hi = dist(gen);
    uint64_t lo = dist(gen);

    // Set version 4 (bits 12-15 of time_hi_and_version)
    hi = (hi & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    // Set variant 1 (bits 6-7 of clock_seq_hi_and_reserved)
    lo = (lo & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    char buf[37];
    std::snprintf(buf,
                  sizeof(buf),
                  "%08x-%04x-%04x-%04x-%012llx",
                  static_cast<uint32_t>(hi >> 32),
                  static_cast<uint16_t>((hi >> 16) & 0xFFFF),
                  static_cast<uint16_t>(hi & 0xFFFF),
                  static_cast<uint16_t>(lo >> 48),
                  static_cast<unsigned long long>(lo & 0x0000FFFFFFFFFFFFULL));
    return buf;
}

// ---------------------------------------------------------------------------
// Thread-local correlation ID storage
// ---------------------------------------------------------------------------
static thread_local std::string tl_correlationId;

CorrelationScope::CorrelationScope(std::string correlationId)
    : previous_(std::move(tl_correlationId)) {
    tl_correlationId = std::move(correlationId);
}

CorrelationScope::~CorrelationScope() {
    tl_correlationId = std::move(previous_);
}

const std::string& CorrelationScope::current() {
    return tl_correlationId;
}

// ---------------------------------------------------------------------------
// JsonLogFormatter::format()
// ---------------------------------------------------------------------------
std::string JsonLogFormatter::format(LogLevel level,
                                     LogCategory category,
                                     std::string_view message,
                                     const LogContext& ctx) {
    std::string out;
    out.reserve(256);

    out += '{';

    // timestamp
    out += "\"timestamp\":";
    appendJsonString(out, formatTimestamp());

    // level
    out += ",\"level\":";
    appendJsonString(out, logLevelName(level));

    // category
    out += ",\"category\":";
    appendJsonString(out, logCategoryName(category));

    // correlation_id: prefer LogContext.traceId, fall back to thread-local
    const auto& corrId = (ctx.traceId && !ctx.traceId->empty()) ? *ctx.traceId : tl_correlationId;
    if (!corrId.empty()) {
        out += ",\"correlation_id\":";
        appendJsonString(out, corrId);
    }

    // message
    out += ",\"message\":";
    appendJsonString(out, message);

    // Optional context fields
    if (ctx.entityId && ctx.entityId->isValid()) {
        out += ",\"entity_id\":";
        out += std::to_string(ctx.entityId->value());
    }
    if (ctx.playerId && ctx.playerId->isValid()) {
        out += ",\"player_id\":";
        out += std::to_string(ctx.playerId->value());
    }
    if (ctx.sessionId && ctx.sessionId->isValid()) {
        out += ",\"session_id\":";
        out += std::to_string(ctx.sessionId->value());
    }

    // Extra fields as nested "extra" object
    if (!ctx.extra.empty()) {
        out += ",\"extra\":{";
        bool first = true;
        for (const auto& [key, val] : ctx.extra) {
            if (!first)
                out += ',';
            appendJsonString(out, key);
            out += ':';
            appendJsonString(out, val);
            first = false;
        }
        out += '}';
    }

    out += '}';
    return out;
}

}  // namespace cgs::foundation
