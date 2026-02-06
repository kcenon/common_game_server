#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_logger.hpp"

// kcenon headers for test infrastructure (mock logger registration)
#include <kcenon/common/interfaces/global_logger_registry.h>
#include <kcenon/common/interfaces/logger_interface.h>

using namespace cgs::foundation;
using kcenon::common::interfaces::log_level;
using kcenon::common::interfaces::ILogger;
using kcenon::common::interfaces::GlobalLoggerRegistry;

// ---------------------------------------------------------------------------
// MockLogger: captures log messages for assertion
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
        logCount_.fetch_add(1, std::memory_order_relaxed);
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

    bool is_enabled(log_level level) const override {
        return level >= minLevel_.load(std::memory_order_acquire);
    }

    kcenon::common::VoidResult set_level(log_level level) override {
        minLevel_.store(level, std::memory_order_release);
        return kcenon::common::VoidResult::ok(std::monostate{});
    }

    log_level get_level() const override {
        return minLevel_.load(std::memory_order_acquire);
    }

    kcenon::common::VoidResult flush() override {
        flushed_.store(true, std::memory_order_release);
        return kcenon::common::VoidResult::ok(std::monostate{});
    }

    // Test helpers
    std::vector<LogRecord> records() const {
        std::lock_guard lock(mutex_);
        return records_;
    }

    std::size_t logCount() const {
        return logCount_.load(std::memory_order_relaxed);
    }

    bool wasFlushed() const {
        return flushed_.load(std::memory_order_acquire);
    }

    void reset() {
        std::lock_guard lock(mutex_);
        records_.clear();
        logCount_.store(0, std::memory_order_relaxed);
        flushed_.store(false, std::memory_order_relaxed);
    }

private:
    mutable std::mutex mutex_;
    std::vector<LogRecord> records_;
    std::atomic<std::size_t> logCount_{0};
    std::atomic<log_level> minLevel_{log_level::trace};
    std::atomic<bool> flushed_{false};
};

// ---------------------------------------------------------------------------
// Test fixture: registers a MockLogger as the default logger
// ---------------------------------------------------------------------------

class GameLoggerTest : public ::testing::Test {
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

// ---------------------------------------------------------------------------
// ErrorCode: Logger subsystem lookup
// ---------------------------------------------------------------------------

TEST(LoggerErrorCodeTest, SubsystemLookup) {
    EXPECT_EQ(errorSubsystem(ErrorCode::LoggerError), "Logger");
    EXPECT_EQ(errorSubsystem(ErrorCode::LoggerNotInitialized), "Logger");
    EXPECT_EQ(errorSubsystem(ErrorCode::LoggerFlushFailed), "Logger");
}

TEST(LoggerErrorCodeTest, GameErrorSubsystem) {
    GameError err(ErrorCode::LoggerFlushFailed, "test");
    EXPECT_EQ(err.subsystem(), "Logger");
}

// ---------------------------------------------------------------------------
// LogCategory / LogLevel helpers
// ---------------------------------------------------------------------------

TEST(LogCategoryTest, AllCategoryNamesAreValid) {
    EXPECT_EQ(logCategoryName(LogCategory::Core), "Core");
    EXPECT_EQ(logCategoryName(LogCategory::ECS), "ECS");
    EXPECT_EQ(logCategoryName(LogCategory::Network), "Network");
    EXPECT_EQ(logCategoryName(LogCategory::Database), "Database");
    EXPECT_EQ(logCategoryName(LogCategory::Plugin), "Plugin");
    EXPECT_EQ(logCategoryName(LogCategory::Combat), "Combat");
    EXPECT_EQ(logCategoryName(LogCategory::World), "World");
    EXPECT_EQ(logCategoryName(LogCategory::AI), "AI");
}

TEST(LogCategoryTest, CategoryCountIsEight) {
    EXPECT_EQ(kLogCategoryCount, 8u);
}

TEST(LogLevelTest, AllLevelNamesAreValid) {
    EXPECT_EQ(logLevelName(LogLevel::Trace), "TRACE");
    EXPECT_EQ(logLevelName(LogLevel::Debug), "DEBUG");
    EXPECT_EQ(logLevelName(LogLevel::Info), "INFO");
    EXPECT_EQ(logLevelName(LogLevel::Warning), "WARNING");
    EXPECT_EQ(logLevelName(LogLevel::Error), "ERROR");
    EXPECT_EQ(logLevelName(LogLevel::Critical), "CRITICAL");
    EXPECT_EQ(logLevelName(LogLevel::Off), "OFF");
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(GameLoggerBasicTest, DefaultConstruction) {
    GameLogger logger;
    // Should not crash
}

TEST(GameLoggerBasicTest, MoveConstruction) {
    GameLogger a;
    GameLogger b(std::move(a));
    // Moved-from object should be in valid state
}

TEST(GameLoggerBasicTest, MoveAssignment) {
    GameLogger a;
    GameLogger b;
    b = std::move(a);
}

// ---------------------------------------------------------------------------
// Default category levels (from SDS-MOD-003)
// ---------------------------------------------------------------------------

TEST(GameLoggerBasicTest, DefaultCategoryLevels) {
    GameLogger logger;
    EXPECT_EQ(logger.getCategoryLevel(LogCategory::Core), LogLevel::Info);
    EXPECT_EQ(logger.getCategoryLevel(LogCategory::ECS), LogLevel::Debug);
    EXPECT_EQ(logger.getCategoryLevel(LogCategory::Network), LogLevel::Info);
    EXPECT_EQ(logger.getCategoryLevel(LogCategory::Database), LogLevel::Info);
    EXPECT_EQ(logger.getCategoryLevel(LogCategory::Plugin), LogLevel::Info);
    EXPECT_EQ(logger.getCategoryLevel(LogCategory::Combat), LogLevel::Debug);
    EXPECT_EQ(logger.getCategoryLevel(LogCategory::World), LogLevel::Info);
    EXPECT_EQ(logger.getCategoryLevel(LogCategory::AI), LogLevel::Debug);
}

// ---------------------------------------------------------------------------
// isEnabled / setCategoryLevel
// ---------------------------------------------------------------------------

TEST(GameLoggerBasicTest, IsEnabledRespectsDefaultLevels) {
    GameLogger logger;
    // Core defaults to Info
    EXPECT_FALSE(logger.isEnabled(LogLevel::Debug, LogCategory::Core));
    EXPECT_TRUE(logger.isEnabled(LogLevel::Info, LogCategory::Core));
    EXPECT_TRUE(logger.isEnabled(LogLevel::Error, LogCategory::Core));

    // ECS defaults to Debug
    EXPECT_TRUE(logger.isEnabled(LogLevel::Debug, LogCategory::ECS));
    EXPECT_FALSE(logger.isEnabled(LogLevel::Trace, LogCategory::ECS));
}

TEST(GameLoggerBasicTest, SetCategoryLevelChangesFiltering) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Trace);
    EXPECT_TRUE(logger.isEnabled(LogLevel::Trace, LogCategory::Core));

    logger.setCategoryLevel(LogCategory::Core, LogLevel::Error);
    EXPECT_FALSE(logger.isEnabled(LogLevel::Warning, LogCategory::Core));
    EXPECT_TRUE(logger.isEnabled(LogLevel::Error, LogCategory::Core));
}

TEST(GameLoggerBasicTest, SetCategoryLevelToOffDisablesAll) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Network, LogLevel::Off);
    EXPECT_FALSE(logger.isEnabled(LogLevel::Critical, LogCategory::Network));
}

TEST(GameLoggerBasicTest, InvalidCategoryReturnsOff) {
    GameLogger logger;
    auto invalid = static_cast<LogCategory>(99);
    EXPECT_EQ(logger.getCategoryLevel(invalid), LogLevel::Off);
    EXPECT_FALSE(logger.isEnabled(LogLevel::Critical, invalid));
}

// ---------------------------------------------------------------------------
// Basic logging
// ---------------------------------------------------------------------------

TEST_F(GameLoggerTest, LogFormatsMessageWithCategory) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Info);
    logger.log(LogLevel::Info, LogCategory::Core, "Server starting");

    auto records = mockLogger_->records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].level, log_level::info);
    EXPECT_EQ(records[0].message, "[Core] Server starting");
}

TEST_F(GameLoggerTest, LogFiltersMessagesBelowLevel) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Warning);
    logger.log(LogLevel::Info, LogCategory::Core, "Should be filtered");

    auto records = mockLogger_->records();
    EXPECT_TRUE(records.empty());
}

TEST_F(GameLoggerTest, LogMultipleCategories) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Info);
    logger.setCategoryLevel(LogCategory::Network, LogLevel::Info);

    logger.log(LogLevel::Info, LogCategory::Core, "core msg");
    logger.log(LogLevel::Info, LogCategory::Network, "net msg");

    auto records = mockLogger_->records();
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0].message, "[Core] core msg");
    EXPECT_EQ(records[1].message, "[Network] net msg");
}

TEST_F(GameLoggerTest, LogAllLevels) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Trace);

    logger.log(LogLevel::Trace, LogCategory::Core, "trace");
    logger.log(LogLevel::Debug, LogCategory::Core, "debug");
    logger.log(LogLevel::Info, LogCategory::Core, "info");
    logger.log(LogLevel::Warning, LogCategory::Core, "warn");
    logger.log(LogLevel::Error, LogCategory::Core, "error");
    logger.log(LogLevel::Critical, LogCategory::Core, "critical");

    auto records = mockLogger_->records();
    ASSERT_EQ(records.size(), 6u);
    EXPECT_EQ(records[0].level, log_level::trace);
    EXPECT_EQ(records[1].level, log_level::debug);
    EXPECT_EQ(records[2].level, log_level::info);
    EXPECT_EQ(records[3].level, log_level::warning);
    EXPECT_EQ(records[4].level, log_level::error);
    EXPECT_EQ(records[5].level, log_level::critical);
}

// ---------------------------------------------------------------------------
// Structured logging with context
// ---------------------------------------------------------------------------

TEST_F(GameLoggerTest, LogWithContextIncludesFields) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Combat, LogLevel::Debug);

    LogContext ctx;
    ctx.playerId = PlayerId(42);
    ctx.entityId = EntityId(100);
    ctx.traceId = "abc-123";
    ctx.extra["damage"] = "150";

    logger.logWithContext(LogLevel::Debug, LogCategory::Combat,
                          "Damage applied", ctx);

    auto records = mockLogger_->records();
    ASSERT_EQ(records.size(), 1u);

    const auto& msg = records[0].message;
    EXPECT_TRUE(msg.find("[Combat]") != std::string::npos);
    EXPECT_TRUE(msg.find("Damage applied") != std::string::npos);
    EXPECT_TRUE(msg.find("player_id=42") != std::string::npos);
    EXPECT_TRUE(msg.find("entity_id=100") != std::string::npos);
    EXPECT_TRUE(msg.find("trace_id=abc-123") != std::string::npos);
    EXPECT_TRUE(msg.find("damage=150") != std::string::npos);
}

TEST_F(GameLoggerTest, LogWithEmptyContextOmitsBraces) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Info);

    LogContext ctx; // All fields empty
    logger.logWithContext(LogLevel::Info, LogCategory::Core, "No context", ctx);

    auto records = mockLogger_->records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].message, "[Core] No context");
}

TEST_F(GameLoggerTest, LogWithContextFilteredBelowLevel) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::AI, LogLevel::Warning);

    LogContext ctx;
    ctx.entityId = EntityId(1);
    logger.logWithContext(LogLevel::Debug, LogCategory::AI, "filtered", ctx);

    EXPECT_TRUE(mockLogger_->records().empty());
}

TEST_F(GameLoggerTest, LogWithSessionId) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Network, LogLevel::Info);

    LogContext ctx;
    ctx.sessionId = SessionId(999);
    logger.logWithContext(LogLevel::Info, LogCategory::Network,
                          "Session connected", ctx);

    auto records = mockLogger_->records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_TRUE(records[0].message.find("session_id=999") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Flush
// ---------------------------------------------------------------------------

TEST_F(GameLoggerTest, FlushDelegatesToLogger) {
    GameLogger logger;
    auto result = logger.flush();
    EXPECT_TRUE(result.hasValue());
    EXPECT_TRUE(mockLogger_->wasFlushed());
}

// ---------------------------------------------------------------------------
// Singleton instance
// ---------------------------------------------------------------------------

TEST(GameLoggerSingletonTest, InstanceReturnsSameObject) {
    auto& a = GameLogger::instance();
    auto& b = GameLogger::instance();
    EXPECT_EQ(&a, &b);
}

// ---------------------------------------------------------------------------
// CGS_LOG macros
// ---------------------------------------------------------------------------

TEST_F(GameLoggerTest, MacroLogsWhenEnabled) {
    // Set singleton instance categories for macro usage
    GameLogger::instance().setCategoryLevel(LogCategory::Core, LogLevel::Debug);

    CGS_LOG_DEBUG(LogCategory::Core, "macro test");

    auto records = mockLogger_->records();
    ASSERT_GE(records.size(), 1u);

    bool found = false;
    for (const auto& r : records) {
        if (r.message.find("macro test") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(GameLoggerTest, MacroSkipsWhenDisabled) {
    GameLogger::instance().setCategoryLevel(LogCategory::Core, LogLevel::Error);
    mockLogger_->reset();

    CGS_LOG_DEBUG(LogCategory::Core, "should not appear");

    EXPECT_TRUE(mockLogger_->records().empty());
}

// ---------------------------------------------------------------------------
// Thread safety: concurrent logging from multiple threads
// ---------------------------------------------------------------------------

TEST_F(GameLoggerTest, ConcurrentLoggingIsSafe) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Trace);

    constexpr int kThreads = 8;
    constexpr int kMessagesPerThread = 100;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&logger, t] {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                logger.log(LogLevel::Info, LogCategory::Core,
                           "thread " + std::to_string(t) + " msg " +
                               std::to_string(i));
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(mockLogger_->logCount(), kThreads * kMessagesPerThread);
}

// ---------------------------------------------------------------------------
// Throughput benchmark (informational, not a strict pass/fail)
// ---------------------------------------------------------------------------

TEST_F(GameLoggerTest, ThroughputBenchmark) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Info);

    constexpr int kIterations = 1'000'000;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        logger.log(LogLevel::Info, LogCategory::Core, "benchmark message");
    }
    auto end = std::chrono::steady_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double seconds = static_cast<double>(elapsed.count()) / 1'000'000.0;
    double msgPerSec = static_cast<double>(kIterations) / seconds;

    // Report throughput (informational)
    std::cout << "[Benchmark] " << kIterations << " messages in "
              << elapsed.count() << " us (" << msgPerSec / 1'000'000.0
              << "M msg/sec)" << std::endl;

    // Soft assertion: conservative threshold for Debug builds on shared CI
    // runners. The 4.3M msg/sec SRS target applies to optimized Release builds.
    EXPECT_GT(msgPerSec, 100'000.0)
        << "Throughput below 100K msg/sec — possible environment issue";
}

// ---------------------------------------------------------------------------
// Filtered message throughput (messages below level should be very fast)
// ---------------------------------------------------------------------------

TEST(GameLoggerBenchmarkTest, FilteredThroughputBenchmark) {
    GameLogger logger;
    logger.setCategoryLevel(LogCategory::Core, LogLevel::Error);

    constexpr int kIterations = 10'000'000;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        logger.log(LogLevel::Debug, LogCategory::Core, "filtered message");
    }
    auto end = std::chrono::steady_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double seconds = static_cast<double>(elapsed.count()) / 1'000'000.0;
    double msgPerSec = static_cast<double>(kIterations) / seconds;

    std::cout << "[Benchmark] " << kIterations << " filtered messages in "
              << elapsed.count() << " us (" << msgPerSec / 1'000'000.0
              << "M msg/sec)" << std::endl;

    // Filtered messages should be fast (atomic load + compare only).
    // Conservative threshold for Debug builds on shared CI runners.
    EXPECT_GT(msgPerSec, 1'000'000.0)
        << "Filtered throughput below 1M msg/sec — possible environment issue";
}
