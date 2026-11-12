#include "poko/lighting/light_list.h"

namespace poko::lighting {

bool LightList::live(LightHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < slots_.size() &&
           generations_[handle.index] == handle.generation && slots_[handle.index].alive;
}

std::vector<LightList::FrameLight> LightList::frame_lights() const {
    std::lock_guard<std::mutex> guard(mutex_);
    std::vector<FrameLight> out;
    for (const auto& slot : slots_) {
        if (slot.alive) {
            out.push_back(FrameLight{slot.desc.kind, slot.desc});
        }
    }
    return out;
}

std::uint32_t LightList::count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t n = 0;
    for (const auto& slot : slots_) {
        if (slot.alive) {
            ++n;
        }
    }
    return n;
}

} // namespace poko::lighting
