/// @file system_scheduler.cpp
/// @brief System scheduling implementation with parallel execution.
///
/// Provides staged execution with topological ordering via Kahn's
/// algorithm for dependency resolution.  Circular dependencies are
/// detected and reported with the names of the involved systems.
///
/// Parallel execution groups non-conflicting systems into batches
/// that are dispatched to a user-provided ParallelExecutor.
///
/// @see SDS-MOD-012

#include "cgs/ecs/system_scheduler.hpp"

#include <algorithm>
#include <queue>
#include <sstream>

namespace cgs::ecs {

// ── Static members ──────────────────────────────────────────────────────

const std::vector<SystemTypeId> SystemScheduler::kEmptyOrder_;
const std::vector<SystemScheduler::ParallelBatch> SystemScheduler::kEmptyBatches_;

// ── SystemAccessInfo ────────────────────────────────────────────────────

bool SystemAccessInfo::ConflictsWith(const SystemAccessInfo& other) const {
    // Undeclared access (empty reads AND writes) conflicts with everything.
    if ((reads.empty() && writes.empty()) || (other.reads.empty() && other.writes.empty())) {
        return true;
    }

    // Write-write conflict.
    for (auto w : writes) {
        if (other.writes.count(w)) {
            return true;
        }
    }

    // My write conflicts with other's read.
    for (auto w : writes) {
        if (other.reads.count(w)) {
            return true;
        }
    }

    // My read conflicts with other's write.
    for (auto r : reads) {
        if (other.writes.count(r)) {
            return true;
        }
    }

    return false;
}

// ── Registration ────────────────────────────────────────────────────────

std::size_t SystemScheduler::SystemCount() const noexcept {
    return systems_.size();
}

// ── Dependencies ────────────────────────────────────────────────────────

bool SystemScheduler::AddDependency(SystemTypeId before, SystemTypeId after) {
    // Both systems must be registered.
    auto itBefore = systems_.find(before);
    auto itAfter = systems_.find(after);

    if (itBefore == systems_.end() || itAfter == systems_.end()) {
        return false;
    }

    // Dependencies across different stages are meaningless (stage
    // ordering is implicit), so silently reject them.
    if (itBefore->second.stage != itAfter->second.stage) {
        return false;
    }

    dependencies_[before].insert(after);
    reverseDeps_[after].insert(before);

    // Invalidate any previously built plan.
    built_ = false;

    return true;
}

// ── Enable / disable ────────────────────────────────────────────────────

void SystemScheduler::SetEnabled(SystemTypeId system, bool enabled) {
    if (auto it = systems_.find(system); it != systems_.end()) {
        it->second.enabled = enabled;
    }
}

bool SystemScheduler::IsEnabled(SystemTypeId system) const {
    if (auto it = systems_.find(system); it != systems_.end()) {
        return it->second.enabled;
    }
    return false;
}

// ── Build ───────────────────────────────────────────────────────────────

bool SystemScheduler::Build() {
    lastError_.clear();
    executionOrder_.clear();
    parallelBatches_.clear();

    // Process each stage independently.
    for (const auto& [stage, ids] : stageGroups_) {
        std::vector<SystemTypeId> sorted;
        if (!topologicalSort(ids, sorted)) {
            built_ = false;
            return false;
        }
        executionOrder_[stage] = sorted;

        // Compute parallel batches from the sorted order.
        computeParallelBatches(stage, sorted);
    }

    built_ = true;
    return true;
}

bool SystemScheduler::topologicalSort(const std::vector<SystemTypeId>& ids,
                                      std::vector<SystemTypeId>& sorted) {
    // Build a local set of IDs for this stage (for fast lookup).
    std::unordered_set<SystemTypeId> stageSet(ids.begin(), ids.end());

    // Compute in-degree for each system in this stage, considering
    // only dependencies where both endpoints are in the same stage.
    std::unordered_map<SystemTypeId, uint32_t> inDegree;
    for (auto id : ids) {
        inDegree[id] = 0;
    }

    for (auto id : ids) {
        if (auto it = reverseDeps_.find(id); it != reverseDeps_.end()) {
            for (auto dep : it->second) {
                if (stageSet.contains(dep)) {
                    ++inDegree[id];
                }
            }
        }
    }

    // Kahn's algorithm: start with systems that have no incoming edges.
    std::queue<SystemTypeId> ready;
    for (auto id : ids) {
        if (inDegree[id] == 0) {
            ready.push(id);
        }
    }

    sorted.clear();
    sorted.reserve(ids.size());

    while (!ready.empty()) {
        auto current = ready.front();
        ready.pop();
        sorted.push_back(current);

        // Reduce in-degree for successors in this stage.
        if (auto it = dependencies_.find(current); it != dependencies_.end()) {
            for (auto successor : it->second) {
                if (!stageSet.contains(successor)) {
                    continue;
                }
                --inDegree[successor];
                if (inDegree[successor] == 0) {
                    ready.push(successor);
                }
            }
        }
    }

    // If not all systems were visited, there is a cycle.
    if (sorted.size() != ids.size()) {
        std::ostringstream oss;
        oss << "Circular dependency detected among systems: [";
        bool first = true;
        for (auto id : ids) {
            if (inDegree[id] != 0) {
                if (!first) {
                    oss << ", ";
                }
                oss << systems_.at(id).instance->GetName();
                first = false;
            }
        }
        oss << "]";
        lastError_ = oss.str();
        return false;
    }

    return true;
}

// ── Parallel batch computation ──────────────────────────────────────────

void SystemScheduler::computeParallelBatches(SystemStage stage,
                                             const std::vector<SystemTypeId>& order) {
    auto& batches = parallelBatches_[stage];
    batches.clear();

    if (order.empty()) {
        return;
    }

    // Track which batch each system was assigned to, so that
    // dependency constraints (predecessor must be in earlier batch)
    // are respected.
    std::unordered_map<SystemTypeId, std::size_t> systemBatch;

    // Cache access info per system to avoid repeated virtual calls.
    std::unordered_map<SystemTypeId, SystemAccessInfo> accessCache;
    for (auto sysId : order) {
        accessCache[sysId] = systems_.at(sysId).instance->GetAccessInfo();
    }

    // Minimum batch index forced by sync points.
    std::size_t minimumBatch = 0;

    for (auto sysId : order) {
        const auto& myAccess = accessCache[sysId];

        // Earliest batch this system can go into, considering
        // explicit dependencies.
        std::size_t minBatch = minimumBatch;
        if (auto it = reverseDeps_.find(sysId); it != reverseDeps_.end()) {
            for (auto pred : it->second) {
                if (auto bit = systemBatch.find(pred); bit != systemBatch.end()) {
                    minBatch = std::max(minBatch, bit->second + 1);
                }
            }
        }

        // Try to fit into an existing batch starting from minBatch.
        bool placed = false;
        for (std::size_t b = minBatch; b < batches.size(); ++b) {
            bool conflicts = false;
            for (auto otherId : batches[b].systems) {
                if (myAccess.ConflictsWith(accessCache[otherId])) {
                    conflicts = true;
                    break;
                }
            }
            if (!conflicts) {
                batches[b].systems.push_back(sysId);
                systemBatch[sysId] = b;
                placed = true;
                break;
            }
        }

        if (!placed) {
            // Ensure we don't create a batch before minBatch.
            while (batches.size() < minBatch) {
                batches.emplace_back();
            }
            batches.emplace_back();
            batches.back().systems.push_back(sysId);
            systemBatch[sysId] = batches.size() - 1;
        }

        // If this system is a sync point, force subsequent systems
        // into a strictly later batch.
        if (syncPoints_.count(sysId)) {
            minimumBatch = systemBatch[sysId] + 1;
        }
    }
}

// ── Parallel execution control ──────────────────────────────────────────

void SystemScheduler::SetParallelExecutor(ParallelExecutor executor) {
    parallelExecutor_ = std::move(executor);
}

void SystemScheduler::EnableParallelExecution(bool enable) {
    parallelEnabled_ = enable;
}

bool SystemScheduler::IsParallelExecutionEnabled() const noexcept {
    return parallelEnabled_;
}

// ── Sync points ─────────────────────────────────────────────────────────

void SystemScheduler::AddSyncPoint(SystemTypeId afterSystem) {
    syncPoints_.insert(afterSystem);
    built_ = false;
}

// ── Execution ───────────────────────────────────────────────────────────

void SystemScheduler::Execute(float deltaTime) {
    assert(built_ && "SystemScheduler::Build() must be called before Execute()");

    // Stage execution order: PreUpdate -> Update -> PostUpdate.
    static constexpr SystemStage kVariableStages[] = {
        SystemStage::PreUpdate,
        SystemStage::Update,
        SystemStage::PostUpdate,
    };

    for (auto stage : kVariableStages) {
        executeStage(stage, deltaTime);
    }

    // FixedUpdate: accumulate time and tick at fixed intervals.
    auto fixedIt = executionOrder_.find(SystemStage::FixedUpdate);
    if (fixedIt != executionOrder_.end() && !fixedIt->second.empty()) {
        fixedTimeAccumulator_ += deltaTime;

        while (fixedTimeAccumulator_ >= fixedTimeStep_) {
            fixedTimeAccumulator_ -= fixedTimeStep_;
            executeStage(SystemStage::FixedUpdate, fixedTimeStep_);
        }
    }
}

void SystemScheduler::executeStage(SystemStage stage, float deltaTime) {
    if (parallelEnabled_ && parallelExecutor_) {
        auto it = parallelBatches_.find(stage);
        if (it == parallelBatches_.end()) {
            return;
        }
        for (const auto& batch : it->second) {
            executeBatch(batch, deltaTime);
        }
    } else {
        auto it = executionOrder_.find(stage);
        if (it == executionOrder_.end()) {
            return;
        }
        for (auto typeId : it->second) {
            auto& entry = systems_.at(typeId);
            if (entry.enabled) {
                entry.instance->Execute(deltaTime);
            }
        }
    }
}

void SystemScheduler::executeBatch(const ParallelBatch& batch, float deltaTime) {
    // Collect enabled systems.
    std::vector<std::function<void()>> tasks;
    tasks.reserve(batch.systems.size());

    for (auto sysId : batch.systems) {
        auto& entry = systems_.at(sysId);
        if (entry.enabled) {
            ISystem* sys = entry.instance.get();
            tasks.emplace_back([sys, deltaTime] { sys->Execute(deltaTime); });
        }
    }

    if (tasks.empty()) {
        return;
    }

    if (tasks.size() == 1) {
        // Single task — run directly, no parallel overhead.
        tasks[0]();
    } else {
        // Multiple tasks — dispatch via parallel executor.
        parallelExecutor_(tasks);
    }
}

// ── Error reporting ─────────────────────────────────────────────────────

const std::string& SystemScheduler::GetLastError() const noexcept {
    return lastError_;
}

// ── FixedUpdate configuration ───────────────────────────────────────────

void SystemScheduler::SetFixedTimeStep(float seconds) {
    assert(seconds > 0.0f && "Fixed timestep must be positive");
    fixedTimeStep_ = seconds;
}

float SystemScheduler::GetFixedTimeStep() const noexcept {
    return fixedTimeStep_;
}

// ── Queries ─────────────────────────────────────────────────────────────

const std::vector<SystemTypeId>& SystemScheduler::GetExecutionOrder(SystemStage stage) const {
    auto it = executionOrder_.find(stage);
    if (it != executionOrder_.end()) {
        return it->second;
    }
    return kEmptyOrder_;
}

const std::vector<SystemScheduler::ParallelBatch>& SystemScheduler::GetParallelBatches(
    SystemStage stage) const {
    auto it = parallelBatches_.find(stage);
    if (it != parallelBatches_.end()) {
        return it->second;
    }
    return kEmptyBatches_;
}

}  // namespace cgs::ecs
