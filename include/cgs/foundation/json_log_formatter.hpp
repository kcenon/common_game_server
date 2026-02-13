#pragma once

/// @file json_log_formatter.hpp
/// @brief Structured JSON log formatter with correlation ID support (SRS-NFR-019).
///
/// Produces ELK/Grafana Loki compatible JSON log lines with automatic
/// correlation ID generation and propagation via thread-local storage.

#include "cgs/foundation/game_logger.hpp"

#include <string>
#include <string_view>

namespace cgs::foundation {

/// Generate a UUID v4 string (e.g., "550e8400-e29b-41d4-a716-446655440000").
///
/// Uses a thread-local PRNG seeded from std::random_device for
/// high-throughput generation without contention.
[[nodiscard]] std::string generateCorrelationId();

/// RAII scope guard that sets the current thread's correlation ID on
/// construction and restores the previous value on destruction.
///
/// Usage:
/// @code
///   {
///       CorrelationScope scope(generateCorrelationId());
///       // All log calls on this thread will include the correlation ID
///   }
///   // Previous correlation ID (or empty) is restored
/// @endcode
class CorrelationScope {
public:
    /// Set a new correlation ID for the current thread.
    explicit CorrelationScope(std::string correlationId);

    /// Restore the previous correlation ID.
    ~CorrelationScope();

    // Non-copyable, non-movable.
    CorrelationScope(const CorrelationScope&) = delete;
    CorrelationScope& operator=(const CorrelationScope&) = delete;

    /// Get the current thread's correlation ID (empty if none set).
    [[nodiscard]] static const std::string& current();

private:
    std::string previous_;
};

/// Stateless JSON log formatter producing structured log lines.
///
/// Output format (ELK/Loki compatible):
/// @code
///   {"timestamp":"2026-02-14T12:00:00.000Z","level":"INFO",
///    "category":"Core","correlation_id":"uuid","message":"..."}
/// @endcode
class JsonLogFormatter {
public:
    /// Format a log entry as a single-line JSON object.
    ///
    /// If the thread has a correlation ID set via CorrelationScope, it is
    /// automatically included.  Additional context fields from LogContext
    /// are appended as top-level JSON fields.
    [[nodiscard]] static std::string format(LogLevel level,
                                            LogCategory category,
                                            std::string_view message,
                                            const LogContext& ctx = {});
};

}  // namespace cgs::foundation
