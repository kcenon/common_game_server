#pragma once

/// @file rate_limiter.hpp
/// @brief Sliding-window rate limiter for login attempt throttling.
///
/// Tracks timestamps of recent attempts per key (typically client IP)
/// within a configurable time window.
///
/// @see SRS-SVC-001 (rate limiting on login attempts)

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace cgs::service {

/// Sliding-window rate limiter.
///
/// Example:
/// @code
///   RateLimiter limiter(5, std::chrono::seconds{60});
///   if (!limiter.allow("192.168.1.1")) {
///       // Rate limit exceeded
///   }
/// @endcode
class RateLimiter {
public:
    /// Construct with max attempts allowed per window duration.
    RateLimiter(uint32_t maxAttempts, std::chrono::seconds window);

    /// Record an attempt for the given key.
    /// Returns true if the attempt is allowed, false if rate limit exceeded.
    [[nodiscard]] bool allow(const std::string& key);

    /// Get remaining attempts for the given key within the current window.
    [[nodiscard]] uint32_t remaining(const std::string& key) const;

    /// Reset all tracked attempts for the given key.
    void reset(const std::string& key);

private:
    using TimePoint = std::chrono::steady_clock::time_point;

    void purgeExpired(std::deque<TimePoint>& timestamps) const;

    uint32_t maxAttempts_;
    std::chrono::seconds window_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::deque<TimePoint>> attempts_;
};

} // namespace cgs::service
