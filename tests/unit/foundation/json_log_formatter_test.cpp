/// @file json_log_formatter_test.cpp
/// @brief Unit tests for JsonLogFormatter and correlation ID utilities (SRS-NFR-019).

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "cgs/foundation/json_log_formatter.hpp"

// kcenon headers for MockLogger
#include <kcenon/common/interfaces/global_logger_registry.h>
#include <kcenon/common/interfaces/logger_interface.h>

using namespace cgs::foundation;
using kcenon::common::interfaces::GlobalLoggerRegistry;
using kcenon::common::interfaces::ILogger;
using kcenon::common::interfaces::log_level;

// ---------------------------------------------------------------------------
// MockLogger (reused pattern from logger_adapter_test.cpp)
// ---------------------------------------------------------------------------

struct LogRecord {
    log_level level;
    std::string message;
};

class MockLogger : public ILogger {
public:
    kcenon::common::VoidResult log(log_level level,
                                    const std::string& message) override {
        std::lock_guard lock(mutex_);
        records_.push_back({level, message});
        return kcenon::common::VoidResult::ok(std::monostate{});
    }

    kcenon::common::VoidResult log(
        log_level level, std::string_view message,
        const kcenon::common::interfaces::source_location& /*loc*/) override {
        return log(level, std::string(message));
    }

    kcenon::common::VoidResult log(
        const kcenon::common::interfaces::log_entry& entry) override {
        return log(entry.level, entry.message);
    }

    bool is_enabled(log_level /*level*/) const override { return true; }

    kcenon::common::VoidResult set_level(log_level /*level*/) override {
        return kcenon::common::VoidResult::ok(std::monostate{});
    }

    log_level get_level() const override { return log_level::trace; }

    kcenon::common::VoidResult flush() override {
        return kcenon::common::VoidResult::ok(std::monostate{});
    }

    std::vector<LogRecord> records() const {
        std::lock_guard lock(mutex_);
        return records_;
    }

    void reset() {
        std::lock_guard lock(mutex_);
        records_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::vector<LogRecord> records_;
};

// ===========================================================================
// JsonLogFormatter Tests
// ===========================================================================

TEST(JsonLogFormatterTest, ProducesValidJsonStructure) {
    auto json = JsonLogFormatter::format(
        LogLevel::Info, LogCategory::Core, "Server starting");

    // Must start with { and end with }
    ASSERT_FALSE(json.empty());
    EXPECT_EQ(json.front(), '{');
    EXPECT_EQ(json.back(), '}');

    // Must contain required fields
    EXPECT_NE(json.find("\"timestamp\""), std::string::npos);
    EXPECT_NE(json.find("\"level\""), std::string::npos);
    EXPECT_NE(json.find("\"category\""), std::string::npos);
    EXPECT_NE(json.find("\"message\""), std::string::npos);
}

TEST(JsonLogFormatterTest, ContainsCorrectLevelAndCategory) {
    auto json = JsonLogFormatter::format(
        LogLevel::Error, LogCategory::Network, "Connection lost");

    EXPECT_NE(json.find("\"level\":\"ERROR\""), std::string::npos);
    EXPECT_NE(json.find("\"category\":\"Network\""), std::string::npos);
    EXPECT_NE(json.find("\"message\":\"Connection lost\""), std::string::npos);
}

TEST(JsonLogFormatterTest, AllLogLevelsFormatCorrectly) {
    struct TestCase {
        LogLevel level;
        const char* expected;
    };
    TestCase cases[] = {
        {LogLevel::Trace,    "\"level\":\"TRACE\""},
        {LogLevel::Debug,    "\"level\":\"DEBUG\""},
        {LogLevel::Info,     "\"level\":\"INFO\""},
        {LogLevel::Warning,  "\"level\":\"WARNING\""},
        {LogLevel::Error,    "\"level\":\"ERROR\""},
        {LogLevel::Critical, "\"level\":\"CRITICAL\""},
    };

    for (const auto& tc : cases) {
        auto json = JsonLogFormatter::format(tc.level, LogCategory::Core, "test");
        EXPECT_NE(json.find(tc.expected), std::string::npos)
            << "Expected " << tc.expected << " in: " << json;
    }
}

TEST(JsonLogFormatterTest, TimestampIsIso8601) {
    auto json = JsonLogFormatter::format(
        LogLevel::Info, LogCategory::Core, "test");

    // Match ISO 8601 pattern: YYYY-MM-DDTHH:MM:SS.mmmZ
    std::regex isoPattern(
        R"RE("timestamp":"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z)")RE");
    EXPECT_TRUE(std::regex_search(json, isoPattern))
        << "Timestamp not in ISO 8601 format: " << json;
}

TEST(JsonLogFormatterTest, IncludesContextFields) {
    LogContext ctx;
    ctx.entityId = EntityId(100);
    ctx.playerId = PlayerId(42);
    ctx.sessionId = SessionId(999);
    ctx.extra["damage"] = "150";

    auto json = JsonLogFormatter::format(
        LogLevel::Debug, LogCategory::Combat, "Hit", ctx);

    EXPECT_NE(json.find("\"entity_id\":100"), std::string::npos);
    EXPECT_NE(json.find("\"player_id\":42"), std::string::npos);
    EXPECT_NE(json.find("\"session_id\":999"), std::string::npos);
    EXPECT_NE(json.find("\"extra\":{"), std::string::npos);
    EXPECT_NE(json.find("\"damage\":\"150\""), std::string::npos);
}

TEST(JsonLogFormatterTest, OmitsEmptyContextFields) {
    LogContext ctx; // All fields empty

    auto json = JsonLogFormatter::format(
        LogLevel::Info, LogCategory::Core, "No context", ctx);

    EXPECT_EQ(json.find("\"entity_id\""), std::string::npos);
    EXPECT_EQ(json.find("\"player_id\""), std::string::npos);
    EXPECT_EQ(json.find("\"session_id\""), std::string::npos);
    EXPECT_EQ(json.find("\"extra\""), std::string::npos);
    EXPECT_EQ(json.find("\"correlation_id\""), std::string::npos);
}

TEST(JsonLogFormatterTest, IncludesTraceIdAsCorrelationId) {
    LogContext ctx;
    ctx.traceId = "req-abc-123";

    auto json = JsonLogFormatter::format(
        LogLevel::Info, LogCategory::Core, "test", ctx);

    EXPECT_NE(json.find("\"correlation_id\":\"req-abc-123\""), std::string::npos);
}

TEST(JsonLogFormatterTest, EscapesSpecialCharacters) {
    auto json = JsonLogFormatter::format(
        LogLevel::Info, LogCategory::Core,
        "message with \"quotes\" and \\backslash and\nnewline");

    // Verify the special chars are escaped
    EXPECT_NE(json.find("\\\"quotes\\\""), std::string::npos);
    EXPECT_NE(json.find("\\\\backslash"), std::string::npos);
    EXPECT_NE(json.find("\\n"), std::string::npos);

    // Verify overall structure is still valid (starts/ends correctly)
    EXPECT_EQ(json.front(), '{');
    EXPECT_EQ(json.back(), '}');
}

TEST(JsonLogFormatterTest, SingleLineOutput) {
    LogContext ctx;
    ctx.playerId = PlayerId(1);
    ctx.extra["key"] = "value";

    auto json = JsonLogFormatter::format(
        LogLevel::Info, LogCategory::Core, "test", ctx);

    // Must be a single line (no embedded newlines)
    EXPECT_EQ(json.find('\n'), std::string::npos);
}

// ===========================================================================
// Correlation ID Generation Tests
// ===========================================================================

TEST(CorrelationIdTest, GeneratesUuidV4Format) {
    auto id = generateCorrelationId();

    // UUID v4 format: xxxxxxxx-xxxx-4xxx-[89ab]xxx-xxxxxxxxxxxx
    std::regex uuidPattern(
        R"([0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12})");
    EXPECT_TRUE(std::regex_match(id, uuidPattern))
        << "Invalid UUID v4 format: " << id;
}

TEST(CorrelationIdTest, GeneratesUniqueIds) {
    constexpr int kCount = 1000;
    std::set<std::string> ids;

    for (int i = 0; i < kCount; ++i) {
        ids.insert(generateCorrelationId());
    }

    EXPECT_EQ(ids.size(), static_cast<std::size_t>(kCount))
        << "Duplicate correlation IDs generated";
}

TEST(CorrelationIdTest, ThreadSafeGeneration) {
    constexpr int kThreads = 8;
    constexpr int kPerThread = 100;

    std::vector<std::vector<std::string>> results(kThreads);
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&results, t] {
            for (int i = 0; i < kPerThread; ++i) {
                results[static_cast<std::size_t>(t)].push_back(
                    generateCorrelationId());
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // All IDs should be unique across all threads
    std::set<std::string> allIds;
    for (const auto& threadIds : results) {
        for (const auto& id : threadIds) {
            allIds.insert(id);
        }
    }
    EXPECT_EQ(allIds.size(), static_cast<std::size_t>(kThreads * kPerThread));
}

// ===========================================================================
// CorrelationScope Tests
// ===========================================================================

TEST(CorrelationScopeTest, SetsAndRestoresCorrelationId) {
    // Initially empty
    EXPECT_TRUE(CorrelationScope::current().empty());

    {
        CorrelationScope scope("req-001");
        EXPECT_EQ(CorrelationScope::current(), "req-001");
    }

    // Restored to empty
    EXPECT_TRUE(CorrelationScope::current().empty());
}

TEST(CorrelationScopeTest, NestingPreservesOuterScope) {
    {
        CorrelationScope outer("outer-id");
        EXPECT_EQ(CorrelationScope::current(), "outer-id");

        {
            CorrelationScope inner("inner-id");
            EXPECT_EQ(CorrelationScope::current(), "inner-id");
        }

        EXPECT_EQ(CorrelationScope::current(), "outer-id");
    }

    EXPECT_TRUE(CorrelationScope::current().empty());
}

TEST(CorrelationScopeTest, ThreadLocalIsolation) {
    std::atomic<bool> thread1Ready{false};
    std::atomic<bool> thread2Done{false};
    std::string thread2Value;

    CorrelationScope scope("main-thread-id");

    std::thread t([&] {
        // This thread should NOT see the main thread's correlation ID
        EXPECT_TRUE(CorrelationScope::current().empty());

        CorrelationScope threadScope("thread-id");
        thread2Value = CorrelationScope::current();
        thread1Ready.store(true);

        // Wait until main checks its value
        while (!thread2Done.load()) {
            std::this_thread::yield();
        }
    });

    while (!thread1Ready.load()) {
        std::this_thread::yield();
    }

    // Main thread should still have its own ID
    EXPECT_EQ(CorrelationScope::current(), "main-thread-id");
    EXPECT_EQ(thread2Value, "thread-id");

    thread2Done.store(true);
    t.join();
}

TEST(CorrelationScopeTest, JsonFormatterUsesThreadLocalId) {
    CorrelationScope scope("auto-corr-id");

    auto json = JsonLogFormatter::format(
        LogLevel::Info, LogCategory::Core, "test");

    EXPECT_NE(json.find("\"correlation_id\":\"auto-corr-id\""), std::string::npos)
        << "Thread-local correlation ID not included: " << json;
}

TEST(CorrelationScopeTest, ContextTraceIdTakesPrecedence) {
    CorrelationScope scope("thread-id");

    LogContext ctx;
    ctx.traceId = "explicit-id";

    auto json = JsonLogFormatter::format(
        LogLevel::Info, LogCategory::Core, "test", ctx);

    EXPECT_NE(json.find("\"correlation_id\":\"explicit-id\""), std::string::npos)
        << "Context traceId should take precedence: " << json;
    EXPECT_EQ(json.find("thread-id"), std::string::npos)
        << "Thread-local ID should not appear when context traceId is set";
}

// ===========================================================================
// GameLogger JSON Mode Integration Tests
// ===========================================================================

class GameLoggerJsonTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& registry = GlobalLoggerRegistry::instance();
        registry.clear();
        mockLogger_ = std::make_shared<MockLogger>();
        registry.set_default_logger(mockLogger_);
    }

    void TearDown() override {
        GlobalLoggerRegistry::instance().clear();
    }

    std::shared_ptr<MockLogger> mockLogger_;
};

TEST_F(GameLoggerJsonTest, JsonModeDefaultsToOff) {
    GameLogger logger;
    EXPECT_FALSE(logger.isJsonMode());
}

TEST_F(GameLoggerJsonTest, SetJsonModeEnablesJsonOutput) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Info);
    logger.setJsonMode(true);
    EXPECT_TRUE(logger.isJsonMode());

    logger.log(LogLevel::Info, LogCategory::Core, "JSON test");

    auto records = mockLogger_->records();
    ASSERT_EQ(records.size(), 1u);

    const auto& msg = records[0].message;
    EXPECT_EQ(msg.front(), '{');
    EXPECT_EQ(msg.back(), '}');
    EXPECT_NE(msg.find("\"level\":\"INFO\""), std::string::npos);
    EXPECT_NE(msg.find("\"category\":\"Core\""), std::string::npos);
    EXPECT_NE(msg.find("\"message\":\"JSON test\""), std::string::npos);
}

TEST_F(GameLoggerJsonTest, JsonModeWithContext) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Combat, LogLevel::Debug);
    logger.setJsonMode(true);

    LogContext ctx;
    ctx.playerId = PlayerId(42);
    ctx.traceId = "corr-123";

    logger.logWithContext(LogLevel::Debug, LogCategory::Combat,
                          "Damage", ctx);

    auto records = mockLogger_->records();
    ASSERT_EQ(records.size(), 1u);

    const auto& msg = records[0].message;
    EXPECT_NE(msg.find("\"correlation_id\":\"corr-123\""), std::string::npos);
    EXPECT_NE(msg.find("\"player_id\":42"), std::string::npos);
}

TEST_F(GameLoggerJsonTest, JsonModeWithCorrelationScope) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Info);
    logger.setJsonMode(true);

    CorrelationScope scope(generateCorrelationId());

    logger.log(LogLevel::Info, LogCategory::Core, "Scoped log");

    auto records = mockLogger_->records();
    ASSERT_EQ(records.size(), 1u);

    const auto& msg = records[0].message;
    EXPECT_NE(msg.find("\"correlation_id\""), std::string::npos)
        << "Auto correlation ID missing: " << msg;
}

TEST_F(GameLoggerJsonTest, TextModeUnchangedWhenJsonOff) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Info);
    logger.setJsonMode(false);

    logger.log(LogLevel::Info, LogCategory::Core, "Text test");

    auto records = mockLogger_->records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].message, "[Core] Text test");
}
