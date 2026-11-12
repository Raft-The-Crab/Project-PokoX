#include "poko/renderer/vulkan_device.h"

#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "vk_conversions.h"

namespace poko {
Result<RenderHandle> VulkanRenderDevice::create_buffer(const BufferDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc) || vk::buffer_usage(desc.usage) == 0) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid buffer descriptor");
    }
    auto* dev = static_cast<VkDevice>(device_);
    auto* phys = static_cast<VkPhysicalDevice>(physical_device_);
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = desc.size_bytes;
    info.usage = vk::buffer_usage(desc.usage);
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(dev, &info, nullptr, &buffer) != VK_SUCCESS) {
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted, "vkCreateBuffer failed");
    }
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(dev, buffer, &req);
    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mem);
    std::uint32_t mem_type = UINT32_MAX;
    for (std::uint32_t t = 0; t < mem.memoryTypeCount; ++t) {
        if ((req.memoryTypeBits & (1u << t)) != 0 &&
            (mem.memoryTypes[t].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            mem_type = t;
            break;
        }
    }
    if (mem_type == UINT32_MAX) {
        vkDestroyBuffer(dev, buffer, nullptr);
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted,
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

Result<RenderHandle> VulkanRenderDevice::create_texture(const TextureDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc) || desc.depth != 1 || desc.array_layers != 1) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument,
                                          "only 2D non-array textures are supported");
    }
    const VkFormat format = vk::format(desc.format);
    if (format == VK_FORMAT_UNDEFINED) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "unsupported pixel format");
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
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted, "vkCreateImage failed");
    }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(dev, image, &req);
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
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(dev, &alloc, nullptr, &memory) != VK_SUCCESS ||
        vkBindImageMemory(dev, image, memory, 0) != VK_SUCCESS) {
        vkDestroyImage(dev, image, nullptr);
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted,
                                          "texture memory allocation failed");
    }
    VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, desc.mip_levels, 0, 1};
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(dev, &view_info, nullptr, &view) != VK_SUCCESS) {
        vkDestroyImage(dev, image, nullptr);
        vkFreeMemory(dev, memory, nullptr);
        return Result<RenderHandle>::fail(ErrorCode::Internal, "vkCreateImageView failed");
    }
    const HandleIndex index = alloc_slot(Kind::Texture);
    auto& resource = resources_[index];
    resource.object = image;
    resource.object2 = memory;
    resource.object3 = view;
    resource.format = static_cast<std::uint32_t>(format);
    resource.width = desc.width;
    resource.height = desc.height;
    resource.mip_levels = desc.mip_levels;
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Result<RenderHandle> VulkanRenderDevice::create_sampler(const SamplerDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    auto* dev = static_cast<VkDevice>(device_);
    VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    info.magFilter = vk::filter(desc.mag_filter);
    info.minFilter = vk::filter(desc.min_filter);
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = vk::address(desc.wrap_u);
    info.addressModeV = vk::address(desc.wrap_v);
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(dev, &info, nullptr, &sampler) != VK_SUCCESS) {
        return Result<RenderHandle>::fail(ErrorCode::ResourceExhausted, "vkCreateSampler failed");
    }
    const HandleIndex index = alloc_slot(Kind::Sampler);
    auto& resource = resources_[index];
    resource.object = sampler;
    return Result<RenderHandle>::success(make_handle(index, generations_[index]));
}

Result<RenderHandle> VulkanRenderDevice::create_shader(const ShaderDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid shader descriptor");
    }
    std::string source(desc.source);
    std::vector<std::uint32_t> spirv;
    std::string log;
    if (!vk::compile_spirv(desc.stage, source, spirv, log)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument,
                                          "glslang compile failed: " + log);
    }
    auto* dev = static_cast<VkDevice>(device_);
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
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

Result<RenderHandle> VulkanRenderDevice::create_pipeline(const PipelineDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<RenderHandle>::fail(ErrorCode::FailedPrecondition,
                                          "device is not initialized");
    }
    if (!valid(desc.layout)) {
        return Result<RenderHandle>::fail(ErrorCode::InvalidArgument, "invalid vertex layout");
    }
    const Resource* vs = handle_live(desc.vertex_shader) ? find(desc.vertex_shader.index) : nullptr;
    const Resource* fs =
        handle_live(desc.fragment_shader) ? find(desc.fragment_shader.index) : nullptr;
    if (vs == nullptr || fs == nullptr || vs->kind != Kind::Shader || fs->kind != Kind::Shader ||
        vs->spirv.empty() || fs->spirv.empty()) {
        return Result<RenderHandle>::fail(ErrorCode::NotFound, "shader handles are not live");
    }
    auto* dev = static_cast<VkDevice>(device_);

    VkShaderModuleCreateInfo mod_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkPipelineShaderStageCreateInfo stages[2]{};
    VkShaderModule modules[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    for (int i = 0; i < 2; ++i) {
        const Resource* shader = i == 0 ? vs : fs;
        mod_info.codeSize = shader->spirv.size() * sizeof(std::uint32_t);
        mod_info.pCode = shader->spirv.data();
        if (vkCreateShaderModule(dev, &mod_info, nullptr, &modules[i]) != VK_SUCCESS) {
            return Result<RenderHandle>::fail(ErrorCode::Internal, "vkCreateShaderModule failed");
        }
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].stage = i == 0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
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
        attribs[i].format = vk::attrib_format(a.components);
        attribs[i].offset = a.offset_bytes;
    }
    VkPipelineVertexInputStateCreateInfo vertex_input{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input.vertexBindingDescriptionCount = desc.layout.attribute_count > 0 ? 1 : 0;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = desc.layout.attribute_count;
    vertex_input.pVertexAttributeDescriptions = attribs;

    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = vk::topology(desc.layout.topology);
    ia.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = vk::cull(desc.cull_mode);
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.depthTestEnable = desc.depth_test ? VK_TRUE : VK_FALSE;
    depth.depthWriteEnable = desc.depth_write ? VK_TRUE : VK_FALSE;
    depth.depthCompareOp = vk::compare(desc.depth_compare);
    depth.depthBoundsTestEnable = VK_FALSE;
    depth.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend_state{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend_state.attachmentCount = 1;
    blend_state.pAttachments = &blend;

    const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
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
        vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline);
    vkDestroyShaderModule(dev, modules[0], nullptr);
    vkDestroyShaderModule(dev, modules[1], nullptr);
    if (result != VK_SUCCESS) {
        return Result<RenderHandle>::fail(ErrorCode::Internal, "vkCreateGraphicsPipelines failed");
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
        return Status::fail(ErrorCode::FailedPrecondition, "device is not initialized");
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

} // namespace poko
