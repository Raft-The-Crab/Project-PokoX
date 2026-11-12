#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

#include "poko_api_seams.h"

namespace poko {

// MOCK — a test-only fake render device (systemplan/00). It performs no GPU
// work, records submitted commands for inspection, and can simulate device
// loss and recovery so the fallback path of systemplan/04 step 10 is testable
// offline. It must never be mistaken for a production backend: the target
// carries the MOCK implementation label via POKO_IMPL_LABEL.
class NullRenderDevice final : public api::IRenderDevice {
public:
    enum class ResourceKind : std::uint8_t {
        Buffer,
        Texture,
        Shader,
        Pipeline,
        Sampler,
    };

    NullRenderDevice();
    explicit NullRenderDevice(DeviceCapabilities capabilities);

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

    // MOCK-only inspection hooks, deliberately outside IRenderDevice.
    void simulate_device_loss() noexcept;
    std::size_t resource_count() const;
    std::size_t frame_count() const;
    std::size_t buffer_update_count() const;
    std::vector<ClearValue> recorded_clears() const;
    std::vector<DrawCall> recorded_draws() const;
    void reset_records();
    bool is_handle_live(RenderHandle handle) const;

private:
    struct Resource {
        ResourceKind kind = ResourceKind::Buffer;
        bool alive = false;
        std::uint64_t size_bytes = 0;
    };

    Result<RenderHandle> create(ResourceKind kind, std::uint64_t size_bytes = 0);
    // Caller must hold mutex_.
    Result<RenderHandle> create_unlocked(ResourceKind kind, std::uint64_t size_bytes = 0);
    Status require_frame() const;                         // caller must hold mutex_
    bool handle_live(RenderHandle handle) const noexcept; // caller must hold mutex_

    mutable std::mutex mutex_;
    DeviceCapabilities capabilities_;
    std::vector<Resource> resources_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    std::vector<ClearValue> clears_;
    std::vector<DrawCall> draws_;
    std::size_t buffer_updates_ = 0;
    std::size_t frame_count_ = 0;
    bool in_frame_ = false;
    std::atomic<bool> device_lost_{false};
};

// Preferred/fallback chain: Vulkan primary, OpenGL fallback, null last
// (systemplan/04, systemplan/29). No backend is vendored yet; these functions
// record the intended order without linking any graphics library.
GraphicsBackend preferred_backend() noexcept;
GraphicsBackend fallback_backend() noexcept;
GraphicsBackend last_resort_backend() noexcept;
DeviceCapabilities null_device_capabilities() noexcept;

} // namespace poko
