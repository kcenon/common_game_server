/// @file event_bus_test.cpp
/// @brief Unit tests for EventBus (pub/sub, priority, deferred, unsubscribe)
///        and integration tests for plugin lifecycle events.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "cgs/plugin/event_bus.hpp"
#include "cgs/plugin/iplugin.hpp"
#include "cgs/plugin/plugin_events.hpp"
#include "cgs/plugin/plugin_export.hpp"
#include "cgs/plugin/plugin_manager.hpp"

using namespace cgs::plugin;

// ============================================================================
// Test event types
// ============================================================================

struct IntEvent {
    int value = 0;
};

struct StringEvent {
    std::string message;
};

struct PriorityEvent {
    int id = 0;
};

// ============================================================================
// EventBus Core Tests
// ============================================================================

class EventBusTest : public ::testing::Test {
protected:
    EventBus bus_;
};

TEST_F(EventBusTest, SubscribeAndPublish) {
    int received = 0;
    bus_.Subscribe<IntEvent>([&](const IntEvent& e) {
        received = e.value;
    });

    bus_.Publish(IntEvent{42});
    EXPECT_EQ(received, 42);
}

TEST_F(EventBusTest, MultipleHandlersCalledInOrder) {
    std::vector<int> order;

    bus_.Subscribe<IntEvent>([&](const IntEvent&) { order.push_back(1); });
    bus_.Subscribe<IntEvent>([&](const IntEvent&) { order.push_back(2); });
    bus_.Subscribe<IntEvent>([&](const IntEvent&) { order.push_back(3); });

    bus_.Publish(IntEvent{0});
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST_F(EventBusTest, DifferentEventTypesIndependent) {
    int intReceived = 0;
    std::string strReceived;

    bus_.Subscribe<IntEvent>([&](const IntEvent& e) {
        intReceived = e.value;
    });
    bus_.Subscribe<StringEvent>([&](const StringEvent& e) {
        strReceived = e.message;
    });

    bus_.Publish(IntEvent{7});
    EXPECT_EQ(intReceived, 7);
    EXPECT_TRUE(strReceived.empty());

    bus_.Publish(StringEvent{"hello"});
    EXPECT_EQ(strReceived, "hello");
    EXPECT_EQ(intReceived, 7);
}

TEST_F(EventBusTest, PublishWithNoHandlersIsNoOp) {
    // Should not crash or throw.
    bus_.Publish(IntEvent{99});
}

TEST_F(EventBusTest, HandlerCountTracking) {
    EXPECT_EQ(bus_.HandlerCount(), 0u);
    EXPECT_EQ(bus_.HandlerCountFor<IntEvent>(), 0u);

    bus_.Subscribe<IntEvent>([](const IntEvent&) {});
    bus_.Subscribe<IntEvent>([](const IntEvent&) {});
    bus_.Subscribe<StringEvent>([](const StringEvent&) {});

    EXPECT_EQ(bus_.HandlerCount(), 3u);
    EXPECT_EQ(bus_.HandlerCountFor<IntEvent>(), 2u);
    EXPECT_EQ(bus_.HandlerCountFor<StringEvent>(), 1u);
}

// ============================================================================
// Unsubscribe Tests
// ============================================================================

TEST_F(EventBusTest, UnsubscribeRemovesHandler) {
    int callCount = 0;
    auto id = bus_.Subscribe<IntEvent>([&](const IntEvent&) {
        callCount++;
    });

    bus_.Publish(IntEvent{1});
    EXPECT_EQ(callCount, 1);

    bus_.Unsubscribe(id);

    bus_.Publish(IntEvent{2});
    EXPECT_EQ(callCount, 1); // Not called again.
    EXPECT_EQ(bus_.HandlerCountFor<IntEvent>(), 0u);
}

TEST_F(EventBusTest, UnsubscribeInvalidIdIsNoOp) {
    bus_.Unsubscribe(99999); // Should not crash.
}

TEST_F(EventBusTest, UnsubscribeOneOfMany) {
    std::vector<int> calls;

    bus_.Subscribe<IntEvent>([&](const IntEvent&) { calls.push_back(1); });
    auto id2 = bus_.Subscribe<IntEvent>([&](const IntEvent&) { calls.push_back(2); });
    bus_.Subscribe<IntEvent>([&](const IntEvent&) { calls.push_back(3); });

    bus_.Unsubscribe(id2);

    bus_.Publish(IntEvent{0});
    ASSERT_EQ(calls.size(), 2u);
    EXPECT_EQ(calls[0], 1);
    EXPECT_EQ(calls[1], 3);
}

TEST_F(EventBusTest, UnsubscribeAll) {
    bus_.Subscribe<IntEvent>([](const IntEvent&) {});
    bus_.Subscribe<StringEvent>([](const StringEvent&) {});

    EXPECT_EQ(bus_.HandlerCount(), 2u);

    bus_.UnsubscribeAll();

    EXPECT_EQ(bus_.HandlerCount(), 0u);
}

TEST_F(EventBusTest, DoubleUnsubscribeIsNoOp) {
    auto id = bus_.Subscribe<IntEvent>([](const IntEvent&) {});
    bus_.Unsubscribe(id);
    bus_.Unsubscribe(id); // Should not crash.
    EXPECT_EQ(bus_.HandlerCount(), 0u);
}

// ============================================================================
// Priority Tests
// ============================================================================

TEST_F(EventBusTest, HandlersCalledInPriorityOrder) {
    std::vector<int> order;

    // Subscribe out of priority order.
    bus_.Subscribe<PriorityEvent>(
        [&](const PriorityEvent&) { order.push_back(3); }, 10);
    bus_.Subscribe<PriorityEvent>(
        [&](const PriorityEvent&) { order.push_back(1); }, -5);
    bus_.Subscribe<PriorityEvent>(
        [&](const PriorityEvent&) { order.push_back(2); }, 0);

    bus_.Publish(PriorityEvent{0});

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1); // priority -5 (highest)
    EXPECT_EQ(order[1], 2); // priority 0
    EXPECT_EQ(order[2], 3); // priority 10 (lowest)
}

TEST_F(EventBusTest, SamePriorityPreservesInsertionOrder) {
    std::vector<int> order;

    bus_.Subscribe<IntEvent>(
        [&](const IntEvent&) { order.push_back(1); }, 0);
    bus_.Subscribe<IntEvent>(
        [&](const IntEvent&) { order.push_back(2); }, 0);
    bus_.Subscribe<IntEvent>(
        [&](const IntEvent&) { order.push_back(3); }, 0);

    bus_.Publish(IntEvent{0});

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

// ============================================================================
// Deferred Publishing Tests
// ============================================================================

TEST_F(EventBusTest, DeferredNotCalledImmediately) {
    int received = 0;
    bus_.Subscribe<IntEvent>([&](const IntEvent& e) {
        received = e.value;
    });

    bus_.PublishDeferred(IntEvent{42});
    EXPECT_EQ(received, 0); // Not yet dispatched.
    EXPECT_EQ(bus_.DeferredCount(), 1u);
}

TEST_F(EventBusTest, ProcessDeferredFlushesQueue) {
    int received = 0;
    bus_.Subscribe<IntEvent>([&](const IntEvent& e) {
        received = e.value;
    });

    bus_.PublishDeferred(IntEvent{42});
    bus_.ProcessDeferred();

    EXPECT_EQ(received, 42);
    EXPECT_EQ(bus_.DeferredCount(), 0u);
}

TEST_F(EventBusTest, MultipleDeferredProcessedInFIFO) {
    std::vector<int> order;
    bus_.Subscribe<IntEvent>([&](const IntEvent& e) {
        order.push_back(e.value);
    });

    bus_.PublishDeferred(IntEvent{1});
    bus_.PublishDeferred(IntEvent{2});
    bus_.PublishDeferred(IntEvent{3});

    bus_.ProcessDeferred();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST_F(EventBusTest, ProcessDeferredOnEmptyQueueIsNoOp) {
    bus_.ProcessDeferred(); // Should not crash.
    EXPECT_EQ(bus_.DeferredCount(), 0u);
}

TEST_F(EventBusTest, DeferredEventsFromProcessingNotIncluded) {
    std::vector<int> order;

    bus_.Subscribe<IntEvent>([&](const IntEvent& e) {
        order.push_back(e.value);
        if (e.value == 1) {
            // Publish another deferred event during processing.
            bus_.PublishDeferred(IntEvent{99});
        }
    });

    bus_.PublishDeferred(IntEvent{1});
    bus_.ProcessDeferred();

    // Only event 1 should have been processed.
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0], 1);

    // Event 99 should be in the next cycle.
    EXPECT_EQ(bus_.DeferredCount(), 1u);

    bus_.ProcessDeferred();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[1], 99);
}

TEST_F(EventBusTest, MixedSyncAndDeferred) {
    std::vector<int> order;
    bus_.Subscribe<IntEvent>([&](const IntEvent& e) {
        order.push_back(e.value);
    });

    bus_.Publish(IntEvent{1});        // Immediate.
    bus_.PublishDeferred(IntEvent{2}); // Deferred.
    bus_.Publish(IntEvent{3});        // Immediate.

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 3);

    bus_.ProcessDeferred();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[2], 2);
}

// ============================================================================
// Subscription IDs
// ============================================================================

TEST_F(EventBusTest, SubscriptionIdsAreUnique) {
    auto id1 = bus_.Subscribe<IntEvent>([](const IntEvent&) {});
    auto id2 = bus_.Subscribe<IntEvent>([](const IntEvent&) {});
    auto id3 = bus_.Subscribe<StringEvent>([](const StringEvent&) {});

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

// ============================================================================
// Handler subscribes/unsubscribes during Publish (re-entrancy)
// ============================================================================

TEST_F(EventBusTest, SubscribeDuringPublishDoesNotAffectCurrentDispatch) {
    std::vector<int> order;

    bus_.Subscribe<IntEvent>([&](const IntEvent&) {
        order.push_back(1);
        // Subscribe a new handler during dispatch.
        bus_.Subscribe<IntEvent>([&](const IntEvent&) {
            order.push_back(99);
        });
    });

    bus_.Publish(IntEvent{0});

    // Only the original handler should have been called.
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0], 1);

    // New handler should be called on next publish.
    order.clear();
    bus_.Publish(IntEvent{0});
    EXPECT_EQ(order.size(), 2u);
}

// ============================================================================
// Plugin Lifecycle Events via PluginManager
// ============================================================================

namespace {

/// Test plugin that records lifecycle calls for event testing.
class EventTestPlugin : public IPlugin {
public:
    explicit EventTestPlugin(std::string name = "EventTestPlugin")
        : info_{std::move(name), "Test plugin for events",
                {1, 0, 0}, {}, kPluginApiVersion} {}

    const PluginInfo& GetInfo() const override { return info_; }

    bool OnLoad(PluginContext& ctx) override {
        context_ = &ctx;
        return true;
    }
    bool OnInit() override { return true; }
    void OnUpdate(float) override {}
    void OnShutdown() override {}
    void OnUnload() override {}

    PluginContext* context_ = nullptr;

private:
    PluginInfo info_;
};

} // anonymous namespace

class PluginEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Save global static registry and inject our test plugin factory.
        auto& registry = StaticPluginRegistry();
        savedRegistry_ = std::move(registry);
        registry.clear();
        registry.push_back(
            {"EventTestPlugin",
             []() -> std::unique_ptr<IPlugin> {
                 return std::make_unique<EventTestPlugin>();
             }});
    }

    void TearDown() override {
        // Restore the original static registry.
        StaticPluginRegistry() = std::move(savedRegistry_);
    }

    PluginManager manager_;

private:
    std::vector<StaticPluginEntry> savedRegistry_;
};

TEST_F(PluginEventTest, ContextHasEventBus) {
    EXPECT_NE(manager_.GetContext().eventBus, nullptr);
}

TEST_F(PluginEventTest, GetEventBusReturnsSameInstance) {
    EXPECT_EQ(&manager_.GetEventBus(),
              manager_.GetContext().eventBus);
}

TEST_F(PluginEventTest, PluginLoadedEventEmitted) {
    std::string loadedName;
    manager_.GetEventBus().Subscribe<PluginLoadedEvent>(
        [&](const PluginLoadedEvent& e) {
            loadedName = e.pluginName;
        });

    (void)manager_.RegisterStaticPlugins();

    EXPECT_EQ(loadedName, "EventTestPlugin");
}

TEST_F(PluginEventTest, PluginInitializedEventEmitted) {
    std::string initName;
    manager_.GetEventBus().Subscribe<PluginInitializedEvent>(
        [&](const PluginInitializedEvent& e) {
            initName = e.pluginName;
        });

    (void)manager_.RegisterStaticPlugins();
    (void)manager_.InitializeAll();

    EXPECT_FALSE(initName.empty());
}

TEST_F(PluginEventTest, PluginActivatedEventEmitted) {
    std::string activatedName;
    manager_.GetEventBus().Subscribe<PluginActivatedEvent>(
        [&](const PluginActivatedEvent& e) {
            activatedName = e.pluginName;
        });

    (void)manager_.RegisterStaticPlugins();
    (void)manager_.InitializeAll();
    (void)manager_.ActivateAll();

    EXPECT_FALSE(activatedName.empty());
}

TEST_F(PluginEventTest, PluginShutdownEventEmitted) {
    std::string shutdownName;
    manager_.GetEventBus().Subscribe<PluginShutdownEvent>(
        [&](const PluginShutdownEvent& e) {
            shutdownName = e.pluginName;
        });

    (void)manager_.RegisterStaticPlugins();
    (void)manager_.InitializeAll();
    (void)manager_.ActivateAll();
    manager_.ShutdownAll();

    EXPECT_FALSE(shutdownName.empty());
}

TEST_F(PluginEventTest, PluginReceivesEventBusViaContext) {
    (void)manager_.RegisterStaticPlugins();

    auto names = manager_.GetAllPluginNames();
    ASSERT_FALSE(names.empty());

    auto* plugin = dynamic_cast<EventTestPlugin*>(
        manager_.GetPlugin(names[0]));
    ASSERT_NE(plugin, nullptr);
    ASSERT_NE(plugin->context_, nullptr);
    EXPECT_NE(plugin->context_->eventBus, nullptr);
}

TEST_F(PluginEventTest, LifecycleEventOrdering) {
    std::vector<std::string> events;

    auto& bus = manager_.GetEventBus();
    bus.Subscribe<PluginLoadedEvent>(
        [&](const PluginLoadedEvent&) { events.push_back("loaded"); });
    bus.Subscribe<PluginInitializedEvent>(
        [&](const PluginInitializedEvent&) { events.push_back("initialized"); });
    bus.Subscribe<PluginActivatedEvent>(
        [&](const PluginActivatedEvent&) { events.push_back("activated"); });
    bus.Subscribe<PluginShutdownEvent>(
        [&](const PluginShutdownEvent&) { events.push_back("shutdown"); });

    (void)manager_.RegisterStaticPlugins();
    (void)manager_.InitializeAll();
    (void)manager_.ActivateAll();
    manager_.ShutdownAll();

    ASSERT_GE(events.size(), 4u);
    EXPECT_EQ(events[0], "loaded");
    EXPECT_EQ(events[1], "initialized");
    EXPECT_EQ(events[2], "activated");
    EXPECT_EQ(events[3], "shutdown");
}

TEST_F(PluginEventTest, DeferredEventsInUpdateCycle) {
    auto& bus = manager_.GetEventBus();

    int received = 0;
    bus.Subscribe<IntEvent>([&](const IntEvent& e) {
        received = e.value;
    });

    bus.PublishDeferred(IntEvent{777});
    EXPECT_EQ(received, 0);

    bus.ProcessDeferred();
    EXPECT_EQ(received, 777);
}
