#include "poko_light_list.h"

namespace poko::lighting {

bool LightList::live(LightHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < slots_.size() &&
           generations_[handle.index] == handle.generation && slots_[handle.index].alive;
}

Result<LightHandle> LightList::add(const LightDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid(desc)) {
        return Result<LightHandle>::fail(ErrorCode::InvalidArgument, "invalid light descriptor");
    }
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        slots_[index] = Slot{};
    } else {
        index = static_cast<HandleIndex>(slots_.size());
        slots_.push_back(Slot{});
        generations_.push_back(0);
    }
    auto& slot = slots_[index];
    slot.alive = true;
    slot.desc = desc;
    return Result<LightHandle>::success(make_handle(index, generations_[index]));
}

Status LightList::remove(LightHandle light) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(light)) {
        return Status::fail(ErrorCode::NotFound, "light handle is not live");
    }
    slots_[light.index].alive = false;
    ++generations_[light.index];
    free_.push_back(light.index);
    return Status::success();
}

Status LightList::set_desc(LightHandle light, const LightDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(light)) {
        return Status::fail(ErrorCode::NotFound, "light handle is not live");
    }
    if (!valid(desc)) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid light descriptor");
    }
    slots_[light.index].desc = desc;
    return Status::success();
}

Result<LightDesc> LightList::get(LightHandle light) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(light)) {
        return Result<LightDesc>::fail(ErrorCode::NotFound, "light handle is not live");
    }
    return Result<LightDesc>::success(slots_[light.index].desc);
}

void LightList::set_ambient(const AmbientLight& ambient) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    ambient_ = ambient;
}

AmbientLight LightList::ambient() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return ambient_;
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
