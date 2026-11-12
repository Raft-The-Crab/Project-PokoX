#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "poko/core/math.h"
#include "poko/core/result.h"

namespace poko::runtime {

// One drawable resolved from the scene each frame. The mesh/material IDs are
// opaque indices into the client-side graphics libraries; keeping them as
// integers lets the simulation runtime stay free of GPU/backend types so it
// also builds in the headless Server configuration.
struct RenderItem {
    std::uint32_t mesh_id = 0;
    std::uint32_t material_id = 0;
    std::uint32_t instance_count = 1;
    std::uint32_t sort_key = 0;
};

// Immutable per-frame snapshot handed to the render step.
struct FrameContext {
    std::uint32_t frame_index = 0;
    std::uint32_t simulation_steps = 0;
    double simulation_time_seconds = 0.0;
    math::Vec3 camera_position{};
    math::Vec3 camera_target{};
    math::Vec3 camera_up{0.0f, 1.0f, 0.0f};
    std::uint32_t light_count = 0;
    std::uint32_t postprocess_count = 0;
    std::vector<RenderItem> render_items;
};

// Render step invoked once per rendered frame. A null callback makes the
// runtime simulation-only (dedicated/headless use).
using RenderFn = std::function<Status(const FrameContext&)>;

} // namespace poko::runtime
