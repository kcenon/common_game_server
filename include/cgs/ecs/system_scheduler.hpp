#pragma once

/// @file system_scheduler.hpp
/// @brief System scheduling with dependency management and parallel
///        execution for the ECS.
///
/// SystemScheduler manages system registration, stage-based grouping,
/// dependency-driven topological ordering, and runtime enable/disable.
/// FixedUpdate runs at a configurable fixed interval independent of
/// frame rate.
///
/// Parallel execution: systems that declare non-conflicting component
/// access patterns (via `Read<T>`/`Write<T>`) are automatically grouped
/// into parallel batches.  A user-provided ParallelExecutor dispatches
/// these batches to a thread pool.
///
/// @see docs/reference/ECS_DESIGN.md  Section 3
/// @see SDS-MOD-012

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cgs/ecs/component_type_id.hpp"

namespace cgs::ecs {

// ── System type identification ──────────────────────────────────────────

/// Integer type used to identify system types at runtime.
using SystemTypeId = uint32_t;

/// Sentinel value meaning "no system type".
constexpr SystemTypeId kInvalidSystemTypeId = static_cast<SystemTypeId>(-1);

namespace detail {

/// Global atomic counter for generating unique SystemTypeId values.
inline SystemTypeId nextSystemTypeId() noexcept {
    static std::atomic<SystemTypeId> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

} // namespace detail

/// Obtain the unique SystemTypeId for system type `T`.
///
/// Usage:
/// @code
///   auto id = SystemType<MovementSystem>::Id();
/// @endcode
template <typename T>
struct SystemType {
    static SystemTypeId Id() noexcept {
        static const SystemTypeId value = detail::nextSystemTypeId();
        return value;
    }
};

// ── Execution stages ────────────────────────────────────────────────────

/// Execution stage for a system.  Stages are executed in order:
/// PreUpdate -> Update -> PostUpdate -> FixedUpdate.
enum class SystemStage : uint8_t {
    PreUpdate,   ///< Before main update (input processing, etc.)
    Update,      ///< Main game logic update
    PostUpdate,  ///< After main update (cleanup, event dispatch, etc.)
    FixedUpdate  ///< Fixed-timestep update (physics, etc.)
};

// ── Component access patterns ──────────────────────────────────────────

/// Describes which component types a system reads and writes.
///
/// Used by the scheduler to determine which systems can safely execute
/// in parallel.  If both `reads` and `writes` are empty the system is
/// treated as having undeclared access and will never be parallelized.
struct SystemAccessInfo {
    std::unordered_set<ComponentTypeId> reads;
    std::unordered_set<ComponentTypeId> writes;

    /// Check whether this access pattern conflicts with @p other.
    ///
    /// Two systems conflict when they have write-write or read-write
    /// overlap on the same component type, or when either system has
    /// undeclared access (empty reads AND writes).
    [[nodiscard]] bool ConflictsWith(const SystemAccessInfo& other) const;
};

/// Compile-time helper to declare read access to component types.
///
/// @code
///   SystemAccessInfo GetAccessInfo() const override {
///       SystemAccessInfo info;
///       Read<Position, Velocity>::Apply(info);
///       return info;
///   }
/// @endcode
template <typename... Ts>
struct Read {
    static void Apply(SystemAccessInfo& info) {
        ((info.reads.insert(ComponentType<Ts>::Id())), ...);
    }
};

/// Compile-time helper to declare write access to component types.
template <typename... Ts>
struct Write {
    static void Apply(SystemAccessInfo& info) {
        ((info.writes.insert(ComponentType<Ts>::Id())), ...);
    }
};

// ── System interface ────────────────────────────────────────────────────

/// Abstract base class for ECS systems.
///
/// Concrete systems override `Execute()` and optionally `GetStage()`
/// and `GetAccessInfo()`.  The scheduler owns and manages the lifecycle
/// of registered systems.
class ISystem {
public:
    virtual ~ISystem() = default;

    /// Execute this system's logic for the given time step.
    ///
    /// @param deltaTime  Frame delta time in seconds.
    virtual void Execute(float deltaTime) = 0;

    /// Return the execution stage this system belongs to.
    ///
    /// Override to place the system in a non-default stage.
    /// Default is SystemStage::Update.
    [[nodiscard]] virtual SystemStage GetStage() const { return SystemStage::Update; }

    /// Return a human-readable name for this system (for diagnostics).
    [[nodiscard]] virtual std::string_view GetName() const = 0;

    /// Return the component access patterns for this system.
    ///
    /// Override to declare which component types the system reads and
    /// writes.  Systems with declared, non-conflicting access patterns
    /// may be executed in parallel by the scheduler.
    ///
    /// Default: empty (undeclared — always runs sequentially).
    [[nodiscard]] virtual SystemAccessInfo GetAccessInfo() const { return {}; }
};

// ── System scheduler ────────────────────────────────────────────────────

/// Manages system registration, dependency ordering, and staged execution.
///
/// Systems are grouped by stage and topologically sorted within each
/// stage according to explicit dependencies.  Circular dependencies
/// are detected at build time and reported via an error string.
///
/// FixedUpdate accumulates elapsed time and ticks at a fixed interval
/// (default 1/60 s) independent of the variable frame rate.
class SystemScheduler {
public:
    SystemScheduler() = default;

    // Non-copyable, movable.
    SystemScheduler(const SystemScheduler&) = delete;
    SystemScheduler& operator=(const SystemScheduler&) = delete;
    SystemScheduler(SystemScheduler&&) noexcept = default;
    SystemScheduler& operator=(SystemScheduler&&) noexcept = default;

    // ── Registration ────────────────────────────────────────────────

    /// Register a system of type `T`, constructing it in-place.
    ///
    /// The system's stage is determined by `T::GetStage()`.
    /// Re-registering the same type is a no-op and returns the
    /// existing instance.
    ///
    /// @tparam T       Concrete system type (must derive from ISystem).
    /// @tparam Args    Constructor argument types.
    /// @param  args    Forwarded to T's constructor.
    /// @return Reference to the registered system.
    template <typename T, typename... Args>
    T& Register(Args&&... args);

    /// Return the number of registered systems.
    [[nodiscard]] std::size_t SystemCount() const noexcept;

    // ── Dependencies ────────────────────────────────────────────────

    /// Declare that `before` must execute before `after`.
    ///
    /// Both systems must already be registered.  Dependencies across
    /// different stages are silently ignored (stage ordering is
    /// implicit).
    ///
    /// @return true if the dependency was added, false if either
    ///         system is not registered or they belong to different stages.
    bool AddDependency(SystemTypeId before, SystemTypeId after);

    /// Convenience: declare a dependency using system types.
    template <typename Before, typename After>
    bool AddDependency();

    // ── Enable / disable ────────────────────────────────────────────

    /// Enable or disable a system at runtime.
    ///
    /// Disabled systems are skipped during execution without changing
    /// the execution plan.  They remain registered and can be
    /// re-enabled at any time.
    void SetEnabled(SystemTypeId system, bool enabled);

    /// Convenience: enable/disable using system type.
    template <typename T>
    void SetEnabled(bool enabled);

    /// Check if a system is enabled.
    [[nodiscard]] bool IsEnabled(SystemTypeId system) const;

    // ── Execution ───────────────────────────────────────────────────

    /// Build the execution plan (topological sort per stage).
    ///
    /// Must be called after all registrations and dependencies are set
    /// and before the first Execute().  Returns true on success.
    /// On failure (circular dependency), returns false and populates
    /// the error string retrievable via GetLastError().
    [[nodiscard]] bool Build();

    /// Execute all enabled systems in stage order.
    ///
    /// PreUpdate, Update, and PostUpdate receive @p deltaTime.
    /// FixedUpdate accumulates time and ticks at the configured
    /// fixed interval.
    ///
    /// @pre Build() must have been called successfully.
    void Execute(float deltaTime);

    /// Return the error message from the last failed Build().
    [[nodiscard]] const std::string& GetLastError() const noexcept;

    // ── FixedUpdate configuration ───────────────────────────────────

    /// Set the fixed timestep for FixedUpdate (default: 1/60 s).
    void SetFixedTimeStep(float seconds);

    /// Get the current fixed timestep.
    [[nodiscard]] float GetFixedTimeStep() const noexcept;

    // ── Parallel execution ──────────────────────────────────────────

    /// A batch of systems that can execute in parallel.
    struct ParallelBatch {
        std::vector<SystemTypeId> systems;
    };

    /// Function that executes a vector of tasks in parallel.
    /// Must block until all tasks complete.
    using ParallelExecutor =
        std::function<void(const std::vector<std::function<void()>>&)>;

    /// Set the executor for parallel system batches.
    ///
    /// If not set (or set to nullptr), systems always run sequentially
    /// even when parallel execution is enabled.
    void SetParallelExecutor(ParallelExecutor executor);

    /// Enable or disable parallel execution.
    ///
    /// When enabled, Build() computes parallel batches from system
    /// access patterns and Execute() dispatches them via the
    /// ParallelExecutor.
    ///
    /// @pre A ParallelExecutor must be set for actual parallelism.
    void EnableParallelExecution(bool enable);

    /// Check whether parallel execution is enabled.
    [[nodiscard]] bool IsParallelExecutionEnabled() const noexcept;

    // ── Sync points ─────────────────────────────────────────────────

    /// Register a sync point after system @p afterSystem.
    ///
    /// Forces a parallel-batch boundary: all systems up to and
    /// including @p afterSystem must complete before any later system
    /// in the same stage may start.
    void AddSyncPoint(SystemTypeId afterSystem);

    /// Convenience: register a sync point after system type T.
    template <typename T>
    void AddSyncPoint();

    // ── Queries ─────────────────────────────────────────────────────

    /// Retrieve a registered system by type.
    ///
    /// @return Pointer to the system, or nullptr if not registered.
    template <typename T>
    [[nodiscard]] T* GetSystem();

    /// Retrieve the execution order for a specific stage (after Build).
    ///
    /// @return Vector of SystemTypeIds in execution order.
    [[nodiscard]] const std::vector<SystemTypeId>&
    GetExecutionOrder(SystemStage stage) const;

    /// Retrieve the parallel batches for a specific stage (after Build).
    ///
    /// @return Vector of ParallelBatches in execution order.
    [[nodiscard]] const std::vector<ParallelBatch>&
    GetParallelBatches(SystemStage stage) const;

private:
    /// Internal entry for a registered system.
    struct SystemEntry {
        std::unique_ptr<ISystem> instance;
        SystemTypeId typeId = kInvalidSystemTypeId;
        SystemStage stage = SystemStage::Update;
        bool enabled = true;
    };

    /// Perform topological sort on systems within a stage.
    ///
    /// @param ids  System type IDs belonging to the stage.
    /// @param[out] sorted  Output execution order.
    /// @return true on success, false on circular dependency.
    [[nodiscard]] bool topologicalSort(
        const std::vector<SystemTypeId>& ids,
        std::vector<SystemTypeId>& sorted);

    /// Execute all systems in a single stage (sequential or parallel).
    void executeStage(SystemStage stage, float deltaTime);

    /// Execute a single parallel batch.
    void executeBatch(const ParallelBatch& batch, float deltaTime);

    /// Compute parallel batches from the topological order.
    void computeParallelBatches(
        SystemStage stage,
        const std::vector<SystemTypeId>& order);

    /// All registered systems, keyed by SystemTypeId.
    std::unordered_map<SystemTypeId, SystemEntry> systems_;

    /// Systems grouped by stage (registration order within stage).
    std::unordered_map<SystemStage, std::vector<SystemTypeId>> stageGroups_;

    /// Dependency edges: dependencies_[A] contains all B where A -> B
    /// (A must run before B).
    std::unordered_map<SystemTypeId, std::unordered_set<SystemTypeId>> dependencies_;

    /// Reverse dependency edges: reverseDeps_[B] contains all A where A -> B.
    std::unordered_map<SystemTypeId, std::unordered_set<SystemTypeId>> reverseDeps_;

    /// Computed execution order per stage (populated by Build()).
    std::unordered_map<SystemStage, std::vector<SystemTypeId>> executionOrder_;

    /// Parallel batches per stage (populated by Build()).
    std::unordered_map<SystemStage, std::vector<ParallelBatch>> parallelBatches_;

    /// Systems marked as sync-point boundaries.
    std::unordered_set<SystemTypeId> syncPoints_;

    /// User-provided parallel executor.
    ParallelExecutor parallelExecutor_;

    /// Whether parallel execution is enabled.
    bool parallelEnabled_ = false;

    /// Whether Build() has been called successfully.
    bool built_ = false;

    /// Error string from the last failed Build().
    std::string lastError_;

    /// Fixed timestep for FixedUpdate (seconds).
    float fixedTimeStep_ = 1.0f / 60.0f;

    /// Accumulated time for FixedUpdate.
    float fixedTimeAccumulator_ = 0.0f;

    /// Empty vector returned for stages with no systems.
    static const std::vector<SystemTypeId> kEmptyOrder_;

    /// Empty vector returned for stages with no batches.
    static const std::vector<ParallelBatch> kEmptyBatches_;
};

// ── Template implementations ────────────────────────────────────────────

template <typename T, typename... Args>
T& SystemScheduler::Register(Args&&... args) {
    static_assert(std::is_base_of_v<ISystem, T>,
                  "T must derive from ISystem");

    const auto typeId = SystemType<T>::Id();

    // Return existing instance if already registered.
    if (auto it = systems_.find(typeId); it != systems_.end()) {
        return static_cast<T&>(*it->second.instance);
    }

    auto system = std::make_unique<T>(std::forward<Args>(args)...);
    T& ref = *system;

    SystemEntry entry;
    entry.instance = std::move(system);
    entry.typeId = typeId;
    entry.stage = ref.GetStage();
    entry.enabled = true;

    stageGroups_[entry.stage].push_back(typeId);
    systems_.emplace(typeId, std::move(entry));

    // Invalidate any previously built plan.
    built_ = false;

    return ref;
}

template <typename Before, typename After>
bool SystemScheduler::AddDependency() {
    return AddDependency(SystemType<Before>::Id(), SystemType<After>::Id());
}

template <typename T>
void SystemScheduler::SetEnabled(bool enabled) {
    SetEnabled(SystemType<T>::Id(), enabled);
}

template <typename T>
void SystemScheduler::AddSyncPoint() {
    AddSyncPoint(SystemType<T>::Id());
}

template <typename T>
T* SystemScheduler::GetSystem() {
    const auto typeId = SystemType<T>::Id();
    if (auto it = systems_.find(typeId); it != systems_.end()) {
        return static_cast<T*>(it->second.instance.get());
    }
    return nullptr;
}

} // namespace cgs::ecs
