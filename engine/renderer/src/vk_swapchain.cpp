#include "poko/renderer/vulkan_device.h"

#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "vk_conversions.h"

namespace poko {
Status VulkanRenderDevice::create_swapchain_resources(std::uint32_t width, std::uint32_t height) {
    auto* dev = static_cast<VkDevice>(device_);
    auto* surf = static_cast<VkSurfaceKHR>(surface_);
    auto* phys = static_cast<VkPhysicalDevice>(physical_device_);

    VkSurfaceCapabilitiesKHR caps{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surf, &caps) != VK_SUCCESS) {
        return Status::fail(ErrorCode::Internal,
                            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");
    }
    if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
        return Status::fail(ErrorCode::FailedPrecondition, "surface has zero extent");
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
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &format_count, formats.data());
    VkSurfaceFormatKHR chosen =
        formats.empty()
            ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
            : formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) {
            chosen = f;
            break;
        }
    }
    swapchain_format_ = static_cast<std::uint32_t>(chosen.format);

    std::uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
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
    dep.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rp_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp_info.attachmentCount = 2;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies = &dep;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    if (vkCreateRenderPass(dev, &rp_info, nullptr, &render_pass) != VK_SUCCESS) {
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
        if (vkCreateImage(dev, &depth_info, nullptr, &depth_image) != VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted, "vkCreateImage(depth) failed");
        }
        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(dev, depth_image, &req);
        VkPhysicalDeviceMemoryProperties mem{};
        vkGetPhysicalDeviceMemoryProperties(phys, &mem);
        std::uint32_t mem_type = UINT32_MAX;
        for (std::uint32_t t = 0; t < mem.memoryTypeCount; ++t) {
            if ((req.memoryTypeBits & (1u << t)) != 0 &&
                (mem.memoryTypes[t].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
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
            return Status::fail(ErrorCode::ResourceExhausted, "depth memory allocation failed");
        }
        // Store depth image+memory inside the framebuffer attachment slot is
        // not possible; keep them alive via a side vector on the device.
        // (Tracked in depth_objects_; see below.)
        depth_images_.push_back({depth_image, depth_mem});
        VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = depth_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_D32_SFLOAT;
        view_info.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        VkImageView depth_view = VK_NULL_HANDLE;
        if (vkCreateImageView(dev, &view_info, nullptr, &depth_view) != VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal, "vkCreateImageView(depth) failed");
        }
        depth_views_.push_back(depth_view);

        VkImageView color_view = VK_NULL_HANDLE;
        VkImageViewCreateInfo color_view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        color_view_info.image = images[i];
        color_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        color_view_info.format = chosen.format;
        color_view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(dev, &color_view_info, nullptr, &color_view) != VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal, "vkCreateImageView(color) failed");
        }
        color_views_.push_back(color_view);

        VkFramebufferCreateInfo fb_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        VkImageView fb_views[2] = {color_view, depth_view};
        fb_info.renderPass = render_pass;
        fb_info.attachmentCount = 2;
        fb_info.pAttachments = fb_views;
        fb_info.width = swapchain_extent_w_;
        fb_info.height = swapchain_extent_h_;
        fb_info.layers = 1;
        VkFramebuffer fb = VK_NULL_HANDLE;
        if (vkCreateFramebuffer(dev, &fb_info, nullptr, &fb) != VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal, "vkCreateFramebuffer failed");
        }
        slot.framebuffer = fb;
    }

    // Frame-in-flight slots: command buffer + fence + semaphores each.
    // Count matches swapchain images so every image can be in flight once.
    frame_slots_.clear();
    frame_slots_.resize(actual_count);
    VkCommandBufferAllocateInfo cb_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cb_info.commandPool = static_cast<VkCommandPool>(command_pool_);
    cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_info.commandBufferCount = 1;
    VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkSemaphoreCreateInfo sem_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (std::uint32_t i = 0; i < actual_count; ++i) {
        FrameSlot& fs = frame_slots_[i];
        VkCommandBuffer cb = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(dev, &cb_info, &cb) != VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal, "vkAllocateCommandBuffers failed");
        }
        fs.command_buffer = cb;
        VkFence fence = VK_NULL_HANDLE;
        if (vkCreateFence(dev, &fence_info, nullptr, &fence) != VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted, "vkCreateFence failed");
        }
        fs.fence = fence;
        VkSemaphore acq = VK_NULL_HANDLE;
        VkSemaphore rnd = VK_NULL_HANDLE;
        if (vkCreateSemaphore(dev, &sem_info, nullptr, &acq) != VK_SUCCESS ||
            vkCreateSemaphore(dev, &sem_info, nullptr, &rnd) != VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted, "vkCreateSemaphore failed");
        }
        fs.acquire_sem = acq;
        fs.render_sem = rnd;
        // Descriptor pool for this slot's sets; reset when the slot is reused
        // in acquire_image after its fence has been waited. Sized for the
        // worst case: every set carries one UBO plus all texture bindings.
        VkDescriptorPoolSize pool_sizes[2]{};
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = vk::kMaxUniformSetsPerFrame;
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = vk::kMaxUniformSetsPerFrame * DrawCall::kMaxDrawTextures;
        VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool_info.maxSets = vk::kMaxUniformSetsPerFrame;
        pool_info.poolSizeCount = 2;
        pool_info.pPoolSizes = pool_sizes;
        VkDescriptorPool dpool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(dev, &pool_info, nullptr, &dpool) != VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted, "vkCreateDescriptorPool failed");
        }
        fs.descriptor_pool = dpool;
    }
    return Status::success();
}

Status VulkanRenderDevice::recreate_swapchain(std::uint32_t width, std::uint32_t height) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is not initialized");
    }
    // mutex_ is already held; idle directly rather than via wait_idle().
    vkDeviceWaitIdle(static_cast<VkDevice>(device_));
    // Tear down per-image objects, then rebuild.
    auto* dev = static_cast<VkDevice>(device_);
    for (auto& image : swap_images_) {
        auto* fb = static_cast<VkFramebuffer>(image.framebuffer);
        if (fb != nullptr) {
            vkDestroyFramebuffer(dev, fb, nullptr);
        }
    }
    for (auto& slot : frame_slots_) {
        auto* cb = static_cast<VkCommandBuffer>(slot.command_buffer);
        auto* fence = static_cast<VkFence>(slot.fence);
        auto* acq = static_cast<VkSemaphore>(slot.acquire_sem);
        auto* rnd = static_cast<VkSemaphore>(slot.render_sem);
        if (cb != nullptr) {
            vkFreeCommandBuffers(dev, static_cast<VkCommandPool>(command_pool_), 1, &cb);
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
        if (slot.descriptor_pool != nullptr) {
            vkDestroyDescriptorPool(dev, static_cast<VkDescriptorPool>(slot.descriptor_pool),
                                    nullptr);
        }
    }
    for (auto view : color_views_) {
        vkDestroyImageView(dev, static_cast<VkImageView>(view), nullptr);
    }
    for (auto view : depth_views_) {
        vkDestroyImageView(dev, static_cast<VkImageView>(view), nullptr);
    }
    for (auto& pair : depth_images_) {
        vkDestroyImage(dev, static_cast<VkImage>(pair.image), nullptr);
        vkFreeMemory(dev, static_cast<VkDeviceMemory>(pair.memory), nullptr);
    }
    color_views_.clear();
    depth_views_.clear();
    depth_images_.clear();
    swap_images_.clear();
    frame_slots_.clear();
    frame_slot_ = 0;
    image_index_ = ~0u;
    if (render_pass_ != nullptr) {
        vkDestroyRenderPass(dev, static_cast<VkRenderPass>(render_pass_), nullptr);
        render_pass_ = nullptr;
    }
    if (swapchain_ != nullptr) {
        vkDestroySwapchainKHR(dev, static_cast<VkSwapchainKHR>(swapchain_), nullptr);
        swapchain_ = nullptr;
    }
    return create_swapchain_resources(width, height);
}

} // namespace poko
