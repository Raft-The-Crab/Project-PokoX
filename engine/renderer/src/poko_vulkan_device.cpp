#include "poko_vulkan_device.h"

#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>

namespace poko {
namespace {

// The Vulkan loader exports the full entry-point surface, so vk* functions
// are called directly after linking libvulkan.

EShLanguage shader_language(ShaderStage stage) noexcept {
    switch (stage) {
        case ShaderStage::Vertex:
            return EShLangVertex;
        case ShaderStage::Fragment:
            return EShLangFragment;
        case ShaderStage::Compute:
            return EShLangCompute;
    }
    return EShLangVertex;
}

VkFormat vk_format(PixelFormat format) noexcept {
    switch (format) {
        case PixelFormat::RGBA8:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::BGRA8:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case PixelFormat::RGB8:
            return VK_FORMAT_R8G8B8_UNORM;
        case PixelFormat::R8:
            return VK_FORMAT_R8_UNORM;
        case PixelFormat::RGBA16F:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case PixelFormat::RGBA32F:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case PixelFormat::Depth24Stencil8:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case PixelFormat::Depth32F:
            return VK_FORMAT_D32_SFLOAT;
        case PixelFormat::Unknown:
            break;
    }
    return VK_FORMAT_UNDEFINED;
}

VkBufferUsageFlags buffer_usage(BufferUsage usage) noexcept {
    switch (usage) {
        case BufferUsage::Vertex:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case BufferUsage::Index:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case BufferUsage::Uniform:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        case BufferUsage::Storage:
            return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        case BufferUsage::Staging:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    return 0;
}

VkPrimitiveTopology vk_topology(PrimitiveTopology topology) noexcept {
    switch (topology) {
        case PrimitiveTopology::Triangles:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::Lines:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::Points:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkCullModeFlags vk_cull(CullMode mode) noexcept {
    switch (mode) {
        case CullMode::None:
            return VK_CULL_MODE_NONE;
        case CullMode::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:
            return VK_CULL_MODE_BACK_BIT;
    }
    return VK_CULL_MODE_BACK_BIT;
}

VkCompareOp vk_compare(CompareOp op) noexcept {
    switch (op) {
        case CompareOp::Never:
            return VK_COMPARE_OP_NEVER;
        case CompareOp::Less:
            return VK_COMPARE_OP_LESS;
        case CompareOp::Equal:
            return VK_COMPARE_OP_EQUAL;
        case CompareOp::LessOrEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater:
            return VK_COMPARE_OP_GREATER;
        case CompareOp::NotEqual:
            return VK_COMPARE_OP_NOT_EQUAL;
        case CompareOp::GreaterOrEqual:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Always:
            return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_LESS_OR_EQUAL;
}

VkFormat vk_attrib_format(std::uint32_t components) noexcept {
    // Attributes are 32-bit float tuples (same assumption as the GL backend).
    switch (components) {
        case 1:
            return VK_FORMAT_R32_SFLOAT;
        case 2:
            return VK_FORMAT_R32G32_SFLOAT;
        case 3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case 4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

// Compiles GLSL source to SPIR-V via glslang. Returns false on failure with
// the log appended to `log_out`.
bool compile_spirv(ShaderStage stage, const std::string& source,
                   std::vector<std::uint32_t>& out_spirv,
                   std::string& log_out) {
    glslang::TShader shader(static_cast_EShLanguage>(shader_language(stage)));
    const char* text = source.c_str();
    shader.setStrings(&text, 1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    shader.setEnvInput(glslang::EShSourceGlsl, shader_language(stage),
                       glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShClientNone);
    if (!shader.parse(GetDefaultResources(), 460, false, log_out)) {
        return false;
    }
    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(log_out)) {
        return false;
    }
    program.mapIO();
    glslang::GlslangToSpv(*program.getIntermediate(shader_language(stage)),
                          out_spirv);
    return !out_spirv.empty();
}

}  // namespace

VulkanRenderDevice::VulkanRenderDevice(void* instance, void* physical_device,
                                       void* surface)
    : instance_(instance), physical_device_(physical_device),
      surface_(surface) {}

VulkanRenderDevice::~VulkanRenderDevice() {
    if (!initialized_ || device_ == nullptr) {
        return;
    }
    auto* dev = static_cast<VkDevice>(device_);
    vkDeviceWaitIdle(dev);
    for (auto& slot : swap_images_) {
        auto* cb = static_cast<VkCommandBuffer>(slot.command_buffer);
        auto* fb = static_cast<VkFramebuffer>(slot.framebuffer);
        auto* fence = static_cast<VkFence>(slot.fence);
        auto* acq = static_cast<VkSemaphore>(slot.acquire_sem);
        auto* rnd = static_cast<VkSemaphore>(slot.render_sem);
        if (fb != nullptr) {
            vkDestroyFramebuffer(dev, fb, nullptr);
        }
        if (cb != nullptr) {
            vkFreeCommandBuffers(dev, static_cast<VkCommandPool>(command_pool_),
                                 1, &cb);
        }
        if (fence != nullptr) {
            vkDestroyFence(dev, fence, nullptr);
        }
        if (acq != nullptr) {
            vkDestroySemaphore(dev, acq, nullptr);
        }
        if (rnd != nullptr) {
            vkDestroySemaphore(dev, rnd, nullptr);
        }
    }
    for (auto& resource : resources_) {
        destroy_resource_objects(resource);
    }
    if (swapchain_ != nullptr) {
        vkDestroySwapchainKHR(dev, static_cast<VkSwapchainKHR>(swapchain_),
                              nullptr);
    }
    if (render_pass_ != nullptr) {
        vkDestroyRenderPass(dev, static_cast<VkRenderPass>(render_pass_),
                            nullptr);
    }
    if (pipeline_layout_ != nullptr) {
        vkDestroyPipelineLayout(dev,
                                static_cast<VkPipelineLayout>(pipeline_layout_),
                                nullptr);
    }
    if (command_pool_ != nullptr) {
        vkDestroyCommandPool(dev, static_cast<VkCommandPool>(command_pool_),
                             nullptr);
    }
    vkDestroyDevice(dev, nullptr);
    glslang::FinalizeProcess();
}

void VulkanRenderDevice::destroy_resource_objects(Resource& resource) {
    auto* dev = static_cast<VkDevice>(device_);
    switch (resource.kind) {
        case Kind::Buffer:
            if (resource.object != nullptr) {
                vkDestroyBuffer(dev, static_cast<VkBuffer>(resource.object),
                                nullptr);
            }
            if (resource.object2 != nullptr) {
                vkFreeMemory(dev,
                             static_cast<VkDeviceMemory>(resource.object2),
                             nullptr);
            }
            break;
        case Kind::Texture:
            if (resource.object3 != nullptr) {
                vkDestroyImageView(dev,
                                   static_cast<VkImageView>(resource.object3),
                                   nullptr);
            }
            if (resource.object != nullptr) {
                vkDestroyImage(dev, static_cast<VkImage>(resource.object),
                               nullptr);
            }
            if (resource.object2 != nullptr) {
                vkFreeMemory(dev,
                             static_cast<VkDeviceMemory>(resource.object2),
                             nullptr);
            }
            break;
        case Kind::Shader:
            if (resource.object != nullptr) {
                vkDestroyShaderModule(dev,
                                      static_cast<VkShaderModule>(resource.object),
                                      nullptr);
            }
            break;
        case Kind::Pipeline:
            if (resource.object != nullptr) {
                vkDestroyPipeline(dev, static_cast<VkPipeline>(resource.object),
                                  nullptr);
            }
            break;
    }
    resource.object = resource.object2 = resource.object3 = nullptr;
}

bool VulkanRenderDevice::initialize() noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    if (initialized_) {
        return true;
    }
    if (instance_ == nullptr || physical_device_ == nullptr ||
        surface_ == nullptr) {
        return false;
    }
    glslang::InitializeProcess();

    // Queue family with graphics support.
    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        static_cast<VkPhysicalDevice>(physical_device_), &family_count,
        nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        static_cast<VkPhysicalDevice>(physical_device_), &family_count,
        families.data());
    std::uint32_t queue_family = UINT32_MAX;
    for (std::uint32_t i = 0; i < family_count; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            queue_family = i;
            break;
        }
    }
    if (queue_family == UINT32_MAX) {
        glslang::FinalizeProcess();
        return false;
    }

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = device_extensions;

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(static_cast<VkPhysicalDevice>(physical_device_),
                       &device_info, nullptr, &device) != VK_SUCCESS) {
        glslang::FinalizeProcess();
        return false;
    }
    device_ = device;
    vkGetDeviceQueue(device, queue_family, 0, &queue_);

    VkCommandPoolCreateInfo pool_info{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queue_family;
    if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool_) !=
        VK_SUCCESS) {
        return false;
    }

    // Pipeline layout: no descriptors, no push constants (uniforms arrive
    // with the camera seam extension, DEC-005).
    VkPipelineLayoutCreateInfo layout_info{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    if (vkCreatePipelineLayout(device, &layout_info, nullptr,
                               &pipeline_layout_) != VK_SUCCESS) {
        return false;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(static_cast<VkPhysicalDevice>(physical_device_),
                                  &props);
    device_name_ = props.deviceName;
    capabilities_.backend = GraphicsBackend::Vulkan;
    capabilities_.device_name = device_name_.c_str();
    capabilities_.max_texture_size = props.limits.maxImageDimension2D;
    capabilities_.max_buffer_bytes =
        props.limits.maxStorageBufferRange > 0
            ? static_cast<std::uint64_t>(props.limits.maxStorageBufferRange)
            : (std::uint64_t{1} << 30);
    capabilities_.supports_compute =
        (props.limits.maxComputeWorkGroupCount[0] > 0);
    capabilities_.supports_wireframe = true;
    capabilities_.supports_shader_reload = true;

    // Default framebuffer extent: swapchain creation fills these in.
    initialized_ = true;
    return true;
}

Status VulkanRenderDevice::create_swapchain_resources(std::uint32_t width,
                                                      std::uint32_t height) {
    auto* dev = static_cast<VkDevice>(device_);
    auto* surf = static_cast<VkSurfaceKHR>(surface_);
    auto* phys = static_cast<VkPhysicalDevice>(physical_device_);

    VkSurfaceCapabilitiesKHR caps{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surf, &caps) !=
        VK_SUCCESS) {
        return Status::fail(ErrorCode::Internal,
                            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");
    }
    if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "surface has zero extent");
    }
    swapchain_extent_w_ = width != 0 ? width : caps.currentExtent.width;
    swapchain_extent_h_ = height != 0 ? height : caps.currentExtent.height;
    if (swapchain_extent_w_ > caps.maxImageExtent.width) {
        swapchain_extent_w_ = caps.maxImageExtent.width;
    }
    if (swapchain_extent_h_ > caps.maxImageExtent.height) {
        swapchain_extent_h_ = caps.maxImageExtent.height;
    }

    std::uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &format_count,
                                         formats.data());
    VkSurfaceFormatKHR chosen = formats.empty()
                                    ? VkSurfaceFormatKHR{
                                          VK_FORMAT_B8G8R8A8_UNORM,
                                          VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
                                    : formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM ||
            f.format == VK_FORMAT_R8G8B8A8_UNORM) {
            chosen = f;
            break;
        }
    }
    swapchain_format_ = static_cast<std::uint32_t>(chosen.format);

    std::uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci{
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = surf;
    sci.minImageCount = image_count;
    sci.imageFormat = chosen.format;
    sci.imageColorSpace = chosen.colorSpace;
    sci.imageExtent = {swapchain_extent_w_, swapchain_extent_h_};
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(dev, &sci, nullptr, &swapchain) != VK_SUCCESS) {
        return Status::fail(ErrorCode::Internal, "vkCreateSwapchainKHR failed");
    }
    swapchain_ = swapchain;

    std::uint32_t actual_count = 0;
    vkGetSwapchainImagesKHR(dev, swapchain, &actual_count, nullptr);
    std::vector<VkImage> images(actual_count);
    vkGetSwapchainImagesKHR(dev, swapchain, &actual_count, images.data());

    // Render pass: color (clear->present) + depth attachment.
    VkAttachmentDescription attachments[2]{};
    attachments[0].format = chosen.format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[1].format = VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_ref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rp_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp_info.attachmentCount = 2;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies = &dep;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    if (vkCreateRenderPass(dev, &rp_info, nullptr, &render_pass) !=
        VK_SUCCESS) {
        return Status::fail(ErrorCode::Internal, "vkCreateRenderPass failed");
    }
    render_pass_ = render_pass;

    // Depth image shared layout: one per swap image.
    swap_images_.clear();
    swap_images_.resize(actual_count);
    for (std::uint32_t i = 0; i < actual_count; ++i) {
        SwapImage& slot = swap_images_[i];
        slot.image = images[i];

        VkImage depth_image = VK_NULL_HANDLE;
        VkImageCreateInfo depth_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        depth_info.imageType = VK_IMAGE_TYPE_2D;
        depth_info.format = VK_FORMAT_D32_SFLOAT;
        depth_info.extent = {swapchain_extent_w_, swapchain_extent_h_, 1};
        depth_info.mipLevels = 1;
        depth_info.arrayLayers = 1;
        depth_info.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        depth_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depth_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depth_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(dev, &depth_info, nullptr, &depth_image) !=
            VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted,
                                "vkCreateImage(depth) failed");
        }
        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(dev, depth_image, &req);
        VkPhysicalDeviceMemoryProperties mem{};
        vkGetPhysicalDeviceMemoryProperties(phys, &mem);
        std::uint32_t mem_type = UINT32_MAX;
        for (std::uint32_t t = 0; t < mem.memoryTypeCount; ++t) {
            if ((req.memoryTypeBits & (1u << t)) != 0 &&
                (mem.memoryTypes[t].propertyFlags &
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
                mem_type = t;
                break;
            }
        }
        if (mem_type == UINT32_MAX) {
            mem_type = 0;
        }
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = mem_type;
        VkDeviceMemory depth_mem = VK_NULL_HANDLE;
        if (vkAllocateMemory(dev, &alloc, nullptr, &depth_mem) != VK_SUCCESS ||
            vkBindImageMemory(dev, depth_image, depth_mem, 0) != VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted,
                                "depth memory allocation failed");
        }
        // Store depth image+memory inside the framebuffer attachment slot is
        // not possible; keep them alive via a side vector on the device.
        // (Tracked in depth_objects_; see below.)
        depth_images_.push_back({depth_image, depth_mem});
        VkImageViewCreateInfo view_info{
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = depth_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_D32_SFLOAT;
        view_info.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        VkImageView depth_view = VK_NULL_HANDLE;
        if (vkCreateImageView(dev, &view_info, nullptr, &depth_view) !=
            VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal,
                                "vkCreateImageView(depth) failed");
        }
        depth_views_.push_back(depth_view);

        VkImageView color_view = VK_NULL_HANDLE;
        VkImageViewCreateInfo color_view_info{
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        color_view_info.image = images[i];
        color_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        color_view_info.format = chosen.format;
        color_view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0,
                                            1};
        if (vkCreateImageView(dev, &color_view_info, nullptr, &color_view) !=
            VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal,
                                "vkCreateImageView(color) failed");
        }
        color_views_.push_back(color_view);

        VkFramebufferCreateInfo fb_info{
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        VkImageView fb_views[2] = {color_view, depth_view};
        fb_info.renderPass = render_pass;
        fb_info.attachmentCount = 2;
        fb_info.pAttachments = fb_views;
        fb_info.width = swapchain_extent_w_;
        fb_info.height = swapchain_extent_h_;
        fb_info.layers = 1;
        VkFramebuffer fb = VK_NULL_HANDLE;
        if (vkCreateFramebuffer(dev, &fb_info, nullptr, &fb) != VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal,
                                "vkCreateFramebuffer failed");
        }
        slot.framebuffer = fb;

        VkCommandBufferAllocateInfo cb_info{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cb_info.commandPool = static_cast<VkCommandPool>(command_pool_);
        cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cb_info.commandBufferCount = 1;
        VkCommandBuffer cb = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(dev, &cb_info, &cb) != VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal,
                                "vkAllocateCommandBuffers failed");
        }
        slot.command_buffer = cb;

        VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence fence = VK_NULL_HANDLE;
        if (vkCreateFence(dev, &fence_info, nullptr, &fence) != VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted,
                                "vkCreateFence failed");
        }
        slot.fence = fence;

        VkSemaphoreCreateInfo sem_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore acq = VK_NULL_HANDLE;
        VkSemaphore rnd = VK_NULL_HANDLE;
        if (vkCreateSemaphore(dev, &sem_info, nullptr, &acq) != VK_SUCCESS ||
            vkCreateSemaphore(dev, &sem_info, nullptr, &rnd) != VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted,
                                "vkCreateSemaphore failed");
        }
        slot.acquire_sem = acq;
        slot.render_sem = rnd;
    }
    return Status::success();
}

const char* VulkanRenderDevice::name() const noexcept { return "vulkan"; }

GraphicsBackend VulkanRenderDevice::backend() const noexcept {
    return GraphicsBackend::Vulkan;
}

DeviceCapabilities VulkanRenderDevice::capabilities() const noexcept {
    return capabilities_;
}

bool VulkanRenderDevice::available() const noexcept {
    return initialized_ && !lost_;
}

bool VulkanRenderDevice::handle_live(RenderHandle handle) const noexcept {
    return is_valid(handle) &&
           static_cast<std::size_t>(handle.index) < resources_.size() &&
           generations_[handle.index] == handle.generation &&
           resources_[handle.index].alive;
}

VulkanRenderDevice::Resource* VulkanRenderDevice::find(HandleIndex index)
    noexcept {
    if (static_cast<std::size_t>(index) >= resources_.size()) {
        return nullptr;
    }
    return &resources_[index];
}

const VulkanRenderDevice::Resource* VulkanRenderDevice::find(
    HandleIndex index) const noexcept {
    if (static_cast<std::size_t>(index) >= resources_.size()) {
        return nullptr;
    }
    return &resources_[index];
}

HandleIndex VulkanRenderDevice::alloc_slot(Kind kind) {
    if (!free_.empty()) {
        const HandleIndex index = free_.back();
        free_.pop_back();
        resources_[index] = Resource{};
        resources_[index].kind = kind;
        resources_[index].alive = true;
        return index;
    }
    const HandleIndex index = static_cast<HandleIndex>(resources_.size());
    resources_.push_back(Resource{});
    resources_.back().kind = kind;
    resources_.back().alive = true;
    generations_.push_back(0);
    return index;
}

Status VulkanRenderDevice::require_frame() const {
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "device is not initialized");
    }
    if (!in_frame_) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "no frame in progress");
    }
    return Status::success();
}

Status VulkanRenderDevice::fail_vk(const char* what,
                                   std::int32_t result) const {
    return Status::fail(ErrorCode::Internal,
                        std::string(what) + " failed (VkResult " +
                            std::to_string(result) + ")");
}

Result<RenderHandle> VulkanRenderDevice::create_buffer(const BufferDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc) || buffer_usage(desc.usage) == 0) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument,
                                          "invalid buffer descriptor");
    }
    auto* dev = static_cast<VkDevice>(device_);
    auto* phys = static_cast<VkPhysicalDevice>(physical_device_);
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = desc.size_bytes;
    info.usage = buffer_usage(desc.usage);
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(dev, &info, nullptr, &buffer) != VK_SUCCESS) {
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted,
                                          "vkCreateBuffer failed");
    }
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(dev, buffer, &req);
    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mem);
    std::uint32_t mem_type = UINT32_MAX;
    for (std::uint32_t t = 0; t < mem.memoryTypeCount; ++t) {
        if ((req.memoryTypeBits & (1u << t)) != 0 &&
            (mem.memoryTypes[t].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            mem_type = t;
            break;
        }
    }
    if (mem_type == UINT32_MAX) {
        vkDestroyBuffer(dev, buffer, nullptr);
        return Result<RenderHandle>::fail(
            ErrorCode::ResourceExhausted,
            "no host-visible coherent memory for buffer");
    }
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = mem_type;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(dev, &alloc, nullptr, &memory) != VK_SUCCESS ||
        vkBindBufferMemory(dev, buffer, memory, 0) != VK_SUCCESS) {
        vkDestroyBuffer(dev, buffer, nullptr);
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted,
                                          "buffer memory allocation failed");
    }
    const HandleIndex index = alloc_slot(Kind::Buffer);
    auto& resource = resources_[index];
    resource.object = buffer;
    resource.object2 = memory;
    resource.aux = desc.size_bytes;
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Result<RenderHandle> VulkanRenderDevice::create_texture(
    const TextureDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc) || desc.depth != 1 || desc.array_layers != 1) {
        return Result<RenderHandle>::fail(
            ErrorCode::InvalidArgument,
            "only 2D non-array textures are supported");
    }
    const VkFormat format = vk_format(desc.format);
    if (format == VK_FORMAT_UNDEFINED) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument,
                                          "unsupported pixel format");
    }
    auto* dev = static_cast<VkDevice>(device_);
    auto* phys = static_cast<VkPhysicalDevice>(physical_device_);
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent = {desc.width, desc.height, 1};
    info.mipLevels = desc.mip_levels;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(dev, &info, nullptr, &image) != VK_SUCCESS) {
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted,
                                          "vkCreateImage failed");
    }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(dev, image, &req);
    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mem);
    std::uint32_t mem_type = UINT32_MAX;
    for (std::uint32_t t = 0; t < mem.memoryTypeCount; ++t) {
        if ((req.memoryTypeBits & (1u << t)) != 0 &&
            (mem.memoryTypes[t].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
            mem_type = t;
            break;
        }
    }
    if (mem_type == UINT32_MAX) {
        mem_type = 0;
    }
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = mem_type;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(dev, &alloc, nullptr, &memory) != VK_SUCCESS ||
        vkBindImageMemory(dev, image, memory, 0) != VK_SUCCESS) {
        vkDestroyImage(dev, image, nullptr);
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted,
                                          "texture memory allocation failed");
    }
    VkImageViewCreateInfo view_info{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0,
                                  desc.mip_levels, 0, 1};
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(dev, &view_info, nullptr, &view) != VK_SUCCESS) {
        vkDestroyImage(dev, image, nullptr);
        vkFreeMemory(dev, memory, nullptr);
        return Result<RenderHandle>::fail(ErrorCode::Internal,
                                          "vkCreateImageView failed");
    }
    const HandleIndex index = alloc_slot(Kind::Texture);
    auto& resource = resources_[index];
    resource.object = image;
    resource.object2 = memory;
    resource.object3 = view;
    resource.format = static_cast<std::uint32_t>(format);
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Result<RenderHandle> VulkanRenderDevice::create_shader(const ShaderDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument,
                                          "invalid shader descriptor");
    }
    std::string source(desc.source);
    std::vector<std::uint32_t> spirv;
    std::string log;
    if (!compile_spirv(desc.stage, source, spirv, log)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument,
                                          "glslang compile failed: " + log);
    }
    auto* dev = static_cast<VkDevice>(device_);
    VkShaderModuleCreateInfo info{
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = spirv.size() * sizeof(std::uint32_t);
    info.pCode = spirv.data();
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(dev, &info, nullptr, &module) != VK_SUCCESS) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument,
                                          "vkCreateShaderModule failed");
    }
    const HandleIndex index = alloc_slot(Kind::Shader);
    auto& resource = resources_[index];
    resource.object = module;
    resource.spirv = std::move(spirv);
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Result<RenderHandle> VulkanRenderDevice::create_pipeline(
    const PipelineDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc.layout)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument,
                                          "invalid vertex layout");
    }
    const Resource* vs = handle_live(desc.vertex_shader)
                             ? find(desc.vertex_shader.index)
                             : nullptr;
    const Resource* fs = handle_live(desc.fragment_shader)
                             ? find(desc.fragment_shader.index)
                             : nullptr;
    if (vs == nullptr || fs == nullptr || vs->kind != Kind::Shader ||
        fs->kind != Kind::Shader || vs->spirv.empty() || fs->spirv.empty()) {
        return Result<RenderHandle>::fail(ErrorCode::NotFound,
                                          "shader handles are not live");
    }
    auto* dev = static_cast<VkDevice>(device_);

    VkShaderModuleCreateInfo mod_info{
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkPipelineShaderStageCreateInfo stages[2]{};
    VkShaderModule modules[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    for (int i = 0; i < 2; ++i) {
        const Resource* shader = i == 0 ? vs : fs;
        mod_info.codeSize = shader->spirv.size() * sizeof(std::uint32_t);
        mod_info.pCode = shader->spirv.data();
        if (vkCreateShaderModule(dev, &mod_info, nullptr, &modules[i]) !=
            VK_SUCCESS) {
            return Result<RenderHandle>::fail(ErrorCode::Internal,
                                              "vkCreateShaderModule failed");
        }
        stages[i].sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].stage = i == 0 ? VK_SHADER_STAGE_VERTEX_BIT
                                 : VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[i].module = modules[i];
        stages[i].pName = "main";
    }

    // Vertex input: float attributes, offsets in bytes.
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = desc.layout.attributes[0].stride_bytes != 0
                         ? desc.layout.attributes[0].stride_bytes
                         : desc.layout.attributes[0].components * 4;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attribs[kMaxVertexAttributes]{};
    for (std::uint32_t i = 0; i < desc.layout.attribute_count; ++i) {
        const auto& a = desc.layout.attributes[i];
        attribs[i].location = a.location;
        attribs[i].binding = 0;
        attribs[i].format = vk_attrib_format(a.components);
        attribs[i].offset = a.offset_bytes;
    }
    VkPipelineVertexInputStateCreateInfo vertex_input{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input.vertexBindingDescriptionCount =
        desc.layout.attribute_count > 0 ? 1 : 0;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = desc.layout.attribute_count;
    vertex_input.pVertexAttributeDescriptions = attribs;

    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = vk_topology(desc.layout.topology);
    ia.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = vk_cull(desc.cull_mode);
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.depthTestEnable = desc.depth_test ? VK_TRUE : VK_FALSE;
    depth.depthWriteEnable = desc.depth_write ? VK_TRUE : VK_FALSE;
    depth.depthCompareOp = vk_compare(desc.depth_compare);
    depth.depthBoundsTestEnable = VK_FALSE;
    depth.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend_state{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend_state.attachmentCount = 1;
    blend_state.pAttachments = &blend;

    const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                             VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo gp{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vertex_input;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &viewport_state;
    gp.pRasterizationState = &raster;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = &depth;
    gp.pColorBlendState = &blend_state;
    gp.pDynamicState = &dynamic_state;
    gp.layout = static_cast<VkPipelineLayout>(pipeline_layout_);
    gp.renderPass = static_cast<VkRenderPass>(render_pass_);
    gp.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult result =
        vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gp, nullptr,
                                  &pipeline);
    vkDestroyShaderModule(dev, modules[0], nullptr);
    vkDestroyShaderModule(dev, modules[1], nullptr);
    if (result != VK_SUCCESS) {
        return Status::fail(ErrorCode::Internal,
                            "vkCreateGraphicsPipelines failed");
    }
    const HandleIndex index = alloc_slot(Kind::Pipeline);
    auto& resource = resources_[index];
    resource.object = pipeline;
    resource.layout = desc.layout;
    resource.topology = desc.layout.topology;
    resource.cull_mode = desc.cull_mode;
    resource.depth_compare = desc.depth_compare;
    resource.depth_test = desc.depth_test;
    resource.depth_write = desc.depth_write;
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Status VulkanRenderDevice::destroy(RenderHandle handle) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "device is not initialized");
    }
    if (!handle_live(handle)) {
        return Status::fail(ErrorCode::NotFound, "handle is not live");
    }
    auto& resource = resources_[handle.index];
    destroy_resource_objects(resource);
    resource.alive = false;
    ++generations_[handle.index];
    free_.push_back(handle.index);
    return Status::success();
}

Status VulkanRenderDevice::update_buffer(RenderHandle handle,
                                         std::span<const std::byte> data,
                                         std::uint64_t offset_bytes) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "device is not initialized");
    }
    if (!handle_live(handle)) {
        return Status::fail(ErrorCode::NotFound, "buffer handle is not live");
    }
    auto* resource = find(handle.index);
    if (resource->kind != Kind::Buffer) {
        return Status::fail(ErrorCode::InvalidArgument,
                            "handle is not a buffer");
    }
    if (data.empty() || offset_bytes > resource->aux ||
        data.size() > resource->aux - offset_bytes) {
        return Status::fail(ErrorCode::OutOfRange,
                            "buffer update is out of range");
    }
    auto* dev = static_cast<VkDevice>(device_);
    void* mapped = nullptr;
    if (vkMapMemory(dev,
                    static_cast<VkDeviceMemory>(resource->object2),
                    static_cast<VkDeviceSize>(offset_bytes),
                    static_cast<VkDeviceSize>(data.size()), 0, &mapped) !=
        VK_SUCCESS) {
        return Status::fail(ErrorCode::Internal, "vkMapMemory failed");
    }
    std::memcpy(mapped, data.data(), data.size());
    vkUnmapMemory(dev, static_cast<VkDeviceMemory>(resource->object2));
    return Status::success();
}

Status VulkanRenderDevice::acquire_image(SwapImage*& out) {
    auto* dev = static_cast<VkDevice>(device_);
    auto* slot = &swap_images_[image_index_];
    const VkResult result = vkAcquireNextImageKHR(
        dev, static_cast<VkSwapchainKHR>(swapchain_),
        std::numeric_limits<std::uint64_t>::max(),
        static_cast<VkSemaphore>(slot->acquire_sem), VK_NULL_HANDLE,
        &image_index_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "swapchain out of date");
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return fail_vk("vkAcquireNextImageKHR", static_cast<std::int32_t>(result));
    }
    slot = &swap_images_[image_index_];
    // Wait for the previous submission using this image to finish.
    vkWaitForFences(dev, 1, reinterpret_cast<VkFence*>(&slot->fence), VK_TRUE,
                   std::numeric_limits<std::uint64_t>::max());
    vkResetFences(dev, 1, reinterpret_cast<VkFence*>(&slot->fence));
    vkResetCommandBuffer(static_cast<VkCommandBuffer>(slot->command_buffer), 0);
    out = slot;
    return Status::success();
}

Status VulkanRenderDevice::begin_frame() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "device is not initialized");
    }
    if (in_frame_) {
        return Status::fail(ErrorCode::AlreadyExists,
                            "frame already in progress");
    }
    if (swap_images_.empty() || swapchain_ == nullptr) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "no swapchain (call recreate_swapchain)");
    }
    image_index_ = 0;
    SwapImage* slot = nullptr;
    const Status status = acquire_image(slot);
    if (!status.ok()) {
        return status;
    }
    in_frame_ = true;
    pass_open_ = false;
    return Status::success();
}

Status VulkanRenderDevice::clear(const ClearValue& value) {
    std::lock_guard<std::mutex> guard(mutex_);
    const Status status = require_frame();
    if (!status.ok()) {
        return status;
    }
    pending_clear_ = value;
    return Status::success();
}

Status VulkanRenderDevice::record_draws_into_pass() {
    if (pass_open_) {
        return Status::success();
    }
    auto* dev = static_cast<VkDevice>(device_);
    SwapImage* slot = &swap_images_[image_index_];
    auto* cb = static_cast<VkCommandBuffer>(slot->command_buffer);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS) {
        return Status::fail(ErrorCode::Internal, "vkBeginCommandBuffer failed");
    }
    VkClearValue clears[2]{};
    clears[0].color.float32[0] = pending_clear_.r;
    clears[0].color.float32[1] = pending_clear_.g;
    clears[0].color.float32[2] = pending_clear_.b;
    clears[0].color.float32[3] = pending_clear_.a;
    clears[1].depthStencil = {pending_clear_.depth, pending_clear_.stencil};
    VkRenderPassBeginInfo rp_begin{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp_begin.renderPass = static_cast<VkRenderPass>(render_pass_);
    rp_begin.framebuffer = static_cast<VkFramebuffer>(slot->framebuffer);
    rp_begin.renderArea = {{0, 0}, {swapchain_extent_w_, swapchain_extent_h_}};
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clears;
    vkCmdBeginRenderPass(cb, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{0.0f, 0.0f,
                        static_cast<float>(swapchain_extent_w_),
                        static_cast<float>(swapchain_extent_h_), 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, {swapchain_extent_w_, swapchain_extent_h_}};
    vkCmdSetViewport(cb, 0, 1, &viewport);
    vkCmdSetScissor(cb, 0, 1, &scissor);
    pass_open_ = true;
    return Status::success();
}

Status VulkanRenderDevice::draw(const DrawCall& call) {
    std::lock_guard<std::mutex> guard(mutex_);
    const Status status = require_frame();
    if (!status.ok()) {
        return status;
    }
    if (!valid(call)) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid draw call");
    }
    const auto* pipeline = find(call.pipeline.index);
    const auto* vertices = find(call.vertex_buffer.index);
    const auto* indices = find(call.index_buffer.index);
    if (pipeline == nullptr || pipeline->kind != Kind::Pipeline ||
        !handle_live(call.pipeline)) {
        return Status::fail(ErrorCode::NotFound, "pipeline handle is not live");
    }
    if (vertices == nullptr || vertices->kind != Kind::Buffer ||
        !handle_live(call.vertex_buffer)) {
        return Status::fail(ErrorCode::NotFound,
                            "vertex buffer handle is not live");
    }
    if (call.index_count > 0 &&
        (indices == nullptr || indices->kind != Kind::Buffer ||
         !handle_live(call.index_buffer))) {
        return Status::fail(ErrorCode::NotFound,
                            "index buffer handle is not live");
    }
    const Status pass_status = record_draws_into_pass();
    if (!pass_status.ok()) {
        return pass_status;
    }
    auto* dev = static_cast<VkDevice>(device_);
    SwapImage* slot = &swap_images_[image_index_];
    auto* cb = static_cast<VkCommandBuffer>(slot->command_buffer);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      static_cast<VkPipeline>(pipeline->object));
    const VkDeviceSize offset = 0;
    VkBuffer vb = static_cast<VkBuffer>(vertices->object);
    vkCmdBindVertexBuffers(cb, 0, 1, &vb, &offset);
    if (call.index_count > 0) {
        VkBuffer ib = static_cast<VkBuffer>(indices->object);
        vkCmdBindIndexBuffer(cb, ib, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cb, call.index_count, call.instance_count, 0, 0, 0);
    } else {
        vkCmdDraw(cb, call.vertex_count, call.instance_count, 0, 0);
    }
    return Status::success();
}

Status VulkanRenderDevice::end_frame() {
    std::lock_guard<std::mutex> guard(mutex_);
    const Status status = require_frame();
    if (!status.ok()) {
        return status;
    }
    SwapImage* slot = &swap_images_[image_index_];
    auto* dev = static_cast<VkDevice>(device_);
    auto* cb = static_cast<VkCommandBuffer>(slot->command_buffer);
    if (!pass_open_) {
        const Status pass_status = record_draws_into_pass();
        if (!pass_status.ok()) {
            in_frame_ = false;
            return pass_status;
        }
    }
    vkCmdEndRenderPass(cb);
    pass_open_ = false;
    if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
        in_frame_ = false;
        return Status::fail(ErrorCode::Internal, "vkEndCommandBuffer failed");
    }
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkSemaphore wait_sem = static_cast<VkSemaphore>(slot->acquire_sem);
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &wait_sem;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cb;
    VkSemaphore signal_sem = static_cast<VkSemaphore>(slot->render_sem);
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &signal_sem;
    VkFence fence = static_cast<VkFence>(slot->fence);
    if (vkQueueSubmit(queue_, 1, &submit, fence) != VK_SUCCESS) {
        in_frame_ = false;
        return Status::fail(ErrorCode::Internal, "vkQueueSubmit failed");
    }
    slot->fence_submitted = true;
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &signal_sem;
    const VkSwapchainKHR swapchain = static_cast<VkSwapchainKHR>(swapchain_);
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain;
    present.pImageIndices = &image_index_;
    const VkResult pres = vkQueuePresentKHR(queue_, &present);
    in_frame_ = false;
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "swapchain out of date");
    }
    if (pres != VK_SUCCESS) {
        return fail_vk("vkQueuePresentKHR", static_cast<std::int32_t>(pres));
    }
    return Status::success();
}

void VulkanRenderDevice::wait_idle() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (initialized_ && device_ != nullptr) {
        vkDeviceWaitIdle(static_cast<VkDevice>(device_));
    }
}

bool VulkanRenderDevice::device_lost() const noexcept { return lost_; }

Status VulkanRenderDevice::recover_from_device_loss() {
    if (!lost_) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "device is not lost");
    }
    lost_ = false;
    return Status::fail(ErrorCode::Unimplemented,
                        "Vulkan device-loss recovery not implemented");
}

Status VulkanRenderDevice::recreate_swapchain(std::uint32_t width,
                                              std::uint32_t height) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "device is not initialized");
    }
    wait_idle();
    // Tear down per-image objects, then rebuild.
    auto* dev = static_cast<VkDevice>(device_);
    for (auto& slot : swap_images_) {
        auto* cb = static_cast<VkCommandBuffer>(slot.command_buffer);
        auto* fb = static_cast<VkFramebuffer>(slot.framebuffer);
        auto* fence = static_cast<VkFence>(slot.fence);
        auto* acq = static_cast<VkSemaphore>(slot.acquire_sem);
        auto* rnd = static_cast<VkSemaphore>(slot.render_sem);
        if (fb != nullptr) {
            vkDestroyFramebuffer(dev, fb, nullptr);
        }
        if (cb != nullptr) {
            vkFreeCommandBuffers(dev, static_cast<VkCommandPool>(command_pool_),
                                 1, &cb);
        }
        if (fence != nullptr) {
            vkDestroyFence(dev, fence, nullptr);
        }
        if (acq != nullptr) {
            vkDestroySemaphore(dev, acq, nullptr);
        }
        if (rnd != nullptr) {
            vkDestroySemaphore(dev, rnd, nullptr);
        }
    }
    for (auto view : color_views_) {
        vkDestroyImageView(dev, view, nullptr);
    }
    for (auto view : depth_views_) {
        vkDestroyImageView(dev, view, nullptr);
    }
    for (auto& pair : depth_images_) {
        vkDestroyImage(dev, pair.first, nullptr);
        vkFreeMemory(dev, pair.second, nullptr);
    }
    color_views_.clear();
    depth_views_.clear();
    depth_images_.clear();
    swap_images_.clear();
    if (render_pass_ != nullptr) {
        vkDestroyRenderPass(dev, static_cast<VkRenderPass>(render_pass_),
                            nullptr);
        render_pass_ = nullptr;
    }
    if (swapchain_ != nullptr) {
        vkDestroySwapchainKHR(dev, static_cast<VkSwapchainKHR>(swapchain_),
                              nullptr);
        swapchain_ = nullptr;
    }
    return create_swapchain_resources(width, height);
}

}  // namespace poko
