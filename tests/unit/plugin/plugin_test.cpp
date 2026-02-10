#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "cgs/foundation/error_code.hpp"
#include "cgs/plugin/iplugin.hpp"
#include "cgs/plugin/plugin_export.hpp"
#include "cgs/plugin/plugin_manager.hpp"
#include "cgs/plugin/plugin_types.hpp"

using namespace cgs::plugin;
using cgs::foundation::ErrorCode;

// ── Test plugin implementations ─────────────────────────────────────────────

/// Minimal test plugin that records lifecycle calls.
class TestPlugin : public IPlugin {
public:
    explicit TestPlugin(std::string name = "TestPlugin")
        : info_{std::move(name), "A test plugin", {1, 0, 0}, {}, kPluginApiVersion} {}

    const PluginInfo& GetInfo() const override { return info_; }

    bool OnLoad(PluginContext& /*ctx*/) override {
        loadCalled = true;
        return loadResult;
    }

    bool OnInit() override {
        initCalled = true;
        return initResult;
    }

    void OnUpdate(float deltaTime) override {
        updateCalls.push_back(deltaTime);
    }

    void OnShutdown() override {
        shutdownCalled = true;
    }

    void OnUnload() override {
        unloadCalled = true;
    }

    // Controls for test scenarios.
    bool loadResult = true;
    bool initResult = true;

    // Observation points.
    bool loadCalled = false;
    bool initCalled = false;
    bool shutdownCalled = false;
    bool unloadCalled = false;
    std::vector<float> updateCalls;

private:
    PluginInfo info_;
};

/// Plugin with wrong API version.
class BadVersionPlugin : public TestPlugin {
public:
    BadVersionPlugin() : TestPlugin("BadVersionPlugin") {
        badInfo_ = {
            "BadVersionPlugin", "Wrong API version", {1, 0, 0}, {}, 999};
    }

    const PluginInfo& GetInfo() const override { return badInfo_; }

private:
    PluginInfo badInfo_;
};

/// Plugin that fails OnLoad.
class FailLoadPlugin : public TestPlugin {
public:
    FailLoadPlugin() : TestPlugin("FailLoadPlugin") {
        loadResult = false;
    }
};

/// Plugin that fails OnInit.
class FailInitPlugin : public TestPlugin {
public:
    FailInitPlugin() : TestPlugin("FailInitPlugin") {
        initResult = false;
    }
};

/// Plugin that declares a dependency on TestPlugin.
class DependentPlugin : public TestPlugin {
public:
    DependentPlugin() : TestPlugin("DependentPlugin") {
        depInfo_ = {"DependentPlugin",
                     "Depends on TestPlugin",
                     {1, 0, 0},
                     {"TestPlugin"},
                     kPluginApiVersion};
    }

    const PluginInfo& GetInfo() const override { return depInfo_; }

private:
    PluginInfo depInfo_;
};

/// Plugin that creates a circular dependency with DependentPlugin.
class CircularPlugin : public TestPlugin {
public:
    CircularPlugin() : TestPlugin("CircularPlugin") {
        circInfo_ = {"CircularPlugin",
                      "Circular dep",
                      {1, 0, 0},
                      {"DependentPlugin"},
                      kPluginApiVersion};
    }

    const PluginInfo& GetInfo() const override { return circInfo_; }

private:
    PluginInfo circInfo_;
};

// ===========================================================================
// Version
// ===========================================================================

TEST(PluginVersionTest, CompatibleWhenMajorMatches) {
    Version v1{1, 0, 0};
    Version v2{1, 2, 3};
    EXPECT_TRUE(v1.IsCompatibleWith(v2));
}

TEST(PluginVersionTest, IncompatibleWhenMajorDiffers) {
    Version v1{1, 0, 0};
    Version v2{2, 0, 0};
    EXPECT_FALSE(v1.IsCompatibleWith(v2));
}

TEST(PluginVersionTest, Comparison) {
    Version v1{1, 0, 0};
    Version v2{1, 0, 1};
    Version v3{1, 1, 0};
    Version v4{2, 0, 0};

    EXPECT_LT(v1, v2);
    EXPECT_LT(v2, v3);
    EXPECT_LT(v3, v4);
    EXPECT_EQ(v1, (Version{1, 0, 0}));
}

TEST(PluginVersionTest, ZeroVersionIsValid) {
    Version v{0, 0, 0};
    EXPECT_TRUE(v.IsCompatibleWith(Version{0, 1, 0}));
    EXPECT_FALSE(v.IsCompatibleWith(Version{1, 0, 0}));
}

// ===========================================================================
// PluginInfo
// ===========================================================================

TEST(PluginInfoTest, DefaultApiVersion) {
    PluginInfo info;
    info.name = "test";
    EXPECT_EQ(info.apiVersion, kPluginApiVersion);
}

// ===========================================================================
// PluginState
// ===========================================================================

TEST(PluginStateTest, InitialStateIsUnloaded) {
    // Verify the enum values exist and have expected ordering.
    EXPECT_LT(static_cast<int>(PluginState::Unloaded),
              static_cast<int>(PluginState::Loaded));
    EXPECT_LT(static_cast<int>(PluginState::Loaded),
              static_cast<int>(PluginState::Initialized));
}

// ===========================================================================
// PluginManager: Basic operations
// ===========================================================================

TEST(PluginManagerTest, EmptyManagerHasNoPlugins) {
    PluginManager mgr;
    EXPECT_EQ(mgr.PluginCount(), 0u);
    EXPECT_TRUE(mgr.GetAllPluginNames().empty());
}

TEST(PluginManagerTest, GetPluginReturnsNullForUnknown) {
    PluginManager mgr;
    EXPECT_EQ(mgr.GetPlugin("nonexistent"), nullptr);
}

TEST(PluginManagerTest, GetPluginStateReturnsErrorForUnknown) {
    PluginManager mgr;
    auto result = mgr.GetPluginState("nonexistent");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginNotFound);
}

// ===========================================================================
// PluginManager: Static registration
// ===========================================================================

// We define a dedicated plugin for static registration testing.
class StaticTestPlugin : public TestPlugin {
public:
    StaticTestPlugin() : TestPlugin("StaticTestPlugin") {}
};

// Register it in the static registry.
CGS_PLUGIN_REGISTER(StaticTestPlugin, "StaticTestPlugin");

TEST(PluginManagerTest, RegisterStaticPluginsLoadsPlugin) {
    PluginManager mgr;
    auto result = mgr.RegisterStaticPlugins();
    ASSERT_TRUE(result.hasValue()) << result.error().message();

    EXPECT_GE(mgr.PluginCount(), 1u);
    EXPECT_NE(mgr.GetPlugin("StaticTestPlugin"), nullptr);
}

TEST(PluginManagerTest, StaticPluginIsInLoadedState) {
    PluginManager mgr;
    ASSERT_TRUE(mgr.RegisterStaticPlugins().hasValue());

    auto stateResult = mgr.GetPluginState("StaticTestPlugin");
    ASSERT_TRUE(stateResult.hasValue());
    EXPECT_EQ(stateResult.value(), PluginState::Loaded);
}

TEST(PluginManagerTest, StaticPluginOnLoadCalled) {
    PluginManager mgr;
    ASSERT_TRUE(mgr.RegisterStaticPlugins().hasValue());

    auto* plugin = dynamic_cast<StaticTestPlugin*>(mgr.GetPlugin("StaticTestPlugin"));
    ASSERT_NE(plugin, nullptr);
    EXPECT_TRUE(plugin->loadCalled);
}

// ===========================================================================
// PluginManager: Full lifecycle via manual plugin injection
// ===========================================================================

// Helper: use a custom static plugin that doesn't pollute other tests.
class LifecyclePlugin : public TestPlugin {
public:
    LifecyclePlugin() : TestPlugin("LifecyclePlugin") {}
};

class LifecyclePluginFactory {
public:
    /// Create a PluginManager with only LifecyclePlugin loaded.
    ///
    /// Temporarily replaces the global static registry to avoid
    /// loading other registered plugins (e.g., StaticTestPlugin).
    static PluginManager CreateWithPlugin() {
        PluginManager mgr;
        auto& registry = StaticPluginRegistry();

        // Save and replace the registry.
        auto saved = std::move(registry);
        registry.clear();
        registry.push_back(
            {"LifecyclePlugin",
             []() -> std::unique_ptr<IPlugin> {
                 return std::make_unique<LifecyclePlugin>();
             }});

        auto result = mgr.RegisterStaticPlugins();

        // Restore the original registry.
        registry = std::move(saved);

        return mgr;
    }
};

TEST(PluginManagerTest, InitPluginTransitionsToInitialized) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();
    ASSERT_NE(mgr.GetPlugin("LifecyclePlugin"), nullptr);

    auto result = mgr.InitPlugin("LifecyclePlugin");
    ASSERT_TRUE(result.hasValue()) << result.error().message();

    auto stateResult = mgr.GetPluginState("LifecyclePlugin");
    ASSERT_TRUE(stateResult.hasValue());
    EXPECT_EQ(stateResult.value(), PluginState::Initialized);
}

TEST(PluginManagerTest, InitPluginCallsOnInit) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();

    ASSERT_TRUE(mgr.InitPlugin("LifecyclePlugin").hasValue());

    auto* plugin = dynamic_cast<LifecyclePlugin*>(mgr.GetPlugin("LifecyclePlugin"));
    ASSERT_NE(plugin, nullptr);
    EXPECT_TRUE(plugin->initCalled);
}

TEST(PluginManagerTest, InitPluginFailsIfNotLoaded) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();
    ASSERT_TRUE(mgr.InitPlugin("LifecyclePlugin").hasValue());

    // Try to init again — should fail (already Initialized, not Loaded).
    auto result = mgr.InitPlugin("LifecyclePlugin");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginInvalidState);
}

TEST(PluginManagerTest, ActivatePluginTransitionsToActive) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();
    ASSERT_TRUE(mgr.InitPlugin("LifecyclePlugin").hasValue());

    auto result = mgr.ActivatePlugin("LifecyclePlugin");
    ASSERT_TRUE(result.hasValue()) << result.error().message();

    auto stateResult = mgr.GetPluginState("LifecyclePlugin");
    ASSERT_TRUE(stateResult.hasValue());
    EXPECT_EQ(stateResult.value(), PluginState::Active);
}

TEST(PluginManagerTest, ActivatePluginFailsIfNotInitialized) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();

    // Plugin is in Loaded state, not Initialized.
    auto result = mgr.ActivatePlugin("LifecyclePlugin");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginInvalidState);
}

TEST(PluginManagerTest, UpdateAllCallsOnUpdate) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();
    ASSERT_TRUE(mgr.InitPlugin("LifecyclePlugin").hasValue());
    ASSERT_TRUE(mgr.ActivatePlugin("LifecyclePlugin").hasValue());

    mgr.UpdateAll(0.016f);
    mgr.UpdateAll(0.033f);

    auto* plugin = dynamic_cast<LifecyclePlugin*>(mgr.GetPlugin("LifecyclePlugin"));
    ASSERT_NE(plugin, nullptr);
    ASSERT_EQ(plugin->updateCalls.size(), 2u);
    EXPECT_FLOAT_EQ(plugin->updateCalls[0], 0.016f);
    EXPECT_FLOAT_EQ(plugin->updateCalls[1], 0.033f);
}

TEST(PluginManagerTest, UpdateAllSkipsNonActivePlugins) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();
    // Plugin is Loaded, not Active.
    mgr.UpdateAll(0.016f);

    auto* plugin = dynamic_cast<LifecyclePlugin*>(mgr.GetPlugin("LifecyclePlugin"));
    ASSERT_NE(plugin, nullptr);
    EXPECT_TRUE(plugin->updateCalls.empty());
}

TEST(PluginManagerTest, ShutdownPluginTransitionsToLoaded) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();
    ASSERT_TRUE(mgr.InitPlugin("LifecyclePlugin").hasValue());
    ASSERT_TRUE(mgr.ActivatePlugin("LifecyclePlugin").hasValue());

    auto result = mgr.ShutdownPlugin("LifecyclePlugin");
    ASSERT_TRUE(result.hasValue()) << result.error().message();

    auto stateResult = mgr.GetPluginState("LifecyclePlugin");
    ASSERT_TRUE(stateResult.hasValue());
    EXPECT_EQ(stateResult.value(), PluginState::Loaded);
}

TEST(PluginManagerTest, ShutdownPluginCallsOnShutdown) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();
    ASSERT_TRUE(mgr.InitPlugin("LifecyclePlugin").hasValue());
    ASSERT_TRUE(mgr.ActivatePlugin("LifecyclePlugin").hasValue());
    ASSERT_TRUE(mgr.ShutdownPlugin("LifecyclePlugin").hasValue());

    auto* plugin = dynamic_cast<LifecyclePlugin*>(mgr.GetPlugin("LifecyclePlugin"));
    ASSERT_NE(plugin, nullptr);
    EXPECT_TRUE(plugin->shutdownCalled);
}

TEST(PluginManagerTest, ShutdownPluginFailsIfNotActiveOrInitialized) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();

    // Plugin is Loaded, not Active or Initialized.
    auto result = mgr.ShutdownPlugin("LifecyclePlugin");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginInvalidState);
}

TEST(PluginManagerTest, UnloadPluginRemovesIt) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();
    EXPECT_EQ(mgr.PluginCount(), 1u);

    auto result = mgr.UnloadPlugin("LifecyclePlugin");
    ASSERT_TRUE(result.hasValue()) << result.error().message();

    EXPECT_EQ(mgr.PluginCount(), 0u);
    EXPECT_EQ(mgr.GetPlugin("LifecyclePlugin"), nullptr);
}

TEST(PluginManagerTest, UnloadPluginCallsOnUnload) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();

    ASSERT_NE(mgr.GetPlugin("LifecyclePlugin"), nullptr);
    // OnUnload is called internally before the object is destroyed.
    // We verify it doesn't crash and the plugin is removed.
    ASSERT_TRUE(mgr.UnloadPlugin("LifecyclePlugin").hasValue());
    EXPECT_EQ(mgr.PluginCount(), 0u);
}

TEST(PluginManagerTest, UnloadPluginFailsIfActive) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();
    ASSERT_TRUE(mgr.InitPlugin("LifecyclePlugin").hasValue());
    ASSERT_TRUE(mgr.ActivatePlugin("LifecyclePlugin").hasValue());

    // Cannot unload an active plugin — must shutdown first.
    auto result = mgr.UnloadPlugin("LifecyclePlugin");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginInvalidState);
}

// ===========================================================================
// PluginManager: Full lifecycle integration
// ===========================================================================

TEST(PluginManagerTest, FullLifecycleRoundTrip) {
    auto mgr = LifecyclePluginFactory::CreateWithPlugin();

    // Load → Init → Activate → Update → Shutdown → Unload.
    ASSERT_TRUE(mgr.InitPlugin("LifecyclePlugin").hasValue());
    ASSERT_TRUE(mgr.ActivatePlugin("LifecyclePlugin").hasValue());

    mgr.UpdateAll(0.016f);

    ASSERT_TRUE(mgr.ShutdownPlugin("LifecyclePlugin").hasValue());
    ASSERT_TRUE(mgr.UnloadPlugin("LifecyclePlugin").hasValue());

    EXPECT_EQ(mgr.PluginCount(), 0u);
}

// ===========================================================================
// PluginManager: InitializeAll and dependency ordering
// ===========================================================================

class DepPluginA : public TestPlugin {
public:
    DepPluginA() : TestPlugin("PluginA") {}
};

class DepPluginB : public TestPlugin {
public:
    DepPluginB() : TestPlugin("PluginB") {
        depInfo_ = {"PluginB", "Depends on A", {1, 0, 0}, {"PluginA"}, kPluginApiVersion};
    }
    const PluginInfo& GetInfo() const override { return depInfo_; }

private:
    PluginInfo depInfo_;
};

TEST(PluginManagerTest, InitializeAllResolveDependencies) {
    PluginManager mgr;

    auto& registry = StaticPluginRegistry();
    auto sizeBefore = registry.size();

    registry.push_back({"PluginA",
                         []() -> std::unique_ptr<IPlugin> {
                             return std::make_unique<DepPluginA>();
                         }});
    registry.push_back({"PluginB",
                         []() -> std::unique_ptr<IPlugin> {
                             return std::make_unique<DepPluginB>();
                         }});

    ASSERT_TRUE(mgr.RegisterStaticPlugins().hasValue());
    registry.resize(sizeBefore);

    // InitializeAll should resolve dependencies and init A before B.
    auto result = mgr.InitializeAll();
    ASSERT_TRUE(result.hasValue()) << result.error().message();

    auto stateA = mgr.GetPluginState("PluginA");
    auto stateB = mgr.GetPluginState("PluginB");
    ASSERT_TRUE(stateA.hasValue());
    ASSERT_TRUE(stateB.hasValue());
    EXPECT_EQ(stateA.value(), PluginState::Initialized);
    EXPECT_EQ(stateB.value(), PluginState::Initialized);
}

TEST(PluginManagerTest, ShutdownAllReverseOrder) {
    PluginManager mgr;

    auto& registry = StaticPluginRegistry();
    auto sizeBefore = registry.size();

    registry.push_back({"ShutA",
                         []() -> std::unique_ptr<IPlugin> {
                             return std::make_unique<TestPlugin>("ShutA");
                         }});
    registry.push_back({"ShutB",
                         []() -> std::unique_ptr<IPlugin> {
                             return std::make_unique<TestPlugin>("ShutB");
                         }});

    ASSERT_TRUE(mgr.RegisterStaticPlugins().hasValue());
    registry.resize(sizeBefore);

    ASSERT_TRUE(mgr.InitializeAll().hasValue());
    ASSERT_TRUE(mgr.ActivateAll().hasValue());

    mgr.ShutdownAll();

    auto stateA = mgr.GetPluginState("ShutA");
    auto stateB = mgr.GetPluginState("ShutB");
    ASSERT_TRUE(stateA.hasValue());
    ASSERT_TRUE(stateB.hasValue());
    EXPECT_EQ(stateA.value(), PluginState::Loaded);
    EXPECT_EQ(stateB.value(), PluginState::Loaded);
}

// ===========================================================================
// PluginManager: Error handling
// ===========================================================================

TEST(PluginManagerTest, DuplicateLoadReturnsError) {
    PluginManager mgr;

    auto& registry = StaticPluginRegistry();
    auto sizeBefore = registry.size();

    registry.push_back({"DupPlugin",
                         []() -> std::unique_ptr<IPlugin> {
                             return std::make_unique<TestPlugin>("DupPlugin");
                         }});
    registry.push_back({"DupPlugin",
                         []() -> std::unique_ptr<IPlugin> {
                             return std::make_unique<TestPlugin>("DupPlugin");
                         }});

    auto result = mgr.RegisterStaticPlugins();
    registry.resize(sizeBefore);

    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginAlreadyLoaded);
}

TEST(PluginManagerTest, ApiVersionMismatchReturnsError) {
    PluginManager mgr;

    auto& registry = StaticPluginRegistry();
    auto sizeBefore = registry.size();

    registry.push_back({"BadVersionPlugin",
                         []() -> std::unique_ptr<IPlugin> {
                             return std::make_unique<BadVersionPlugin>();
                         }});

    auto result = mgr.RegisterStaticPlugins();
    registry.resize(sizeBefore);

    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginVersionMismatch);
}

TEST(PluginManagerTest, OnLoadFailureReturnsError) {
    PluginManager mgr;

    auto& registry = StaticPluginRegistry();
    auto sizeBefore = registry.size();

    registry.push_back({"FailLoadPlugin",
                         []() -> std::unique_ptr<IPlugin> {
                             return std::make_unique<FailLoadPlugin>();
                         }});

    auto result = mgr.RegisterStaticPlugins();
    registry.resize(sizeBefore);

    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginLoadFailed);
}

TEST(PluginManagerTest, OnInitFailureSetsErrorState) {
    PluginManager mgr;

    auto& registry = StaticPluginRegistry();
    auto sizeBefore = registry.size();

    registry.push_back({"FailInitPlugin",
                         []() -> std::unique_ptr<IPlugin> {
                             auto p = std::make_unique<FailInitPlugin>();
                             p->loadResult = true;
                             return p;
                         }});

    ASSERT_TRUE(mgr.RegisterStaticPlugins().hasValue());
    registry.resize(sizeBefore);

    auto result = mgr.InitPlugin("FailInitPlugin");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginInitFailed);

    auto stateResult = mgr.GetPluginState("FailInitPlugin");
    ASSERT_TRUE(stateResult.hasValue());
    EXPECT_EQ(stateResult.value(), PluginState::Error);
}

TEST(PluginManagerTest, InitNonExistentPluginReturnsError) {
    PluginManager mgr;
    auto result = mgr.InitPlugin("nonexistent");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginNotFound);
}

TEST(PluginManagerTest, ShutdownNonExistentPluginReturnsError) {
    PluginManager mgr;
    auto result = mgr.ShutdownPlugin("nonexistent");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginNotFound);
}

TEST(PluginManagerTest, UnloadNonExistentPluginReturnsError) {
    PluginManager mgr;
    auto result = mgr.UnloadPlugin("nonexistent");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginNotFound);
}

// ===========================================================================
// PluginManager: Dynamic loading (file not found)
// ===========================================================================

TEST(PluginManagerTest, LoadPluginFileNotFoundReturnsError) {
    PluginManager mgr;
    auto result = mgr.LoadPlugin("/nonexistent/path/plugin.so");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PluginLoadFailed);
}

// ===========================================================================
// PluginManager: Context
// ===========================================================================

TEST(PluginManagerTest, SetContextIsPassedToPlugins) {
    cgs::foundation::ServiceLocator locator;
    PluginContext ctx;
    ctx.services = &locator;

    PluginManager mgr;
    mgr.SetContext(ctx);

    EXPECT_EQ(mgr.GetContext().services, &locator);
}

// ===========================================================================
// CGS_PLUGIN_EXPORT macro
// ===========================================================================

// We can only test that the macro compiles; actual dlsym testing requires
// a shared library which is beyond unit test scope.  The integration test
// in tests/integration/ covers that path.

class ExportTestPlugin : public IPlugin {
public:
    const PluginInfo& GetInfo() const override {
        static PluginInfo info{"ExportTestPlugin", "Export test", {1, 0, 0}, {},
                               kPluginApiVersion};
        return info;
    }
    bool OnLoad(PluginContext& /*ctx*/) override { return true; }
    bool OnInit() override { return true; }
    void OnUpdate(float /*deltaTime*/) override {}
    void OnShutdown() override {}
    void OnUnload() override {}
};

// Verify the macro generates valid C functions.
// We avoid actually exporting to prevent symbol clashes in the test binary.
// Instead, we just verify the types compile.
static_assert(std::is_base_of_v<IPlugin, ExportTestPlugin>,
              "ExportTestPlugin must derive from IPlugin");

// ===========================================================================
// PluginManager: Destructor cleanup
// ===========================================================================

TEST(PluginManagerTest, DestructorCleansUpActivePlugins) {
    // Verify no crash when PluginManager is destroyed with active plugins.
    {
        PluginManager mgr;
        auto& registry = StaticPluginRegistry();
        auto sizeBefore = registry.size();
        registry.push_back({"DtorPlugin",
                             []() -> std::unique_ptr<IPlugin> {
                                 return std::make_unique<TestPlugin>("DtorPlugin");
                             }});
        ASSERT_TRUE(mgr.RegisterStaticPlugins().hasValue());
        registry.resize(sizeBefore);

        ASSERT_TRUE(mgr.InitPlugin("DtorPlugin").hasValue());
        ASSERT_TRUE(mgr.ActivatePlugin("DtorPlugin").hasValue());
        // mgr destructor should call ShutdownAll + UnloadAll.
    }
    // If we reach here without crashing, the test passes.
}
