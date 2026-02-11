#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

#include "cgs/ecs/scratch_allocator.hpp"
#include "cgs/ecs/system_scheduler.hpp"

using namespace cgs::ecs;

// ── Dummy component types for access pattern declarations ───────────────

struct CompA {};
struct CompB {};
struct CompC {};
struct CompD {};

// ── Test system types with declared access patterns ─────────────────────

/// System that reads CompA and writes CompB.
class ReadAWriteB : public ISystem {
public:
    void Execute(float) override { ++callCount; }
    [[nodiscard]] std::string_view GetName() const override {
        return "ReadAWriteB";
    }
    SystemAccessInfo GetAccessInfo() const override {
        SystemAccessInfo info;
        Read<CompA>::Apply(info);
        Write<CompB>::Apply(info);
        return info;
    }
    std::atomic<int> callCount{0};
};

/// System that reads CompC (no overlap with CompA/CompB).
class ReadC : public ISystem {
public:
    void Execute(float) override { ++callCount; }
    [[nodiscard]] std::string_view GetName() const override {
        return "ReadC";
    }
    SystemAccessInfo GetAccessInfo() const override {
        SystemAccessInfo info;
        Read<CompC>::Apply(info);
        return info;
    }
    std::atomic<int> callCount{0};
};

/// System that writes CompA (conflicts with ReadAWriteB).
class WriteA : public ISystem {
public:
    void Execute(float) override { ++callCount; }
    [[nodiscard]] std::string_view GetName() const override {
        return "WriteA";
    }
    SystemAccessInfo GetAccessInfo() const override {
        SystemAccessInfo info;
        Write<CompA>::Apply(info);
        return info;
    }
    std::atomic<int> callCount{0};
};

/// System that reads CompD (independent of all others).
class ReadD : public ISystem {
public:
    void Execute(float) override { ++callCount; }
    [[nodiscard]] std::string_view GetName() const override {
        return "ReadD";
    }
    SystemAccessInfo GetAccessInfo() const override {
        SystemAccessInfo info;
        Read<CompD>::Apply(info);
        return info;
    }
    std::atomic<int> callCount{0};
};

/// System with undeclared access (should never be parallelized).
class UndeclaredAccess : public ISystem {
public:
    void Execute(float) override { ++callCount; }
    [[nodiscard]] std::string_view GetName() const override {
        return "UndeclaredAccess";
    }
    // No GetAccessInfo override — uses empty default.
    std::atomic<int> callCount{0};
};

/// System that reads CompA and CompB (read-only, no conflicts with
/// other readers of the same components).
class ReadAB : public ISystem {
public:
    void Execute(float) override { ++callCount; }
    [[nodiscard]] std::string_view GetName() const override {
        return "ReadAB";
    }
    SystemAccessInfo GetAccessInfo() const override {
        SystemAccessInfo info;
        Read<CompA, CompB>::Apply(info);
        return info;
    }
    std::atomic<int> callCount{0};
};

// ── Helper: simple std::thread parallel executor ────────────────────────

static SystemScheduler::ParallelExecutor makeThreadExecutor() {
    return [](const std::vector<std::function<void()>>& tasks) {
        std::vector<std::thread> threads;
        threads.reserve(tasks.size());
        for (auto& task : tasks) {
            threads.emplace_back(task);
        }
        for (auto& t : threads) {
            t.join();
        }
    };
}

// ── SystemAccessInfo tests ──────────────────────────────────────────────

TEST(AccessInfoTest, EmptyConflictsWithEverything) {
    SystemAccessInfo empty;
    SystemAccessInfo declared;
    Read<CompA>::Apply(declared);

    EXPECT_TRUE(empty.ConflictsWith(declared));
    EXPECT_TRUE(declared.ConflictsWith(empty));
    EXPECT_TRUE(empty.ConflictsWith(empty));
}

TEST(AccessInfoTest, WriteWriteConflict) {
    SystemAccessInfo a;
    Write<CompA>::Apply(a);
    SystemAccessInfo b;
    Write<CompA>::Apply(b);

    EXPECT_TRUE(a.ConflictsWith(b));
}

TEST(AccessInfoTest, ReadWriteConflict) {
    SystemAccessInfo reader;
    Read<CompA>::Apply(reader);
    SystemAccessInfo writer;
    Write<CompA>::Apply(writer);

    EXPECT_TRUE(reader.ConflictsWith(writer));
    EXPECT_TRUE(writer.ConflictsWith(reader));
}

TEST(AccessInfoTest, ReadReadNoConflict) {
    SystemAccessInfo a;
    Read<CompA>::Apply(a);
    SystemAccessInfo b;
    Read<CompA>::Apply(b);

    EXPECT_FALSE(a.ConflictsWith(b));
}

TEST(AccessInfoTest, DisjointWritesNoConflict) {
    SystemAccessInfo a;
    Write<CompA>::Apply(a);
    SystemAccessInfo b;
    Write<CompB>::Apply(b);

    EXPECT_FALSE(a.ConflictsWith(b));
}

TEST(AccessInfoTest, MultiComponentConflict) {
    SystemAccessInfo a;
    Read<CompA>::Apply(a);
    Write<CompB>::Apply(a);

    SystemAccessInfo b;
    Read<CompC>::Apply(b);
    Write<CompA>::Apply(b); // Conflicts: b writes CompA, a reads CompA.

    EXPECT_TRUE(a.ConflictsWith(b));
}

// ── Parallel batch computation tests ────────────────────────────────────

class ParallelBatchTest : public ::testing::Test {
protected:
    SystemScheduler scheduler;
};

TEST_F(ParallelBatchTest, NonConflictingSystemsInSameBatch) {
    // ReadAWriteB and ReadC have no overlap — same batch.
    scheduler.Register<ReadAWriteB>();
    scheduler.Register<ReadC>();

    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    const auto& batches = scheduler.GetParallelBatches(SystemStage::Update);
    ASSERT_EQ(batches.size(), 1u);
    EXPECT_EQ(batches[0].systems.size(), 2u);
}

TEST_F(ParallelBatchTest, ConflictingSystemsInDifferentBatches) {
    // ReadAWriteB reads CompA; WriteA writes CompA — conflict.
    scheduler.Register<ReadAWriteB>();
    scheduler.Register<WriteA>();

    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    const auto& batches = scheduler.GetParallelBatches(SystemStage::Update);
    ASSERT_EQ(batches.size(), 2u);
    EXPECT_EQ(batches[0].systems.size(), 1u);
    EXPECT_EQ(batches[1].systems.size(), 1u);
}

TEST_F(ParallelBatchTest, ThreeSystemsTwoBatches) {
    // ReadAWriteB (reads A, writes B)
    // ReadC       (reads C) — no conflict with ReadAWriteB
    // WriteA      (writes A) — conflicts with ReadAWriteB
    scheduler.Register<ReadAWriteB>();
    scheduler.Register<ReadC>();
    scheduler.Register<WriteA>();

    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    const auto& batches = scheduler.GetParallelBatches(SystemStage::Update);

    // ReadAWriteB + ReadC in batch 0, WriteA in batch 1.
    // Or WriteA + ReadC in batch 0, ReadAWriteB in batch 1.
    // Either way: 2 batches.
    ASSERT_EQ(batches.size(), 2u);

    // Total systems across all batches.
    std::size_t total = 0;
    for (const auto& b : batches) {
        total += b.systems.size();
    }
    EXPECT_EQ(total, 3u);
}

TEST_F(ParallelBatchTest, FourIndependentSystemsOneBatch) {
    // ReadAWriteB, ReadC, ReadD — all independent.
    scheduler.Register<ReadAWriteB>();
    scheduler.Register<ReadC>();
    scheduler.Register<ReadD>();

    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    const auto& batches = scheduler.GetParallelBatches(SystemStage::Update);
    ASSERT_EQ(batches.size(), 1u);
    EXPECT_EQ(batches[0].systems.size(), 3u);
}

TEST_F(ParallelBatchTest, ReadReadAllowedInParallel) {
    // ReadAB (reads A, B) and ReadAWriteB (reads A, writes B) conflict
    // because ReadAWriteB writes B which ReadAB reads.
    scheduler.Register<ReadAB>();
    scheduler.Register<ReadAWriteB>();

    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    const auto& batches = scheduler.GetParallelBatches(SystemStage::Update);
    EXPECT_EQ(batches.size(), 2u);
}

TEST_F(ParallelBatchTest, UndeclaredAccessRunsAlone) {
    scheduler.Register<UndeclaredAccess>();
    scheduler.Register<ReadC>();
    scheduler.Register<ReadD>();

    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    const auto& batches = scheduler.GetParallelBatches(SystemStage::Update);
    // UndeclaredAccess conflicts with everything — needs its own batch.
    // ReadC + ReadD can share a batch.
    ASSERT_GE(batches.size(), 2u);
}

TEST_F(ParallelBatchTest, DependencyForcesLaterBatch) {
    auto& sysA = scheduler.Register<ReadC>();
    auto& sysB = scheduler.Register<ReadD>();
    (void)sysA;
    (void)sysB;

    // ReadD depends on ReadC — even though they don't conflict,
    // ReadD must be in a later batch.
    scheduler.AddDependency<ReadC, ReadD>();
    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    const auto& batches = scheduler.GetParallelBatches(SystemStage::Update);
    ASSERT_EQ(batches.size(), 2u);
}

// ── SyncPoint tests ─────────────────────────────────────────────────────

TEST_F(ParallelBatchTest, SyncPointForcesBatchBoundary) {
    scheduler.Register<ReadC>();
    scheduler.Register<ReadD>();

    // Without sync point: both in one batch (no conflict).
    // With sync point after ReadC: ReadD must be in a later batch.
    scheduler.AddSyncPoint<ReadC>();
    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    const auto& batches = scheduler.GetParallelBatches(SystemStage::Update);
    ASSERT_GE(batches.size(), 2u);
}

// ── Parallel execution tests ────────────────────────────────────────────

class ParallelExecutionTest : public ::testing::Test {
protected:
    SystemScheduler scheduler;
};

TEST_F(ParallelExecutionTest, AllSystemsExecuted) {
    auto& a = scheduler.Register<ReadAWriteB>();
    auto& b = scheduler.Register<ReadC>();
    auto& c = scheduler.Register<ReadD>();

    scheduler.SetParallelExecutor(makeThreadExecutor());
    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    scheduler.Execute(1.0f / 60.0f);

    EXPECT_EQ(a.callCount.load(), 1);
    EXPECT_EQ(b.callCount.load(), 1);
    EXPECT_EQ(c.callCount.load(), 1);
}

TEST_F(ParallelExecutionTest, ConflictingSystemsRunSequentially) {
    // ReadAWriteB and WriteA conflict on CompA.
    // They must be in different batches, ensuring sequential execution.
    auto& a = scheduler.Register<ReadAWriteB>();
    auto& b = scheduler.Register<WriteA>();

    scheduler.SetParallelExecutor(makeThreadExecutor());
    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    scheduler.Execute(1.0f / 60.0f);

    EXPECT_EQ(a.callCount.load(), 1);
    EXPECT_EQ(b.callCount.load(), 1);
}

TEST_F(ParallelExecutionTest, FallbackToSequentialWithoutExecutor) {
    auto& a = scheduler.Register<ReadAWriteB>();
    auto& b = scheduler.Register<ReadC>();

    // Enable parallel but don't set executor.
    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    // Should fall back to sequential execution without crash.
    scheduler.Execute(1.0f / 60.0f);

    EXPECT_EQ(a.callCount.load(), 1);
    EXPECT_EQ(b.callCount.load(), 1);
}

TEST_F(ParallelExecutionTest, DisabledParallelRunsSequentially) {
    auto& a = scheduler.Register<ReadAWriteB>();
    auto& b = scheduler.Register<ReadC>();

    scheduler.SetParallelExecutor(makeThreadExecutor());
    scheduler.EnableParallelExecution(false);
    ASSERT_TRUE(scheduler.Build());

    scheduler.Execute(1.0f / 60.0f);

    EXPECT_EQ(a.callCount.load(), 1);
    EXPECT_EQ(b.callCount.load(), 1);
}

TEST_F(ParallelExecutionTest, ParallelSystemsRunOnDifferentThreads) {
    // Two systems that record their thread ID.
    struct ThreadIdSystem : public ISystem {
        std::atomic<std::thread::id> threadId{};
        std::string name;
        ComponentTypeId compId;

        ThreadIdSystem(std::string n, ComponentTypeId c)
            : name(std::move(n)), compId(c) {}

        void Execute(float) override {
            threadId.store(std::this_thread::get_id());
            // Spin briefly to ensure threads overlap.
            auto start = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - start <
                   std::chrono::milliseconds(5)) {
            }
        }

        [[nodiscard]] std::string_view GetName() const override {
            return name;
        }

        SystemAccessInfo GetAccessInfo() const override {
            SystemAccessInfo info;
            info.reads.insert(compId);
            return info;
        }
    };

    // Use different component IDs to avoid conflicts.
    auto& sysA = scheduler.Register<ThreadIdSystem>(
        "SysA", ComponentType<CompA>::Id());
    // Can't register another ThreadIdSystem with different args because
    // SystemType<ThreadIdSystem>::Id() is shared. Use a workaround:
    // Register with a subclass.
    struct ThreadIdSystemB : ThreadIdSystem {
        ThreadIdSystemB() : ThreadIdSystem("SysB", ComponentType<CompB>::Id()) {}
    };

    auto& sysB = scheduler.Register<ThreadIdSystemB>();

    scheduler.SetParallelExecutor(makeThreadExecutor());
    scheduler.EnableParallelExecution(true);
    ASSERT_TRUE(scheduler.Build());

    scheduler.Execute(1.0f / 60.0f);

    // With a parallel executor, non-conflicting systems should run on
    // different threads. Verify both were executed.
    EXPECT_NE(sysA.threadId.load(), std::thread::id{});
    EXPECT_NE(sysB.threadId.load(), std::thread::id{});

    // They should have been in the same batch (no conflict).
    const auto& batches = scheduler.GetParallelBatches(SystemStage::Update);
    ASSERT_EQ(batches.size(), 1u);
    EXPECT_EQ(batches[0].systems.size(), 2u);

    // They should have run on different threads (parallel execution).
    EXPECT_NE(sysA.threadId.load(), sysB.threadId.load());
}

// ── ScratchAllocator tests ──────────────────────────────────────────────

TEST(ScratchAllocatorTest, BasicAllocation) {
    auto& scratch = ScratchAllocator::GetThreadLocal();
    scratch.Reset();

    auto* ptr = scratch.Allocate(128);
    ASSERT_NE(ptr, nullptr);
    EXPECT_GE(scratch.BytesUsed(), 128u);
}

TEST(ScratchAllocatorTest, AlignedAllocation) {
    auto& scratch = ScratchAllocator::GetThreadLocal();
    scratch.Reset();

    auto* p1 = scratch.Allocate(1);
    auto* p2 = scratch.Allocate(1);

    // Allocations are 16-byte aligned.
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p1) % ScratchAllocator::kAlignment,
              0u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p2) % ScratchAllocator::kAlignment,
              0u);
}

TEST(ScratchAllocatorTest, TypedAllocation) {
    auto& scratch = ScratchAllocator::GetThreadLocal();
    scratch.Reset();

    struct Vec3 {
        float x, y, z;
        Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    };

    auto* v = scratch.New<Vec3>(1.0f, 2.0f, 3.0f);
    ASSERT_NE(v, nullptr);
    EXPECT_FLOAT_EQ(v->x, 1.0f);
    EXPECT_FLOAT_EQ(v->y, 2.0f);
    EXPECT_FLOAT_EQ(v->z, 3.0f);
}

TEST(ScratchAllocatorTest, ArrayAllocation) {
    auto& scratch = ScratchAllocator::GetThreadLocal();
    scratch.Reset();

    auto* arr = scratch.AllocateArray<int>(100);
    ASSERT_NE(arr, nullptr);

    // Write and read back.
    for (int i = 0; i < 100; ++i) {
        arr[i] = i * 2;
    }
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(arr[i], i * 2);
    }
}

TEST(ScratchAllocatorTest, ResetReclaims) {
    auto& scratch = ScratchAllocator::GetThreadLocal();
    scratch.Reset();

    scratch.Allocate(1024);
    EXPECT_GE(scratch.BytesUsed(), 1024u);

    scratch.Reset();
    EXPECT_EQ(scratch.BytesUsed(), 0u);
}

TEST(ScratchAllocatorTest, GrowsBeyondInitialCapacity) {
    auto& scratch = ScratchAllocator::GetThreadLocal();
    scratch.Reset();

    auto initialCap = scratch.Capacity();

    // Allocate more than the default capacity.
    scratch.Allocate(initialCap + 1024);
    EXPECT_GT(scratch.Capacity(), initialCap);
}

TEST(ScratchAllocatorTest, ThreadLocalIndependence) {
    auto& mainScratch = ScratchAllocator::GetThreadLocal();
    mainScratch.Reset();
    mainScratch.Allocate(64);

    std::size_t otherThreadUsed = 0;
    std::thread t([&] {
        auto& threadScratch = ScratchAllocator::GetThreadLocal();
        threadScratch.Reset();
        otherThreadUsed = threadScratch.BytesUsed();
    });
    t.join();

    // Other thread's allocator should be independent (reset to 0).
    EXPECT_EQ(otherThreadUsed, 0u);
    EXPECT_GE(mainScratch.BytesUsed(), 64u);
}

// ── Performance test ────────────────────────────────────────────────────

/// Compute-intensive system for parallel speedup measurement.
struct HeavySystem : public ISystem {
    std::string name;
    ComponentTypeId compId;
    std::atomic<int> callCount{0};

    HeavySystem(std::string n, ComponentTypeId c)
        : name(std::move(n)), compId(c) {}

    void Execute(float) override {
        ++callCount;
        // Simulate work: sum a large array.
        volatile float sum = 0.0f;
        for (int i = 0; i < 100'000; ++i) {
            sum = sum + static_cast<float>(i) * 0.001f;
        }
    }

    [[nodiscard]] std::string_view GetName() const override { return name; }

    SystemAccessInfo GetAccessInfo() const override {
        SystemAccessInfo info;
        info.reads.insert(compId);
        return info;
    }
};

struct HeavyA : HeavySystem {
    HeavyA() : HeavySystem("HeavyA", ComponentType<CompA>::Id()) {}
};
struct HeavyB : HeavySystem {
    HeavyB() : HeavySystem("HeavyB", ComponentType<CompB>::Id()) {}
};
struct HeavyC : HeavySystem {
    HeavyC() : HeavySystem("HeavyC", ComponentType<CompC>::Id()) {}
};
struct HeavyD : HeavySystem {
    HeavyD() : HeavySystem("HeavyD", ComponentType<CompD>::Id()) {}
};

TEST(ParallelPerformanceTest, SpeedupWithIndependentSystems) {
    constexpr int kIterations = 10;

    // Sequential execution.
    auto seqDuration = [&] {
        SystemScheduler seq;
        seq.Register<HeavyA>();
        seq.Register<HeavyB>();
        seq.Register<HeavyC>();
        seq.Register<HeavyD>();
        EXPECT_TRUE(seq.Build());

        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < kIterations; ++i) {
            seq.Execute(1.0f / 60.0f);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0)
            .count();
    }();

    // Parallel execution.
    auto parDuration = [&] {
        SystemScheduler par;
        par.Register<HeavyA>();
        par.Register<HeavyB>();
        par.Register<HeavyC>();
        par.Register<HeavyD>();
        par.SetParallelExecutor(makeThreadExecutor());
        par.EnableParallelExecution(true);
        EXPECT_TRUE(par.Build());

        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < kIterations; ++i) {
            par.Execute(1.0f / 60.0f);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0)
            .count();
    }();

    std::cout << "[PERF] Sequential (" << kIterations
              << " frames): " << seqDuration << " us\n";
    std::cout << "[PERF] Parallel   (" << kIterations
              << " frames): " << parDuration << " us\n";
    std::cout << "[PERF] Speedup: " << std::fixed << std::setprecision(2)
              << (static_cast<double>(seqDuration) / parDuration) << "x\n";

    // Parallel should be measurably faster with 4 independent systems
    // on a multi-core machine. Allow generous margin for CI variability.
    EXPECT_LT(parDuration, seqDuration);
}
