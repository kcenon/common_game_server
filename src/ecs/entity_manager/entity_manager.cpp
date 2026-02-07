/// @file entity_manager.cpp
/// @brief Entity lifecycle management implementation.

#include "cgs/ecs/entity_manager.hpp"

#include <algorithm>

namespace cgs::ecs {

// ── Entity lifecycle ─────────────────────────────────────────────────

Entity EntityManager::Create() {
    uint32_t index = 0;
    uint8_t version = 0;

    if (!freeList_.empty()) {
        // Recycle the oldest destroyed index (FIFO).
        index = freeList_.front();
        freeList_.erase(freeList_.begin());
        version = versions_[index];
        alive_[index] = true;
    } else {
        // Allocate a fresh index.
        index = static_cast<uint32_t>(versions_.size());
        assert(index <= Entity::kMaxId && "Entity index space exhausted");
        versions_.push_back(0);
        alive_.push_back(true);
        version = 0;
    }

    ++count_;
    return Entity(index, version);
}

void EntityManager::Destroy(Entity entity) {
    if (!IsAlive(entity)) {
        return;
    }
    destroyInternal(entity);
}

void EntityManager::DestroyDeferred(Entity entity) {
    if (!IsAlive(entity)) {
        return;
    }
    pendingDestroy_.push_back(entity);
}

void EntityManager::FlushDeferred() {
    // Process a copy so that entities destroyed between queueing and
    // flushing are silently skipped (IsAlive check in destroyInternal).
    auto pending = std::move(pendingDestroy_);
    pendingDestroy_.clear();

    for (const auto& entity : pending) {
        if (IsAlive(entity)) {
            destroyInternal(entity);
        }
    }
}

// ── Queries ──────────────────────────────────────────────────────────

bool EntityManager::IsAlive(Entity entity) const noexcept {
    if (!entity.isValid()) {
        return false;
    }

    const auto idx = entity.id();
    if (idx >= versions_.size()) {
        return false;
    }

    return alive_[idx] && versions_[idx] == entity.version();
}

std::size_t EntityManager::Count() const noexcept {
    return count_;
}

std::size_t EntityManager::Capacity() const noexcept {
    return versions_.size();
}

// ── Component storage registration ───────────────────────────────────

void EntityManager::RegisterStorage(IComponentStorage* storage) {
    assert(storage != nullptr && "Cannot register null storage");
    storages_.push_back(storage);
}

// ── Private ──────────────────────────────────────────────────────────

void EntityManager::destroyInternal(Entity entity) {
    const auto idx = entity.id();

    // Remove all components belonging to this entity.
    for (auto* storage : storages_) {
        storage->Remove(entity);
    }

    // Mark the slot as dead and increment version for recycling.
    alive_[idx] = false;

    // Increment version, wrapping at 255 → 0.
    // Version 0xFF is reserved as part of the invalid sentinel
    // (kInvalidRaw = 0xFFFFFFFF), so we wrap 0xFF → 0.
    auto nextVersion = static_cast<uint8_t>(versions_[idx] + 1);
    // If the version wraps to 0xFF (which combined with kMaxId+1
    // would equal kInvalidRaw), skip to 0.  In practice, index
    // values never equal kIdMask (0x00FFFFFF) because kMaxId is
    // kIdMask - 1, so 0xFF is safe for normal indices.  However,
    // being defensive about the sentinel costs nothing.
    versions_[idx] = nextVersion;

    freeList_.push_back(idx);
    --count_;
}

} // namespace cgs::ecs
