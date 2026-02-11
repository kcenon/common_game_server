#pragma once

/// @file event_bus.hpp
/// @brief Type-safe event bus for plugin-to-plugin communication.
///
/// Provides synchronous and deferred event publishing with priority
/// ordering and automatic subscription management.
///
/// @see SRS-PLG-003
/// @see SDS-MOD-017

#include <algorithm>
#include <any>
#include <cstdint>
#include <functional>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace cgs::plugin {

/// Unique identifier for an event subscription.
using SubscriptionId = uint64_t;

/// Type-safe event bus supporting synchronous and deferred delivery.
///
/// Events are dispatched to handlers registered for the concrete event
/// type. Handlers are invoked in priority order (lower value = higher
/// priority). Within the same priority, handlers are called in
/// subscription order.
///
/// Usage:
/// @code
///   EventBus bus;
///
///   // Subscribe with priority (default = 0).
///   auto id = bus.Subscribe<MyEvent>([](const MyEvent& e) {
///       // handle event
///   });
///
///   // Synchronous publish — handlers called immediately.
///   bus.Publish(MyEvent{42});
///
///   // Deferred publish — queued for later.
///   bus.PublishDeferred(MyEvent{99});
///   bus.ProcessDeferred();  // Flush the queue.
///
///   // Unsubscribe when done.
///   bus.Unsubscribe(id);
/// @endcode
class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    EventBus(EventBus&& other) noexcept {
        std::lock_guard lock(other.mutex_);
        handlers_ = std::move(other.handlers_);
        subscriptionTypes_ = std::move(other.subscriptionTypes_);
        deferredQueue_ = std::move(other.deferredQueue_);
        nextId_ = other.nextId_;
    }

    EventBus& operator=(EventBus&& other) noexcept {
        if (this != &other) {
            std::scoped_lock lock(mutex_, other.mutex_);
            handlers_ = std::move(other.handlers_);
            subscriptionTypes_ = std::move(other.subscriptionTypes_);
            deferredQueue_ = std::move(other.deferredQueue_);
            nextId_ = other.nextId_;
        }
        return *this;
    }

    // -- Subscribe ------------------------------------------------------------

    /// Subscribe a handler for events of type E.
    ///
    /// @tparam E  The event type to subscribe to.
    /// @param handler   Callback invoked when an event of type E is published.
    /// @param priority  Handler priority (lower = called first, default 0).
    /// @return A unique subscription ID for later unsubscription.
    template <typename E>
    SubscriptionId Subscribe(std::function<void(const E&)> handler,
                             int32_t priority = 0) {
        std::lock_guard lock(mutex_);

        auto id = nextId_++;
        auto typeIdx = std::type_index(typeid(E));

        HandlerEntry entry;
        entry.id = id;
        entry.priority = priority;
        entry.handler = [fn = std::move(handler)](const std::any& event) {
            fn(std::any_cast<const E&>(event));
        };

        auto& handlers = handlers_[typeIdx];
        handlers.push_back(std::move(entry));
        sortHandlers(handlers);

        // Track type for type-erased unsubscribe.
        subscriptionTypes_.insert_or_assign(id, typeIdx);

        return id;
    }

    // -- Unsubscribe ----------------------------------------------------------

    /// Remove a subscription by ID.
    ///
    /// Safe to call with an already-removed or invalid ID (no-op).
    void Unsubscribe(SubscriptionId id) {
        std::lock_guard lock(mutex_);
        unsubscribeInternal(id);
    }

    /// Remove all subscriptions, clearing the handler table entirely.
    void UnsubscribeAll() {
        std::lock_guard lock(mutex_);
        handlers_.clear();
        subscriptionTypes_.clear();
    }

    // -- Synchronous publish --------------------------------------------------

    /// Publish an event synchronously.
    ///
    /// All registered handlers for type E are called immediately,
    /// in priority order, on the calling thread.
    template <typename E>
    void Publish(const E& event) {
        std::vector<HandlerEntry> snapshot;
        {
            std::lock_guard lock(mutex_);
            auto it = handlers_.find(std::type_index(typeid(E)));
            if (it == handlers_.end()) {
                return;
            }
            snapshot = it->second;
        }

        std::any wrapped = event;
        for (const auto& entry : snapshot) {
            entry.handler(wrapped);
        }
    }

    // -- Deferred publish -----------------------------------------------------

    /// Queue an event for deferred processing.
    ///
    /// The event is copied and stored. Call ProcessDeferred() to
    /// dispatch all queued events (typically at frame boundaries).
    template <typename E>
    void PublishDeferred(E event) {
        std::lock_guard lock(mutex_);
        deferredQueue_.push_back([this, evt = std::move(event)]() {
            Publish(evt);
        });
    }

    /// Flush the deferred event queue.
    ///
    /// Dispatches all queued events in FIFO order. New events published
    /// during processing are NOT included in this cycle.
    void ProcessDeferred() {
        std::vector<std::function<void()>> queue;
        {
            std::lock_guard lock(mutex_);
            queue.swap(deferredQueue_);
        }

        for (auto& fn : queue) {
            fn();
        }
    }

    // -- Queries --------------------------------------------------------------

    /// Get the total number of active subscriptions across all event types.
    [[nodiscard]] std::size_t HandlerCount() const {
        std::lock_guard lock(mutex_);
        std::size_t count = 0;
        for (const auto& [_, handlers] : handlers_) {
            count += handlers.size();
        }
        return count;
    }

    /// Get the number of handlers for a specific event type.
    template <typename E>
    [[nodiscard]] std::size_t HandlerCountFor() const {
        std::lock_guard lock(mutex_);
        auto it = handlers_.find(std::type_index(typeid(E)));
        if (it == handlers_.end()) {
            return 0;
        }
        return it->second.size();
    }

    /// Get the number of events waiting in the deferred queue.
    [[nodiscard]] std::size_t DeferredCount() const {
        std::lock_guard lock(mutex_);
        return deferredQueue_.size();
    }

private:
    /// A type-erased handler with priority and ID.
    struct HandlerEntry {
        SubscriptionId id = 0;
        int32_t priority = 0;
        std::function<void(const std::any&)> handler;
    };

    /// Sort handlers by priority (ascending), stable to preserve
    /// insertion order within the same priority level.
    static void sortHandlers(std::vector<HandlerEntry>& handlers) {
        std::stable_sort(handlers.begin(), handlers.end(),
                         [](const HandlerEntry& a, const HandlerEntry& b) {
                             return a.priority < b.priority;
                         });
    }

    /// Internal unsubscribe without locking (caller must hold mutex_).
    void unsubscribeInternal(SubscriptionId id) {
        auto typeIt = subscriptionTypes_.find(id);
        if (typeIt == subscriptionTypes_.end()) {
            return;
        }

        auto handlersIt = handlers_.find(typeIt->second);
        if (handlersIt != handlers_.end()) {
            auto& vec = handlersIt->second;
            vec.erase(
                std::remove_if(vec.begin(), vec.end(),
                               [id](const HandlerEntry& e) {
                                   return e.id == id;
                               }),
                vec.end());

            if (vec.empty()) {
                handlers_.erase(handlersIt);
            }
        }

        subscriptionTypes_.erase(typeIt);
    }

    /// Handler table: type_index → sorted handler list.
    std::unordered_map<std::type_index, std::vector<HandlerEntry>> handlers_;

    /// Reverse lookup: subscription ID → type_index.
    std::unordered_map<SubscriptionId, std::type_index> subscriptionTypes_;

    /// Queue of deferred event dispatches.
    std::vector<std::function<void()>> deferredQueue_;

    /// Next subscription ID.
    SubscriptionId nextId_ = 1;

    /// Mutex for thread safety.
    mutable std::mutex mutex_;
};

} // namespace cgs::plugin
