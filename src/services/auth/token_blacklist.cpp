/// @file token_blacklist.cpp
/// @brief TokenBlacklist implementation with TTL-based auto-cleanup.
///
/// @see SRS-NFR-014

#include "cgs/service/token_blacklist.hpp"

namespace cgs::service {

TokenBlacklist::TokenBlacklist(std::chrono::seconds cleanupInterval)
    : cleanupInterval_(cleanupInterval), lastCleanup_(std::chrono::system_clock::now()) {}

void TokenBlacklist::revoke(std::string_view jti, std::chrono::system_clock::time_point expiresAt) {
    std::unique_lock lock(mutex_);
    entries_.emplace(std::string(jti), expiresAt);

    // Periodic auto-cleanup: remove expired entries when interval has elapsed.
    auto now = std::chrono::system_clock::now();
    if (now - lastCleanup_ >= cleanupInterval_) {
        lastCleanup_ = now;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second <= now) {
                it = entries_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

bool TokenBlacklist::isRevoked(std::string_view jti) const {
    std::shared_lock lock(mutex_);
    return entries_.find(std::string(jti)) != entries_.end();
}

std::size_t TokenBlacklist::cleanup() {
    std::unique_lock lock(mutex_);
    auto now = std::chrono::system_clock::now();
    lastCleanup_ = now;

    std::size_t removed = 0;
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second <= now) {
            it = entries_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

std::size_t TokenBlacklist::size() const {
    std::shared_lock lock(mutex_);
    return entries_.size();
}

}  // namespace cgs::service
