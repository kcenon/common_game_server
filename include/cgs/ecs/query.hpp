#pragma once

/// @file query.hpp
/// @brief Multi-component query system for the ECS.
///
/// Query<Includes...> provides filtered iteration over entities that
/// possess all specified component types, with support for exclude
/// filters, optional component access, and cached results.
///
/// @see SDS-MOD-013
/// @see docs/reference/ECS_DESIGN.md  Section 2.5

#include <cassert>
#include <cstdint>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/component_type_id.hpp"
#include "cgs/ecs/entity.hpp"

namespace cgs::ecs {

/// Multi-component query with include/exclude filters and caching.
///
/// A Query iterates all entities that possess every component type in
/// the Includes... pack, optionally excluding entities that have certain
/// other components.  Query results are cached and automatically
/// invalidated when any relevant storage is modified.
///
/// @note Entity handles yielded by ForEach and iterators carry version 0.
///       The entity id is valid for all component storage operations.
///       Use EntityManager::IsAlive() if full version checking is needed.
///
/// @note Modifying a storage that is being iterated by ForEach is
///       undefined behavior.  Schedule mutations via deferred operations
///       or separate passes.
///
/// Usage:
/// @code
///   ComponentStorage<Position> posStore;
///   ComponentStorage<Velocity> velStore;
///   ComponentStorage<Static>   staticStore;
///
///   Query<Position, Velocity> movables(posStore, velStore);
///   movables.Exclude(staticStore)
///           .ForEach([](Entity e, Position& pos, Velocity& vel) {
///               pos.x += vel.dx;
///           });
/// @endcode
template <typename... Includes>
class Query {
    static_assert(sizeof...(Includes) > 0,
                  "Query must have at least one component type");

public:
    using iterator = typename std::vector<Entity>::iterator;
    using const_iterator = typename std::vector<Entity>::const_iterator;

    /// Construct a query from the storages for each included component type.
    explicit Query(ComponentStorage<Includes>&... storages)
        : storages_{&storages...} {}

    // Non-copyable, movable.
    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;
    Query(Query&&) noexcept = default;
    Query& operator=(Query&&) noexcept = default;

    // ── Filters ──────────────────────────────────────────────────────────

    /// Exclude entities that have a component in @p storage.
    ///
    /// Multiple exclude filters may be chained.  An entity is excluded
    /// if it has a component in *any* excluded storage.
    Query& Exclude(const IComponentStorage& storage) {
        excludes_.push_back(&storage);
        cacheValid_ = false;
        return *this;
    }

    /// Register an optional component storage for @p T.
    ///
    /// Optional components do not affect entity matching but can be
    /// accessed via GetOptional<T>() inside a ForEach callback.
    template <typename T>
    Query& Optional(ComponentStorage<T>& storage) {
        optionals_[ComponentType<T>::Id()] = &storage;
        return *this;
    }

    // ── Iteration ────────────────────────────────────────────────────────

    /// Invoke @p func for every matching entity.
    ///
    /// The callback receives the Entity handle followed by mutable
    /// references to each included component type.
    ///
    /// @code
    ///   query.ForEach([](Entity e, Position& pos, Velocity& vel) {
    ///       pos.x += vel.dx;
    ///   });
    /// @endcode
    template <typename Func>
    void ForEach(Func&& func) {
        RefreshCache();
        for (Entity e : cachedEntities_) {
            func(e, std::get<ComponentStorage<Includes>*>(storages_)->Get(e)...);
        }
    }

    /// Return the number of matching entities.
    [[nodiscard]] std::size_t Count() const {
        RefreshCache();
        return cachedEntities_.size();
    }

    // ── Optional access ──────────────────────────────────────────────────

    /// Get an optional component for @p entity, or nullptr if absent.
    ///
    /// @pre The component type @p T must have been registered via
    ///      Optional() before calling this method.
    ///
    /// @code
    ///   query.Optional(healthStore);
    ///   query.ForEach([&](Entity e, Position& pos) {
    ///       Health* h = query.GetOptional<Health>(e);
    ///       if (h) h->current -= 10;
    ///   });
    /// @endcode
    template <typename T>
    [[nodiscard]] T* GetOptional(Entity entity) {
        auto it = optionals_.find(ComponentType<T>::Id());
        if (it == optionals_.end()) {
            return nullptr;
        }
        auto* storage = static_cast<ComponentStorage<T>*>(it->second);
        if (!storage->Has(entity)) {
            return nullptr;
        }
        return &storage->Get(entity);
    }

    /// Get an optional component (const) for @p entity, or nullptr.
    template <typename T>
    [[nodiscard]] const T* GetOptional(Entity entity) const {
        auto it = optionals_.find(ComponentType<T>::Id());
        if (it == optionals_.end()) {
            return nullptr;
        }
        const auto* storage =
            static_cast<const ComponentStorage<T>*>(it->second);
        if (!storage->Has(entity)) {
            return nullptr;
        }
        return &storage->Get(entity);
    }

    // ── Range-based for loop ─────────────────────────────────────────────

    /// @{
    /// Iterator access for range-based for loops.
    ///
    /// The iterators yield Entity values.  Use ForEach for typed
    /// component access, or call storage.Get(e) manually.
    iterator begin() {
        RefreshCache();
        return cachedEntities_.begin();
    }
    iterator end() { return cachedEntities_.end(); }

    [[nodiscard]] const_iterator begin() const {
        RefreshCache();
        return cachedEntities_.cbegin();
    }
    [[nodiscard]] const_iterator end() const { return cachedEntities_.cend(); }
    /// @}

private:
    /// Compute a version fingerprint from all relevant storages.
    ///
    /// The fingerprint changes whenever any Include or Exclude storage
    /// is modified.  Optional storages do not affect entity matching
    /// and are excluded from the fingerprint.
    [[nodiscard]] uint64_t computeVersionFingerprint() const noexcept {
        uint64_t fp = 0;
        std::apply(
            [&](auto*... ptrs) { ((fp += ptrs->GlobalVersion()), ...); },
            storages_);
        for (const auto* ex : excludes_) {
            fp += ex->Version();
        }
        return fp;
    }

    /// Rebuild the cached entity list if stale.
    void RefreshCache() const {
        const uint64_t fp = computeVersionFingerprint();
        if (cacheValid_ && cacheVersion_ == fp) {
            return;
        }

        cachedEntities_.clear();

        // Find the smallest Include storage for optimal iteration.
        const IComponentStorage* smallest = nullptr;
        std::size_t smallestSize = std::numeric_limits<std::size_t>::max();
        std::apply(
            [&](auto*... ptrs) {
                auto pick = [&](const IComponentStorage* p) {
                    if (p->Size() < smallestSize) {
                        smallest = p;
                        smallestSize = p->Size();
                    }
                };
                (pick(ptrs), ...);
            },
            storages_);

        if (smallest == nullptr || smallestSize == 0) {
            cacheVersion_ = fp;
            cacheValid_ = true;
            return;
        }

        // Iterate entities in the smallest storage and test membership.
        for (std::size_t i = 0; i < smallestSize; ++i) {
            const uint32_t eid = smallest->EntityAt(i);
            const Entity entity(eid, 0);

            // All Include storages must contain this entity.
            bool matchesAll = true;
            std::apply(
                [&](auto*... ptrs) {
                    auto check = [&](const IComponentStorage* p) {
                        if (!matchesAll) {
                            return;
                        }
                        if (p == smallest) {
                            return; // Already known to be present.
                        }
                        if (!p->Has(entity)) {
                            matchesAll = false;
                        }
                    };
                    (check(ptrs), ...);
                },
                storages_);

            if (!matchesAll) {
                continue;
            }

            // None of the Exclude storages may contain this entity.
            bool excluded = false;
            for (const auto* ex : excludes_) {
                if (ex->Has(entity)) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) {
                continue;
            }

            cachedEntities_.push_back(entity);
        }

        cacheVersion_ = fp;
        cacheValid_ = true;
    }

    /// Pointers to the Include component storages.
    std::tuple<ComponentStorage<Includes>*...> storages_;

    /// Pointers to storages whose presence excludes an entity.
    std::vector<const IComponentStorage*> excludes_;

    /// Optional component storages keyed by ComponentTypeId.
    std::unordered_map<ComponentTypeId, void*> optionals_;

    /// Cached matching entities (rebuilt on version mismatch).
    mutable std::vector<Entity> cachedEntities_;

    /// Version fingerprint at the time the cache was built.
    mutable uint64_t cacheVersion_ = 0;

    /// Whether the cache is currently valid.
    mutable bool cacheValid_ = false;
};

} // namespace cgs::ecs
