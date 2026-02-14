#pragma once

/// @file game_loop.hpp
/// @brief Fixed-rate game loop with frame timing and metrics.
///
/// GameLoop runs a user-provided tick callback at a configurable rate
/// (default 20 Hz = 50 ms per tick) on a dedicated thread.  Each tick
/// measures frame time, reports budget utilization, and detects
/// overruns (ticks that exceed the target frame time).
///
/// @see SRS-SVC-003.1, SRS-NFR-002
/// @see SDS-MOD-032

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace cgs::service {

/// Per-tick performance metrics.
struct TickMetrics {
    /// Actual time spent in the tick callback.
    std::chrono::microseconds updateTime{0};

    /// Total frame time including sleep.
    std::chrono::microseconds frameTime{0};

    /// Ratio of updateTime to target frame time (1.0 = full budget).
    float budgetUtilization = 0.0f;

    /// Monotonically increasing tick counter (starts at 0).
    uint64_t tickNumber = 0;

    /// True when updateTime exceeded the target frame time.
    bool overrun = false;
};

/// Fixed-rate game loop with dedicated thread.
///
/// Usage:
/// @code
///   GameLoop loop(20); // 20 Hz
///   loop.setTickCallback([&](float dt) {
///       scheduler.Execute(dt);
///   });
///   loop.start();
///   // ...
///   loop.stop();
/// @endcode
class GameLoop {
public:
    using TickCallback = std::function<void(float deltaTime)>;
    using MetricsCallback = std::function<void(const TickMetrics&)>;

    /// Construct a game loop with the given tick rate.
    ///
    /// @param tickRate  Ticks per second (default: 20).
    explicit GameLoop(uint32_t tickRate = 20);

    ~GameLoop();

    // Non-copyable, non-movable (owns a thread).
    GameLoop(const GameLoop&) = delete;
    GameLoop& operator=(const GameLoop&) = delete;
    GameLoop(GameLoop&&) = delete;
    GameLoop& operator=(GameLoop&&) = delete;

    /// Set the callback invoked each tick with the delta time in seconds.
    void setTickCallback(TickCallback callback);

    /// Set an optional callback invoked after each tick with metrics.
    void setMetricsCallback(MetricsCallback callback);

    /// Start the game loop on a dedicated thread.
    ///
    /// @return true on success, false if already running.
    [[nodiscard]] bool start();

    /// Signal the loop to stop and wait for the thread to join.
    void stop();

    /// Execute a single tick manually (for testing).
    ///
    /// The loop must not be running on a thread when calling this.
    /// @return The metrics for the executed tick.
    TickMetrics tick();

    /// Check whether the loop is currently running.
    [[nodiscard]] bool isRunning() const noexcept;

    /// Get the configured tick rate (ticks per second).
    [[nodiscard]] uint32_t tickRate() const noexcept;

    /// Get the target frame duration.
    [[nodiscard]] std::chrono::microseconds targetFrameTime() const noexcept;

    /// Get the total number of ticks executed.
    [[nodiscard]] uint64_t tickCount() const noexcept;

    /// Get the metrics from the last completed tick.
    [[nodiscard]] TickMetrics lastMetrics() const;

private:
    /// Main loop body run on the dedicated thread.
    void run();

    /// Execute one tick and return its metrics.
    TickMetrics executeTick();

    uint32_t tickRate_;
    std::chrono::microseconds targetFrameTime_;

    TickCallback tickCallback_;
    MetricsCallback metricsCallback_;

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> tickCount_{0};
    std::thread thread_;

    mutable std::mutex metricsMutex_;
    TickMetrics lastMetrics_;

    mutable std::mutex callbackMutex_;
};

}  // namespace cgs::service
