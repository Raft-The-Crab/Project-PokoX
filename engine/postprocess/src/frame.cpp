#include "poko/postprocess/chain.h"

namespace poko::postprocess {

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
