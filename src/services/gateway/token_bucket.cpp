/// @file token_bucket.cpp
/// @brief TokenBucket implementation.

#include "cgs/service/token_bucket.hpp"

#include <algorithm>

namespace cgs::service {

TokenBucket::TokenBucket(uint32_t capacity, uint32_t refillRate)
    : capacity_(capacity), refillRate_(refillRate) {}

bool TokenBucket::consume(const std::string& key) {
    return consume(key, 1);
}

bool TokenBucket::consume(const std::string& key, uint32_t tokens) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = buckets_.find(key);
    if (it == buckets_.end()) {
        auto cap = static_cast<double>(capacity_);
        auto [inserted, _] = buckets_.emplace(
            key, Bucket{cap, std::chrono::steady_clock::now()});
        it = inserted;
    }

    refill(it->second);

    auto required = static_cast<double>(tokens);
    if (it->second.tokens < required) {
        return false;
    }

    it->second.tokens -= required;
    return true;
}

uint32_t TokenBucket::available(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = buckets_.find(key);
    if (it == buckets_.end()) {
        return capacity_;
    }

    refill(it->second);
    return static_cast<uint32_t>(it->second.tokens);
}

void TokenBucket::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    buckets_.erase(key);
}

void TokenBucket::reset(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    buckets_[key] = Bucket{static_cast<double>(capacity_),
                           std::chrono::steady_clock::now()};
}

void TokenBucket::refill(Bucket& bucket) const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration<double>(now - bucket.lastRefill).count();

    auto added = elapsed * static_cast<double>(refillRate_);
    bucket.tokens = std::min(bucket.tokens + added,
                             static_cast<double>(capacity_));
    bucket.lastRefill = now;
}

} // namespace cgs::service
