#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "cgs/foundation/monitoring_adapter.hpp"
#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"

using namespace cgs::foundation;

// ===========================================================================
// ErrorCode: Monitoring subsystem lookup
// ===========================================================================

TEST(MonitoringErrorCodeTest, SubsystemLookup) {
    EXPECT_EQ(errorSubsystem(ErrorCode::MonitoringError), "Monitoring");
    EXPECT_EQ(errorSubsystem(ErrorCode::MetricNotFound), "Monitoring");
    EXPECT_EQ(errorSubsystem(ErrorCode::InvalidMetricType), "Monitoring");
    EXPECT_EQ(errorSubsystem(ErrorCode::HistogramNotRegistered), "Monitoring");
}

TEST(MonitoringErrorCodeTest, GameErrorSubsystem) {
    GameError err(ErrorCode::MetricNotFound, "metric not found");
    EXPECT_EQ(err.subsystem(), "Monitoring");
    EXPECT_EQ(err.message(), "metric not found");
}

// ===========================================================================
// Counter: basic operations
// ===========================================================================

class GameMetricsTest : public ::testing::Test {
protected:
    void SetUp() override { metrics_.reset(); }

    GameMetrics metrics_;
};

TEST_F(GameMetricsTest, CounterDefaultZero) {
    EXPECT_EQ(metrics_.counterValue("nonexistent"), 0u);
}

TEST_F(GameMetricsTest, CounterIncrement) {
    metrics_.incrementCounter("cgs_messages_total");
    EXPECT_EQ(metrics_.counterValue("cgs_messages_total"), 1u);
}

TEST_F(GameMetricsTest, CounterIncrementByValue) {
    metrics_.incrementCounter("cgs_messages_total", 100);
    EXPECT_EQ(metrics_.counterValue("cgs_messages_total"), 100u);
}

TEST_F(GameMetricsTest, CounterMultipleIncrements) {
    metrics_.incrementCounter("cgs_messages_total", 10);
    metrics_.incrementCounter("cgs_messages_total", 5);
    metrics_.incrementCounter("cgs_messages_total");
    EXPECT_EQ(metrics_.counterValue("cgs_messages_total"), 16u);
}

TEST_F(GameMetricsTest, CounterMultipleNames) {
    metrics_.incrementCounter("counter_a", 1);
    metrics_.incrementCounter("counter_b", 2);
    EXPECT_EQ(metrics_.counterValue("counter_a"), 1u);
    EXPECT_EQ(metrics_.counterValue("counter_b"), 2u);
}

// ===========================================================================
// Gauge: basic operations
// ===========================================================================

TEST_F(GameMetricsTest, GaugeDefaultZero) {
    EXPECT_DOUBLE_EQ(metrics_.gaugeValue("nonexistent"), 0.0);
}

TEST_F(GameMetricsTest, GaugeSet) {
    metrics_.setGauge("cgs_ccu", 1500.0);
    EXPECT_DOUBLE_EQ(metrics_.gaugeValue("cgs_ccu"), 1500.0);
}

TEST_F(GameMetricsTest, GaugeIncrement) {
    metrics_.setGauge("cgs_ccu", 100.0);
    metrics_.incrementGauge("cgs_ccu", 50.0);
    EXPECT_DOUBLE_EQ(metrics_.gaugeValue("cgs_ccu"), 150.0);
}

TEST_F(GameMetricsTest, GaugeDecrement) {
    metrics_.setGauge("cgs_ccu", 100.0);
    metrics_.decrementGauge("cgs_ccu", 30.0);
    EXPECT_DOUBLE_EQ(metrics_.gaugeValue("cgs_ccu"), 70.0);
}

TEST_F(GameMetricsTest, GaugeIncrementDefaultDelta) {
    metrics_.incrementGauge("cgs_ccu");
    EXPECT_DOUBLE_EQ(metrics_.gaugeValue("cgs_ccu"), 1.0);
}

TEST_F(GameMetricsTest, GaugeDecrementDefaultDelta) {
    metrics_.setGauge("cgs_ccu", 5.0);
    metrics_.decrementGauge("cgs_ccu");
    EXPECT_DOUBLE_EQ(metrics_.gaugeValue("cgs_ccu"), 4.0);
}

TEST_F(GameMetricsTest, GaugeOverwrite) {
    metrics_.setGauge("cgs_ccu", 100.0);
    metrics_.setGauge("cgs_ccu", 200.0);
    EXPECT_DOUBLE_EQ(metrics_.gaugeValue("cgs_ccu"), 200.0);
}

// ===========================================================================
// Histogram: registration and recording
// ===========================================================================

TEST_F(GameMetricsTest, HistogramRegisterAndRecord) {
    metrics_.registerHistogram("cgs_tick_ms",
                               HistogramBuckets{{1, 5, 10, 50, 100}});
    metrics_.recordHistogram("cgs_tick_ms", 3.0);
    metrics_.recordHistogram("cgs_tick_ms", 7.5);
    metrics_.recordHistogram("cgs_tick_ms", 75.0);

    // Verify via scrape output
    auto output = metrics_.scrape();
    EXPECT_TRUE(output.find("cgs_tick_ms_count 3") != std::string::npos);
}

TEST_F(GameMetricsTest, HistogramDefaultLatencyBuckets) {
    auto buckets = HistogramBuckets::defaultLatency();
    EXPECT_EQ(buckets.boundaries.size(), 9u);
    EXPECT_DOUBLE_EQ(buckets.boundaries.front(), 1.0);
    EXPECT_DOUBLE_EQ(buckets.boundaries.back(), 1000.0);
}

TEST_F(GameMetricsTest, HistogramDefaultDurationBuckets) {
    auto buckets = HistogramBuckets::defaultDuration();
    EXPECT_EQ(buckets.boundaries.size(), 8u);
    EXPECT_DOUBLE_EQ(buckets.boundaries.front(), 0.1);
    EXPECT_DOUBLE_EQ(buckets.boundaries.back(), 50.0);
}

TEST_F(GameMetricsTest, HistogramUnregisteredRecordIsNoop) {
    // Should not crash or create the histogram
    metrics_.recordHistogram("not_registered", 42.0);
    auto output = metrics_.scrape();
    EXPECT_TRUE(output.find("not_registered") == std::string::npos);
}

TEST_F(GameMetricsTest, HistogramBucketCounts) {
    metrics_.registerHistogram("latency", HistogramBuckets{{10, 50, 100}});
    metrics_.recordHistogram("latency", 5.0);   // le=10, le=50, le=100, +Inf
    metrics_.recordHistogram("latency", 30.0);  // le=50, le=100, +Inf
    metrics_.recordHistogram("latency", 200.0); // +Inf only

    auto output = metrics_.scrape();
    EXPECT_TRUE(output.find("latency_bucket{le=\"10\"} 1") != std::string::npos);
    EXPECT_TRUE(output.find("latency_bucket{le=\"50\"} 2") != std::string::npos);
    EXPECT_TRUE(output.find("latency_bucket{le=\"100\"} 2") != std::string::npos);
    EXPECT_TRUE(output.find("latency_bucket{le=\"+Inf\"} 3") != std::string::npos);
    EXPECT_TRUE(output.find("latency_sum 235") != std::string::npos);
    EXPECT_TRUE(output.find("latency_count 3") != std::string::npos);
}

TEST_F(GameMetricsTest, HistogramDuplicateRegisterIgnored) {
    metrics_.registerHistogram("h", HistogramBuckets{{10}});
    metrics_.recordHistogram("h", 5.0);
    // Re-register should not reset data
    metrics_.registerHistogram("h", HistogramBuckets{{20, 40}});
    auto output = metrics_.scrape();
    // Original buckets (le=10) should still be present
    EXPECT_TRUE(output.find("h_bucket{le=\"10\"}") != std::string::npos);
}

// ===========================================================================
// TraceSpan: start and end
// ===========================================================================

TEST_F(GameMetricsTest, TraceSpanStartEnd) {
    auto span = metrics_.startSpan("db_query");
    EXPECT_EQ(span.name, "db_query");
    EXPECT_FALSE(span.ended);
    EXPECT_GT(span.id, 0u);

    metrics_.endSpan(span);
    EXPECT_TRUE(span.ended);
}

TEST_F(GameMetricsTest, TraceSpanUniqueIds) {
    auto span1 = metrics_.startSpan("a");
    auto span2 = metrics_.startSpan("b");
    auto span3 = metrics_.startSpan("c");
    EXPECT_NE(span1.id, span2.id);
    EXPECT_NE(span2.id, span3.id);
    EXPECT_NE(span1.id, span3.id);
}

TEST_F(GameMetricsTest, TraceSpanDurationMeasurable) {
    auto span = metrics_.startSpan("sleep_test");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    metrics_.endSpan(span);

    auto elapsed = std::chrono::steady_clock::now() - span.startTime;
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
              10);
}

// ===========================================================================
// HealthCheck: component status aggregation
// ===========================================================================

TEST_F(GameMetricsTest, HealthCheckDefaultHealthy) {
    auto result = metrics_.healthCheck();
    EXPECT_EQ(result.status, HealthStatus::Healthy);
    EXPECT_TRUE(result.components.empty());
    EXPECT_EQ(result.serviceName, "cgs");
}

TEST_F(GameMetricsTest, HealthCheckSetComponent) {
    metrics_.setComponentHealth("database", HealthStatus::Healthy);
    metrics_.setComponentHealth("network", HealthStatus::Healthy);

    auto result = metrics_.healthCheck();
    EXPECT_EQ(result.status, HealthStatus::Healthy);
    EXPECT_EQ(result.components.size(), 2u);
    EXPECT_EQ(result.components.at("database"), HealthStatus::Healthy);
    EXPECT_EQ(result.components.at("network"), HealthStatus::Healthy);
}

TEST_F(GameMetricsTest, HealthCheckDegradedPropagation) {
    metrics_.setComponentHealth("database", HealthStatus::Healthy);
    metrics_.setComponentHealth("cache", HealthStatus::Degraded);

    auto result = metrics_.healthCheck();
    EXPECT_EQ(result.status, HealthStatus::Degraded);
}

TEST_F(GameMetricsTest, HealthCheckUnhealthyOverridesDegraded) {
    metrics_.setComponentHealth("database", HealthStatus::Unhealthy);
    metrics_.setComponentHealth("cache", HealthStatus::Degraded);
    metrics_.setComponentHealth("network", HealthStatus::Healthy);

    auto result = metrics_.healthCheck();
    EXPECT_EQ(result.status, HealthStatus::Unhealthy);
}

TEST_F(GameMetricsTest, HealthCheckTimestamp) {
    auto before = std::chrono::system_clock::now();
    auto result = metrics_.healthCheck();
    auto after = std::chrono::system_clock::now();

    EXPECT_GE(result.timestamp, before);
    EXPECT_LE(result.timestamp, after);
}

// ===========================================================================
// Prometheus scrape: format verification
// ===========================================================================

TEST_F(GameMetricsTest, ScrapeEmpty) {
    auto output = metrics_.scrape();
    EXPECT_TRUE(output.empty());
}

TEST_F(GameMetricsTest, ScrapeCounterFormat) {
    metrics_.incrementCounter("cgs_requests_total", 42);
    auto output = metrics_.scrape();
    EXPECT_TRUE(output.find("# TYPE cgs_requests_total counter") != std::string::npos);
    EXPECT_TRUE(output.find("cgs_requests_total 42") != std::string::npos);
}

TEST_F(GameMetricsTest, ScrapeGaugeFormat) {
    metrics_.setGauge("cgs_ccu", 1500.0);
    auto output = metrics_.scrape();
    EXPECT_TRUE(output.find("# TYPE cgs_ccu gauge") != std::string::npos);
    EXPECT_TRUE(output.find("cgs_ccu 1500") != std::string::npos);
}

TEST_F(GameMetricsTest, ScrapeHistogramFormat) {
    metrics_.registerHistogram("cgs_latency", HistogramBuckets{{10, 50}});
    metrics_.recordHistogram("cgs_latency", 5.0);
    auto output = metrics_.scrape();
    EXPECT_TRUE(output.find("# TYPE cgs_latency histogram") != std::string::npos);
    EXPECT_TRUE(output.find("cgs_latency_bucket{le=\"10\"} 1") != std::string::npos);
    EXPECT_TRUE(output.find("cgs_latency_bucket{le=\"50\"} 1") != std::string::npos);
    EXPECT_TRUE(output.find("cgs_latency_bucket{le=\"+Inf\"} 1") != std::string::npos);
    EXPECT_TRUE(output.find("cgs_latency_sum 5") != std::string::npos);
    EXPECT_TRUE(output.find("cgs_latency_count 1") != std::string::npos);
}

// ===========================================================================
// Construction and move semantics
// ===========================================================================

TEST(GameMetricsConstructionTest, DefaultConstruction) {
    GameMetrics metrics;
    EXPECT_EQ(metrics.counterValue("any"), 0u);
    EXPECT_DOUBLE_EQ(metrics.gaugeValue("any"), 0.0);
}

TEST(GameMetricsConstructionTest, MoveConstruction) {
    GameMetrics a;
    a.incrementCounter("test", 5);
    GameMetrics b(std::move(a));
    EXPECT_EQ(b.counterValue("test"), 5u);
}

TEST(GameMetricsConstructionTest, MoveAssignment) {
    GameMetrics a;
    a.incrementCounter("test", 10);
    GameMetrics b;
    b = std::move(a);
    EXPECT_EQ(b.counterValue("test"), 10u);
}

TEST(GameMetricsConstructionTest, NonCopyable) {
    static_assert(!std::is_copy_constructible_v<GameMetrics>);
    static_assert(!std::is_copy_assignable_v<GameMetrics>);
}

// ===========================================================================
// Singleton
// ===========================================================================

TEST(GameMetricsSingletonTest, InstanceReturnsSameObject) {
    auto& a = GameMetrics::instance();
    auto& b = GameMetrics::instance();
    EXPECT_EQ(&a, &b);
}

// ===========================================================================
// Reset
// ===========================================================================

TEST_F(GameMetricsTest, ResetClearsAll) {
    metrics_.incrementCounter("c", 10);
    metrics_.setGauge("g", 5.0);
    metrics_.registerHistogram("h", HistogramBuckets{{10}});
    metrics_.recordHistogram("h", 5.0);
    metrics_.setComponentHealth("db", HealthStatus::Unhealthy);

    metrics_.reset();

    EXPECT_EQ(metrics_.counterValue("c"), 0u);
    EXPECT_DOUBLE_EQ(metrics_.gaugeValue("g"), 0.0);
    EXPECT_TRUE(metrics_.scrape().empty());
    EXPECT_EQ(metrics_.healthCheck().status, HealthStatus::Healthy);
    EXPECT_TRUE(metrics_.healthCheck().components.empty());
}

// ===========================================================================
// Aggregate header test
// ===========================================================================

TEST(MonitoringAdapterTest, AggregateHeaderIncludesAll) {
    // Verify that monitoring_adapter.hpp includes all metric types
    GameMetrics metrics;
    HistogramBuckets buckets = HistogramBuckets::defaultLatency();
    TraceSpan span{};
    HealthCheckResult result{};
    EXPECT_EQ(metrics.counterValue("x"), 0u);
    EXPECT_FALSE(buckets.boundaries.empty());
    EXPECT_EQ(span.id, 0u);
    EXPECT_EQ(result.status, HealthStatus::Healthy);
}
