#pragma once

/// @file job_scheduler.hpp
/// @brief GameJobScheduler wrapping kcenon thread_system for game-specific job scheduling.

#include "cgs/foundation/game_result.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

namespace cgs::foundation {

/// Priority levels for scheduled game jobs.
///
/// Maps to kcenon::thread::job_priority internally:
///   Critical -> highest, High -> high, Normal -> normal, Low -> low
enum class JobPriority { Critical, High, Normal, Low };

/// Game-specific job scheduler wrapping kcenon's thread_system.
///
/// Provides priority-based scheduling, dependency chaining, and tick-based
/// recurring jobs for the game loop. Uses PIMPL to hide thread_system details
/// from the public API, preventing header dependency leakage.
///
/// Example:
/// @code
///   GameJobScheduler scheduler(4);
///   auto id = scheduler.schedule([] { computeAI(); }, JobPriority::High);
///   scheduler.wait(id.value());
/// @endcode
class GameJobScheduler {
public:
    using JobId = uint64_t;
    using JobFunc = std::function<void()>;

    /// Construct a scheduler backed by a thread pool with @p numThreads workers.
    explicit GameJobScheduler(std::size_t numThreads = std::thread::hardware_concurrency());

    ~GameJobScheduler();

    // Non-copyable, movable.
    GameJobScheduler(const GameJobScheduler&) = delete;
    GameJobScheduler& operator=(const GameJobScheduler&) = delete;
    GameJobScheduler(GameJobScheduler&&) noexcept;
    GameJobScheduler& operator=(GameJobScheduler&&) noexcept;

    /// Schedule a job with the given priority.
    /// @return The assigned JobId on success, or a GameError on failure.
    GameResult<JobId> schedule(JobFunc job, JobPriority priority = JobPriority::Normal);

    /// Schedule a job to execute after the given dependency completes.
    /// @return The assigned JobId, or JobNotFound if the dependency is unknown.
    GameResult<JobId> scheduleAfter(JobId dependency, JobFunc job);

    /// Register a recurring tick job that fires every @p interval.
    /// The job is dispatched into the thread pool each time it fires.
    /// @return The assigned JobId for the tick entry.
    GameResult<JobId> scheduleTick(std::chrono::milliseconds interval, JobFunc job);

    /// Advance tick timers by @p deltaTime and dispatch due tick jobs.
    /// Call this once per game-loop iteration from the main thread.
    void processTick(std::chrono::milliseconds deltaTime);

    /// Block until the job identified by @p id completes.
    /// @return Success, or JobNotFound / JobTimeout on failure.
    GameResult<void> wait(JobId id);

    /// Request cancellation of a pending job.
    /// Already-completed jobs return JobCancelled as a no-op error.
    GameResult<void> cancel(JobId id);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cgs::foundation
