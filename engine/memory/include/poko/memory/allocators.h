#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace poko::memory {

// Memory skeleton (Phase 1): deterministic allocators for frame-scoped and
// long-lived data. No STL container uses these yet — adoption is opt-in.

// Bump allocator over a caller-owned or owned block. O(1) alloc/free-all,
// no per-object free. Typical use: per-frame temporaries.
class Arena {
public:
    explicit Arena(std::size_t capacity_bytes);
    ~Arena();

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    // Returns nullptr when out of space (caller may grow by making a new
    // arena — arenas intentionally cannot realloc in place).
    void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));

    template <typename T>
    T* allocate() {
        return static_cast<T*>(allocate(sizeof(T), alignof(T)));
    }

    void reset() noexcept {
        offset_ = 0;
    }
    std::size_t used() const noexcept {
        return offset_;
    }
    std::size_t capacity() const noexcept {
        return capacity_;
    }

private:
    std::unique_ptr<std::uint8_t[]> owned_;
    std::uint8_t* base_;
    std::size_t capacity_;
    std::size_t offset_ = 0;
};

// Fixed-size pool allocator: O(1) alloc/free, stable addresses, no headers
// per object beyond the freelist link. For known-type repeated objects
// (particles, draw items, network messages).
template <typename T>
class Pool {
public:
    explicit Pool(std::size_t capacity) : blocks_(capacity), free_list_(capacity) {
        for (std::size_t i = 0; i < capacity; ++i) {
            free_list_[i] = static_cast<std::uint32_t>(i + 1);
        }
        free_list_[capacity - 1] = kNil;
    }

    static constexpr std::uint32_t kNil = 0xFFFFFFFFu;

    // Returns index+1 as handle (0 = invalid), or 0 when exhausted.
    std::uint32_t acquire() noexcept {
        if (free_head_ == kNil) {
            return 0;
        }
        const std::uint32_t index = free_head_;
        free_head_ = free_list_[index];
        new (&blocks_[index]) T();
        return index + 1;
    }

    void release(std::uint32_t handle) noexcept {
        if (handle == 0) {
            return;
        }
        const std::uint32_t index = handle - 1;
        blocks_[index].~T();
        free_list_[index] = free_head_;
        free_head_ = index;
    }

    T* get(std::uint32_t handle) noexcept {
        return handle == 0 ? nullptr : &blocks_[handle - 1];
    }
    const T* get(std::uint32_t handle) const noexcept {
        return handle == 0 ? nullptr : &blocks_[handle - 1];
    }

    std::size_t capacity() const noexcept {
        return blocks_.size();
    }

private:
    std::vector<T> blocks_;
    std::vector<std::uint32_t> free_list_;
    std::uint32_t free_head_ = 0;
};

} // namespace poko::memory
