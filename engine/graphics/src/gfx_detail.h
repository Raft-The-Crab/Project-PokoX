#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "poko/core/api_seams.h"
#include "poko/core/render_contracts.h"

namespace poko::graphics::detail {

// View of a byte blob for IRenderDevice::update_buffer.
inline std::span<const std::byte> byte_span(const std::vector<std::byte>& bytes) noexcept {
    return {bytes.data(), bytes.size()};
}

// Releases a set of owned device handles in reverse creation order. Used both
// for rollback on a partially-failed create and for normal destruction.
inline void destroy_all(api::IRenderDevice& device,
                        const std::vector<RenderHandle>& handles) noexcept {
    for (auto it = handles.rbegin(); it != handles.rend(); ++it) {
        (void)device.destroy(*it);
    }
}

} // namespace poko::graphics::detail
