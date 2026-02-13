/// @file map_instance_manager.cpp
/// @brief MapInstanceManager implementation.

#include "cgs/service/map_instance_manager.hpp"

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"

namespace cgs::service {

MapInstanceManager::MapInstanceManager(uint32_t maxInstances) : maxInstances_(maxInstances) {}

cgs::foundation::GameResult<uint32_t> MapInstanceManager::createInstance(uint32_t mapId,
                                                                         cgs::game::MapType type,
                                                                         uint32_t maxPlayers) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (instances_.size() >= static_cast<std::size_t>(maxInstances_)) {
        return cgs::Result<uint32_t, cgs::foundation::GameError>::err(
            cgs::foundation::GameError(cgs::foundation::ErrorCode::MapInstanceLimitReached,
                                       "Maximum number of map instances reached"));
    }

    MapInstanceInfo info;
    info.instanceId = nextInstanceId_++;
    info.mapId = mapId;
    info.type = type;
    info.state = InstanceState::Active;
    info.playerCount = 0;
    info.maxPlayers = maxPlayers;
    info.createdAt = std::chrono::steady_clock::now();

    uint32_t id = info.instanceId;
    instances_.emplace(id, std::move(info));

    return cgs::Result<uint32_t, cgs::foundation::GameError>::ok(id);
}

cgs::foundation::GameResult<void> MapInstanceManager::destroyInstance(uint32_t instanceId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = instances_.find(instanceId);
    if (it == instances_.end()) {
        return cgs::Result<void, cgs::foundation::GameError>::err(cgs::foundation::GameError(
            cgs::foundation::ErrorCode::MapInstanceNotFound, "Map instance not found"));
    }

    if (it->second.playerCount > 0) {
        return cgs::Result<void, cgs::foundation::GameError>::err(
            cgs::foundation::GameError(cgs::foundation::ErrorCode::MapInstanceInvalidState,
                                       "Cannot destroy instance with active players"));
    }

    instances_.erase(it);
    return cgs::Result<void, cgs::foundation::GameError>::ok();
}

bool MapInstanceManager::setInstanceState(uint32_t instanceId, InstanceState state) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = instances_.find(instanceId);
    if (it == instances_.end()) {
        return false;
    }

    auto current = it->second.state;

    // Enforce valid transitions: Active → Draining → ShuttingDown.
    if (state == InstanceState::Draining && current != InstanceState::Active) {
        return false;
    }
    if (state == InstanceState::ShuttingDown && current != InstanceState::Draining) {
        return false;
    }
    if (state == InstanceState::Active) {
        // Cannot transition back to Active.
        return false;
    }

    it->second.state = state;
    return true;
}

std::optional<MapInstanceInfo> MapInstanceManager::getInstance(uint32_t instanceId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = instances_.find(instanceId);
    if (it == instances_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<MapInstanceInfo> MapInstanceManager::getInstancesByMap(uint32_t mapId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<MapInstanceInfo> result;
    for (const auto& [id, info] : instances_) {
        if (info.mapId == mapId) {
            result.push_back(info);
        }
    }
    return result;
}

std::vector<MapInstanceInfo> MapInstanceManager::getInstancesByState(InstanceState state) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<MapInstanceInfo> result;
    for (const auto& [id, info] : instances_) {
        if (info.state == state) {
            result.push_back(info);
        }
    }
    return result;
}

bool MapInstanceManager::addPlayer(uint32_t instanceId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = instances_.find(instanceId);
    if (it == instances_.end()) {
        return false;
    }

    if (it->second.state != InstanceState::Active) {
        return false;
    }

    if (it->second.playerCount >= it->second.maxPlayers) {
        return false;
    }

    ++it->second.playerCount;
    return true;
}

bool MapInstanceManager::removePlayer(uint32_t instanceId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = instances_.find(instanceId);
    if (it == instances_.end()) {
        return false;
    }

    if (it->second.playerCount == 0) {
        return false;
    }

    --it->second.playerCount;
    return true;
}

uint32_t MapInstanceManager::instanceCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint32_t>(instances_.size());
}

uint32_t MapInstanceManager::instanceCount(InstanceState state) const {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t count = 0;
    for (const auto& [id, info] : instances_) {
        if (info.state == state) {
            ++count;
        }
    }
    return count;
}

std::vector<uint32_t> MapInstanceManager::findEmptyInstances() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<uint32_t> result;
    for (const auto& [id, info] : instances_) {
        if (info.playerCount == 0) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<uint32_t> MapInstanceManager::findAvailableInstances(uint32_t mapId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<uint32_t> result;
    for (const auto& [id, info] : instances_) {
        if (info.mapId == mapId && info.state == InstanceState::Active &&
            info.playerCount < info.maxPlayers) {
            result.push_back(id);
        }
    }
    return result;
}

}  // namespace cgs::service
