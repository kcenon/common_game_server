#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "cgs/foundation/error_code.hpp"
#include "cgs/plugin/iplugin.hpp"
#include "cgs/plugin/plugin_export.hpp"
#include "cgs/plugin/plugin_manager.hpp"
#include "cgs/plugin/plugin_types.hpp"
#include "cgs/plugin/version_constraint.hpp"

using namespace cgs::plugin;
using cgs::foundation::ErrorCode;

// ── Test helpers ────────────────────────────────────────────────────────────

/// Configurable test plugin for dependency resolution tests.
class DepTestPlugin : public IPlugin {
public:
    explicit DepTestPlugin(PluginInfo info) : info_(std::move(info)) {}

    const PluginInfo& GetInfo() const override { return info_; }
    bool OnLoad(PluginContext& /*ctx*/) override { return true; }
    bool OnInit() override { return true; }
    void OnUpdate(float /*deltaTime*/) override {}
    void OnShutdown() override {}
    void OnUnload() override {}

private:
    PluginInfo info_;
};

/// Helper to register temporary plugins and load them into a PluginManager.
class TestPluginLoader {
public:
    void Add(PluginInfo info) { infos_.push_back(std::move(info)); }

    PluginManager Build() {
        PluginManager mgr;
        auto& registry = StaticPluginRegistry();
        auto savedRegistry = std::move(registry);
        registry.clear();

        for (const auto& info : infos_) {
            auto captured = info;
            registry.push_back(
                {info.name,
                 [captured]() -> std::unique_ptr<IPlugin> {
                     return std::make_unique<DepTestPlugin>(captured);
                 }});
        }

        auto result = mgr.RegisterStaticPlugins();
        registry = std::move(savedRegistry);
        return mgr;
    }

private:
    std::vector<PluginInfo> infos_;
};

// ===========================================================================
// ParseVersion
// ===========================================================================

TEST(ParseVersionTest, FullVersion) {
    auto result = ParseVersion("1.2.3");
    ASSERT_TRUE(result.hasValue()) << result.error().message();
    EXPECT_EQ(result.value().major, 1);
    EXPECT_EQ(result.value().minor, 2);
    EXPECT_EQ(result.value().patch, 3);
}

TEST(ParseVersionTest, MajorOnly) {
    auto result = ParseVersion("3");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().major, 3);
    EXPECT_EQ(result.value().minor, 0);
    EXPECT_EQ(result.value().patch, 0);
}

TEST(ParseVersionTest, MajorMinor) {
    auto result = ParseVersion("2.5");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().major, 2);
    EXPECT_EQ(result.value().minor, 5);
    EXPECT_EQ(result.value().patch, 0);
}

TEST(ParseVersionTest, EmptyStringFails) {
    auto result = ParseVersion("");
    EXPECT_TRUE(result.hasError());
}

TEST(ParseVersionTest, InvalidStringFails) {
    auto result = ParseVersion("abc");
    EXPECT_TRUE(result.hasError());
}

TEST(ParseVersionTest, WhitespaceTrimmed) {
    auto result = ParseVersion("  1.0.0  ");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().major, 1);
}

// ===========================================================================
// VersionConstraint: Parse
// ===========================================================================

TEST(VersionConstraintTest, ParseGreaterEqual) {
    auto result = VersionConstraint::Parse(">=1.2.0");
    ASSERT_TRUE(result.hasValue()) << result.error().message();
    EXPECT_EQ(result.value().op, ConstraintOp::GreaterEqual);
    EXPECT_EQ(result.value().version, (Version{1, 2, 0}));
}

TEST(VersionConstraintTest, ParseGreaterThan) {
    auto result = VersionConstraint::Parse(">2.0");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().op, ConstraintOp::GreaterThan);
    EXPECT_EQ(result.value().version, (Version{2, 0, 0}));
}

TEST(VersionConstraintTest, ParseLessEqual) {
    auto result = VersionConstraint::Parse("<=3.0.0");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().op, ConstraintOp::LessEqual);
}

TEST(VersionConstraintTest, ParseLessThan) {
    auto result = VersionConstraint::Parse("<2.0.0");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().op, ConstraintOp::LessThan);
    EXPECT_EQ(result.value().version, (Version{2, 0, 0}));
}

TEST(VersionConstraintTest, ParseEqual) {
    auto result = VersionConstraint::Parse("==1.5.0");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().op, ConstraintOp::Equal);
}

TEST(VersionConstraintTest, ParseCompatibleRelease) {
    auto result = VersionConstraint::Parse("~=1.5");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().op, ConstraintOp::CompatibleRelease);
    EXPECT_EQ(result.value().version, (Version{1, 5, 0}));
}

TEST(VersionConstraintTest, ParseEmptyFails) {
    auto result = VersionConstraint::Parse("");
    EXPECT_TRUE(result.hasError());
}

// ===========================================================================
// VersionConstraint: IsSatisfiedBy
// ===========================================================================

TEST(VersionConstraintTest, GreaterEqualSatisfied) {
    auto c = VersionConstraint{ConstraintOp::GreaterEqual, {1, 0, 0}};
    EXPECT_TRUE(c.IsSatisfiedBy({1, 0, 0}));
    EXPECT_TRUE(c.IsSatisfiedBy({1, 1, 0}));
    EXPECT_TRUE(c.IsSatisfiedBy({2, 0, 0}));
    EXPECT_FALSE(c.IsSatisfiedBy({0, 9, 9}));
}

TEST(VersionConstraintTest, GreaterThanSatisfied) {
    auto c = VersionConstraint{ConstraintOp::GreaterThan, {1, 0, 0}};
    EXPECT_FALSE(c.IsSatisfiedBy({1, 0, 0}));
    EXPECT_TRUE(c.IsSatisfiedBy({1, 0, 1}));
}

TEST(VersionConstraintTest, LessThanSatisfied) {
    auto c = VersionConstraint{ConstraintOp::LessThan, {2, 0, 0}};
    EXPECT_TRUE(c.IsSatisfiedBy({1, 9, 9}));
    EXPECT_FALSE(c.IsSatisfiedBy({2, 0, 0}));
    EXPECT_FALSE(c.IsSatisfiedBy({2, 0, 1}));
}

TEST(VersionConstraintTest, LessEqualSatisfied) {
    auto c = VersionConstraint{ConstraintOp::LessEqual, {2, 0, 0}};
    EXPECT_TRUE(c.IsSatisfiedBy({2, 0, 0}));
    EXPECT_TRUE(c.IsSatisfiedBy({1, 9, 9}));
    EXPECT_FALSE(c.IsSatisfiedBy({2, 0, 1}));
}

TEST(VersionConstraintTest, EqualSatisfied) {
    auto c = VersionConstraint{ConstraintOp::Equal, {1, 5, 0}};
    EXPECT_TRUE(c.IsSatisfiedBy({1, 5, 0}));
    EXPECT_FALSE(c.IsSatisfiedBy({1, 5, 1}));
    EXPECT_FALSE(c.IsSatisfiedBy({1, 4, 0}));
}

TEST(VersionConstraintTest, CompatibleReleaseSatisfied) {
    auto c = VersionConstraint{ConstraintOp::CompatibleRelease, {1, 5, 0}};
    // ~=1.5.0 means >=1.5.0 and same major (1.x.x).
    EXPECT_TRUE(c.IsSatisfiedBy({1, 5, 0}));
    EXPECT_TRUE(c.IsSatisfiedBy({1, 9, 0}));
    EXPECT_FALSE(c.IsSatisfiedBy({1, 4, 9}));
    EXPECT_FALSE(c.IsSatisfiedBy({2, 0, 0}));
}

// ===========================================================================
// VersionConstraint: ToString
// ===========================================================================

TEST(VersionConstraintTest, ToStringFormat) {
    auto c = VersionConstraint{ConstraintOp::GreaterEqual, {1, 2, 3}};
    EXPECT_EQ(c.ToString(), ">=1.2.3");

    auto c2 = VersionConstraint{ConstraintOp::CompatibleRelease, {1, 5, 0}};
    EXPECT_EQ(c2.ToString(), "~=1.5.0");
}

// ===========================================================================
// DependencySpec: Parse
// ===========================================================================

TEST(DependencySpecTest, NameOnly) {
    auto result = DependencySpec::Parse("NetworkPlugin");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().name, "NetworkPlugin");
    EXPECT_TRUE(result.value().constraints.empty());
}

TEST(DependencySpecTest, NameWithConstraint) {
    auto result = DependencySpec::Parse("CoreLib>=1.0.0");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().name, "CoreLib");
    ASSERT_EQ(result.value().constraints.size(), 1u);
    EXPECT_EQ(result.value().constraints[0].op, ConstraintOp::GreaterEqual);
    EXPECT_EQ(result.value().constraints[0].version, (Version{1, 0, 0}));
}

TEST(DependencySpecTest, NameWithMultipleConstraints) {
    auto result = DependencySpec::Parse("CoreLib>=1.0,<2.0");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().name, "CoreLib");
    ASSERT_EQ(result.value().constraints.size(), 2u);
    EXPECT_EQ(result.value().constraints[0].op, ConstraintOp::GreaterEqual);
    EXPECT_EQ(result.value().constraints[1].op, ConstraintOp::LessThan);
}

TEST(DependencySpecTest, CompatibleRelease) {
    auto result = DependencySpec::Parse("Renderer~=1.5");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().name, "Renderer");
    ASSERT_EQ(result.value().constraints.size(), 1u);
    EXPECT_EQ(result.value().constraints[0].op, ConstraintOp::CompatibleRelease);
}

TEST(DependencySpecTest, EmptyStringFails) {
    auto result = DependencySpec::Parse("");
    EXPECT_TRUE(result.hasError());
}

TEST(DependencySpecTest, IsSatisfiedByMultipleConstraints) {
    auto result = DependencySpec::Parse("CoreLib>=1.0,<2.0");
    ASSERT_TRUE(result.hasValue());
    const auto& spec = result.value();

    EXPECT_TRUE(spec.IsSatisfiedBy({1, 0, 0}));
    EXPECT_TRUE(spec.IsSatisfiedBy({1, 5, 0}));
    EXPECT_TRUE(spec.IsSatisfiedBy({1, 9, 9}));
    EXPECT_FALSE(spec.IsSatisfiedBy({0, 9, 0}));
    EXPECT_FALSE(spec.IsSatisfiedBy({2, 0, 0}));
}

TEST(DependencySpecTest, ConstraintsToString) {
    auto result = DependencySpec::Parse("Foo>=1.0,<2.0");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().ConstraintsToString(), ">=1.0.0, <2.0.0");
}

TEST(DependencySpecTest, NoConstraintsToString) {
    auto result = DependencySpec::Parse("Foo");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().ConstraintsToString(), "(any version)");
}

// ===========================================================================
// PluginManager::ValidateDependencies - Missing dependency
// ===========================================================================

TEST(PluginDependencyTest, MissingDependencyReported) {
    TestPluginLoader loader;
    loader.Add({"PluginA", "Test A", {1, 0, 0}, {"MissingPlugin"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_FALSE(report.success);
    ASSERT_EQ(report.issues.size(), 1u);
    EXPECT_EQ(report.issues[0].kind,
              PluginManager::DependencyIssue::Kind::Missing);
    EXPECT_EQ(report.issues[0].plugin, "PluginA");
    EXPECT_EQ(report.issues[0].dependency, "MissingPlugin");
}

TEST(PluginDependencyTest, MissingDependencyMessageIsDescriptive) {
    TestPluginLoader loader;
    loader.Add({"PluginA", "Test A", {1, 0, 0}, {"MissingPlugin>=2.0"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_FALSE(report.success);
    ASSERT_GE(report.issues.size(), 1u);
    EXPECT_NE(report.issues[0].detail.find("MissingPlugin"), std::string::npos);
    EXPECT_NE(report.issues[0].detail.find("PluginA"), std::string::npos);
}

// ===========================================================================
// PluginManager::ValidateDependencies - Version mismatch
// ===========================================================================

TEST(PluginDependencyTest, VersionMismatchReported) {
    TestPluginLoader loader;
    loader.Add({"CoreLib", "Core library", {1, 0, 0}, {}, kPluginApiVersion});
    loader.Add({"PluginA", "Uses Core", {1, 0, 0}, {"CoreLib>=2.0.0"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_FALSE(report.success);
    ASSERT_GE(report.issues.size(), 1u);

    bool foundMismatch = false;
    for (const auto& issue : report.issues) {
        if (issue.kind == PluginManager::DependencyIssue::Kind::VersionMismatch) {
            EXPECT_EQ(issue.plugin, "PluginA");
            EXPECT_EQ(issue.dependency, "CoreLib");
            EXPECT_NE(issue.detail.find("1.0.0"), std::string::npos);
            foundMismatch = true;
        }
    }
    EXPECT_TRUE(foundMismatch);
}

TEST(PluginDependencyTest, VersionConstraintSatisfied) {
    TestPluginLoader loader;
    loader.Add({"CoreLib", "Core library", {1, 5, 0}, {}, kPluginApiVersion});
    loader.Add({"PluginA", "Uses Core", {1, 0, 0}, {"CoreLib>=1.0,<2.0"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_TRUE(report.success);
    EXPECT_TRUE(report.issues.empty());
}

TEST(PluginDependencyTest, CompatibleReleaseConstraintSatisfied) {
    TestPluginLoader loader;
    loader.Add({"Renderer", "Render engine", {1, 7, 0}, {}, kPluginApiVersion});
    loader.Add({"Game", "Game plugin", {1, 0, 0}, {"Renderer~=1.5"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_TRUE(report.success);
}

TEST(PluginDependencyTest, CompatibleReleaseConstraintViolated) {
    TestPluginLoader loader;
    loader.Add({"Renderer", "Render engine", {2, 0, 0}, {}, kPluginApiVersion});
    loader.Add({"Game", "Game plugin", {1, 0, 0}, {"Renderer~=1.5"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_FALSE(report.success);
    bool foundMismatch = false;
    for (const auto& issue : report.issues) {
        if (issue.kind == PluginManager::DependencyIssue::Kind::VersionMismatch &&
            issue.dependency == "Renderer") {
            foundMismatch = true;
        }
    }
    EXPECT_TRUE(foundMismatch);
}

// ===========================================================================
// PluginManager::ValidateDependencies - Circular dependency
// ===========================================================================

TEST(PluginDependencyTest, CircularDependencyDetected) {
    TestPluginLoader loader;
    loader.Add({"PluginA", "A", {1, 0, 0}, {"PluginB"}, kPluginApiVersion});
    loader.Add({"PluginB", "B", {1, 0, 0}, {"PluginA"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_FALSE(report.success);
    EXPECT_FALSE(report.cyclePath.empty());

    // The cycle path should contain both plugins.
    bool hasA = false;
    bool hasB = false;
    for (const auto& name : report.cyclePath) {
        if (name == "PluginA") hasA = true;
        if (name == "PluginB") hasB = true;
    }
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasB);
}

TEST(PluginDependencyTest, ThreeWayCircularDependency) {
    TestPluginLoader loader;
    loader.Add({"A", "A", {1, 0, 0}, {"B"}, kPluginApiVersion});
    loader.Add({"B", "B", {1, 0, 0}, {"C"}, kPluginApiVersion});
    loader.Add({"C", "C", {1, 0, 0}, {"A"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_FALSE(report.success);
    EXPECT_FALSE(report.cyclePath.empty());

    // The cycle should form a closed loop.
    EXPECT_EQ(report.cyclePath.front(), report.cyclePath.back());
}

TEST(PluginDependencyTest, CircularDependencyErrorInResolveDependencies) {
    TestPluginLoader loader;
    loader.Add({"PluginA", "A", {1, 0, 0}, {"PluginB"}, kPluginApiVersion});
    loader.Add({"PluginB", "B", {1, 0, 0}, {"PluginA"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto result = mgr.InitializeAll();

    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::DependencyError);
    // Error message should contain cycle information.
    EXPECT_NE(result.error().message().find("cycle"), std::string::npos);
}

// ===========================================================================
// PluginManager::ValidateDependencies - DAG resolution
// ===========================================================================

TEST(PluginDependencyTest, LinearDependencyChain) {
    TestPluginLoader loader;
    loader.Add({"Base", "Base", {1, 0, 0}, {}, kPluginApiVersion});
    loader.Add({"Mid", "Mid", {1, 0, 0}, {"Base"}, kPluginApiVersion});
    loader.Add({"Top", "Top", {1, 0, 0}, {"Mid"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_TRUE(report.success);
    ASSERT_EQ(report.loadOrder.size(), 3u);

    // Base must come before Mid, Mid before Top.
    auto posBase = std::find(report.loadOrder.begin(), report.loadOrder.end(), "Base");
    auto posMid = std::find(report.loadOrder.begin(), report.loadOrder.end(), "Mid");
    auto posTop = std::find(report.loadOrder.begin(), report.loadOrder.end(), "Top");
    EXPECT_LT(posBase, posMid);
    EXPECT_LT(posMid, posTop);
}

TEST(PluginDependencyTest, DiamondDependency) {
    // A depends on B and C; both B and C depend on D.
    TestPluginLoader loader;
    loader.Add({"D", "Shared base", {1, 0, 0}, {}, kPluginApiVersion});
    loader.Add({"B", "Left", {1, 0, 0}, {"D"}, kPluginApiVersion});
    loader.Add({"C", "Right", {1, 0, 0}, {"D"}, kPluginApiVersion});
    loader.Add({"A", "Top", {1, 0, 0}, {"B", "C"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_TRUE(report.success);
    ASSERT_EQ(report.loadOrder.size(), 4u);

    auto posD = std::find(report.loadOrder.begin(), report.loadOrder.end(), "D");
    auto posB = std::find(report.loadOrder.begin(), report.loadOrder.end(), "B");
    auto posC = std::find(report.loadOrder.begin(), report.loadOrder.end(), "C");
    auto posA = std::find(report.loadOrder.begin(), report.loadOrder.end(), "A");

    // D before B and C; B and C before A.
    EXPECT_LT(posD, posB);
    EXPECT_LT(posD, posC);
    EXPECT_LT(posB, posA);
    EXPECT_LT(posC, posA);
}

TEST(PluginDependencyTest, NoDepPluginsAllResolved) {
    TestPluginLoader loader;
    loader.Add({"Alpha", "A", {1, 0, 0}, {}, kPluginApiVersion});
    loader.Add({"Beta", "B", {1, 0, 0}, {}, kPluginApiVersion});
    loader.Add({"Gamma", "C", {1, 0, 0}, {}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_TRUE(report.success);
    EXPECT_EQ(report.loadOrder.size(), 3u);
}

// ===========================================================================
// PluginManager::ValidateDependencies - Multiple issues
// ===========================================================================

TEST(PluginDependencyTest, MultipleIssuesCollected) {
    TestPluginLoader loader;
    loader.Add({"CoreLib", "Core", {1, 0, 0}, {}, kPluginApiVersion});
    loader.Add({"PluginA", "A", {1, 0, 0},
                {"CoreLib>=2.0", "MissingDep"}, kPluginApiVersion});

    auto mgr = loader.Build();
    auto report = mgr.ValidateDependencies();

    EXPECT_FALSE(report.success);
    // Should have at least 2 issues: version mismatch + missing dep.
    EXPECT_GE(report.issues.size(), 2u);

    bool hasMissing = false;
    bool hasMismatch = false;
    for (const auto& issue : report.issues) {
        if (issue.kind == PluginManager::DependencyIssue::Kind::Missing) {
            hasMissing = true;
        }
        if (issue.kind == PluginManager::DependencyIssue::Kind::VersionMismatch) {
            hasMismatch = true;
        }
    }
    EXPECT_TRUE(hasMissing);
    EXPECT_TRUE(hasMismatch);
}

// ===========================================================================
// PluginManager: InitializeAll with version constraints
// ===========================================================================

TEST(PluginDependencyTest, InitializeAllWithValidConstraints) {
    TestPluginLoader loader;
    loader.Add({"CoreLib", "Core", {1, 5, 0}, {}, kPluginApiVersion});
    loader.Add({"Feature", "Feature", {1, 0, 0}, {"CoreLib>=1.0,<2.0"},
                kPluginApiVersion});

    auto mgr = loader.Build();
    auto result = mgr.InitializeAll();

    ASSERT_TRUE(result.hasValue()) << result.error().message();

    auto stateCore = mgr.GetPluginState("CoreLib");
    auto stateFeat = mgr.GetPluginState("Feature");
    ASSERT_TRUE(stateCore.hasValue());
    ASSERT_TRUE(stateFeat.hasValue());
    EXPECT_EQ(stateCore.value(), PluginState::Initialized);
    EXPECT_EQ(stateFeat.value(), PluginState::Initialized);
}

TEST(PluginDependencyTest, InitializeAllFailsOnVersionMismatch) {
    TestPluginLoader loader;
    loader.Add({"CoreLib", "Core", {1, 0, 0}, {}, kPluginApiVersion});
    loader.Add({"Feature", "Feature", {1, 0, 0}, {"CoreLib>=2.0"},
                kPluginApiVersion});

    auto mgr = loader.Build();
    auto result = mgr.InitializeAll();

    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::DependencyError);
}

TEST(PluginDependencyTest, InitializeAllFailsOnMissingDependency) {
    TestPluginLoader loader;
    loader.Add({"Feature", "Feature", {1, 0, 0}, {"NonExistent"},
                kPluginApiVersion});

    auto mgr = loader.Build();
    auto result = mgr.InitializeAll();

    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::DependencyError);
}
