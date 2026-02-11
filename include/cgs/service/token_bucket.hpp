#pragma once

/// @file token_bucket.hpp
/// @brief Token bucket rate limiter for per-client message throttling.
///
/// Unlike the sliding-window RateLimiter used in AuthServer (which counts
/// discrete attempts), this token bucket implementation controls sustained
/// throughput with configurable burst capacity.
///
/// @see SRS-SVC-002.5

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace cgs::service {

/// Token bucket rate limiter for per-key message throttling.
///
/// Each key (typically a session or IP) maintains a bucket that fills
/// at a constant rate and can burst up to a configured capacity.
///
/// Example:
/// @code
///   TokenBucket bucket(100, 50); // 100 burst, 50 tokens/sec refill
///   if (bucket.consume("session-1")) {
///       // Message allowed
///   }
/// @endcode
class TokenBucket {
public:
    /// Construct with capacity (max burst) and refill rate (tokens/second).
    TokenBucket(uint32_t capacity, uint32_t refillRate);

    /// Try to consume one token for the given key.
    /// Returns true if allowed, false if rate limit exceeded.
    [[nodiscard]] bool consume(const std::string& key);

    /// Try to consume N tokens for the given key.
    [[nodiscard]] bool consume(const std::string& key, uint32_t tokens);

    /// Get current available tokens for the given key.
    [[nodiscard]] uint32_t available(const std::string& key) const;

    /// Remove tracking for the given key (e.g., on disconnect).
    void remove(const std::string& key);

    /// Reset a key's bucket to full capacity.
    void reset(const std::string& key);

private:
    struct Bucket {
        double tokens;
        std::chrono::steady_clock::time_point lastRefill;
    };

    void refill(Bucket& bucket) const;

    uint32_t capacity_;
    uint32_t refillRate_;
    mutable std::mutex mutex_;
    mutable std::unordered_map<std::string, Bucket> buckets_;
};

} // namespace cgs::service
