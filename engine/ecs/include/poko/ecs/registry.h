#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "poko/core/types.h"

namespace poko::ecs {

using Entity = TypedHandle;

// Entity-component registry. Entities are generation-checked handles;
// components live in per-type sparse sets (dense arrays, O(1) add/remove,
// cache-friendly and deterministic iteration order: insertion order, with a
// removal moving the last element into the hole).
//
// Queries: `each<A, B>(fn)` / `entities_with<A, B>()` visit entities that own
// every listed component, driven by the smallest pool. Iteration works on a
// snapshot, so callbacks may add or remove components and create or destroy
// entities; an entity that no longer matches (or died) when its turn comes is
// skipped, and entities added during the pass are not visited.
//
// Thread-safety contract: every method is synchronised, but pointers returned
// by get_component() are only valid until the next structural change (add,
// remove, destroy); concurrent mutation needs external coordination.
class Registry {
public:
    Registry() = default;

    Entity create_entity();
    void destroy_entity(Entity entity); // removes all components

    // Adds or overwrites a component. Returns false for a dead/stale entity.
    template <typename T>
    bool add_component(Entity entity, const T& component) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!alive_unlocked(entity)) {
            return false;
        }
        pool_for<T>().assign(entity, component);
        return true;
    }

    template <typename T>
    T* get_component(Entity entity) {
        std::lock_guard<std::mutex> guard(mutex_);
        return alive_unlocked(entity) ? pool_for<T>().get(entity) : nullptr;
    }

    template <typename T>
    const T* get_component(Entity entity) const {
        return const_cast<Registry*>(this)->get_component<T>(entity);
    }

    template <typename T>
    bool remove_component(Entity entity) {
        std::lock_guard<std::mutex> guard(mutex_);
        return alive_unlocked(entity) && pool_for<T>().remove(entity);
    }

    template <typename T>
    bool has_component(Entity entity) const {
        std::lock_guard<std::mutex> guard(mutex_);
        return alive_unlocked(entity) &&
               const_cast<Registry*>(this)->pool_for<T>().contains(entity);
    }

    // Number of live entities owning a T.
    template <typename T>
    std::uint32_t count() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return static_cast<std::uint32_t>(const_cast<Registry*>(this)->pool_for<T>().size());
    }

    // Entities owning every listed component, in the smallest pool's order.
    template <typename First, typename... Rest>
    std::vector<Entity> entities_with() const {
        std::lock_guard<std::mutex> guard(mutex_);
        auto* self = const_cast<Registry*>(this);
        std::vector<Entity> out;
        const auto smallest = self->smallest_pool<First, Rest...>();
        for (const Entity e : smallest) {
            if (self->pool_for<First>().contains(e) && (self->pool_for<Rest>().contains(e) && ...)) {
                out.push_back(e);
            }
        }
        return out;
    }

    // Calls fn(Entity, First&, Rest&...) for every matching entity.
    template <typename First, typename... Rest, typename Fn>
    void each(Fn&& fn) {
        for (const Entity e : entities_with<First, Rest...>()) {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!alive_unlocked(e)) {
                continue;
            }
            auto& first = pool_for<First>();
            if (!first.contains(e) || !(pool_for<Rest>().contains(e) && ...)) {
                continue;
            }
            First* a = first.get(e);
            std::tuple<Rest*...> rest{pool_for<Rest>().get(e)...};
            lock.unlock();
            std::apply([&](auto*... r) { fn(e, *a, *r...); }, rest);
        }
    }

    std::uint32_t entity_count() const noexcept;
    bool alive(Entity entity) const noexcept;

private:
    struct IComponentPool {
        virtual ~IComponentPool() = default;
        virtual void destroy_entity(Entity entity) = 0;
        virtual std::vector<Entity> entities() const = 0;
    };

    template <typename T>
    class ComponentPool final : public IComponentPool {
    public:
        void assign(Entity entity, const T& value) {
            if (T* existing = get(entity)) {
                *existing = value;
                return;
            }
            if (sparse_.size() <= entity.index) {
                sparse_.resize(static_cast<std::size_t>(entity.index) + 1, kNone);
            }
            sparse_[entity.index] = static_cast<std::uint32_t>(dense_.size());
            dense_.push_back(entity);
            values_.push_back(value);
        }
        T* get(Entity entity) {
            const auto slot = slot_of(entity);
            return slot == kNone ? nullptr : &values_[slot];
        }
        bool contains(Entity entity) const { return slot_of(entity) != kNone; }
        bool remove(Entity entity) {
            const auto slot = slot_of(entity);
            if (slot == kNone) {
                return false;
            }
            const auto last = static_cast<std::uint32_t>(dense_.size() - 1);
            if (slot != last) {
                dense_[slot] = dense_[last];
                values_[slot] = std::move(values_[last]);
                sparse_[dense_[slot].index] = slot;
            }
            dense_.pop_back();
            values_.pop_back();
            sparse_[entity.index] = kNone;
            return true;
        }
        std::size_t size() const { return dense_.size(); }
        void destroy_entity(Entity entity) override { remove(entity); }
        std::vector<Entity> entities() const override { return dense_; }

    private:
        static constexpr std::uint32_t kNone = 0xFFFFFFFFu;
        std::uint32_t slot_of(Entity entity) const {
            if (entity.index >= sparse_.size()) {
                return kNone;
            }
            const auto slot = sparse_[entity.index];
            if (slot == kNone || dense_[slot].generation != entity.generation) {
                return kNone;
            }
            return slot;
        }
        std::vector<std::uint32_t> sparse_;
        std::vector<Entity> dense_;
        std::vector<T> values_;
    };

    bool alive_unlocked(Entity entity) const noexcept {
        return is_valid(entity) && static_cast<std::size_t>(entity.index) < alive_.size() &&
               generations_[entity.index] == entity.generation && alive_[entity.index];
    }

    template <typename T>
    ComponentPool<T>& pool_for() {
        const std::type_index key = typeid(T);
        auto it = pools_.find(key);
        if (it == pools_.end()) {
            it = pools_.emplace(key, std::make_unique<ComponentPool<T>>()).first;
        }
        return static_cast<ComponentPool<T>&>(*it->second);
    }

    // Entities of the pool with the fewest members among the listed types.
    template <typename First, typename... Rest>
    std::vector<Entity> smallest_pool() {
        std::size_t best_size = pool_for<First>().size();
        std::vector<Entity> best = pool_for<First>().entities();
        (void)best_size; // unused when only one type is listed
        ((pool_for<Rest>().size() < best_size
              ? (best_size = pool_for<Rest>().size(), best = pool_for<Rest>().entities(), 0)
              : 0),
         ...);
        return best;
    }

    mutable std::mutex mutex_;
    std::vector<bool> alive_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools_;
};

} // namespace poko::ecs
