#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "poko/core/api_seams.h"

namespace poko {

// PRODUCTION CPU rasterizer behind the render device seam. It really draws:
// vertex fetch from the bound layout, homogeneous near-plane clipping,
// perspective-correct interpolation, depth testing, back/front-face culling,
// lit/textured shading and RGBA8 framebuffer output. It needs no GPU, window
// or driver, so headless servers, CI and tools can produce real pixels.
//
// Shaders are not compiled. A fragment shader source selects a built-in
// program by containing one of these tags (default: `poko:unlit`):
//   poko:unlit         colour = base_color
//   poko:vertex_color  colour = attribute 1 (rgb[a]) * base_color
//   poko:lambert       attribute 1 = object-space normal; diffuse + ambient
//   poko:textured      attribute 1 = uv; colour = texture slot 0 * base_color
//
// Attribute 0 is the position (2-4 floats). All attributes are float32.
// Uniform block (std140-compatible), read from the bound uniform buffer:
//   offset  0: mat4 mvp, row-major (same memory layout as poko::math::Mat4)
//   offset 64: vec4 base_color
//   offset 80: vec4 light (xyz = direction toward the light in object space,
//              w = ambient term)
// A missing or short uniform block falls back to identity / white /
// (0,0,1,0.2). instance_count is accepted; instances are identical because the
// seam carries no per-instance data, so each draw rasterizes once.
//
// Thread-safety contract: synchronised; any thread may call any method.
class SoftwareRenderDevice final : public api::IRenderDevice {
public:
    explicit SoftwareRenderDevice(std::uint32_t width = 640, std::uint32_t height = 360);
    ~SoftwareRenderDevice() override;

    const char* name() const noexcept override;
    GraphicsBackend backend() const noexcept override;
    DeviceCapabilities capabilities() const noexcept override;
    bool available() const noexcept override;

    Result<RenderHandle> create_buffer(const BufferDesc& desc) override;
    Result<RenderHandle> create_texture(const TextureDesc& desc) override;
    Result<RenderHandle> create_shader(const ShaderDesc& desc) override;
    Result<RenderHandle> create_pipeline(const PipelineDesc& desc) override;
    Result<RenderHandle> create_sampler(const SamplerDesc& desc) override;
    Status destroy(RenderHandle handle) override;
    Status update_buffer(RenderHandle handle, std::span<const std::byte> data,
                         std::uint64_t offset_bytes = 0) override;
    Status update_texture(RenderHandle handle, std::uint32_t mip_level,
                          std::span<const std::byte> data) override;

    Status begin_frame() override;
    Status clear(const ClearValue& value) override;
    Status draw(const DrawCall& call) override;
    Status end_frame() override;
    void wait_idle() override;

    bool device_lost() const noexcept override;
    Status recover_from_device_loss() override;

    // Readback, outside the seam. Pixels are packed little-endian as
    // r | g << 8 | b << 16 | a << 24; row 0 is the top of the image.
    std::uint32_t width() const noexcept { return width_; }
    std::uint32_t height() const noexcept { return height_; }
    std::vector<std::uint32_t> pixels() const;
    std::uint32_t pixel(std::uint32_t x, std::uint32_t y) const;
    float depth_at(std::uint32_t x, std::uint32_t y) const;
    Status write_ppm(const std::string& path) const;

    std::uint64_t triangles_drawn() const;
    std::uint64_t pixels_shaded() const;
    std::uint64_t frame_count() const;
    void simulate_device_loss() noexcept;

private:
    struct Slot;

    Result<RenderHandle> add(Slot&& slot);
    const Slot* get(RenderHandle handle) const noexcept;

    std::uint32_t width_;
    std::uint32_t height_;
    mutable std::mutex mutex_;
    std::vector<Slot> slots_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    std::vector<std::uint32_t> color_;
    std::vector<float> depth_;
    bool in_frame_ = false;
    bool lost_ = false;
    std::uint64_t frames_ = 0;
    std::uint64_t triangles_ = 0;
    std::uint64_t shaded_ = 0;
};

} // namespace poko
