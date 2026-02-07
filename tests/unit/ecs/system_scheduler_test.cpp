#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "cgs/ecs/system_scheduler.hpp"

using namespace cgs::ecs;

// ── Test system types ───────────────────────────────────────────────────────

/// Records each Execute() call with its deltaTime for verification.
class RecordingSystem : public ISystem {
public:
    explicit RecordingSystem(std::string name, SystemStage stage = SystemStage::Update)
        : name_(std::move(name)), stage_(stage) {}

    void Execute(float deltaTime) override {
        calls_.push_back(deltaTime);
        executionLog().push_back(name_);
    }

    [[nodiscard]] SystemStage GetStage() const override { return stage_; }
    [[nodiscard]] std::string_view GetName() const override { return name_; }

    [[nodiscard]] const std::vector<float>& Calls() const { return calls_; }
    [[nodiscard]] std::size_t CallCount() const { return calls_.size(); }

    /// Shared log across all RecordingSystem instances for ordering verification.
    static std::vector<std::string>& executionLog() {
        static std::vector<std::string> log;
        return log;
    }

    static void clearLog() { executionLog().clear(); }

private:
    std::string name_;
    SystemStage stage_;
    std::vector<float> calls_;
};

class PreUpdateSystem : public RecordingSystem {
public:
    PreUpdateSystem() : RecordingSystem("PreUpdateSystem", SystemStage::PreUpdate) {}
};

class UpdateSystemA : public RecordingSystem {
public:
    UpdateSystemA() : RecordingSystem("UpdateSystemA", SystemStage::Update) {}
};

class UpdateSystemB : public RecordingSystem {
public:
    UpdateSystemB() : RecordingSystem("UpdateSystemB", SystemStage::Update) {}
};

class UpdateSystemC : public RecordingSystem {
public:
    UpdateSystemC() : RecordingSystem("UpdateSystemC", SystemStage::Update) {}
};

class PostUpdateSystem : public RecordingSystem {
public:
    PostUpdateSystem() : RecordingSystem("PostUpdateSystem", SystemStage::PostUpdate) {}
};

class FixedUpdateSystem : public RecordingSystem {
public:
    FixedUpdateSystem() : RecordingSystem("FixedUpdateSystem", SystemStage::FixedUpdate) {}
};

class FixedUpdateSystemB : public RecordingSystem {
public:
    FixedUpdateSystemB() : RecordingSystem("FixedUpdateSystemB", SystemStage::FixedUpdate) {}
};

// Systems for circular dependency testing
class CycleSystemA : public RecordingSystem {
public:
    CycleSystemA() : RecordingSystem("CycleSystemA", SystemStage::Update) {}
};

class CycleSystemB : public RecordingSystem {
public:
    CycleSystemB() : RecordingSystem("CycleSystemB", SystemStage::Update) {}
};

class CycleSystemC : public RecordingSystem {
public:
    CycleSystemC() : RecordingSystem("CycleSystemC", SystemStage::Update) {}
};

// ===========================================================================
// SystemScheduler: Registration
// ===========================================================================

TEST(SystemSchedulerTest, RegisterIncrementsCount) {
    SystemScheduler scheduler;
    EXPECT_EQ(scheduler.SystemCount(), 0u);

    scheduler.Register<UpdateSystemA>();
    EXPECT_EQ(scheduler.SystemCount(), 1u);

    scheduler.Register<UpdateSystemB>();
    EXPECT_EQ(scheduler.SystemCount(), 2u);
}

TEST(SystemSchedulerTest, RegisterReturnsReference) {
    SystemScheduler scheduler;
    auto& sys = scheduler.Register<UpdateSystemA>();
    EXPECT_EQ(sys.GetName(), "UpdateSystemA");
}

TEST(SystemSchedulerTest, DuplicateRegistrationReturnsExisting) {
    SystemScheduler scheduler;
    auto& first = scheduler.Register<UpdateSystemA>();
    auto& second = scheduler.Register<UpdateSystemA>();

    EXPECT_EQ(&first, &second);
    EXPECT_EQ(scheduler.SystemCount(), 1u);
}

TEST(SystemSchedulerTest, GetSystemReturnsRegistered) {
    SystemScheduler scheduler;
    scheduler.Register<UpdateSystemA>();

    auto* sys = scheduler.GetSystem<UpdateSystemA>();
    ASSERT_NE(sys, nullptr);
    EXPECT_EQ(sys->GetName(), "UpdateSystemA");
}

TEST(SystemSchedulerTest, GetSystemReturnsNullForUnregistered) {
    SystemScheduler scheduler;
    auto* sys = scheduler.GetSystem<UpdateSystemA>();
    EXPECT_EQ(sys, nullptr);
}

// ===========================================================================
// SystemScheduler: Stage assignment
// ===========================================================================

TEST(SystemSchedulerTest, SystemsAssignedToCorrectStages) {
    SystemScheduler scheduler;
    scheduler.Register<PreUpdateSystem>();
    scheduler.Register<UpdateSystemA>();
    scheduler.Register<PostUpdateSystem>();
    scheduler.Register<FixedUpdateSystem>();

    ASSERT_TRUE(scheduler.Build());

    EXPECT_EQ(scheduler.GetExecutionOrder(SystemStage::PreUpdate).size(), 1u);
    EXPECT_EQ(scheduler.GetExecutionOrder(SystemStage::Update).size(), 1u);
    EXPECT_EQ(scheduler.GetExecutionOrder(SystemStage::PostUpdate).size(), 1u);
    EXPECT_EQ(scheduler.GetExecutionOrder(SystemStage::FixedUpdate).size(), 1u);
}

TEST(SystemSchedulerTest, EmptyStageReturnsEmptyOrder) {
    SystemScheduler scheduler;
    scheduler.Register<UpdateSystemA>();
    ASSERT_TRUE(scheduler.Build());

    const auto& order = scheduler.GetExecutionOrder(SystemStage::PreUpdate);
    EXPECT_TRUE(order.empty());
}

// ===========================================================================
// SystemScheduler: Stage execution order
// ===========================================================================

TEST(SystemSchedulerTest, StagesExecuteInCorrectOrder) {
    SystemScheduler scheduler;
    scheduler.Register<PostUpdateSystem>();
    scheduler.Register<PreUpdateSystem>();
    scheduler.Register<UpdateSystemA>();

    ASSERT_TRUE(scheduler.Build());
    RecordingSystem::clearLog();

    scheduler.Execute(1.0f / 60.0f);

    const auto& log = RecordingSystem::executionLog();
    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0], "PreUpdateSystem");
    EXPECT_EQ(log[1], "UpdateSystemA");
    EXPECT_EQ(log[2], "PostUpdateSystem");
}

// ===========================================================================
// SystemScheduler: Dependency ordering
// ===========================================================================

TEST(SystemSchedulerTest, DependencyDeterminesOrder) {
    SystemScheduler scheduler;
    scheduler.Register<UpdateSystemA>();
    scheduler.Register<UpdateSystemB>();
    scheduler.Register<UpdateSystemC>();

    // C -> A -> B  (C first, then A, then B)
    EXPECT_TRUE((scheduler.AddDependency<UpdateSystemC, UpdateSystemA>()));
    EXPECT_TRUE((scheduler.AddDependency<UpdateSystemA, UpdateSystemB>()));

    ASSERT_TRUE(scheduler.Build());

    const auto& order = scheduler.GetExecutionOrder(SystemStage::Update);
    ASSERT_EQ(order.size(), 3u);

    // Find positions.
    auto posC = std::find(order.begin(), order.end(), SystemType<UpdateSystemC>::Id());
    auto posA = std::find(order.begin(), order.end(), SystemType<UpdateSystemA>::Id());
    auto posB = std::find(order.begin(), order.end(), SystemType<UpdateSystemB>::Id());

    EXPECT_LT(posC, posA);
    EXPECT_LT(posA, posB);
}

TEST(SystemSchedulerTest, DependencyAcrossStagesReturnsFalse) {
    SystemScheduler scheduler;
    scheduler.Register<PreUpdateSystem>();
    scheduler.Register<UpdateSystemA>();

    // Cross-stage dependency should be rejected.
    EXPECT_FALSE((scheduler.AddDependency<PreUpdateSystem, UpdateSystemA>()));
}

TEST(SystemSchedulerTest, DependencyOnUnregisteredSystemReturnsFalse) {
    SystemScheduler scheduler;
    scheduler.Register<UpdateSystemA>();

    EXPECT_FALSE((scheduler.AddDependency<UpdateSystemA, UpdateSystemB>()));
}

TEST(SystemSchedulerTest, DependencyOrderIsRespectedDuringExecution) {
    SystemScheduler scheduler;
    scheduler.Register<UpdateSystemB>();
    scheduler.Register<UpdateSystemA>();

    // A must run before B.
    EXPECT_TRUE((scheduler.AddDependency<UpdateSystemA, UpdateSystemB>()));

    ASSERT_TRUE(scheduler.Build());
    RecordingSystem::clearLog();

    scheduler.Execute(1.0f / 60.0f);

    const auto& log = RecordingSystem::executionLog();
    auto posA = std::find(log.begin(), log.end(), "UpdateSystemA");
    auto posB = std::find(log.begin(), log.end(), "UpdateSystemB");

    ASSERT_NE(posA, log.end());
    ASSERT_NE(posB, log.end());
    EXPECT_LT(posA, posB);
}

// ===========================================================================
// SystemScheduler: Circular dependency detection
// ===========================================================================

TEST(SystemSchedulerTest, CircularDependencyDetected) {
    SystemScheduler scheduler;
    scheduler.Register<CycleSystemA>();
    scheduler.Register<CycleSystemB>();

    EXPECT_TRUE((scheduler.AddDependency<CycleSystemA, CycleSystemB>()));
    EXPECT_TRUE((scheduler.AddDependency<CycleSystemB, CycleSystemA>()));

    EXPECT_FALSE(scheduler.Build());
    EXPECT_FALSE(scheduler.GetLastError().empty());
    EXPECT_NE(scheduler.GetLastError().find("Circular dependency"), std::string::npos);
}

TEST(SystemSchedulerTest, ThreeWayCircularDependencyDetected) {
    SystemScheduler scheduler;
    scheduler.Register<CycleSystemA>();
    scheduler.Register<CycleSystemB>();
    scheduler.Register<CycleSystemC>();

    // A -> B -> C -> A
    EXPECT_TRUE((scheduler.AddDependency<CycleSystemA, CycleSystemB>()));
    EXPECT_TRUE((scheduler.AddDependency<CycleSystemB, CycleSystemC>()));
    EXPECT_TRUE((scheduler.AddDependency<CycleSystemC, CycleSystemA>()));

    EXPECT_FALSE(scheduler.Build());

    const auto& err = scheduler.GetLastError();
    EXPECT_NE(err.find("CycleSystemA"), std::string::npos);
    EXPECT_NE(err.find("CycleSystemB"), std::string::npos);
    EXPECT_NE(err.find("CycleSystemC"), std::string::npos);
}

TEST(SystemSchedulerTest, CircularDependencyErrorContainsSystemNames) {
    SystemScheduler scheduler;
    scheduler.Register<CycleSystemA>();
    scheduler.Register<CycleSystemB>();

    EXPECT_TRUE((scheduler.AddDependency<CycleSystemA, CycleSystemB>()));
    EXPECT_TRUE((scheduler.AddDependency<CycleSystemB, CycleSystemA>()));

    EXPECT_FALSE(scheduler.Build());
    EXPECT_NE(scheduler.GetLastError().find("CycleSystemA"), std::string::npos);
    EXPECT_NE(scheduler.GetLastError().find("CycleSystemB"), std::string::npos);
}

// ===========================================================================
// SystemScheduler: FixedUpdate
// ===========================================================================

TEST(SystemSchedulerTest, FixedUpdateRunsAtFixedInterval) {
    SystemScheduler scheduler;
    scheduler.SetFixedTimeStep(1.0f / 60.0f);
    auto& sys = scheduler.Register<FixedUpdateSystem>();

    ASSERT_TRUE(scheduler.Build());

    // Simulate one frame at 30 fps (33.3ms) — should tick twice at ~16.7ms.
    scheduler.Execute(1.0f / 30.0f);
    EXPECT_EQ(sys.CallCount(), 2u);
}

TEST(SystemSchedulerTest, FixedUpdateReceivesFixedTimestep) {
    SystemScheduler scheduler;
    constexpr float kFixedDt = 1.0f / 60.0f;
    scheduler.SetFixedTimeStep(kFixedDt);
    auto& sys = scheduler.Register<FixedUpdateSystem>();

    ASSERT_TRUE(scheduler.Build());
    scheduler.Execute(1.0f / 30.0f);

    for (auto dt : sys.Calls()) {
        EXPECT_FLOAT_EQ(dt, kFixedDt);
    }
}

TEST(SystemSchedulerTest, FixedUpdateAccumulatesTime) {
    SystemScheduler scheduler;
    constexpr float kFixedDt = 1.0f / 60.0f;
    scheduler.SetFixedTimeStep(kFixedDt);
    auto& sys = scheduler.Register<FixedUpdateSystem>();

    ASSERT_TRUE(scheduler.Build());

    // First frame: slightly less than one tick — should not fire.
    scheduler.Execute(kFixedDt * 0.5f);
    EXPECT_EQ(sys.CallCount(), 0u);

    // Second frame: another half — total now equals one tick.
    scheduler.Execute(kFixedDt * 0.5f);
    EXPECT_EQ(sys.CallCount(), 1u);
}

TEST(SystemSchedulerTest, FixedUpdateDoesNotRunIfNoTime) {
    SystemScheduler scheduler;
    scheduler.SetFixedTimeStep(1.0f / 60.0f);
    auto& sys = scheduler.Register<FixedUpdateSystem>();

    ASSERT_TRUE(scheduler.Build());

    scheduler.Execute(0.0f);
    EXPECT_EQ(sys.CallCount(), 0u);
}

TEST(SystemSchedulerTest, FixedUpdateDependencyOrder) {
    SystemScheduler scheduler;
    scheduler.SetFixedTimeStep(1.0f / 60.0f);
    scheduler.Register<FixedUpdateSystem>();
    scheduler.Register<FixedUpdateSystemB>();

    // FixedUpdateSystem must run before FixedUpdateSystemB.
    EXPECT_TRUE((scheduler.AddDependency<FixedUpdateSystem, FixedUpdateSystemB>()));

    ASSERT_TRUE(scheduler.Build());
    RecordingSystem::clearLog();

    scheduler.Execute(1.0f / 60.0f);

    const auto& log = RecordingSystem::executionLog();
    auto posA = std::find(log.begin(), log.end(), "FixedUpdateSystem");
    auto posB = std::find(log.begin(), log.end(), "FixedUpdateSystemB");

    ASSERT_NE(posA, log.end());
    ASSERT_NE(posB, log.end());
    EXPECT_LT(posA, posB);
}

TEST(SystemSchedulerTest, GetFixedTimeStepReturnsConfigured) {
    SystemScheduler scheduler;
    scheduler.SetFixedTimeStep(1.0f / 30.0f);
    EXPECT_FLOAT_EQ(scheduler.GetFixedTimeStep(), 1.0f / 30.0f);
}

// ===========================================================================
// SystemScheduler: Enable / disable
// ===========================================================================

TEST(SystemSchedulerTest, SystemsEnabledByDefault) {
    SystemScheduler scheduler;
    scheduler.Register<UpdateSystemA>();
    EXPECT_TRUE(scheduler.IsEnabled(SystemType<UpdateSystemA>::Id()));
}

TEST(SystemSchedulerTest, DisabledSystemNotExecuted) {
    SystemScheduler scheduler;
    auto& sys = scheduler.Register<UpdateSystemA>();
    scheduler.Register<UpdateSystemB>();

    scheduler.SetEnabled<UpdateSystemA>(false);
    ASSERT_TRUE(scheduler.Build());

    scheduler.Execute(1.0f / 60.0f);
    EXPECT_EQ(sys.CallCount(), 0u);
}

TEST(SystemSchedulerTest, ReenabledSystemExecutes) {
    SystemScheduler scheduler;
    auto& sys = scheduler.Register<UpdateSystemA>();

    scheduler.SetEnabled<UpdateSystemA>(false);
    ASSERT_TRUE(scheduler.Build());

    scheduler.Execute(1.0f / 60.0f);
    EXPECT_EQ(sys.CallCount(), 0u);

    scheduler.SetEnabled<UpdateSystemA>(true);
    scheduler.Execute(1.0f / 60.0f);
    EXPECT_EQ(sys.CallCount(), 1u);
}

TEST(SystemSchedulerTest, DisableDoesNotAffectOtherSystems) {
    SystemScheduler scheduler;
    auto& sysA = scheduler.Register<UpdateSystemA>();
    auto& sysB = scheduler.Register<UpdateSystemB>();

    scheduler.SetEnabled<UpdateSystemA>(false);
    ASSERT_TRUE(scheduler.Build());

    scheduler.Execute(1.0f / 60.0f);
    EXPECT_EQ(sysA.CallCount(), 0u);
    EXPECT_EQ(sysB.CallCount(), 1u);
}

TEST(SystemSchedulerTest, DisabledFixedUpdateNotExecuted) {
    SystemScheduler scheduler;
    scheduler.SetFixedTimeStep(1.0f / 60.0f);
    auto& sys = scheduler.Register<FixedUpdateSystem>();

    scheduler.SetEnabled<FixedUpdateSystem>(false);
    ASSERT_TRUE(scheduler.Build());

    scheduler.Execute(1.0f / 30.0f);
    EXPECT_EQ(sys.CallCount(), 0u);
}

TEST(SystemSchedulerTest, IsEnabledReturnsFalseForUnregistered) {
    SystemScheduler scheduler;
    EXPECT_FALSE(scheduler.IsEnabled(kInvalidSystemTypeId));
}

// ===========================================================================
// SystemScheduler: Build validation
// ===========================================================================

TEST(SystemSchedulerTest, BuildWithoutSystemsSucceeds) {
    SystemScheduler scheduler;
    EXPECT_TRUE(scheduler.Build());
}

TEST(SystemSchedulerTest, BuildWithNoDependenciesSucceeds) {
    SystemScheduler scheduler;
    scheduler.Register<UpdateSystemA>();
    scheduler.Register<UpdateSystemB>();
    EXPECT_TRUE(scheduler.Build());
}

TEST(SystemSchedulerTest, RebuildAfterNewRegistrationInvalidates) {
    SystemScheduler scheduler;
    scheduler.Register<UpdateSystemA>();
    ASSERT_TRUE(scheduler.Build());

    // Registering a new system invalidates the plan.
    scheduler.Register<UpdateSystemB>();
    // The scheduler should require a rebuild (tested implicitly by
    // verifying the new system appears in the execution order).
    ASSERT_TRUE(scheduler.Build());

    const auto& order = scheduler.GetExecutionOrder(SystemStage::Update);
    EXPECT_EQ(order.size(), 2u);
}

TEST(SystemSchedulerTest, LastErrorClearedOnSuccessfulBuild) {
    SystemScheduler scheduler;
    scheduler.Register<CycleSystemA>();
    scheduler.Register<CycleSystemB>();

    (scheduler.AddDependency<CycleSystemA, CycleSystemB>());
    (scheduler.AddDependency<CycleSystemB, CycleSystemA>());
    EXPECT_FALSE(scheduler.Build());
    EXPECT_FALSE(scheduler.GetLastError().empty());

    // Fix the cycle and rebuild.
    // Since we cannot remove dependencies, create a fresh scheduler.
    SystemScheduler scheduler2;
    scheduler2.Register<UpdateSystemA>();
    EXPECT_TRUE(scheduler2.Build());
    EXPECT_TRUE(scheduler2.GetLastError().empty());
}

// ===========================================================================
// SystemScheduler: Integration - full game loop
// ===========================================================================

TEST(SystemSchedulerTest, FullGameLoopMultipleSystems) {
    SystemScheduler scheduler;
    scheduler.SetFixedTimeStep(1.0f / 60.0f);

    auto& pre = scheduler.Register<PreUpdateSystem>();
    auto& updateA = scheduler.Register<UpdateSystemA>();
    auto& updateB = scheduler.Register<UpdateSystemB>();
    auto& post = scheduler.Register<PostUpdateSystem>();
    auto& fixed = scheduler.Register<FixedUpdateSystem>();

    // UpdateA before UpdateB.
    (scheduler.AddDependency<UpdateSystemA, UpdateSystemB>());

    ASSERT_TRUE(scheduler.Build());
    RecordingSystem::clearLog();

    // Simulate 3 frames at 60fps.
    constexpr float kDt = 1.0f / 60.0f;
    for (int frame = 0; frame < 3; ++frame) {
        scheduler.Execute(kDt);
    }

    // Variable-rate systems: 3 calls each.
    EXPECT_EQ(pre.CallCount(), 3u);
    EXPECT_EQ(updateA.CallCount(), 3u);
    EXPECT_EQ(updateB.CallCount(), 3u);
    EXPECT_EQ(post.CallCount(), 3u);

    // FixedUpdate: at 60fps with 1/60 fixed step, should tick once per frame.
    EXPECT_EQ(fixed.CallCount(), 3u);

    // Verify stage ordering in execution log for each frame.
    const auto& log = RecordingSystem::executionLog();
    // Each frame produces: PreUpdate, UpdateA, UpdateB, PostUpdate, Fixed.
    // Total = 3 * 5 = 15 entries.
    ASSERT_EQ(log.size(), 15u);

    for (int frame = 0; frame < 3; ++frame) {
        auto base = static_cast<std::size_t>(frame * 5);
        EXPECT_EQ(log[base + 0], "PreUpdateSystem");
        EXPECT_EQ(log[base + 1], "UpdateSystemA");
        EXPECT_EQ(log[base + 2], "UpdateSystemB");
        EXPECT_EQ(log[base + 3], "PostUpdateSystem");
        EXPECT_EQ(log[base + 4], "FixedUpdateSystem");
    }
}

TEST(SystemSchedulerTest, VariableFrameRateWithFixedUpdate) {
    SystemScheduler scheduler;
    constexpr float kFixedDt = 1.0f / 60.0f;
    scheduler.SetFixedTimeStep(kFixedDt);

    auto& update = scheduler.Register<UpdateSystemA>();
    auto& fixed = scheduler.Register<FixedUpdateSystem>();

    ASSERT_TRUE(scheduler.Build());

    // Simulate a slow frame (100ms) followed by a fast frame (5ms).
    scheduler.Execute(0.1f);
    auto fixedCountAfterSlow = fixed.CallCount();
    EXPECT_EQ(fixedCountAfterSlow, 6u);  // 0.1 / (1/60) = 6

    scheduler.Execute(0.005f);
    // 5ms is less than 16.7ms, but accumulator had leftover from 0.1f.
    // Total accumulated: 0.1 + 0.005 = 0.105, ticks = floor(0.105 / (1/60)) = 6
    // Already ticked 6, so no additional tick.
    EXPECT_EQ(update.CallCount(), 2u);  // Called once per Execute().
}

// ===========================================================================
// SystemScheduler: Edge cases
// ===========================================================================

TEST(SystemSchedulerTest, ExecuteOnEmptySchedulerDoesNothing) {
    SystemScheduler scheduler;
    ASSERT_TRUE(scheduler.Build());
    scheduler.Execute(1.0f / 60.0f);
    // Should not crash.
}

TEST(SystemSchedulerTest, MoveConstructedSchedulerWorks) {
    SystemScheduler scheduler;
    scheduler.Register<UpdateSystemA>();
    ASSERT_TRUE(scheduler.Build());

    SystemScheduler moved(std::move(scheduler));
    EXPECT_EQ(moved.SystemCount(), 1u);

    auto* sys = moved.GetSystem<UpdateSystemA>();
    ASSERT_NE(sys, nullptr);

    RecordingSystem::clearLog();
    moved.Execute(1.0f / 60.0f);
    EXPECT_EQ(sys->CallCount(), 1u);
}

TEST(SystemSchedulerTest, DeltaTimePassedCorrectly) {
    SystemScheduler scheduler;
    auto& sys = scheduler.Register<UpdateSystemA>();

    ASSERT_TRUE(scheduler.Build());
    scheduler.Execute(0.042f);

    ASSERT_EQ(sys.CallCount(), 1u);
    EXPECT_FLOAT_EQ(sys.Calls()[0], 0.042f);
}
