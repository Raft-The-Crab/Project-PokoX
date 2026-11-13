#include "poko/renderer/vulkan_device.h"

#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "vk_conversions.h"

namespace poko {
VulkanRenderDevice::VulkanRenderDevice(void* instance, void* physical_device, void* surface)
    : instance_(instance), physical_device_(physical_device), surface_(surface) {}

VulkanRenderDevice::~VulkanRenderDevice() {
    if (!initialized_ || device_ == nullptr) {
        return;
    }
    auto* dev = static_cast<VkDevice>(device_);
    vkDeviceWaitIdle(dev);
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
    for (auto& resource : resources_) {
        destroy_resource_objects(resource);
    }
    if (swapchain_ != nullptr) {
        vkDestroySwapchainKHR(dev, static_cast<VkSwapchainKHR>(swapchain_), nullptr);
    }
    if (render_pass_ != nullptr) {
        vkDestroyRenderPass(dev, static_cast<VkRenderPass>(render_pass_), nullptr);
    }
    if (pipeline_layout_ != nullptr) {
        vkDestroyPipelineLayout(dev, static_cast<VkPipelineLayout>(pipeline_layout_), nullptr);
    }
    if (descriptor_set_layout_ != nullptr) {
        vkDestroyDescriptorSetLayout(
            dev, static_cast<VkDescriptorSetLayout>(descriptor_set_layout_), nullptr);
    }
    if (command_pool_ != nullptr) {
        vkDestroyCommandPool(dev, static_cast<VkCommandPool>(command_pool_), nullptr);
    }
    vkDestroyDevice(dev, nullptr);
    glslang::FinalizeProcess();
}

void VulkanRenderDevice::destroy_resource_objects(Resource& resource) {
    auto* dev = static_cast<VkDevice>(device_);
    switch (resource.kind) {
    case Kind::Buffer:
        if (resource.object != nullptr) {
            vkDestroyBuffer(dev, static_cast<VkBuffer>(resource.object), nullptr);
        }
        if (resource.object2 != nullptr) {
            vkFreeMemory(dev, static_cast<VkDeviceMemory>(resource.object2), nullptr);
        }
        break;
    case Kind::Texture:
        if (resource.object3 != nullptr) {
            vkDestroyImageView(dev, static_cast<VkImageView>(resource.object3), nullptr);
        }
        if (resource.object != nullptr) {
            vkDestroyImage(dev, static_cast<VkImage>(resource.object), nullptr);
        }
        if (resource.object2 != nullptr) {
            vkFreeMemory(dev, static_cast<VkDeviceMemory>(resource.object2), nullptr);
        }
        break;
    case Kind::Shader:
        if (resource.object != nullptr) {
            vkDestroyShaderModule(dev, static_cast<VkShaderModule>(resource.object), nullptr);
        }
        break;
    case Kind::Pipeline:
        if (resource.object != nullptr) {
            vkDestroyPipeline(dev, static_cast<VkPipeline>(resource.object), nullptr);
        }
        break;
    case Kind::Sampler:
        if (resource.object != nullptr) {
            vkDestroySampler(dev, static_cast<VkSampler>(resource.object), nullptr);
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
    if (instance_ == nullptr || physical_device_ == nullptr || surface_ == nullptr) {
        return false;
    }
    glslang::InitializeProcess();

    // Queue family with graphics support.
    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(static_cast<VkPhysicalDevice>(physical_device_),
                                             &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(static_cast<VkPhysicalDevice>(physical_device_),
                                             &family_count, families.data());
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
    if (vkCreateDevice(static_cast<VkPhysicalDevice>(physical_device_), &device_info, nullptr,
                       &device) != VK_SUCCESS) {
        glslang::FinalizeProcess();
        return false;
    }
    device_ = device;
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queue_family, 0, &queue);
    queue_ = queue;

    VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queue_family;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
        return false;
    }
    command_pool_ = command_pool;

    // Descriptor set 0: binding 0 is one uniform buffer; bindings 1..4 are
    // combined image samplers for DrawCall::textures[0..3]. Visible to both
    // stages so vertex-stage sampling keeps working.
    VkDescriptorSetLayoutBinding bindings[vk::kMaxDescriptorBindings]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    for (std::uint32_t i = 1; i < vk::kMaxDescriptorBindings; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dsl_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl_info.bindingCount = vk::kMaxDescriptorBindings;
    dsl_info.pBindings = bindings;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device, &dsl_info, nullptr, &descriptor_set_layout) !=
        VK_SUCCESS) {
        return false;
    }
    descriptor_set_layout_ = descriptor_set_layout;

    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &descriptor_set_layout;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(device, &layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        return false;
    }
    pipeline_layout_ = pipeline_layout;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(static_cast<VkPhysicalDevice>(physical_device_), &props);
    device_name_ = props.deviceName;
    capabilities_.backend = GraphicsBackend::Vulkan;
    capabilities_.device_name = device_name_.c_str();
    capabilities_.max_texture_size = props.limits.maxImageDimension2D;
    capabilities_.max_buffer_bytes =
        props.limits.maxStorageBufferRange > 0
            ? static_cast<std::uint64_t>(props.limits.maxStorageBufferRange)
            : (std::uint64_t{1} << 30);
    capabilities_.supports_compute = (props.limits.maxComputeWorkGroupCount[0] > 0);
    capabilities_.supports_wireframe = true;
    capabilities_.supports_shader_reload = true;

    // Default framebuffer extent: swapchain creation fills these in.
    initialized_ = true;
    return true;
}

const char* VulkanRenderDevice::name() const noexcept {
    return "vulkan";
}

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
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < resources_.size() &&
           generations_[handle.index] == handle.generation && resources_[handle.index].alive;
}

VulkanRenderDevice::Resource* VulkanRenderDevice::find(HandleIndex index) noexcept {
    if (static_cast<std::size_t>(index) >= resources_.size()) {
        return nullptr;
    }
    return &resources_[index];
}

const VulkanRenderDevice::Resource* VulkanRenderDevice::find(HandleIndex index) const noexcept {
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
        return Status::fail(ErrorCode::FailedPrecondition, "device is not initialized");
    }
    if (!in_frame_) {
        return Status::fail(ErrorCode::FailedPrecondition, "no frame in progress");
    }
    return Status::success();
}

Status VulkanRenderDevice::fail_vk(const char* what, std::int32_t result) const {
    return Status::fail(ErrorCode::Internal,
                        std::string(what) + " failed (VkResult " + std::to_string(result) + ")");
}

void VulkanRenderDevice::wait_idle() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (initialized_ && device_ != nullptr) {
        vkDeviceWaitIdle(static_cast<VkDevice>(device_));
    }
}

bool VulkanRenderDevice::device_lost() const noexcept {
    return lost_;
}

Status VulkanRenderDevice::recover_from_device_loss() {
    if (!lost_) {
        return Status::fail(ErrorCode::FailedPrecondition, "device is not lost");
    }
    // Stays lost: recovery would need a full device rebuild.
    return Status::fail(ErrorCode::Unimplemented, "Vulkan device-loss recovery not implemented");
}

} // namespace poko
