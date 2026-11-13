#include "poko/ecs/registry.h"

namespace poko::ecs {

Entity Registry::create_entity() {
    std::lock_guard<std::mutex> guard(mutex_);
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        alive_[index] = true;
    } else {
        index = static_cast<HandleIndex>(alive_.size());
        alive_.push_back(true);
        generations_.push_back(0);
    }
    return make_handle(index, generations_[index]);
}

void Registry::destroy_entity(Entity entity) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!alive_unlocked(entity)) {
        return;
    }
    for (auto& [key, pool] : pools_) {
        (void)key;
        pool->destroy_entity(entity);
    }
    alive_[entity.index] = false;
    ++generations_[entity.index];
    free_.push_back(entity.index);
}

std::uint32_t Registry::entity_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t count = 0;
    for (bool is_alive : alive_) {
        if (is_alive) {
            ++count;
        }
    }
    return count;
}

bool Registry::alive(Entity entity) const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return alive_unlocked(entity);
}

} // namespace poko::ecs
