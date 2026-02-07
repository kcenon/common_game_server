/// @file system_scheduler.cpp
/// @brief System scheduling implementation.
///
/// Provides staged execution with topological ordering via Kahn's
/// algorithm for dependency resolution.  Circular dependencies are
/// detected and reported with the names of the involved systems.
///
/// @see SDS-MOD-012

#include "cgs/ecs/system_scheduler.hpp"

#include <algorithm>
#include <queue>
#include <sstream>

namespace cgs::ecs {

// ── Static member ───────────────────────────────────────────────────────

const std::vector<SystemTypeId> SystemScheduler::kEmptyOrder_;

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

    // Process each stage independently.
    for (const auto& [stage, ids] : stageGroups_) {
        std::vector<SystemTypeId> sorted;
        if (!topologicalSort(ids, sorted)) {
            built_ = false;
            return false;
        }
        executionOrder_[stage] = std::move(sorted);
    }

    built_ = true;
    return true;
}

bool SystemScheduler::topologicalSort(
    const std::vector<SystemTypeId>& ids,
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
        auto it = executionOrder_.find(stage);
        if (it == executionOrder_.end()) {
            continue;
        }
        for (auto typeId : it->second) {
            auto& entry = systems_.at(typeId);
            if (entry.enabled) {
                entry.instance->Execute(deltaTime);
            }
        }
    }

    // FixedUpdate: accumulate time and tick at fixed intervals.
    auto fixedIt = executionOrder_.find(SystemStage::FixedUpdate);
    if (fixedIt != executionOrder_.end() && !fixedIt->second.empty()) {
        fixedTimeAccumulator_ += deltaTime;

        while (fixedTimeAccumulator_ >= fixedTimeStep_) {
            fixedTimeAccumulator_ -= fixedTimeStep_;

            for (auto typeId : fixedIt->second) {
                auto& entry = systems_.at(typeId);
                if (entry.enabled) {
                    entry.instance->Execute(fixedTimeStep_);
                }
            }
        }
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

const std::vector<SystemTypeId>&
SystemScheduler::GetExecutionOrder(SystemStage stage) const {
    auto it = executionOrder_.find(stage);
    if (it != executionOrder_.end()) {
        return it->second;
    }
    return kEmptyOrder_;
}

} // namespace cgs::ecs
