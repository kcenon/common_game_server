#pragma once

/// @file signal.hpp
/// @brief Thread-safe Signal<Args...> for observer pattern (event dispatch).
///
/// Provides a lightweight, header-only publish/subscribe mechanism. Slots
/// (callbacks) are registered via connect() and invoked when emit() is called.
/// Uses std::shared_mutex so readers (emit) can run concurrently while writers
/// (connect/disconnect) obtain exclusive access.

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace cgs::foundation {

/// Thread-safe signal (observer pattern) that dispatches events to registered
/// callbacks.
///
/// @tparam Args The argument types passed to each slot when the signal fires.
///
/// Example:
/// @code
///   Signal<SessionId> onConnected;
///   auto id = onConnected.connect([](SessionId sid) {
///       std::cout << "session " << sid.value() << " connected\n";
///   });
///   onConnected.emit(SessionId(42));
///   onConnected.disconnect(id);
/// @endcode
template <typename... Args>
class Signal {
public:
    using Slot = std::function<void(Args...)>;
    using SlotId = uint64_t;

    Signal() = default;
    ~Signal() = default;

    // Non-copyable. Movable with custom move (std::shared_mutex is not movable).
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;

    Signal(Signal&& other) noexcept {
        std::unique_lock lock(other.mutex_);
        slots_ = std::move(other.slots_);
        nextId_.store(other.nextId_.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
    }

    Signal& operator=(Signal&& other) noexcept {
        if (this != &other) {
            // Lock both, always in address order to prevent deadlock
            auto* first = this < &other ? this : &other;
            auto* second = this < &other ? &other : this;
            std::unique_lock lock1(first->mutex_);
            std::unique_lock lock2(second->mutex_);
            slots_ = std::move(other.slots_);
            nextId_.store(other.nextId_.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
        }
        return *this;
    }

    /// Register a callback. Returns a SlotId for later disconnect().
    SlotId connect(Slot slot) {
        auto id = nextId_.fetch_add(1, std::memory_order_relaxed);
        std::unique_lock lock(mutex_);
        slots_.emplace(id, std::move(slot));
        return id;
    }

    /// Remove a previously registered callback by its SlotId.
    void disconnect(SlotId id) {
        std::unique_lock lock(mutex_);
        slots_.erase(id);
    }

    /// Fire the signal, invoking every registered slot with the given args.
    /// Slots are invoked under a shared (reader) lock so emit() calls from
    /// multiple threads can overlap.  However, each slot invocation itself
    /// is not protected — slots must be internally thread-safe if shared
    /// state is accessed.
    void emit(Args... args) const {
        // Snapshot the slots under shared lock, then invoke outside the lock
        // to avoid deadlock if a slot calls connect/disconnect.
        std::vector<Slot> snapshot;
        {
            std::shared_lock lock(mutex_);
            snapshot.reserve(slots_.size());
            for (const auto& [id, slot] : slots_) {
                snapshot.push_back(slot);
            }
        }
        for (const auto& slot : snapshot) {
            slot(args...);
        }
    }

    /// Return the number of currently connected slots.
    [[nodiscard]] std::size_t slotCount() const {
        std::shared_lock lock(mutex_);
        return slots_.size();
    }

private:
    std::unordered_map<SlotId, Slot> slots_;
    std::atomic<SlotId> nextId_{1};
    mutable std::shared_mutex mutex_;
};

} // namespace cgs::foundation
