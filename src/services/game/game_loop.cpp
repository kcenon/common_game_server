/// @file game_loop.cpp
/// @brief GameLoop implementation.

#include "cgs/service/game_loop.hpp"

namespace cgs::service {

GameLoop::GameLoop(uint32_t tickRate)
    : tickRate_(tickRate > 0 ? tickRate : 20),
      targetFrameTime_(std::chrono::microseconds(
          1'000'000 / (tickRate > 0 ? tickRate : 20))) {}

GameLoop::~GameLoop() {
    stop();
}

void GameLoop::setTickCallback(TickCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    tickCallback_ = std::move(callback);
}

void GameLoop::setMetricsCallback(MetricsCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    metricsCallback_ = std::move(callback);
}

bool GameLoop::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return false;
    }

    thread_ = std::thread([this] { run(); });
    return true;
}

void GameLoop::stop() {
    running_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }
}

TickMetrics GameLoop::tick() {
    return executeTick();
}

bool GameLoop::isRunning() const noexcept {
    return running_.load();
}

uint32_t GameLoop::tickRate() const noexcept {
    return tickRate_;
}

std::chrono::microseconds GameLoop::targetFrameTime() const noexcept {
    return targetFrameTime_;
}

uint64_t GameLoop::tickCount() const noexcept {
    return tickCount_.load();
}

TickMetrics GameLoop::lastMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return lastMetrics_;
}

void GameLoop::run() {
    auto nextTick = std::chrono::steady_clock::now();

    while (running_.load()) {
        nextTick += targetFrameTime_;

        auto metrics = executeTick();

        // Store metrics for external queries.
        {
            std::lock_guard<std::mutex> lock(metricsMutex_);
            lastMetrics_ = metrics;
        }

        // Notify metrics observer.
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (metricsCallback_) {
                metricsCallback_(metrics);
            }
        }

        // Sleep until next tick, but skip if we already overran.
        auto now = std::chrono::steady_clock::now();
        if (now < nextTick) {
            std::this_thread::sleep_until(nextTick);
        } else {
            // Overrun: reset the target to avoid cascading catch-up.
            nextTick = now;
        }
    }
}

TickMetrics GameLoop::executeTick() {
    auto frameStart = std::chrono::steady_clock::now();

    // Invoke the tick callback.
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (tickCallback_) {
            auto dtSeconds = static_cast<float>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    targetFrameTime_)
                    .count()) /
                1'000'000.0f;
            tickCallback_(dtSeconds);
        }
    }

    auto updateEnd = std::chrono::steady_clock::now();

    auto updateDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(
            updateEnd - frameStart);

    auto targetUs =
        std::chrono::duration_cast<std::chrono::microseconds>(targetFrameTime_);

    TickMetrics metrics;
    metrics.updateTime = updateDuration;
    metrics.frameTime = updateDuration; // For manual tick(), frame = update.
    metrics.budgetUtilization =
        targetUs.count() > 0
            ? static_cast<float>(updateDuration.count()) /
                  static_cast<float>(targetUs.count())
            : 0.0f;
    metrics.tickNumber = tickCount_.fetch_add(1);
    metrics.overrun = updateDuration > targetFrameTime_;

    return metrics;
}

} // namespace cgs::service
