#include "poko/renderer/vulkan_device.h"

#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "vk_conversions.h"

namespace poko {
Status VulkanRenderDevice::acquire_image() {
    auto* dev = static_cast<VkDevice>(device_);
    FrameSlot& slot = frame_slots_[frame_slot_ % frame_slots_.size()];
    // Wait for this slot's previous submission before reusing it; this also
    // guarantees its semaphores have been fully consumed.
    if (slot.fence_submitted) {
        vkWaitForFences(dev, 1, reinterpret_cast<VkFence*>(&slot.fence), VK_TRUE,
                        std::numeric_limits<std::uint64_t>::max());
    }
    vkResetFences(dev, 1, reinterpret_cast<VkFence*>(&slot.fence));
    vkResetDescriptorPool(dev, static_cast<VkDescriptorPool>(slot.descriptor_pool), 0);
    vkResetCommandBuffer(static_cast<VkCommandBuffer>(slot.command_buffer), 0);
    const VkResult result = vkAcquireNextImageKHR(
        dev, static_cast<VkSwapchainKHR>(swapchain_), std::numeric_limits<std::uint64_t>::max(),
        static_cast<VkSemaphore>(slot.acquire_sem), VK_NULL_HANDLE, &image_index_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return Status::fail(ErrorCode::FailedPrecondition, "swapchain out of date");
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return fail_vk("vkAcquireNextImageKHR", static_cast<std::int32_t>(result));
    }
    ++frame_slot_;
    return Status::success();
}

Status VulkanRenderDevice::begin_frame() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is not initialized");
    }
    if (in_frame_) {
        return Status::fail(ErrorCode::AlreadyExists, "frame already in progress");
    }
    if (swap_images_.empty() || swapchain_ == nullptr) {
        return Status::fail(ErrorCode::FailedPrecondition,
                            "no swapchain (call recreate_swapchain)");
    }
    image_index_ = 0;
    const Status status = acquire_image();
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
    const SwapImage& image = swap_images_[image_index_];
    FrameSlot& slot = frame_slots_[(frame_slot_ - 1) % frame_slots_.size()];
    auto* cb = static_cast<VkCommandBuffer>(slot.command_buffer);
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
    VkRenderPassBeginInfo rp_begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp_begin.renderPass = static_cast<VkRenderPass>(render_pass_);
    rp_begin.framebuffer = static_cast<VkFramebuffer>(image.framebuffer);
    rp_begin.renderArea = {{0, 0}, {swapchain_extent_w_, swapchain_extent_h_}};
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clears;
    vkCmdBeginRenderPass(cb, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{0.0f,
                        0.0f,
                        static_cast<float>(swapchain_extent_w_),
                        static_cast<float>(swapchain_extent_h_),
                        0.0f,
                        1.0f};
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
    if (pipeline == nullptr || pipeline->kind != Kind::Pipeline || !handle_live(call.pipeline)) {
        return Status::fail(ErrorCode::NotFound, "pipeline handle is not live");
    }
    if (vertices == nullptr || vertices->kind != Kind::Buffer || !handle_live(call.vertex_buffer)) {
        return Status::fail(ErrorCode::NotFound, "vertex buffer handle is not live");
    }
    if (call.index_count > 0 &&
        (indices == nullptr || indices->kind != Kind::Buffer || !handle_live(call.index_buffer))) {
        return Status::fail(ErrorCode::NotFound, "index buffer handle is not live");
    }
    const auto* uniforms =
        is_valid(call.uniform_buffer) ? find(call.uniform_buffer.index) : nullptr;
    if (is_valid(call.uniform_buffer) && (uniforms == nullptr || uniforms->kind != Kind::Buffer ||
                                          !handle_live(call.uniform_buffer))) {
        return Status::fail(ErrorCode::NotFound, "uniform buffer handle is not live");
    }
    if (uniforms != nullptr && call.uniform_offset >= uniforms->aux) {
        return Status::fail(ErrorCode::OutOfRange, "uniform offset is out of range");
    }
    for (std::uint32_t i = 0; i < call.texture_count; ++i) {
        const auto* texture = is_valid(call.textures[i]) ? find(call.textures[i].index) : nullptr;
        const auto* sampler = is_valid(call.samplers[i]) ? find(call.samplers[i].index) : nullptr;
        if (texture == nullptr || texture->kind != Kind::Texture ||
            !handle_live(call.textures[i]) || sampler == nullptr ||
            sampler->kind != Kind::Sampler || !handle_live(call.samplers[i])) {
            return Status::fail(ErrorCode::NotFound, "texture/sampler handle is not live");
        }
    }
    const Status pass_status = record_draws_into_pass();
    if (!pass_status.ok()) {
        return pass_status;
    }
    FrameSlot& slot = frame_slots_[(frame_slot_ - 1) % frame_slots_.size()];
    auto* cb = static_cast<VkCommandBuffer>(slot.command_buffer);
    auto* dev = static_cast<VkDevice>(device_);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      static_cast<VkPipeline>(pipeline->object));
    if (uniforms != nullptr || call.texture_count > 0) {
        VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        alloc_info.descriptorPool = static_cast<VkDescriptorPool>(slot.descriptor_pool);
        alloc_info.descriptorSetCount = 1;
        VkDescriptorSetLayout set_layout =
            static_cast<VkDescriptorSetLayout>(descriptor_set_layout_);
        alloc_info.pSetLayouts = &set_layout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(dev, &alloc_info, &set) != VK_SUCCESS) {
            return Status::fail(ErrorCode::ResourceExhausted,
                                "vkAllocateDescriptorSets failed (frame uniform limit?)");
        }
        VkWriteDescriptorSet writes[vk::kMaxDescriptorBindings]{};
        VkDescriptorBufferInfo buffer_info{};
        VkDescriptorImageInfo image_infos[DrawCall::kMaxDrawTextures]{};
        std::uint32_t write_count = 0;
        if (uniforms != nullptr) {
            buffer_info.buffer = static_cast<VkBuffer>(uniforms->object);
            buffer_info.offset = static_cast<VkDeviceSize>(call.uniform_offset);
            buffer_info.range = static_cast<VkDeviceSize>(uniforms->aux - call.uniform_offset);
            writes[write_count] = VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[write_count].dstSet = set;
            writes[write_count].dstBinding = 0;
            writes[write_count].descriptorCount = 1;
            writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[write_count].pBufferInfo = &buffer_info;
            ++write_count;
        }
        for (std::uint32_t i = 0; i < call.texture_count; ++i) {
            const auto* texture = find(call.textures[i].index);
            const auto* sampler = find(call.samplers[i].index);
            image_infos[i].sampler = static_cast<VkSampler>(sampler->object);
            image_infos[i].imageView = static_cast<VkImageView>(texture->object3);
            image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            writes[write_count] = VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[write_count].dstSet = set;
            writes[write_count].dstBinding = 1 + i;
            writes[write_count].descriptorCount = 1;
            writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[write_count].pImageInfo = &image_infos[i];
            ++write_count;
        }
        vkUpdateDescriptorSets(dev, write_count, writes, 0, nullptr);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                static_cast<VkPipelineLayout>(pipeline_layout_), 0, 1, &set, 0,
                                nullptr);
    }
    const VkDeviceSize offset = 0;
    VkBuffer vb = static_cast<VkBuffer>(vertices->object);
    vkCmdBindVertexBuffers(cb, 0, 1, &vb, &offset);
    if (call.index_count > 0) {
        VkBuffer ib = static_cast<VkBuffer>(indices->object);
        const VkIndexType index_type =
            call.index_type == IndexType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
        vkCmdBindIndexBuffer(cb, ib, 0, index_type);
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
    FrameSlot& slot = frame_slots_[(frame_slot_ - 1) % frame_slots_.size()];
    auto* cb = static_cast<VkCommandBuffer>(slot.command_buffer);
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
    VkSemaphore wait_sem = static_cast<VkSemaphore>(slot.acquire_sem);
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &wait_sem;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cb;
    VkSemaphore signal_sem = static_cast<VkSemaphore>(slot.render_sem);
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &signal_sem;
    VkFence fence = static_cast<VkFence>(slot.fence);
    const VkQueue queue = static_cast<VkQueue>(queue_);
    if (vkQueueSubmit(queue, 1, &submit, fence) != VK_SUCCESS) {
        in_frame_ = false;
        return Status::fail(ErrorCode::Internal, "vkQueueSubmit failed");
    }
    slot.fence_submitted = true;
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &signal_sem;
    const VkSwapchainKHR swapchain = static_cast<VkSwapchainKHR>(swapchain_);
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain;
    present.pImageIndices = &image_index_;
    const VkResult pres = vkQueuePresentKHR(queue, &present);
    in_frame_ = false;
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
        return Status::fail(ErrorCode::FailedPrecondition, "swapchain out of date");
    }
    if (pres != VK_SUCCESS) {
        return fail_vk("vkQueuePresentKHR", static_cast<std::int32_t>(pres));
    }
    return Status::success();
}

} // namespace poko
