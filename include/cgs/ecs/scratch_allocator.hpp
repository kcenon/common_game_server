#pragma once

/// @file scratch_allocator.hpp
/// @brief Thread-local linear allocator for temporary per-frame data.
///
/// ScratchAllocator provides fast, contention-free temporary memory
/// for systems running in parallel.  Each thread gets its own buffer
/// via thread_local storage.  Call Reset() to reclaim all memory
/// (typically once per frame or per batch).
///
/// @see SDS-MOD-012 (Parallel Execution)

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <vector>

namespace cgs::ecs {

/// Thread-local linear (arena) allocator for scratch memory.
///
/// All allocations are 16-byte aligned for SIMD compatibility.
/// Memory is not freed individually; call Reset() to reclaim
/// everything at once.
///
/// Usage:
/// @code
///   void MySystem::Execute(float dt) {
///       auto& scratch = ScratchAllocator::GetThreadLocal();
///       scratch.Reset();
///
///       auto* temp = scratch.AllocateArray<float>(1024);
///       // Use temp for intermediate computation...
///   }
/// @endcode
class ScratchAllocator {
public:
    static constexpr std::size_t kDefaultCapacity = 64 * 1024;  // 64 KB
    static constexpr std::size_t kAlignment = 16;

    explicit ScratchAllocator(std::size_t capacity = kDefaultCapacity) : buffer_(capacity) {}

    // Non-copyable, non-movable (thread-local singleton).
    ScratchAllocator(const ScratchAllocator&) = delete;
    ScratchAllocator& operator=(const ScratchAllocator&) = delete;

    /// Get the thread-local scratch allocator instance.
    static ScratchAllocator& GetThreadLocal() {
        thread_local ScratchAllocator instance;
        return instance;
    }

    /// Allocate @p bytes of raw memory (16-byte aligned).
    ///
    /// @return Pointer to the allocated memory, or nullptr if the
    ///         buffer is exhausted.
    void* Allocate(std::size_t bytes) {
        // Align up to kAlignment.
        const std::size_t aligned = (bytes + kAlignment - 1) & ~(kAlignment - 1);

        if (offset_ + aligned > buffer_.size()) {
            // Grow the buffer to accommodate the request.
            buffer_.resize((offset_ + aligned) * 2);
        }

        void* ptr = buffer_.data() + offset_;
        offset_ += aligned;
        return ptr;
    }

    /// Allocate and construct a single object of type @p T.
    template <typename T, typename... Args>
    T* New(Args&&... args) {
        static_assert(alignof(T) <= kAlignment,
                      "Type alignment exceeds scratch allocator alignment");
        void* mem = Allocate(sizeof(T));
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    /// Allocate an uninitialized array of @p count elements.
    template <typename T>
    T* AllocateArray(std::size_t count) {
        static_assert(alignof(T) <= kAlignment,
                      "Type alignment exceeds scratch allocator alignment");
        void* mem = Allocate(sizeof(T) * count);
        return static_cast<T*>(mem);
    }

    /// Reset all allocations without freeing the underlying buffer.
    void Reset() noexcept { offset_ = 0; }

    /// Current number of bytes in use.
    [[nodiscard]] std::size_t BytesUsed() const noexcept { return offset_; }

    /// Total capacity of the underlying buffer.
    [[nodiscard]] std::size_t Capacity() const noexcept { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
    std::size_t offset_ = 0;
};

}  // namespace cgs::ecs
