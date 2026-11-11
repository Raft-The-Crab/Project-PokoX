#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "poko_types.h"

namespace poko {

// Explicit ownership vocabulary (systemplan/03: "explicit ownership and
// thread-safety contracts", "resource lifetime").

// Non-owning pointer. Never deletes; documents that the callee borrows.
template <typename T>
class ObserverPtr {
public:
    ObserverPtr() = default;
    explicit ObserverPtr(T* pointer) noexcept : pointer_(pointer) {}

    T* get() const noexcept {
        return pointer_;
    }
    T& operator*() const {
        return *pointer_;
    }
    T* operator->() const noexcept {
        return pointer_;
    }
    explicit operator bool() const noexcept {
        return pointer_ != nullptr;
    }

    void reset(T* pointer = nullptr) noexcept {
        pointer_ = pointer;
    }

private:
    T* pointer_ = nullptr;
};

// Runs a cleanup action on scope exit unless released. Use for deterministic
// resource lifetime without global state.
class ScopeExit {
public:
    explicit ScopeExit(std::function<void()> action) : action_(std::move(action)) {}

    ~ScopeExit() {
        if (action_) {
            action_();
        }
    }

    void release() noexcept {
        action_ = {};
    }

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;

private:
    std::function<void()> action_;
};

// Owns resources via unique_ptr and hands out generation-checked handles.
// A released slot bumps its generation, so stale handles become invalid
// instead of aliasing a new resource.
//
// Thread-safety contract: add(), get(), release() and size() may be called
// concurrently. The returned raw pointer is borrowed and valid only while the
// handle remains valid; callers must not retain it across release().
template <typename T>
class ResourceRegistry {
public:
    TypedHandle add(std::unique_ptr<T> resource) {
        if (!resource) {
            return {};
        }
        std::lock_guard<std::mutex> guard(mutex_);
        HandleIndex index;
        if (!free_.empty()) {
            index = free_.back();
            free_.pop_back();
            slots_[index] = std::move(resource);
        } else {
            index = static_cast<HandleIndex>(slots_.size());
            slots_.push_back(std::move(resource));
            generations_.push_back(0);
        }
        return make_handle(index, generations_[index]);
    }

    T* get(const TypedHandle& handle) const {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!handle_is_live(handle)) {
            return nullptr;
        }
        return slots_[handle.index].get();
    }

    bool release(const TypedHandle& handle) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!handle_is_live(handle)) {
            return false;
        }
        slots_[handle.index].reset();
        ++generations_[handle.index];
        free_.push_back(handle.index);
        return true;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> guard(mutex_);
        std::size_t live = 0;
        for (const auto& slot : slots_) {
            if (slot) {
                ++live;
            }
        }
        return live;
    }

private:
    bool handle_is_live(const TypedHandle& handle) const noexcept {
        return is_valid(handle) && static_cast<std::size_t>(handle.index) < slots_.size() &&
               generations_[handle.index] == handle.generation && slots_[handle.index] != nullptr;
    }

    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<T>> slots_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
};

} // namespace poko
