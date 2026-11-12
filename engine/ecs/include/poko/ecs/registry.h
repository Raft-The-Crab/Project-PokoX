#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "poko/core/types.h"

namespace poko::ecs {

using Entity = TypedHandle;

// Minimal ECS registry skeleton (Phase 1). Entities are generation-checked
// handles; components are POD-ish values stored per type. Queries/views and
// archetypes are deferred — this skeleton owns identity + component storage.
class Registry {
public:
    Registry() = default;

    Entity create_entity();
    void destroy_entity(Entity entity); // removes all components

    template <typename T>
    void add_component(Entity entity, const T& component) {
        pool_for<T>().assign(entity, component);
    }

    template <typename T>
    T* get_component(Entity entity) {
        return pool_for<T>().get(entity);
    }

    template <typename T>
    const T* get_component(Entity entity) const {
        return const_cast<Registry*>(this)->pool_for<T>().get(entity);
    }

    template <typename T>
    bool remove_component(Entity entity) {
        return pool_for<T>().remove(entity);
    }

    template <typename T>
    bool has_component(Entity entity) const {
        return const_cast<Registry*>(this)->pool_for<T>().contains(entity);
    }

    std::uint32_t entity_count() const noexcept;
    bool alive(Entity entity) const noexcept;

private:
    struct IComponentPool {
        virtual ~IComponentPool() = default;
        virtual void destroy_entity(Entity entity) = 0;
    };

    template <typename T>
    class ComponentPool final : public IComponentPool {
    public:
        void assign(Entity entity, const T& value) {
            slots_[entity.index] = Slot{entity.generation, value};
        }
        T* get(Entity entity) {
            auto it = slots_.find(entity.index);
            if (it == slots_.end() || it->second.generation != entity.generation) {
                return nullptr;
            }
            return &it->second.value;
        }
        bool remove(Entity entity) {
            auto it = slots_.find(entity.index);
            if (it == slots_.end() || it->second.generation != entity.generation) {
                return false;
            }
            slots_.erase(it);
            return true;
        }
        bool contains(Entity entity) const {
            auto it = slots_.find(entity.index);
            return it != slots_.end() && it->second.generation == entity.generation;
        }
        void destroy_entity(Entity entity) override {
            slots_.erase(entity.index);
        }

    private:
        struct Slot {
            Generation generation = 0;
            T value{};
        };
        std::unordered_map<HandleIndex, Slot> slots_;
    };

    template <typename T>
    ComponentPool<T>& pool_for() {
        const std::type_index key = typeid(T);
        auto it = pools_.find(key);
        if (it == pools_.end()) {
            auto pool = std::make_unique<ComponentPool<T>>();
            auto& ref = *pool;
            pools_.emplace(key, std::move(pool));
            return ref;
        }
        return static_cast<ComponentPool<T>&>(*it->second);
    }

    mutable std::mutex mutex_;
    std::vector<bool> alive_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools_;
};

} // namespace poko::ecs
