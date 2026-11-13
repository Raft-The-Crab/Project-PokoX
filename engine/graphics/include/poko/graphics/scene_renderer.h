#pragma once

#include <cstdint>
#include <vector>

#include "poko/core/api_seams.h"
#include "poko/core/math.h"
#include "poko/graphics/mesh.h"

namespace poko::graphics {

// Layout of the per-draw uniform block (std140, row-major mvp). Documented in
// the SoftwareRenderDevice header; GPU backends read the same block.
struct DrawUniforms {
    float mvp[16];
    float color[4];
    float light[4]; // xyz = direction toward the light in object space, w = ambient
};
static_assert(sizeof(DrawUniforms) == 96);

// Immediate-mode lit-mesh renderer on top of the IRenderDevice seam. The caller
// owns frame boundaries (begin_frame / clear / end_frame); SceneRenderer turns
// (mesh, model matrix, colour) into validated draws with a per-draw mvp.
//
// Uniforms live in one ring buffer; each draw writes a 256-byte-aligned slot
// and points DrawCall::uniform_offset at it, so a frame holds at most
// `max_draws` draws.
class SceneRenderer {
public:
    explicit SceneRenderer(api::IRenderDevice& device, std::uint32_t max_draws = 4096) noexcept
        : device_(device), meshes_(device), max_draws_(max_draws) {}

    Status initialize();

    // Uploads a mesh; the returned index is stable for the renderer's lifetime.
    Result<std::uint32_t> add_mesh(const MeshDesc& desc);

    // Starts a frame's draw list. `light_direction` points toward the light in
    // world space; `ambient` is in [0, 1].
    void begin(const math::Mat4& view_projection, const math::Vec3& light_direction,
               float ambient = 0.2f) noexcept;
    Status draw(std::uint32_t mesh, const math::Mat4& model, const math::Vec3& color);

    std::uint32_t draws_issued() const noexcept { return draw_index_; }

private:
    api::IRenderDevice& device_;
    MeshLibrary meshes_;
    std::uint32_t max_draws_;
    bool initialized_ = false;
    RenderHandle pipeline_{};
    RenderHandle uniform_ring_{};
    std::vector<MeshHandle> mesh_handles_;
    math::Mat4 view_projection_ = math::Mat4::identity();
    math::Vec3 light_{0.4f, 0.8f, 0.5f};
    float ambient_ = 0.2f;
    std::uint32_t draw_index_ = 0;
};

} // namespace poko::graphics
