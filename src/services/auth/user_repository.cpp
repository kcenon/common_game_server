/// @file user_repository.cpp
/// @brief InMemoryUserRepository implementation.

#include "cgs/service/user_repository.hpp"

#include <chrono>

namespace cgs::service {

std::optional<UserRecord> InMemoryUserRepository::findById(uint64_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(id);
    if (it == users_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<UserRecord> InMemoryUserRepository::findByUsername(
    std::string_view username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, user] : users_) {
        if (user.username == username) {
            return user;
        }
    }
    return std::nullopt;
}

std::optional<UserRecord> InMemoryUserRepository::findByEmail(
    std::string_view email) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, user] : users_) {
        if (user.email == email) {
            return user;
        }
    }
    return std::nullopt;
}

uint64_t InMemoryUserRepository::create(UserRecord record) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto id = nextId_++;
    record.id = id;
    auto now = std::chrono::system_clock::now();
    record.createdAt = now;
    record.updatedAt = now;
    users_.emplace(id, std::move(record));
    return id;
}

bool InMemoryUserRepository::update(const UserRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(record.id);
    if (it == users_.end()) {
        return false;
    }
    it->second = record;
    it->second.updatedAt = std::chrono::system_clock::now();
    return true;
}

} // namespace cgs::service
