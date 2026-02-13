#pragma once

/// @file entity_manager.hpp
/// @brief Entity lifecycle management for the ECS.
///
/// EntityManager owns the canonical entity state: creation, versioned ID
/// recycling, immediate and deferred destruction, and alive-ness checks.
/// It also holds references to all registered component storages so that
/// components are automatically cleaned up when an entity is destroyed.
///
/// @see docs/reference/ECS_DESIGN.md  Section 2.1
/// @see SDS-MOD-010

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

namespace cgs::ecs {

/// Manages the full lifecycle of entities.
///
/// Entities are identified by a compact 32-bit handle (24-bit index +
/// 8-bit version).  When an entity is destroyed its index is pushed
/// onto a free list for recycling; the version counter is incremented
/// to invalidate any stale handles that still refer to the old entity.
///
/// Component storages may be registered with the manager so that
/// `Destroy()` and `FlushDeferred()` automatically remove all
/// components belonging to the destroyed entity.
class EntityManager {
public:
    EntityManager() = default;

    // Non-copyable, movable.
    EntityManager(const EntityManager&) = delete;
    EntityManager& operator=(const EntityManager&) = delete;
    EntityManager(EntityManager&&) noexcept = default;
    EntityManager& operator=(EntityManager&&) noexcept = default;

    // ── Entity lifecycle ─────────────────────────────────────────────

    /// Create a new entity (or recycle a previously destroyed one).
    ///
    /// If the free list is non-empty the oldest recycled index is
    /// reused with an incremented version.  Otherwise a fresh index
    /// is allocated.
    ///
    /// @return A valid Entity handle.
    [[nodiscard]] Entity Create();

    /// Immediately destroy @p entity and remove all its components.
    ///
    /// The entity's index is recycled (version incremented) and pushed
    /// onto the free list.  Registered component storages are notified
    /// to drop any data for this entity.
    ///
    /// @pre `IsAlive(entity)` — destroying a dead entity is a no-op.
    void Destroy(Entity entity);

    /// Queue @p entity for destruction at the next `FlushDeferred()`.
    ///
    /// This is useful when destroying entities during system iteration
    /// where immediate removal would invalidate iterators.
    ///
    /// @pre `IsAlive(entity)` — queueing a dead entity is a no-op.
    void DestroyDeferred(Entity entity);

    /// Flush all deferred destructions queued via `DestroyDeferred()`.
    ///
    /// Each queued entity is destroyed exactly as if `Destroy()` were
    /// called.  The pending queue is cleared after flushing.  Entities
    /// that became dead between queueing and flushing are silently
    /// skipped.
    void FlushDeferred();

    // ── Queries ──────────────────────────────────────────────────────

    /// Check whether @p entity is currently alive.
    ///
    /// An entity is alive when its index is within range, it has been
    /// created (not on the free list), and its version matches the
    /// stored version.
    [[nodiscard]] bool IsAlive(Entity entity) const noexcept;

    /// Return the number of currently alive entities.
    [[nodiscard]] std::size_t Count() const noexcept;

    /// Return the total number of indices that have been allocated
    /// (including destroyed ones sitting on the free list).
    [[nodiscard]] std::size_t Capacity() const noexcept;

    // ── Component storage registration ───────────────────────────────

    /// Register a component storage so that entity destruction
    /// automatically calls `storage->Remove(entity)`.
    ///
    /// The manager does **not** take ownership of the storage; the
    /// caller must ensure the storage outlives the manager.
    void RegisterStorage(IComponentStorage* storage);

private:
    /// Perform the actual destruction of a single entity.
    void destroyInternal(Entity entity);

    /// Version counter per index.  `versions_[index]` holds the
    /// current version for that slot.  An entity handle is alive iff
    /// `entity.version() == versions_[entity.id()]` and the slot is
    /// not on the free list.
    std::vector<uint8_t> versions_;

    /// Alive flag per index.  True when the slot holds a live entity.
    /// This disambiguates a freshly-allocated slot (version 0, alive)
    /// from a recycled-and-not-yet-reused slot (version N, dead).
    std::vector<bool> alive_;

    /// FIFO queue of recycled indices available for reuse.
    std::vector<uint32_t> freeList_;

    /// Entities queued for deferred destruction.
    std::vector<Entity> pendingDestroy_;

    /// Registered component storages for automatic cleanup.
    std::vector<IComponentStorage*> storages_;

    /// Number of currently alive entities (cached for O(1) Count()).
    std::size_t count_ = 0;
};

}  // namespace cgs::ecs
