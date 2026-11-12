#pragma once

#include <memory>

#include "poko/core/api_seams.h"
#include "poko/renderer/camera.h"
#include "poko/renderer/frame_stats.h"
#include "poko/core/render_contracts.h"
#include "poko/renderer/scene_submission.h"
#include "poko/renderer/shader_cache.h"

namespace poko {

// Top-level renderer facade. Owns the device (injected), so a MOCK device can
// never be confused with a production backend at the call site: the caller
// chooses it explicitly. Business rules stay out of UI (systemplan/01).
//
// Thread-safety contract: one frame thread. The device has its own contract.
class Renderer {
public:
    explicit Renderer(std::unique_ptr<api::IRenderDevice> device);

    api::IRenderDevice& device() noexcept {
        return *device_;
    }
    const api::IRenderDevice& device() const noexcept {
        return *device_;
    }
    ShaderCache& shaders() noexcept {
        return shaders_;
    }
    Camera& camera() noexcept {
        return camera_;
    }
    SceneSubmission& submission() noexcept {
        return submission_;
    }
    FrameStats& stats() noexcept {
        return stats_;
    }

    // Clears, submits the batched draw list, and ends the frame.
    Status render_frame(const ClearValue& clear);

private:
    static std::uint32_t triangle_count(const DrawCall& call) noexcept;

    std::unique_ptr<api::IRenderDevice> device_;
    ShaderCache shaders_;
    Camera camera_;
    SceneSubmission submission_;
    FrameStats stats_;
};

} // namespace poko
