/// @file hot_reload_test.cpp
/// @brief Unit tests for FileWatcher, HotReloadManager, and IHotReloadable.

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "cgs/plugin/file_watcher.hpp"
#include "cgs/plugin/hot_reload_manager.hpp"
#include "cgs/plugin/hot_reloadable.hpp"
#include "cgs/plugin/iplugin.hpp"
#include "cgs/plugin/plugin_manager.hpp"

using namespace cgs::plugin;
namespace fs = std::filesystem;

// ============================================================================
// Helper: Create a temporary file for testing
// ============================================================================

namespace {

class TempFile {
public:
    explicit TempFile(const std::string& name) {
        path_ = fs::temp_directory_path() / ("cgs_test_" + name);
        std::ofstream ofs(path_);
        ofs << "test content";
        ofs.close();
    }

    ~TempFile() {
        std::error_code ec;
        fs::remove(path_, ec);
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    [[nodiscard]] const fs::path& Path() const { return path_; }

    void Touch() {
        // Write new content to update modification time.
        std::ofstream ofs(path_, std::ios::app);
        ofs << ".";
        ofs.close();
    }

private:
    fs::path path_;
};

} // anonymous namespace

// ============================================================================
// FileWatcher Tests
// ============================================================================

class FileWatcherTest : public ::testing::Test {
protected:
    FileWatcher watcher_;
};

TEST_F(FileWatcherTest, InitialStateIsEmpty) {
    EXPECT_EQ(watcher_.WatchCount(), 0u);
}

TEST_F(FileWatcherTest, WatchExistingFile) {
    TempFile tmp("watch_test.txt");
    EXPECT_TRUE(watcher_.Watch(tmp.Path()));
    EXPECT_EQ(watcher_.WatchCount(), 1u);
    EXPECT_TRUE(watcher_.IsWatching(tmp.Path()));
}

TEST_F(FileWatcherTest, WatchNonExistentFileReturnsFalse) {
    EXPECT_FALSE(watcher_.Watch("/nonexistent/path/to/file.so"));
    EXPECT_EQ(watcher_.WatchCount(), 0u);
}

TEST_F(FileWatcherTest, UnwatchRemovesFile) {
    TempFile tmp("unwatch_test.txt");
    watcher_.Watch(tmp.Path());
    EXPECT_EQ(watcher_.WatchCount(), 1u);

    watcher_.Unwatch(tmp.Path());
    EXPECT_EQ(watcher_.WatchCount(), 0u);
    EXPECT_FALSE(watcher_.IsWatching(tmp.Path()));
}

TEST_F(FileWatcherTest, UnwatchAllClearsEverything) {
    TempFile tmp1("unwatch_all_1.txt");
    TempFile tmp2("unwatch_all_2.txt");
    watcher_.Watch(tmp1.Path());
    watcher_.Watch(tmp2.Path());
    EXPECT_EQ(watcher_.WatchCount(), 2u);

    watcher_.UnwatchAll();
    EXPECT_EQ(watcher_.WatchCount(), 0u);
}

TEST_F(FileWatcherTest, PollWithNoChangesNoCallback) {
    TempFile tmp("no_change.txt");
    int callCount = 0;
    watcher_.SetCallback([&](const fs::path&) { callCount++; });
    watcher_.Watch(tmp.Path());

    watcher_.Poll();
    EXPECT_EQ(callCount, 0);
}

TEST_F(FileWatcherTest, PollDetectsFileModification) {
    TempFile tmp("detect_change.txt");
    watcher_.SetDebounceMs(0); // Disable debounce for test.

    fs::path changedPath;
    watcher_.SetCallback([&](const fs::path& p) { changedPath = p; });
    watcher_.Watch(tmp.Path());

    // Wait briefly to ensure filesystem timestamp resolution.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tmp.Touch();

    watcher_.Poll();
    EXPECT_EQ(changedPath, tmp.Path());
}

TEST_F(FileWatcherTest, DebounceCoalescesChanges) {
    TempFile tmp("debounce_test.txt");
    watcher_.SetDebounceMs(100);

    int callCount = 0;
    watcher_.SetCallback([&](const fs::path&) { callCount++; });
    watcher_.Watch(tmp.Path());

    // Modify the file.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tmp.Touch();

    // Poll immediately — debounce hasn't elapsed yet.
    watcher_.Poll();
    EXPECT_EQ(callCount, 0);

    // Wait for debounce to elapse.
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    watcher_.Poll();
    EXPECT_EQ(callCount, 1);
}

TEST_F(FileWatcherTest, MultiplePollsAfterChangeCallOnce) {
    TempFile tmp("multi_poll.txt");
    watcher_.SetDebounceMs(0);

    int callCount = 0;
    watcher_.SetCallback([&](const fs::path&) { callCount++; });
    watcher_.Watch(tmp.Path());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tmp.Touch();

    watcher_.Poll();
    EXPECT_EQ(callCount, 1);

    // Second poll without further changes — no callback.
    watcher_.Poll();
    EXPECT_EQ(callCount, 1);
}

TEST_F(FileWatcherTest, WatchMultipleFiles) {
    TempFile tmp1("multi_1.txt");
    TempFile tmp2("multi_2.txt");
    watcher_.SetDebounceMs(0);

    std::vector<std::string> changed;
    watcher_.SetCallback([&](const fs::path& p) {
        changed.push_back(p.filename().string());
    });
    watcher_.Watch(tmp1.Path());
    watcher_.Watch(tmp2.Path());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tmp1.Touch();
    tmp2.Touch();

    watcher_.Poll();
    EXPECT_EQ(changed.size(), 2u);
}

// ============================================================================
// IHotReloadable Tests
// ============================================================================

namespace {

/// Test plugin that implements IHotReloadable.
class ReloadablePlugin : public IPlugin, public IHotReloadable {
public:
    explicit ReloadablePlugin(std::string name = "ReloadablePlugin")
        : info_{std::move(name), "Test reloadable plugin",
                {1, 0, 0}, {}, kPluginApiVersion} {}

    const PluginInfo& GetInfo() const override { return info_; }
    bool OnLoad(PluginContext&) override { return true; }
    bool OnInit() override { return true; }
    void OnUpdate(float) override {}
    void OnShutdown() override {}
    void OnUnload() override {}

    // IHotReloadable
    cgs::foundation::GameResult<std::vector<uint8_t>>
    SerializeState() override {
        std::vector<uint8_t> data(sizeof(counter_));
        std::memcpy(data.data(), &counter_, sizeof(counter_));
        return cgs::foundation::GameResult<std::vector<uint8_t>>::ok(
            std::move(data));
    }

    cgs::foundation::GameResult<void>
    DeserializeState(const uint8_t* data, std::size_t size) override {
        if (size != sizeof(counter_)) {
            return cgs::foundation::GameResult<void>::err(
                cgs::foundation::GameError(
                    cgs::foundation::ErrorCode::StateDeserializationFailed,
                    "Invalid state size"));
        }
        std::memcpy(&counter_, data, sizeof(counter_));
        return cgs::foundation::GameResult<void>::ok();
    }

    uint32_t GetStateVersion() const override { return stateVersion_; }

    int counter_ = 0;
    uint32_t stateVersion_ = 1;

private:
    PluginInfo info_;
};

/// Test plugin that does NOT implement IHotReloadable.
class NonReloadablePlugin : public IPlugin {
public:
    const PluginInfo& GetInfo() const override { return info_; }
    bool OnLoad(PluginContext&) override { return true; }
    bool OnInit() override { return true; }
    void OnUpdate(float) override {}
    void OnShutdown() override {}
    void OnUnload() override {}

private:
    PluginInfo info_{"NonReloadable", "Non-reloadable test plugin",
                     {1, 0, 0}, {}, kPluginApiVersion};
};

} // anonymous namespace

TEST(HotReloadableTest, SerializeAndDeserializeRoundTrip) {
    ReloadablePlugin plugin;
    plugin.counter_ = 42;

    auto serResult = plugin.SerializeState();
    ASSERT_TRUE(serResult.hasValue());
    auto data = std::move(serResult).value();
    EXPECT_EQ(data.size(), sizeof(int));

    ReloadablePlugin plugin2;
    EXPECT_EQ(plugin2.counter_, 0);

    auto desResult = plugin2.DeserializeState(data.data(), data.size());
    ASSERT_TRUE(desResult.hasValue());
    EXPECT_EQ(plugin2.counter_, 42);
}

TEST(HotReloadableTest, DeserializeInvalidSizeFails) {
    ReloadablePlugin plugin;
    std::vector<uint8_t> badData = {0x01};

    auto result = plugin.DeserializeState(badData.data(), badData.size());
    EXPECT_TRUE(result.hasError());
}

TEST(HotReloadableTest, DynamicCastDetectsReloadable) {
    ReloadablePlugin reloadable;
    NonReloadablePlugin nonReloadable;

    IPlugin* p1 = &reloadable;
    IPlugin* p2 = &nonReloadable;

    EXPECT_NE(dynamic_cast<IHotReloadable*>(p1), nullptr);
    EXPECT_EQ(dynamic_cast<IHotReloadable*>(p2), nullptr);
}

// ============================================================================
// HotReloadManager Tests
// ============================================================================

class HotReloadManagerTest : public ::testing::Test {
protected:
    PluginManager pluginManager_;
    HotReloadManager hotReloadManager_{pluginManager_};
};

TEST_F(HotReloadManagerTest, IsAvailableMatchesBuildFlag) {
#ifdef CGS_HOT_RELOAD
    EXPECT_TRUE(HotReloadManager::IsAvailable());
#else
    EXPECT_FALSE(HotReloadManager::IsAvailable());
#endif
}

TEST_F(HotReloadManagerTest, InitialState) {
    EXPECT_EQ(hotReloadManager_.WatchedPluginCount(), 0u);
    EXPECT_EQ(hotReloadManager_.ReloadCount(), 0u);
    EXPECT_EQ(hotReloadManager_.GetSnapshot("any"), nullptr);
}

TEST_F(HotReloadManagerTest, WatchPluginBehavior) {
    TempFile tmp("watch_plugin.so");

    auto result = hotReloadManager_.WatchPlugin("TestPlugin", tmp.Path());

#ifdef CGS_HOT_RELOAD
    ASSERT_TRUE(result.hasValue()) << result.error().message();
    EXPECT_EQ(hotReloadManager_.WatchedPluginCount(), 1u);
#else
    // When hot reload is disabled, WatchPlugin returns an error.
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(hotReloadManager_.WatchedPluginCount(), 0u);
#endif
}

TEST_F(HotReloadManagerTest, WatchPluginNonExistentFileFails) {
    auto result = hotReloadManager_.WatchPlugin(
        "TestPlugin", "/nonexistent/plugin.so");
    EXPECT_TRUE(result.hasError());
}

TEST_F(HotReloadManagerTest, UnwatchPluginRemovesWatch) {
    TempFile tmp("unwatch_plugin.so");
    (void)hotReloadManager_.WatchPlugin("TestPlugin", tmp.Path());

    hotReloadManager_.UnwatchPlugin("TestPlugin");
    EXPECT_EQ(hotReloadManager_.WatchedPluginCount(), 0u);
}

TEST_F(HotReloadManagerTest, PollWithNoChangesIsNoOp) {
    // Should not crash even with no watched plugins.
    hotReloadManager_.Poll();
    EXPECT_EQ(hotReloadManager_.ReloadCount(), 0u);
}

TEST_F(HotReloadManagerTest, ReloadPluginNotWatchedFails) {
    auto result = hotReloadManager_.ReloadPlugin("NonExistent");
    EXPECT_TRUE(result.hasError());
}

TEST_F(HotReloadManagerTest, SetDebounceMsDoesNotCrash) {
    hotReloadManager_.SetDebounceMs(500);
    // Just verify it doesn't crash.
}

// ============================================================================
// PluginStateSnapshot Tests
// ============================================================================

TEST(PluginStateSnapshotTest, DefaultConstruction) {
    PluginStateSnapshot snapshot;
    EXPECT_TRUE(snapshot.pluginName.empty());
    EXPECT_EQ(snapshot.stateVersion, 0u);
    EXPECT_TRUE(snapshot.data.empty());
}

TEST(PluginStateSnapshotTest, PopulatedSnapshot) {
    PluginStateSnapshot snapshot;
    snapshot.pluginName = "TestPlugin";
    snapshot.pluginVersion = {1, 2, 3};
    snapshot.stateVersion = 1;
    snapshot.data = {0xDE, 0xAD, 0xBE, 0xEF};
    snapshot.capturedAt = std::chrono::steady_clock::now();

    EXPECT_EQ(snapshot.pluginName, "TestPlugin");
    EXPECT_EQ(snapshot.pluginVersion.major, 1);
    EXPECT_EQ(snapshot.pluginVersion.minor, 2);
    EXPECT_EQ(snapshot.pluginVersion.patch, 3);
    EXPECT_EQ(snapshot.stateVersion, 1u);
    EXPECT_EQ(snapshot.data.size(), 4u);
}
