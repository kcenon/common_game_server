#pragma once

/// @file circuit_breaker.hpp
/// @brief Service-level circuit breaker for inter-service communication.
///
/// Implements the circuit breaker pattern (Closed -> Open -> HalfOpen)
/// to prevent cascading failures when a downstream service is unhealthy.
/// Part of SRS-NFR-011 (99.9% uptime).

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace cgs::service {

/// Configuration for a ServiceCircuitBreaker instance.
struct CircuitBreakerConfig {
    /// Number of consecutive failures before the circuit opens.
    uint32_t failureThreshold = 5;

    /// Duration the circuit stays open before transitioning to half-open.
    std::chrono::seconds recoveryTimeout{30};

    /// Number of consecutive successes in half-open to close the circuit.
    uint32_t successThreshold = 2;

    /// Human-readable name for logging and metrics.
    std::string name = "default";
};

/// Circuit breaker state machine for protecting inter-service calls.
///
/// Usage:
/// @code
///   ServiceCircuitBreaker cb(CircuitBreakerConfig{.name = "auth"});
///   if (cb.allowRequest()) {
///       auto result = callAuthService();
///       if (result.hasValue()) {
///           cb.recordSuccess();
///       } else {
///           cb.recordFailure();
///       }
///   } else {
///       // Circuit is open, fail fast
///   }
/// @endcode
///
/// Thread-safe: all state transitions use a mutex.
class ServiceCircuitBreaker {
public:
    /// Circuit breaker states.
    enum class State : uint8_t {
        Closed,   ///< Normal operation; calls pass through.
        Open,     ///< Failure threshold reached; calls are rejected.
        HalfOpen  ///< Recovery probe; limited calls allowed.
    };

    explicit ServiceCircuitBreaker(CircuitBreakerConfig config = {});

    /// Check whether a request is allowed through the circuit.
    ///
    /// If the circuit is Open, checks whether the recovery timeout has
    /// elapsed and transitions to HalfOpen if so.
    ///
    /// @return true if the call should proceed, false if rejected.
    [[nodiscard]] bool allowRequest();

    /// Record a successful call. Resets failure count; may close the circuit.
    void recordSuccess();

    /// Record a failed call. Increments failure count; may open the circuit.
    void recordFailure();

    /// Force the circuit into a specific state (for testing or manual override).
    void forceState(State newState);

    /// Reset all counters and return to Closed state.
    void reset();

    // ── Queries ──────────────────────────────────────────────────────────

    /// Current circuit state.
    [[nodiscard]] State state() const;

    /// Number of consecutive failures in the current state.
    [[nodiscard]] uint32_t failureCount() const;

    /// Number of consecutive successes in HalfOpen state.
    [[nodiscard]] uint32_t halfOpenSuccessCount() const;

    /// Total number of requests rejected due to open circuit.
    [[nodiscard]] uint64_t rejectedCount() const;

    /// Configuration name.
    [[nodiscard]] std::string_view name() const;

private:
    void transitionTo(State newState);

    CircuitBreakerConfig config_;
    mutable std::mutex mutex_;
    State state_{State::Closed};
    uint32_t consecutiveFailures_{0};
    uint32_t halfOpenSuccesses_{0};
    uint64_t totalRejected_{0};
    std::chrono::steady_clock::time_point lastFailureTime_{};
};

/// Convert circuit breaker state to string.
[[nodiscard]] constexpr std::string_view toString(ServiceCircuitBreaker::State s) {
    switch (s) {
        case ServiceCircuitBreaker::State::Closed:
            return "closed";
        case ServiceCircuitBreaker::State::Open:
            return "open";
        case ServiceCircuitBreaker::State::HalfOpen:
            return "half_open";
    }
    return "unknown";
}

}  // namespace cgs::service
