#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/job_scheduler.hpp"

using namespace cgs::foundation;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// ErrorCode: Thread subsystem lookup
// ---------------------------------------------------------------------------

TEST(ThreadErrorCodeTest, SubsystemLookup) {
    EXPECT_EQ(errorSubsystem(ErrorCode::ThreadError), "Thread");
    EXPECT_EQ(errorSubsystem(ErrorCode::JobScheduleFailed), "Thread");
    EXPECT_EQ(errorSubsystem(ErrorCode::JobNotFound), "Thread");
    EXPECT_EQ(errorSubsystem(ErrorCode::JobCancelled), "Thread");
    EXPECT_EQ(errorSubsystem(ErrorCode::JobTimeout), "Thread");
    EXPECT_EQ(errorSubsystem(ErrorCode::JobDependencyFailed), "Thread");
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(GameJobSchedulerTest, DefaultConstruction) {
    GameJobScheduler scheduler;
    // Should not crash; pool is started with hardware_concurrency threads.
}

TEST(GameJobSchedulerTest, CustomThreadCount) {
    GameJobScheduler scheduler(2);
    auto result = scheduler.schedule([] {});
    ASSERT_TRUE(result.hasValue());
    auto waitResult = scheduler.wait(result.value());
    EXPECT_TRUE(waitResult.hasValue());
}

TEST(GameJobSchedulerTest, MoveConstruction) {
    GameJobScheduler a(2);
    GameJobScheduler b(std::move(a));
    auto result = b.schedule([] {});
    ASSERT_TRUE(result.hasValue());
    auto waitResult = b.wait(result.value());
    EXPECT_TRUE(waitResult.hasValue());
}

TEST(GameJobSchedulerTest, MoveAssignment) {
    GameJobScheduler a(2);
    GameJobScheduler b(2);
    b = std::move(a);
    auto result = b.schedule([] {});
    ASSERT_TRUE(result.hasValue());
    auto waitResult = b.wait(result.value());
    EXPECT_TRUE(waitResult.hasValue());
}

// ---------------------------------------------------------------------------
// Basic scheduling
// ---------------------------------------------------------------------------

TEST(GameJobSchedulerTest, ScheduleAndWaitSingleJob) {
    GameJobScheduler scheduler(2);
    std::atomic<bool> executed{false};

    auto result = scheduler.schedule([&] { executed.store(true); });
    ASSERT_TRUE(result.hasValue());

    auto waitResult = scheduler.wait(result.value());
    EXPECT_TRUE(waitResult.hasValue());
    EXPECT_TRUE(executed.load());
}

TEST(GameJobSchedulerTest, ScheduleMultipleJobs) {
    GameJobScheduler scheduler(4);
    constexpr int kJobs = 50;
    std::atomic<int> counter{0};

    std::vector<GameJobScheduler::JobId> ids;
    for (int i = 0; i < kJobs; ++i) {
        auto result = scheduler.schedule([&] { counter.fetch_add(1); });
        ASSERT_TRUE(result.hasValue());
        ids.push_back(result.value());
    }

    for (auto id : ids) {
        auto waitResult = scheduler.wait(id);
        EXPECT_TRUE(waitResult.hasValue());
    }
    EXPECT_EQ(counter.load(), kJobs);
}

TEST(GameJobSchedulerTest, JobReturnsUniqueIds) {
    GameJobScheduler scheduler(2);
    auto r1 = scheduler.schedule([] {});
    auto r2 = scheduler.schedule([] {});
    ASSERT_TRUE(r1.hasValue());
    ASSERT_TRUE(r2.hasValue());
    EXPECT_NE(r1.value(), r2.value());
}

// ---------------------------------------------------------------------------
// Priority levels
// ---------------------------------------------------------------------------

TEST(GameJobSchedulerTest, AllPriorityLevelsAccepted) {
    GameJobScheduler scheduler(2);
    std::atomic<int> counter{0};

    auto r1 = scheduler.schedule([&] { counter++; }, JobPriority::Critical);
    auto r2 = scheduler.schedule([&] { counter++; }, JobPriority::High);
    auto r3 = scheduler.schedule([&] { counter++; }, JobPriority::Normal);
    auto r4 = scheduler.schedule([&] { counter++; }, JobPriority::Low);

    ASSERT_TRUE(r1.hasValue());
    ASSERT_TRUE(r2.hasValue());
    ASSERT_TRUE(r3.hasValue());
    ASSERT_TRUE(r4.hasValue());

    scheduler.wait(r1.value());
    scheduler.wait(r2.value());
    scheduler.wait(r3.value());
    scheduler.wait(r4.value());

    EXPECT_EQ(counter.load(), 4);
}

// ---------------------------------------------------------------------------
// Dependency chaining
// ---------------------------------------------------------------------------

TEST(GameJobSchedulerTest, ScheduleAfterExecutesInOrder) {
    GameJobScheduler scheduler(2);
    std::vector<int> order;
    std::mutex orderMutex;

    auto first = scheduler.schedule([&] {
        std::this_thread::sleep_for(10ms);
        std::lock_guard lock(orderMutex);
        order.push_back(1);
    });
    ASSERT_TRUE(first.hasValue());

    auto second = scheduler.scheduleAfter(first.value(), [&] {
        std::lock_guard lock(orderMutex);
        order.push_back(2);
    });
    ASSERT_TRUE(second.hasValue());

    scheduler.wait(second.value());

    std::lock_guard lock(orderMutex);
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
}

TEST(GameJobSchedulerTest, ChainOfThreeDependentJobs) {
    GameJobScheduler scheduler(2);
    std::vector<int> order;
    std::mutex orderMutex;

    auto j1 = scheduler.schedule([&] {
        std::lock_guard lock(orderMutex);
        order.push_back(1);
    });
    ASSERT_TRUE(j1.hasValue());

    auto j2 = scheduler.scheduleAfter(j1.value(), [&] {
        std::lock_guard lock(orderMutex);
        order.push_back(2);
    });
    ASSERT_TRUE(j2.hasValue());

    auto j3 = scheduler.scheduleAfter(j2.value(), [&] {
        std::lock_guard lock(orderMutex);
        order.push_back(3);
    });
    ASSERT_TRUE(j3.hasValue());

    scheduler.wait(j3.value());

    std::lock_guard lock(orderMutex);
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(GameJobSchedulerTest, ScheduleAfterInvalidDependencyReturnsError) {
    GameJobScheduler scheduler(2);
    auto result = scheduler.scheduleAfter(9999, [] {});
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::JobNotFound);
}

// ---------------------------------------------------------------------------
// Tick scheduling
// ---------------------------------------------------------------------------

TEST(GameJobSchedulerTest, TickJobFiresAtInterval) {
    GameJobScheduler scheduler(2);
    std::atomic<int> ticks{0};

    auto result = scheduler.scheduleTick(50ms, [&] { ticks.fetch_add(1); });
    ASSERT_TRUE(result.hasValue());

    // Simulate 3 ticks at 50ms intervals: total 150ms
    scheduler.processTick(50ms);
    scheduler.processTick(50ms);
    scheduler.processTick(50ms);

    // Allow dispatched jobs to complete
    std::this_thread::sleep_for(100ms);

    EXPECT_GE(ticks.load(), 3);
}

TEST(GameJobSchedulerTest, ProcessTickNoDueJobsDoesNothing) {
    GameJobScheduler scheduler(2);
    std::atomic<int> ticks{0};

    scheduler.scheduleTick(100ms, [&] { ticks.fetch_add(1); });

    // Advance 30ms — not enough for 100ms interval
    scheduler.processTick(30ms);
    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(ticks.load(), 0);
}

TEST(GameJobSchedulerTest, MultipleTickJobsDifferentIntervals) {
    GameJobScheduler scheduler(2);
    std::atomic<int> fastTicks{0};
    std::atomic<int> slowTicks{0};

    scheduler.scheduleTick(20ms, [&] { fastTicks.fetch_add(1); });
    scheduler.scheduleTick(50ms, [&] { slowTicks.fetch_add(1); });

    // Advance 60ms in 20ms steps
    scheduler.processTick(20ms);
    scheduler.processTick(20ms);
    scheduler.processTick(20ms);

    std::this_thread::sleep_for(100ms);

    EXPECT_GE(fastTicks.load(), 3);
    EXPECT_GE(slowTicks.load(), 1);
}

// ---------------------------------------------------------------------------
// Job control: wait
// ---------------------------------------------------------------------------

TEST(GameJobSchedulerTest, WaitBlocksUntilCompletion) {
    GameJobScheduler scheduler(2);
    std::atomic<bool> done{false};

    auto result = scheduler.schedule([&] {
        std::this_thread::sleep_for(50ms);
        done.store(true);
    });
    ASSERT_TRUE(result.hasValue());

    auto waitResult = scheduler.wait(result.value());
    EXPECT_TRUE(waitResult.hasValue());
    EXPECT_TRUE(done.load());
}

TEST(GameJobSchedulerTest, WaitNonExistentJobReturnsError) {
    GameJobScheduler scheduler(2);
    auto result = scheduler.wait(99999);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::JobNotFound);
}

// ---------------------------------------------------------------------------
// Job control: cancel
// ---------------------------------------------------------------------------

TEST(GameJobSchedulerTest, CancelPreventsExecution) {
    GameJobScheduler scheduler(1);
    std::atomic<bool> barrier{false};
    std::atomic<bool> targetExecuted{false};

    // Block the single worker with a long job
    auto blocker = scheduler.schedule([&] {
        while (!barrier.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(1ms);
        }
    });
    ASSERT_TRUE(blocker.hasValue());

    // Schedule target while worker is blocked
    auto target = scheduler.schedule([&] {
        targetExecuted.store(true);
    });
    ASSERT_TRUE(target.hasValue());

    // Cancel the pending target
    auto cancelResult = scheduler.cancel(target.value());
    EXPECT_TRUE(cancelResult.hasValue());

    // Release blocker
    barrier.store(true, std::memory_order_release);
    scheduler.wait(blocker.value());

    // Wait a bit for target to potentially execute
    scheduler.wait(target.value());

    EXPECT_FALSE(targetExecuted.load());
}

TEST(GameJobSchedulerTest, CancelCompletedJobReturnsError) {
    GameJobScheduler scheduler(2);

    auto result = scheduler.schedule([] {});
    ASSERT_TRUE(result.hasValue());
    scheduler.wait(result.value());

    auto cancelResult = scheduler.cancel(result.value());
    ASSERT_TRUE(cancelResult.hasError());
    EXPECT_EQ(cancelResult.error().code(), ErrorCode::JobCancelled);
}

TEST(GameJobSchedulerTest, CancelNonExistentJobReturnsError) {
    GameJobScheduler scheduler(2);
    auto result = scheduler.cancel(99999);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::JobNotFound);
}

TEST(GameJobSchedulerTest, CancelTickJob) {
    GameJobScheduler scheduler(2);
    std::atomic<int> ticks{0};

    auto result = scheduler.scheduleTick(20ms, [&] { ticks.fetch_add(1); });
    ASSERT_TRUE(result.hasValue());

    auto cancelResult = scheduler.cancel(result.value());
    EXPECT_TRUE(cancelResult.hasValue());

    // Process ticks — cancelled tick should not fire
    scheduler.processTick(20ms);
    scheduler.processTick(20ms);
    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(ticks.load(), 0);
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

TEST(GameJobSchedulerTest, ErrorCodesAreInThreadSubsystem) {
    GameError err(ErrorCode::JobScheduleFailed, "test");
    EXPECT_EQ(err.subsystem(), "Thread");
}

TEST(GameJobSchedulerTest, ScheduleAfterReturnsCorrectErrorCode) {
    GameJobScheduler scheduler(2);
    auto result = scheduler.scheduleAfter(0, [] {});
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::JobNotFound);
    EXPECT_EQ(result.error().subsystem(), "Thread");
}
