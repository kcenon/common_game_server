#pragma once

/// @file behavior_tree.hpp
/// @brief Behavior Tree framework: nodes, blackboard, and execution context.
///
/// Header-only BT framework with polymorphic nodes for runtime composition.
/// Composite nodes (Sequence, Selector, Parallel), decorator nodes
/// (Inverter, Repeater), and leaf nodes (Condition, Action) form a tree
/// that is ticked each AI update to drive entity behavior.
///
/// @see SRS-GML-004.2, SRS-GML-004.3
/// @see SDS-MOD-023

#include "cgs/ecs/entity.hpp"
#include "cgs/game/ai_types.hpp"

#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cgs::game {

// Forward declaration.
struct BTContext;

// ═══════════════════════════════════════════════════════════════════════════
// Blackboard
// ═══════════════════════════════════════════════════════════════════════════

/// Key-value store for sharing data between BT nodes.
///
/// Each AIBrain holds its own Blackboard instance.  Common keys:
/// "target" (Entity), "move_target" (Vector3), "waypoints" (vector<Vector3>).
class Blackboard {
public:
    /// Store a value under the given key (overwrites any existing value).
    template <typename T>
    void Set(const std::string& key, T value) {
        data_[key] = std::move(value);
    }

    /// Retrieve a mutable pointer to the stored value, or nullptr if
    /// the key is absent or the type does not match.
    template <typename T>
    T* Get(const std::string& key) {
        auto it = data_.find(key);
        if (it == data_.end()) {
            return nullptr;
        }
        return std::any_cast<T>(&it->second);
    }

    /// Retrieve a const pointer to the stored value.
    template <typename T>
    const T* Get(const std::string& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) {
            return nullptr;
        }
        return std::any_cast<T>(&it->second);
    }

    /// Check whether a key exists in the blackboard.
    [[nodiscard]] bool Has(const std::string& key) const { return data_.contains(key); }

    /// Remove a single entry.
    void Erase(const std::string& key) { data_.erase(key); }

    /// Remove all entries.
    void Clear() { data_.clear(); }

private:
    std::unordered_map<std::string, std::any> data_;
};

// ═══════════════════════════════════════════════════════════════════════════
// BTContext
// ═══════════════════════════════════════════════════════════════════════════

/// Execution context passed to every BT node during a tick.
struct BTContext {
    cgs::ecs::Entity entity;
    float deltaTime = 0.0f;
    Blackboard* blackboard = nullptr;
};

// ═══════════════════════════════════════════════════════════════════════════
// BTNode base class
// ═══════════════════════════════════════════════════════════════════════════

/// Abstract base for all Behavior Tree nodes.
class BTNode {
public:
    virtual ~BTNode() = default;

    /// Execute this node for one tick.
    virtual BTStatus Tick(BTContext& context) = 0;

    /// Reset node state (called when parent interrupts/restarts).
    virtual void Reset() {}
};

// ═══════════════════════════════════════════════════════════════════════════
// Composite nodes
// ═══════════════════════════════════════════════════════════════════════════

/// Sequence: runs children left-to-right.
/// Succeeds when ALL children succeed.
/// Fails immediately on the first child failure.
/// Returns Running when a child returns Running (resumes from that child).
class BTSequence : public BTNode {
public:
    void AddChild(std::unique_ptr<BTNode> child) { children_.push_back(std::move(child)); }

    BTStatus Tick(BTContext& context) override {
        while (currentChild_ < children_.size()) {
            auto status = children_[currentChild_]->Tick(context);
            if (status == BTStatus::Running) {
                return BTStatus::Running;
            }
            if (status == BTStatus::Failure) {
                currentChild_ = 0;
                return BTStatus::Failure;
            }
            ++currentChild_;
        }
        currentChild_ = 0;
        return BTStatus::Success;
    }

    void Reset() override {
        currentChild_ = 0;
        for (auto& child : children_) {
            child->Reset();
        }
    }

    [[nodiscard]] std::size_t ChildCount() const { return children_.size(); }

private:
    std::vector<std::unique_ptr<BTNode>> children_;
    std::size_t currentChild_ = 0;
};

/// Selector: tries children left-to-right.
/// Succeeds immediately on the first child success.
/// Fails when ALL children fail.
/// Returns Running when a child returns Running (resumes from that child).
class BTSelector : public BTNode {
public:
    void AddChild(std::unique_ptr<BTNode> child) { children_.push_back(std::move(child)); }

    BTStatus Tick(BTContext& context) override {
        while (currentChild_ < children_.size()) {
            auto status = children_[currentChild_]->Tick(context);
            if (status == BTStatus::Running) {
                return BTStatus::Running;
            }
            if (status == BTStatus::Success) {
                currentChild_ = 0;
                return BTStatus::Success;
            }
            ++currentChild_;
        }
        currentChild_ = 0;
        return BTStatus::Failure;
    }

    void Reset() override {
        currentChild_ = 0;
        for (auto& child : children_) {
            child->Reset();
        }
    }

    [[nodiscard]] std::size_t ChildCount() const { return children_.size(); }

private:
    std::vector<std::unique_ptr<BTNode>> children_;
    std::size_t currentChild_ = 0;
};

/// Parallel: ticks ALL children every call.
/// Result depends on the configured policy:
///   RequireAll  — Success when all succeed, Failure on first failure.
///   RequireOne  — Success on first success, Failure when all fail.
///   Returns Running when the decisive threshold is not yet met.
class BTParallel : public BTNode {
public:
    explicit BTParallel(BTParallelPolicy policy = BTParallelPolicy::RequireAll) : policy_(policy) {}

    void AddChild(std::unique_ptr<BTNode> child) { children_.push_back(std::move(child)); }

    BTStatus Tick(BTContext& context) override {
        std::size_t successCount = 0;
        std::size_t failureCount = 0;

        for (auto& child : children_) {
            auto status = child->Tick(context);
            if (status == BTStatus::Success) {
                ++successCount;
            } else if (status == BTStatus::Failure) {
                ++failureCount;
            }
        }

        if (policy_ == BTParallelPolicy::RequireAll) {
            if (successCount == children_.size()) {
                return BTStatus::Success;
            }
            if (failureCount > 0) {
                return BTStatus::Failure;
            }
        } else {
            if (successCount > 0) {
                return BTStatus::Success;
            }
            if (failureCount == children_.size()) {
                return BTStatus::Failure;
            }
        }

        return BTStatus::Running;
    }

    void Reset() override {
        for (auto& child : children_) {
            child->Reset();
        }
    }

    [[nodiscard]] std::size_t ChildCount() const { return children_.size(); }
    [[nodiscard]] BTParallelPolicy GetPolicy() const { return policy_; }

private:
    std::vector<std::unique_ptr<BTNode>> children_;
    BTParallelPolicy policy_;
};

// ═══════════════════════════════════════════════════════════════════════════
// Decorator nodes
// ═══════════════════════════════════════════════════════════════════════════

/// Inverter: inverts the child result (Success <-> Failure).
/// Running passes through unchanged.
class BTInverter : public BTNode {
public:
    explicit BTInverter(std::unique_ptr<BTNode> child) : child_(std::move(child)) {}

    BTStatus Tick(BTContext& context) override {
        auto status = child_->Tick(context);
        if (status == BTStatus::Success) {
            return BTStatus::Failure;
        }
        if (status == BTStatus::Failure) {
            return BTStatus::Success;
        }
        return BTStatus::Running;
    }

    void Reset() override { child_->Reset(); }

private:
    std::unique_ptr<BTNode> child_;
};

/// Repeater: repeats child execution up to maxRepeats times.
/// Returns Running while repetitions remain.
/// Returns Success when all repetitions complete successfully.
/// If maxRepeats is 0, repeats indefinitely (always returns Running).
class BTRepeater : public BTNode {
public:
    explicit BTRepeater(std::unique_ptr<BTNode> child, uint32_t maxRepeats = 0)
        : child_(std::move(child)), maxRepeats_(maxRepeats) {}

    BTStatus Tick(BTContext& context) override {
        auto status = child_->Tick(context);
        if (status == BTStatus::Running) {
            return BTStatus::Running;
        }

        // Child finished (success or failure). Count the iteration.
        ++currentCount_;
        child_->Reset();

        if (maxRepeats_ > 0 && currentCount_ >= maxRepeats_) {
            currentCount_ = 0;
            return BTStatus::Success;
        }

        return BTStatus::Running;
    }

    void Reset() override {
        currentCount_ = 0;
        child_->Reset();
    }

    [[nodiscard]] uint32_t MaxRepeats() const { return maxRepeats_; }

private:
    std::unique_ptr<BTNode> child_;
    uint32_t maxRepeats_ = 0;
    uint32_t currentCount_ = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// Leaf nodes
// ═══════════════════════════════════════════════════════════════════════════

/// Condition: evaluates a predicate and returns Success or Failure.
///
/// Never returns Running — conditions are instantaneous checks.
class BTCondition : public BTNode {
public:
    using Predicate = std::function<bool(BTContext&)>;

    explicit BTCondition(Predicate predicate) : predicate_(std::move(predicate)) {}

    BTStatus Tick(BTContext& context) override {
        return predicate_(context) ? BTStatus::Success : BTStatus::Failure;
    }

private:
    Predicate predicate_;
};

/// Action: performs a game action and returns the result.
///
/// Actions may return Running to indicate multi-tick operations.
class BTAction : public BTNode {
public:
    using ActionFunc = std::function<BTStatus(BTContext&)>;

    explicit BTAction(ActionFunc action) : action_(std::move(action)) {}

    BTStatus Tick(BTContext& context) override { return action_(context); }

private:
    ActionFunc action_;
};

}  // namespace cgs::game
