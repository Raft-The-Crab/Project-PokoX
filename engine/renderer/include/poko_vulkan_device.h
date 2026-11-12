#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "poko_api_seams.h"

// Vulkan backend for api::IRenderDevice (systemplan/04 primary backend,
// DEC-005). Built only when POKO_RENDER_VULKAN=ON (requires
// POKO_FETCH_DEPENDENCIES=ON and an SDL3 surface provider).
//
// Threading contract: all methods on the thread that owns the Vulkan device.
// The device does not own the window: the window layer creates the SDL3
// window, calls SDL_Vulkan_CreateSurface, and hands the VkSurfaceKHR here.
// The window layer also owns swapchain presentation cadence via end_frame().
namespace poko {

class VulkanRenderDevice final : public api::IRenderDevice {
public:
    // surface must be a valid VkSurfaceKHR created for the target window.
    // The device does not take ownership of the surface.
    VulkanRenderDevice(void* instance, void* physical_device, void* surface);
    ~VulkanRenderDevice() override;

    VulkanRenderDevice(const VulkanRenderDevice&) = delete;
    VulkanRenderDevice& operator=(const VulkanRenderDevice&) = delete;

    bool initialize() noexcept;

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

    // Recreates the swapchain (e.g. after window resize).
    Status recreate_swapchain(std::uint32_t width, std::uint32_t height);

private:
    enum class Kind : std::uint8_t { Buffer, Texture, Shader, Pipeline, Sampler };

    struct Resource {
        Kind kind = Kind::Buffer;
        bool alive = false;
        std::uint64_t aux = 0; // buffer size / pipeline program bits
        // Vulkan object handles (cast from void* to keep headers out).
        void* object = nullptr;           // VkBuffer / VkImage / VkShaderModule / VkPipeline
        void* object2 = nullptr;          // VkDeviceMemory (buffer/texture)
        void* object3 = nullptr;          // VkImageView (texture)
        std::vector<std::uint32_t> spirv; // shader stage only
        VertexLayout layout{};            // pipeline only
        CullMode cull_mode = CullMode::Back;
        CompareOp depth_compare = CompareOp::LessOrEqual;
        bool depth_test = true;
        bool depth_write = true;
        PrimitiveTopology topology = PrimitiveTopology::Triangles;
        std::uint32_t format = 0; // texture pixel format (VkFormat)
        std::uint32_t width = 0;  // texture only
        std::uint32_t height = 0; // texture only
        std::uint32_t mip_levels = 1;
    };

    struct DepthImage {
        void* image = nullptr;  // VkImage
        void* memory = nullptr; // VkDeviceMemory
    };

    // Per-frame-in-flight submission state. Semaphores and fences belong to
    // the frame slot, not the swapchain image, because vkAcquireNextImageKHR
    // can return any image — the acquire semaphore must be chosen before the
    // image index is known.
    struct FrameSlot {
        void* command_buffer = nullptr;  // VkCommandBuffer
        void* fence = nullptr;           // VkFence (submit completion)
        void* acquire_sem = nullptr;     // VkSemaphore (image available)
        void* render_sem = nullptr;      // VkSemaphore (render finished)
        void* descriptor_pool = nullptr; // VkDescriptorPool
        bool fence_submitted = false;
    };

    struct SwapImage {
        void* image = nullptr;       // VkImage
        void* framebuffer = nullptr; // VkFramebuffer
    };

    bool handle_live(RenderHandle handle) const noexcept;
    Status require_frame() const;
    Status fail_vk(const char* what, std::int32_t result) const;
    Resource* find(HandleIndex index) noexcept;
    const Resource* find(HandleIndex index) const noexcept;
    HandleIndex alloc_slot(Kind kind);
    void destroy_resource_objects(Resource& resource);

    Status create_instance_resources();
    Status create_device_resources();
    Status create_swapchain_resources(std::uint32_t width, std::uint32_t height);
    Status create_render_pass_resources();
    Status acquire_image();
    Status record_draws_into_pass();

    void* instance_ = nullptr;              // VkInstance
    void* physical_device_ = nullptr;       // VkPhysicalDevice (borrowed)
    void* device_ = nullptr;                // VkDevice
    void* queue_ = nullptr;                 // VkQueue
    void* surface_ = nullptr;               // VkSurfaceKHR (borrowed)
    void* command_pool_ = nullptr;          // VkCommandPool
    void* render_pass_ = nullptr;           // VkRenderPass
    void* swapchain_ = nullptr;             // VkSwapchainKHR
    void* pipeline_layout_ = nullptr;       // VkPipelineLayout
    void* descriptor_set_layout_ = nullptr; // VkDescriptorSetLayout (set 0: UBO binding 0)
    std::uint32_t swapchain_format_ = 0;
    std::uint32_t swapchain_extent_w_ = 0;
    std::uint32_t swapchain_extent_h_ = 0;
    std::vector<SwapImage> swap_images_;
    std::vector<FrameSlot> frame_slots_;
    std::vector<DepthImage> depth_images_;
    std::vector<void*> color_views_; // VkImageView
    std::vector<void*> depth_views_; // VkImageView
    std::uint32_t image_index_ = ~0u;
    std::uint32_t frame_slot_ = 0;
    bool pass_open_ = false;
    ClearValue pending_clear_{};
    bool initialized_ = false;
    bool in_frame_ = false;
    bool lost_ = false;
    std::vector<Resource> resources_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    DeviceCapabilities capabilities_{};
    std::string device_name_;
    mutable std::mutex mutex_;
};

} // namespace poko
