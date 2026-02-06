/// @file job_scheduler.cpp
/// @brief GameJobScheduler implementation wrapping kcenon thread_system.

#include "cgs/foundation/job_scheduler.hpp"

// kcenon thread_system headers (hidden behind PIMPL)
#include <kcenon/thread/core/thread_pool.h>
#include <kcenon/thread/core/thread_worker.h>
#include <kcenon/thread/core/job_builder.h>

#include <atomic>
#include <future>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cgs::foundation {

// ---------------------------------------------------------------------------
// Priority mapping: CGS -> kcenon
// ---------------------------------------------------------------------------
static kcenon::thread::job_priority mapPriority(JobPriority p) {
    switch (p) {
        case JobPriority::Critical: return kcenon::thread::job_priority::highest;
        case JobPriority::High:     return kcenon::thread::job_priority::high;
        case JobPriority::Normal:   return kcenon::thread::job_priority::normal;
        case JobPriority::Low:      return kcenon::thread::job_priority::low;
    }
    return kcenon::thread::job_priority::normal;
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct GameJobScheduler::Impl {
    struct TickEntry {
        JobId id;
        std::chrono::milliseconds interval;
        std::chrono::milliseconds elapsed{0};
        JobFunc func;
        bool enabled{true};
    };

    std::shared_ptr<kcenon::thread::thread_pool> pool;
    std::atomic<uint64_t> nextJobId{1};

    // Tracking: JobId -> shared_future for wait()/cancel() support
    std::unordered_map<JobId, std::shared_future<void>> futures;
    std::unordered_map<JobId, std::shared_ptr<std::atomic<bool>>> cancelFlags;

    // Tick scheduling
    std::vector<TickEntry> tickJobs;

    std::mutex mutex;
};

// ---------------------------------------------------------------------------
// Construction / Destruction / Move
// ---------------------------------------------------------------------------
GameJobScheduler::GameJobScheduler(std::size_t numThreads)
    : impl_(std::make_unique<Impl>())
{
    impl_->pool = std::make_shared<kcenon::thread::thread_pool>("GameJobScheduler");

    // Add worker threads
    std::vector<std::unique_ptr<kcenon::thread::thread_worker>> workers;
    workers.reserve(numThreads);
    for (std::size_t i = 0; i < numThreads; ++i) {
        workers.push_back(std::make_unique<kcenon::thread::thread_worker>());
    }
    impl_->pool->enqueue_batch(std::move(workers));
    impl_->pool->start();
}

GameJobScheduler::~GameJobScheduler() {
    if (impl_ && impl_->pool) {
        impl_->pool->stop(false); // graceful: wait for running jobs
    }
}

GameJobScheduler::GameJobScheduler(GameJobScheduler&&) noexcept = default;
GameJobScheduler& GameJobScheduler::operator=(GameJobScheduler&&) noexcept = default;

// ---------------------------------------------------------------------------
// schedule()
// ---------------------------------------------------------------------------
GameResult<GameJobScheduler::JobId> GameJobScheduler::schedule(
    JobFunc job, JobPriority priority)
{
    auto id = impl_->nextJobId.fetch_add(1, std::memory_order_relaxed);
    auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future().share();

    // Build a kcenon job with priority and cancellation awareness
    auto threadJob = kcenon::thread::job_builder()
        .name("cgs_job_" + std::to_string(id))
        .priority(mapPriority(priority))
        .work([fn = std::move(job), cancelFlag, promise]()
              -> kcenon::common::VoidResult {
            try {
                if (!cancelFlag->load(std::memory_order_acquire)) {
                    fn();
                }
                promise->set_value();
            } catch (...) {
                try {
                    promise->set_exception(std::current_exception());
                } catch (...) {
                    // promise already satisfied — ignore
                }
            }
            return kcenon::common::VoidResult::ok(std::monostate{});
        })
        .build();

    auto enqResult = impl_->pool->enqueue(std::move(threadJob));
    if (enqResult.is_err()) {
        return GameResult<JobId>::err(
            GameError(ErrorCode::JobScheduleFailed, "failed to enqueue job"));
    }

    {
        std::lock_guard lock(impl_->mutex);
        impl_->futures[id] = future;
        impl_->cancelFlags[id] = cancelFlag;
    }

    return GameResult<JobId>::ok(id);
}

// ---------------------------------------------------------------------------
// scheduleAfter()
// ---------------------------------------------------------------------------
GameResult<GameJobScheduler::JobId> GameJobScheduler::scheduleAfter(
    JobId dependency, JobFunc job)
{
    std::shared_future<void> depFuture;
    {
        std::lock_guard lock(impl_->mutex);
        auto it = impl_->futures.find(dependency);
        if (it == impl_->futures.end()) {
            return GameResult<JobId>::err(
                GameError(ErrorCode::JobNotFound, "dependency job not found"));
        }
        depFuture = it->second;
    }

    // Wrap the user function: wait for dependency, then execute.
    return schedule(
        [depFuture = std::move(depFuture), fn = std::move(job)]() {
            depFuture.wait();
            fn();
        },
        JobPriority::Normal);
}

// ---------------------------------------------------------------------------
// scheduleTick()
// ---------------------------------------------------------------------------
GameResult<GameJobScheduler::JobId> GameJobScheduler::scheduleTick(
    std::chrono::milliseconds interval, JobFunc job)
{
    auto id = impl_->nextJobId.fetch_add(1, std::memory_order_relaxed);

    std::lock_guard lock(impl_->mutex);
    impl_->tickJobs.push_back(
        Impl::TickEntry{id, interval, std::chrono::milliseconds{0},
                        std::move(job), true});

    return GameResult<JobId>::ok(id);
}

// ---------------------------------------------------------------------------
// processTick()
// ---------------------------------------------------------------------------
void GameJobScheduler::processTick(std::chrono::milliseconds deltaTime) {
    std::lock_guard lock(impl_->mutex);

    for (auto& tick : impl_->tickJobs) {
        if (!tick.enabled) {
            continue;
        }
        tick.elapsed += deltaTime;
        if (tick.elapsed >= tick.interval) {
            tick.elapsed = std::chrono::milliseconds{0};
            // Copy the function and dispatch into the pool (no lock held during enqueue)
            auto fn = tick.func;
            auto threadJob = kcenon::thread::job_builder()
                .name("cgs_tick_" + std::to_string(tick.id))
                .work([fn]() -> kcenon::common::VoidResult {
                    fn();
                    return kcenon::common::VoidResult::ok(std::monostate{});
                })
                .build();
            impl_->pool->enqueue(std::move(threadJob));
        }
    }
}

// ---------------------------------------------------------------------------
// wait()
// ---------------------------------------------------------------------------
GameResult<void> GameJobScheduler::wait(JobId id) {
    std::shared_future<void> future;
    {
        std::lock_guard lock(impl_->mutex);
        auto it = impl_->futures.find(id);
        if (it == impl_->futures.end()) {
            return GameResult<void>::err(
                GameError(ErrorCode::JobNotFound, "job not found"));
        }
        future = it->second;
    }

    try {
        future.get();
    } catch (...) {
        return GameResult<void>::err(
            GameError(ErrorCode::ThreadError, "job execution failed"));
    }

    return GameResult<void>::ok();
}

// ---------------------------------------------------------------------------
// cancel()
// ---------------------------------------------------------------------------
GameResult<void> GameJobScheduler::cancel(JobId id) {
    std::lock_guard lock(impl_->mutex);

    auto flagIt = impl_->cancelFlags.find(id);
    if (flagIt == impl_->cancelFlags.end()) {
        // Might be a tick job — disable it
        for (auto& tick : impl_->tickJobs) {
            if (tick.id == id) {
                tick.enabled = false;
                return GameResult<void>::ok();
            }
        }
        return GameResult<void>::err(
            GameError(ErrorCode::JobNotFound, "job not found"));
    }

    // Check if already completed
    auto futIt = impl_->futures.find(id);
    if (futIt != impl_->futures.end()) {
        auto status = futIt->second.wait_for(std::chrono::seconds(0));
        if (status == std::future_status::ready) {
            return GameResult<void>::err(
                GameError(ErrorCode::JobCancelled, "job already completed"));
        }
    }

    flagIt->second->store(true, std::memory_order_release);
    return GameResult<void>::ok();
}

} // namespace cgs::foundation
