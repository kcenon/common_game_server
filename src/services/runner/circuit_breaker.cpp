/// @file circuit_breaker.cpp
/// @brief ServiceCircuitBreaker state machine implementation.

#include "cgs/service/circuit_breaker.hpp"

namespace cgs::service {

ServiceCircuitBreaker::ServiceCircuitBreaker(CircuitBreakerConfig config)
    : config_(std::move(config)) {}

bool ServiceCircuitBreaker::allowRequest() {
    std::lock_guard lock(mutex_);

    switch (state_) {
        case State::Closed:
            return true;

        case State::Open: {
            auto elapsed = std::chrono::steady_clock::now() - lastFailureTime_;
            if (elapsed >= config_.recoveryTimeout) {
                transitionTo(State::HalfOpen);
                return true;
            }
            ++totalRejected_;
            return false;
        }

        case State::HalfOpen:
            return true;
    }
    return false;
}

void ServiceCircuitBreaker::recordSuccess() {
    std::lock_guard lock(mutex_);

    switch (state_) {
        case State::Closed:
            consecutiveFailures_ = 0;
            break;

        case State::HalfOpen:
            ++halfOpenSuccesses_;
            if (halfOpenSuccesses_ >= config_.successThreshold) {
                transitionTo(State::Closed);
            }
            break;

        case State::Open:
            // Success in Open state should not happen (allowRequest returns false),
            // but handle gracefully.
            break;
    }
}

void ServiceCircuitBreaker::recordFailure() {
    std::lock_guard lock(mutex_);

    lastFailureTime_ = std::chrono::steady_clock::now();

    switch (state_) {
        case State::Closed:
            ++consecutiveFailures_;
            if (consecutiveFailures_ >= config_.failureThreshold) {
                transitionTo(State::Open);
            }
            break;

        case State::HalfOpen:
            // Any failure in half-open immediately re-opens.
            transitionTo(State::Open);
            break;

        case State::Open:
            break;
    }
}

void ServiceCircuitBreaker::forceState(State newState) {
    std::lock_guard lock(mutex_);
    transitionTo(newState);
}

void ServiceCircuitBreaker::reset() {
    std::lock_guard lock(mutex_);
    state_ = State::Closed;
    consecutiveFailures_ = 0;
    halfOpenSuccesses_ = 0;
    totalRejected_ = 0;
    lastFailureTime_ = {};
}

ServiceCircuitBreaker::State ServiceCircuitBreaker::state() const {
    std::lock_guard lock(mutex_);
    return state_;
}

uint32_t ServiceCircuitBreaker::failureCount() const {
    std::lock_guard lock(mutex_);
    return consecutiveFailures_;
}

uint32_t ServiceCircuitBreaker::halfOpenSuccessCount() const {
    std::lock_guard lock(mutex_);
    return halfOpenSuccesses_;
}

uint64_t ServiceCircuitBreaker::rejectedCount() const {
    std::lock_guard lock(mutex_);
    return totalRejected_;
}

std::string_view ServiceCircuitBreaker::name() const {
    return config_.name;
}

void ServiceCircuitBreaker::transitionTo(State newState) {
    state_ = newState;
    if (newState == State::Closed) {
        consecutiveFailures_ = 0;
        halfOpenSuccesses_ = 0;
    } else if (newState == State::Open) {
        // Ensure recovery timeout starts from now when entering Open state.
        lastFailureTime_ = std::chrono::steady_clock::now();
    } else if (newState == State::HalfOpen) {
        halfOpenSuccesses_ = 0;
    }
}

} // namespace cgs::service
