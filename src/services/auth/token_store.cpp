/// @file token_store.cpp
/// @brief InMemoryTokenStore implementation.

#include "cgs/service/token_store.hpp"

#include <chrono>

namespace cgs::service {

void InMemoryTokenStore::store(RefreshTokenRecord record) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = record.token;
    tokens_.emplace(std::move(key), std::move(record));
}

std::optional<RefreshTokenRecord> InMemoryTokenStore::find(
    std::string_view token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tokens_.find(std::string(token));
    if (it == tokens_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool InMemoryTokenStore::revoke(std::string_view token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tokens_.find(std::string(token));
    if (it == tokens_.end()) {
        return false;
    }
    it->second.revoked = true;
    return true;
}

void InMemoryTokenStore::revokeAllForUser(uint64_t userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [key, record] : tokens_) {
        if (record.userId == userId) {
            record.revoked = true;
        }
    }
}

void InMemoryTokenStore::removeExpired() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    for (auto it = tokens_.begin(); it != tokens_.end();) {
        if (it->second.expiresAt <= now || it->second.revoked) {
            it = tokens_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace cgs::service
