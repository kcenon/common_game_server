#pragma once

/// @file component_storage.hpp
/// @brief Sparse-set based component storage for the ECS.
///
/// ComponentStorage<T> provides O(1) add / get / has / remove and
/// cache-friendly dense iteration over all components of type T.
/// A per-component version counter supports change detection.
///
/// @see docs/reference/ECS_DESIGN.md  Section 2.3

#include "cgs/ecs/component_type_id.hpp"
#include "cgs/ecs/entity.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace cgs::ecs {

/// Type-erased base for component pools, allowing EntityManager to
/// call Remove / Has / Clear without knowing the component type.
class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;

    virtual void Remove(Entity entity) = 0;
    [[nodiscard]] virtual bool Has(Entity entity) const = 0;
    virtual void Clear() = 0;
    [[nodiscard]] virtual std::size_t Size() const = 0;

    /// Return the entity id stored at dense @p index.
    [[nodiscard]] virtual uint32_t EntityAt(std::size_t index) const = 0;

    /// Return the storage's global modification version counter.
    [[nodiscard]] virtual uint32_t Version() const noexcept = 0;
};

/// Sparse-set component storage.
///
/// Memory layout:
/// @code
///   sparse_  [entity.id] -> dense index  (or kInvalidIndex)
///   dense_   [index]     -> component data
///   entities_[index]     -> entity.id that owns dense_[index]
///   versions_[index]     -> change counter for dense_[index]
/// @endcode
///
/// All mutating operations bump a global version counter; the per-component
/// version is set to globalVersion_ at the point of modification so that
/// systems can efficiently detect stale data.
template <typename T>
class ComponentStorage final : public IComponentStorage {
public:
    static_assert(std::is_move_constructible_v<T>, "Component type must be move-constructible");

    using value_type = T;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    // ── Capacity ────────────────────────────────────────────────────────

    /// Number of stored components.
    [[nodiscard]] std::size_t Size() const override { return dense_.size(); }

    /// True when no components are stored.
    [[nodiscard]] bool Empty() const noexcept { return dense_.empty(); }

    // ── CRUD ────────────────────────────────────────────────────────────

    /// Add a component for @p entity, constructed from @p args.
    /// @pre `!Has(entity)` — adding a duplicate is undefined behavior.
    /// @return Mutable reference to the newly stored component.
    template <typename... Args>
    T& Add(Entity entity, Args&&... args) {
        assert(entity.isValid() && "Cannot add component to invalid entity");
        assert(!Has(entity) && "Entity already has this component");

        const auto idx = static_cast<uint32_t>(dense_.size());

        ensureSparseSize(entity.id());
        sparse_[entity.id()] = idx;

        dense_.emplace_back(std::forward<Args>(args)...);
        entities_.push_back(entity.id());
        versions_.push_back(++globalVersion_);

        return dense_.back();
    }

    /// Get a mutable reference to the component owned by @p entity.
    /// @pre `Has(entity)` — accessing a missing component is undefined.
    [[nodiscard]] T& Get(Entity entity) {
        assert(Has(entity) && "Entity does not have this component");
        return dense_[sparse_[entity.id()]];
    }

    /// Get a const reference to the component owned by @p entity.
    [[nodiscard]] const T& Get(Entity entity) const {
        assert(Has(entity) && "Entity does not have this component");
        return dense_[sparse_[entity.id()]];
    }

    /// Replace the component for @p entity and bump its version.
    /// @pre `Has(entity)`.
    void Replace(Entity entity, T&& component) {
        assert(Has(entity) && "Entity does not have this component");
        auto idx = sparse_[entity.id()];
        dense_[idx] = std::move(component);
        versions_[idx] = ++globalVersion_;
    }

    /// Check whether @p entity has a component in this storage.
    [[nodiscard]] bool Has(Entity entity) const override {
        auto eid = entity.id();
        return eid < sparse_.size() && sparse_[eid] != kInvalidIndex;
    }

    /// Remove the component owned by @p entity.
    /// Safe to call even if the entity has no component (no-op).
    void Remove(Entity entity) override {
        if (!Has(entity)) {
            return;
        }

        auto idx = sparse_[entity.id()];
        auto lastIdx = static_cast<uint32_t>(dense_.size() - 1);

        if (idx != lastIdx) {
            // Swap the removed element with the last element.
            dense_[idx] = std::move(dense_[lastIdx]);
            entities_[idx] = entities_[lastIdx];
            versions_[idx] = versions_[lastIdx];

            // Update the sparse entry for the moved entity.
            sparse_[entities_[idx]] = idx;
        }

        dense_.pop_back();
        entities_.pop_back();
        versions_.pop_back();
        sparse_[entity.id()] = kInvalidIndex;

        ++globalVersion_;
    }

    /// Get or add: return the existing component or default-construct one.
    T& GetOrAdd(Entity entity) {
        if (Has(entity)) {
            return Get(entity);
        }
        return Add(entity);
    }

    /// Remove all components and reset version tracking.
    void Clear() override {
        dense_.clear();
        entities_.clear();
        versions_.clear();
        std::fill(sparse_.begin(), sparse_.end(), kInvalidIndex);
        ++globalVersion_;
    }

    // ── Iteration ───────────────────────────────────────────────────────

    iterator begin() noexcept { return dense_.begin(); }
    iterator end() noexcept { return dense_.end(); }
    [[nodiscard]] const_iterator begin() const noexcept { return dense_.begin(); }
    [[nodiscard]] const_iterator end() const noexcept { return dense_.end(); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return dense_.cbegin(); }
    [[nodiscard]] const_iterator cend() const noexcept { return dense_.cend(); }

    /// Return the entity id that owns the component at @p index.
    [[nodiscard]] uint32_t EntityAt(std::size_t index) const override {
        assert(index < entities_.size());
        return entities_[index];
    }

    /// Return the storage's global modification version counter.
    [[nodiscard]] uint32_t Version() const noexcept override { return globalVersion_; }

    // ── Version / change detection ──────────────────────────────────────

    /// Explicitly mark the component for @p entity as changed.
    void MarkChanged(Entity entity) {
        assert(Has(entity) && "Entity does not have this component");
        versions_[sparse_[entity.id()]] = ++globalVersion_;
    }

    /// Return the version counter for the component owned by @p entity.
    [[nodiscard]] uint32_t GetVersion(Entity entity) const {
        assert(Has(entity) && "Entity does not have this component");
        return versions_[sparse_[entity.id()]];
    }

    /// True when the component's version is newer than @p sinceVersion.
    [[nodiscard]] bool HasChanged(Entity entity, uint32_t sinceVersion) const {
        assert(Has(entity) && "Entity does not have this component");
        return versions_[sparse_[entity.id()]] > sinceVersion;
    }

    /// The current global version counter.
    [[nodiscard]] uint32_t GlobalVersion() const noexcept { return globalVersion_; }

    // ── Type ID ─────────────────────────────────────────────────────────

    /// The compile-time-generated type ID for this component type.
    [[nodiscard]] static ComponentTypeId TypeId() noexcept { return ComponentType<T>::Id(); }

private:
    static constexpr uint32_t kInvalidIndex = std::numeric_limits<uint32_t>::max();

    void ensureSparseSize(uint32_t entityId) {
        if (entityId >= sparse_.size()) {
            sparse_.resize(static_cast<std::size_t>(entityId) + 1, kInvalidIndex);
        }
    }

    std::vector<T> dense_;            ///< Packed component data.
    std::vector<uint32_t> entities_;  ///< dense index -> entity id.
    std::vector<uint32_t> sparse_;    ///< entity id  -> dense index.
    std::vector<uint32_t> versions_;  ///< dense index -> change version.
    uint32_t globalVersion_ = 0;      ///< Monotonically increasing version.
};

}  // namespace cgs::ecs
