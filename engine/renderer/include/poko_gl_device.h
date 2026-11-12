#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "poko_api_seams.h"

// OpenGL 3.3 core backend for api::IRenderDevice (systemplan/04, DEC-004).
// This target only exists when POKO_RENDER_OPENGL=ON; the default offline
// build ships only the MOCK NullRenderDevice.
//
// Threading/context contract: all methods must be called on the thread that
// owns the current OpenGL context. The device renders to the currently bound
// default framebuffer; window creation, context management and buffer swaps
// belong to the window layer (SDL3 in the windowed demo), not to this class.
// The device must be destroyed before its GL context is destroyed.
namespace poko {

class GlRenderDevice final : public api::IRenderDevice {
public:
    GlRenderDevice();
    ~GlRenderDevice() override;

    GlRenderDevice(const GlRenderDevice&) = delete;
    GlRenderDevice& operator=(const GlRenderDevice&) = delete;

    // Loads GL entry points via the vendored gl3w loader and queries device
    // capabilities. Requires a current, core-profile GL context. Safe to call
    // once; returns false if loading or context queries fail.
    bool initialize() noexcept;

    const char* name() const noexcept override;
    GraphicsBackend backend() const noexcept override;
    DeviceCapabilities capabilities() const noexcept override;
    bool available() const noexcept override;

    Result<RenderHandle> create_buffer(const BufferDesc& desc) override;
    Result<RenderHandle> create_texture(const TextureDesc& desc) override;
    Result<RenderHandle> create_shader(const ShaderDesc& desc) override;
    Result<RenderHandle> create_pipeline(const PipelineDesc& desc) override;
    Status destroy(RenderHandle handle) override;
    Status update_buffer(RenderHandle handle, std::span<const std::byte> data,
                         std::uint64_t offset_bytes = 0) override;

    Status begin_frame() override;
    Status clear(const ClearValue& value) override;
    Status draw(const DrawCall& call) override;
    Status end_frame() override;
    void wait_idle() override;

    // WGL/GLX do not expose a portable context-lost notification; detection is
    // deferred (DEC-004). These return the not-lost state until then.
    bool device_lost() const noexcept override;
    Status recover_from_device_loss() override;

    // Non-seam helper for the window layer: applies the default framebuffer
    // viewport, e.g. after window creation or resize.
    void set_viewport(std::uint32_t width, std::uint32_t height) noexcept;

private:
    enum class Kind : std::uint8_t { Buffer, Texture, Shader, Pipeline };

    struct Resource {
        Kind kind = Kind::Buffer;
        bool alive = false;
        std::uint32_t gl_id = 0; // buffer/texture/shader object or program
        // Buffer bookkeeping.
        std::uint32_t target = 0;
        std::uint32_t usage = 0;
        std::uint64_t size_bytes = 0;
        // Pipeline bookkeeping (fixed-function state snapshot).
        VertexLayout layout{};
        CullMode cull_mode = CullMode::Back;
        CompareOp depth_compare = CompareOp::LessOrEqual;
        bool depth_test = true;
        bool depth_write = true;
        PrimitiveTopology topology = PrimitiveTopology::Triangles;
        // Number of std140 uniform blocks in the linked program; block i is
        // bound to uniform-buffer binding point i at pipeline creation.
        std::uint32_t uniform_block_count = 0;
    };

    bool handle_live(RenderHandle handle) const noexcept; // caller holds mutex_
    Status require_frame() const;                         // caller holds mutex_
    Status gl_error_status(const char* what) const;
    Resource* find(HandleIndex index) noexcept;
    const Resource* find(HandleIndex index) const noexcept;

    mutable std::mutex mutex_;
    bool initialized_ = false;
    bool in_frame_ = false;
    std::uint32_t vao_ = 0;
    std::vector<Resource> resources_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    DeviceCapabilities capabilities_{};
    std::string renderer_name_;
};

} // namespace poko
