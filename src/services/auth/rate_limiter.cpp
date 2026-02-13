/// @file rate_limiter.cpp
/// @brief Sliding-window RateLimiter implementation.

#include "cgs/service/rate_limiter.hpp"

namespace cgs::service {

RateLimiter::RateLimiter(uint32_t maxAttempts, std::chrono::seconds window)
    : maxAttempts_(maxAttempts), window_(window) {}

bool RateLimiter::allow(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& timestamps = attempts_[key];
    purgeExpired(timestamps);

    if (timestamps.size() >= static_cast<std::size_t>(maxAttempts_)) {
        return false;
    }

    timestamps.push_back(std::chrono::steady_clock::now());
    return true;
}

uint32_t RateLimiter::remaining(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = attempts_.find(key);
    if (it == attempts_.end()) {
        return maxAttempts_;
    }

    auto timestamps = it->second;  // copy to purge
    purgeExpired(timestamps);

    auto used = static_cast<uint32_t>(timestamps.size());
    return (used >= maxAttempts_) ? 0u : (maxAttempts_ - used);
}

void RateLimiter::reset(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    attempts_.erase(key);
}

void RateLimiter::purgeExpired(std::deque<TimePoint>& timestamps) const {
    auto cutoff = std::chrono::steady_clock::now() - window_;
    while (!timestamps.empty() && timestamps.front() < cutoff) {
        timestamps.pop_front();
    }
}

}  // namespace cgs::service
