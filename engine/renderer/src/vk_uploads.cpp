#include "poko/renderer/vulkan_device.h"

#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "vk_conversions.h"

namespace poko {
Status VulkanRenderDevice::update_buffer(RenderHandle handle, std::span<const std::byte> data,
                                         std::uint64_t offset_bytes) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is not initialized");
    }
    if (!handle_live(handle)) {
        return Status::fail(ErrorCode::NotFound, "buffer handle is not live");
    }
    auto* resource = find(handle.index);
    if (resource->kind != Kind::Buffer) {
        return Status::fail(ErrorCode::InvalidArgument, "handle is not a buffer");
    }
    if (data.empty() || offset_bytes > resource->aux ||
        data.size() > resource->aux - offset_bytes) {
        return Status::fail(ErrorCode::OutOfRange, "buffer update is out of range");
    }
    auto* dev = static_cast<VkDevice>(device_);
    void* mapped = nullptr;
    if (vkMapMemory(dev, static_cast<VkDeviceMemory>(resource->object2),
                    static_cast<VkDeviceSize>(offset_bytes), static_cast<VkDeviceSize>(data.size()),
                    0, &mapped) != VK_SUCCESS) {
        return Status::fail(ErrorCode::Internal, "vkMapMemory failed");
    }
    std::memcpy(mapped, data.data(), data.size());
    vkUnmapMemory(dev, static_cast<VkDeviceMemory>(resource->object2));
    return Status::success();
}

Status VulkanRenderDevice::update_texture(RenderHandle handle, std::uint32_t mip_level,
                                          std::span<const std::byte> data) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is not initialized");
    }
    if (!handle_live(handle)) {
        return Status::fail(ErrorCode::NotFound, "texture handle is not live");
    }
    auto* resource = find(handle.index);
    if (resource->kind != Kind::Texture) {
        return Status::fail(ErrorCode::InvalidArgument, "handle is not a texture");
    }
    if (mip_level >= resource->mip_levels) {
        return Status::fail(ErrorCode::OutOfRange, "mip level is out of range");
    }
    const auto format = static_cast<VkFormat>(resource->format);
    const std::uint32_t bpp = vk::bpp(format);
    if (bpp == 0) {
        return Status::fail(ErrorCode::Unsupported, "texture format is not uploadable");
    }
    const std::uint32_t level_w = vk::mip_dimension(resource->width, mip_level);
    const std::uint32_t level_h = vk::mip_dimension(resource->height, mip_level);
    const VkDeviceSize expected = static_cast<VkDeviceSize>(level_w) * level_h * bpp;
    if (data.size() != expected) {
        return Status::fail(ErrorCode::OutOfRange, "texture data size does not match mip level");
    }

    auto* dev = static_cast<VkDevice>(device_);
    auto* phys = static_cast<VkPhysicalDevice>(physical_device_);
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory staging_mem = VK_NULL_HANDLE;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

    const Status result = [&]() -> Status {
        // Staging buffer: host-visible coherent, filled by memcpy.
        VkBufferCreateInfo buf_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buf_info.size = expected;
        buf_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(dev, &buf_info, nullptr, &staging) != VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted, "vkCreateBuffer(staging) failed");
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(dev, staging, &req);
        VkPhysicalDeviceMemoryProperties mem{};
        vkGetPhysicalDeviceMemoryProperties(phys, &mem);
        const std::uint32_t mem_type = vk::memory_type(mem, req.memoryTypeBits,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (mem_type == UINT32_MAX) {
            return Status::fail(ErrorCode::ResourceExhausted,
                                "no host-visible coherent memory for staging");
        }
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = mem_type;
        if (vkAllocateMemory(dev, &alloc, nullptr, &staging_mem) != VK_SUCCESS ||
            vkBindBufferMemory(dev, staging, staging_mem, 0) != VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted, "staging memory allocation failed");
        }
        void* mapped = nullptr;
        if (vkMapMemory(dev, staging_mem, 0, expected, 0, &mapped) != VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal, "vkMapMemory(staging) failed");
        }
        std::memcpy(mapped, data.data(), data.size());
        vkUnmapMemory(dev, staging_mem);

        // One-shot command buffer: UNDEFINED -> TRANSFER_DST -> SHADER_READ_ONLY.
        VkCommandBufferAllocateInfo cb_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cb_info.commandPool = static_cast<VkCommandPool>(command_pool_);
        cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cb_info.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(dev, &cb_info, &cb) != VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal, "vkAllocateCommandBuffers failed");
        }
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(cb, &begin) != VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal, "vkBeginCommandBuffer failed");
        }
        const VkImage image = static_cast<VkImage>(resource->object);
        VkImageMemoryBarrier to_dst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to_dst.srcAccessMask = 0;
        to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_dst.image = image;
        to_dst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip_level, 1, 0, 1};
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &to_dst);
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip_level, 0, 1};
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {level_w, level_h, 1};
        vkCmdCopyBufferToImage(cb, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &region);
        VkImageMemoryBarrier to_read{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        to_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        to_read.image = image;
        to_read.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip_level, 1, 0, 1};
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &to_read);
        if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal, "vkEndCommandBuffer failed");
        }
        VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if (vkCreateFence(dev, &fence_info, nullptr, &fence) != VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted, "vkCreateFence failed");
        }
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cb;
        const VkQueue queue = static_cast<VkQueue>(queue_);
        if (vkQueueSubmit(queue, 1, &submit, fence) != VK_SUCCESS ||
            vkWaitForFences(dev, 1, &fence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()) !=
                VK_SUCCESS) {
            return Status::fail(ErrorCode::Internal, "texture upload submit failed");
        }
        return Status::success();
    }();

    if (fence != VK_NULL_HANDLE) {
        vkDestroyFence(dev, fence, nullptr);
    }
    if (cb != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(dev, static_cast<VkCommandPool>(command_pool_), 1, &cb);
    }
    if (staging_mem != VK_NULL_HANDLE) {
        vkFreeMemory(dev, staging_mem, nullptr);
    }
    if (staging != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, staging, nullptr);
    }
    return result;
}

} // namespace poko
