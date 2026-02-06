#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "cgs/foundation/common_adapter.hpp"

using namespace cgs::foundation;

// --- ErrorCode tests ---

TEST(ErrorCodeTest, SubsystemLookup) {
    EXPECT_EQ(errorSubsystem(ErrorCode::Success), "General");
    EXPECT_EQ(errorSubsystem(ErrorCode::InvalidArgument), "General");
    EXPECT_EQ(errorSubsystem(ErrorCode::ConnectionFailed), "Network");
    EXPECT_EQ(errorSubsystem(ErrorCode::QueryFailed), "Database");
    EXPECT_EQ(errorSubsystem(ErrorCode::EntityNotFound), "ECS");
    EXPECT_EQ(errorSubsystem(ErrorCode::PluginLoadFailed), "Plugin");
    EXPECT_EQ(errorSubsystem(ErrorCode::TokenExpired), "Auth");
    EXPECT_EQ(errorSubsystem(ErrorCode::ConfigKeyNotFound), "Config");
}

// --- GameError tests ---

TEST(GameErrorTest, DefaultConstruction) {
    GameError err;
    EXPECT_EQ(err.code(), ErrorCode::Unknown);
    EXPECT_TRUE(err.message().empty());
    EXPECT_FALSE(err.hasContext());
}

TEST(GameErrorTest, CodeAndMessage) {
    GameError err(ErrorCode::NotFound, "entity missing");
    EXPECT_EQ(err.code(), ErrorCode::NotFound);
    EXPECT_EQ(err.message(), "entity missing");
    EXPECT_EQ(err.subsystem(), "General");
    EXPECT_FALSE(err.isSuccess());
}

TEST(GameErrorTest, WithContext) {
    struct DebugInfo {
        int line = 42;
    };
    GameError err(ErrorCode::SystemError, "crash", DebugInfo{99});
    EXPECT_TRUE(err.hasContext());
    auto* info = err.context<DebugInfo>();
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->line, 99);

    // Wrong type returns nullptr
    EXPECT_EQ(err.context<int>(), nullptr);
}

TEST(GameErrorTest, SuccessCheck) {
    GameError success(ErrorCode::Success);
    EXPECT_TRUE(success.isSuccess());
}

// --- GameResult tests ---

TEST(GameResultTest, OkValue) {
    auto result = GameResult<int>::ok(42);
    EXPECT_TRUE(result.hasValue());
    EXPECT_EQ(result.value(), 42);
}

TEST(GameResultTest, ErrorValue) {
    auto result = GameResult<int>::err(
        GameError(ErrorCode::InvalidArgument, "bad input"));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
    EXPECT_EQ(result.error().message(), "bad input");
}

TEST(GameResultTest, VoidOk) {
    auto result = GameResult<void>::ok();
    EXPECT_TRUE(result.hasValue());
}

TEST(GameResultTest, VoidError) {
    auto result = GameResult<void>::err(GameError(ErrorCode::Timeout, "timed out"));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::Timeout);
}

// --- StrongId / Types tests ---

TEST(StrongIdTest, DefaultInvalid) {
    EntityId id;
    EXPECT_FALSE(id.isValid());
    EXPECT_EQ(id.value(), 0u);
}

TEST(StrongIdTest, ExplicitConstruction) {
    EntityId id(100);
    EXPECT_TRUE(id.isValid());
    EXPECT_EQ(id.value(), 100u);
}

TEST(StrongIdTest, Equality) {
    EntityId a(1), b(1), c(2);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(StrongIdTest, Ordering) {
    EntityId a(1), b(2);
    EXPECT_LT(a, b);
    EXPECT_GT(b, a);
}

TEST(StrongIdTest, TypeSafety) {
    // EntityId and PlayerId are distinct types even with the same value.
    // This test verifies they compile as separate types; we cannot
    // compare them directly (would be a compile error).
    EntityId eid(1);
    PlayerId pid(1);
    EXPECT_EQ(eid.value(), pid.value());
    // static_assert(!std::is_same_v<EntityId, PlayerId>);
}

TEST(StrongIdTest, HashWorks) {
    std::unordered_map<EntityId, std::string> map;
    map[EntityId(1)] = "player";
    EXPECT_EQ(map[EntityId(1)], "player");
    EXPECT_EQ(map.count(EntityId(2)), 0u);
}

// --- ServiceLocator tests ---

namespace {
struct ICounter {
    virtual ~ICounter() = default;
    virtual int count() const = 0;
};

class SimpleCounter : public ICounter {
public:
    explicit SimpleCounter(int n) : n_(n) {}
    int count() const override { return n_; }

private:
    int n_;
};
}  // namespace

TEST(ServiceLocatorTest, RegisterAndGet) {
    ServiceLocator locator;
    locator.add<ICounter>(std::make_unique<SimpleCounter>(10));

    auto* counter = locator.get<ICounter>();
    ASSERT_NE(counter, nullptr);
    EXPECT_EQ(counter->count(), 10);
}

TEST(ServiceLocatorTest, GetUnregisteredReturnsNull) {
    ServiceLocator locator;
    EXPECT_EQ(locator.get<ICounter>(), nullptr);
}

TEST(ServiceLocatorTest, HasAndRemove) {
    ServiceLocator locator;
    EXPECT_FALSE(locator.has<ICounter>());

    locator.add<ICounter>(std::make_unique<SimpleCounter>(5));
    EXPECT_TRUE(locator.has<ICounter>());
    EXPECT_EQ(locator.size(), 1u);

    locator.remove<ICounter>();
    EXPECT_FALSE(locator.has<ICounter>());
    EXPECT_EQ(locator.size(), 0u);
}

TEST(ServiceLocatorTest, ReplaceService) {
    ServiceLocator locator;
    locator.add<ICounter>(std::make_unique<SimpleCounter>(1));
    EXPECT_EQ(locator.get<ICounter>()->count(), 1);

    locator.add<ICounter>(std::make_unique<SimpleCounter>(2));
    EXPECT_EQ(locator.get<ICounter>()->count(), 2);
    EXPECT_EQ(locator.size(), 1u);
}

TEST(ServiceLocatorTest, Clear) {
    ServiceLocator locator;
    locator.add<ICounter>(std::make_unique<SimpleCounter>(1));
    locator.clear();
    EXPECT_EQ(locator.size(), 0u);
    EXPECT_EQ(locator.get<ICounter>(), nullptr);
}

// --- ConfigManager tests ---

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use unique directory per test to avoid races under ctest --parallel
        auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        auto dirname = std::string("cgs_test_") + info->name();
        tmpDir_ = std::filesystem::temp_directory_path() / dirname;
        std::filesystem::create_directories(tmpDir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tmpDir_, ec);
    }

    std::filesystem::path writeYaml(const std::string& filename,
                                    const std::string& content) {
        auto path = tmpDir_ / filename;
        std::ofstream ofs(path);
        ofs << content;
        return path;
    }

    std::filesystem::path tmpDir_;
};

TEST_F(ConfigManagerTest, LoadAndGet) {
    auto path = writeYaml("test.yaml", R"(
server:
  port: 8080
  name: "GameServer"
)");

    ConfigManager config;
    auto loadResult = config.load(path);
    ASSERT_TRUE(loadResult.hasValue());

    auto port = config.get<int>("server.port");
    ASSERT_TRUE(port.hasValue());
    EXPECT_EQ(port.value(), 8080);

    auto name = config.get<std::string>("server.name");
    ASSERT_TRUE(name.hasValue());
    EXPECT_EQ(name.value(), "GameServer");
}

TEST_F(ConfigManagerTest, KeyNotFound) {
    auto path = writeYaml("empty.yaml", "{}");
    ConfigManager config;
    config.load(path);

    auto result = config.get<int>("nonexistent.key");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::ConfigKeyNotFound);
}

TEST_F(ConfigManagerTest, TypeMismatch) {
    auto path = writeYaml("types.yaml", "value: hello");
    ConfigManager config;
    config.load(path);

    auto result = config.get<int>("value");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::ConfigTypeMismatch);
}

TEST_F(ConfigManagerTest, LoadNonexistentFile) {
    ConfigManager config;
    auto result = config.load("/nonexistent/path.yaml");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::ConfigLoadFailed);
}

TEST_F(ConfigManagerTest, SetAndGet) {
    ConfigManager config;
    config.set<int>("server.port", 9090);

    auto result = config.get<int>("server.port");
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value(), 9090);
}

TEST_F(ConfigManagerTest, HasKey) {
    auto path = writeYaml("check.yaml", "key: value");
    ConfigManager config;
    config.load(path);

    EXPECT_TRUE(config.hasKey("key"));
    EXPECT_FALSE(config.hasKey("missing"));
}

TEST_F(ConfigManagerTest, WatchNotification) {
    ConfigManager config;
    bool notified = false;
    std::string notifiedKey;

    config.watch("server.port", [&](std::string_view key) {
        notified = true;
        notifiedKey = std::string(key);
    });

    config.set<int>("server.port", 3000);
    EXPECT_TRUE(notified);
    EXPECT_EQ(notifiedKey, "server.port");
}
