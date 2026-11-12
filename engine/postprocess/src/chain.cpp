#include "poko/postprocess/chain.h"

namespace poko::postprocess {

bool PostProcessChain::live(PostProcessHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < slots_.size() &&
           generations_[handle.index] == handle.generation && slots_[handle.index].alive;
}

Result<PostProcessHandle> PostProcessChain::add(const PostProcessDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid(desc)) {
        return Result<PostProcessHandle>::fail(ErrorCode::InvalidArgument,
                                               "invalid post-process descriptor");
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
    return Result<PostProcessHandle>::success(make_handle(index, generations_[index]));
}

Status PostProcessChain::remove(PostProcessHandle handle) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(handle)) {
        return Status::fail(ErrorCode::NotFound, "post-process handle is not live");
    }
    slots_[handle.index].alive = false;
    ++generations_[handle.index];
    free_.push_back(handle.index);
    return Status::success();
}

Status PostProcessChain::set_desc(PostProcessHandle handle, const PostProcessDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(handle)) {
        return Status::fail(ErrorCode::NotFound, "post-process handle is not live");
    }
    if (!valid(desc)) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid post-process descriptor");
    }
    slots_[handle.index].desc = desc;
    return Status::success();
}

Result<PostProcessDesc> PostProcessChain::get(PostProcessHandle handle) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(handle)) {
        return Result<PostProcessDesc>::fail(ErrorCode::NotFound,
                                             "post-process handle is not live");
    }
    return Result<PostProcessDesc>::success(slots_[handle.index].desc);
}

std::vector<PostProcessDesc> PostProcessChain::frame_chain() const {
    std::lock_guard<std::mutex> guard(mutex_);
    std::vector<PostProcessDesc> out;
    out.reserve(slots_.size());
    for (const auto& slot : slots_) {
        if (slot.alive) {
            out.push_back(slot.desc);
        }
    }
    return out;
}

std::uint32_t PostProcessChain::count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t n = 0;
    for (const auto& slot : slots_) {
        if (slot.alive) {
            ++n;
        }
    }
    return n;
}

} // namespace poko::postprocess
