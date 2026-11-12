#pragma once

// Internal Vulkan enum/format/memory helpers shared by the backend's
// translation units. Not part of the public API.

#include <cstdint>
#include <string>
#include <vector>

// Vulkan and glslang headers are C-style aggregate APIs; their partial
// initializers and shadowing are idiomatic upstream and not our code.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <vulkan/vulkan.h>

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Include/InfoSink.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
// Vulkan create-info structs are idiomatically brace-initialized with only
// sType; GCC warns on every omitted member. Keep the warning off for this
// translation unit only.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

namespace poko {
namespace vk {

// The Vulkan loader exports the full entry-point surface, so vk* functions
// are called directly after linking libvulkan.

inline EShLanguage shader_language(ShaderStage stage) noexcept {
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

inline VkFormat format(PixelFormat format) noexcept {
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

inline VkBufferUsageFlags buffer_usage(BufferUsage usage) noexcept {
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
        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    return 0;
}

inline VkPrimitiveTopology topology(PrimitiveTopology topology) noexcept {
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

inline VkCullModeFlags cull(CullMode mode) noexcept {
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

inline VkCompareOp compare(CompareOp op) noexcept {
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

inline VkFilter filter(FilterMode mode) noexcept {
    return mode == FilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

inline VkSamplerAddressMode address(WrapMode mode) noexcept {
    switch (mode) {
    case WrapMode::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case WrapMode::ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case WrapMode::MirroredRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

// Bytes per pixel for the color formats update_texture can stage; depth
// formats and unknown formats return 0 (not uploadable through this path).
inline std::uint32_t bpp(VkFormat format) noexcept {
    switch (format) {
    case VK_FORMAT_R8_UNORM:
        return 1;
    case VK_FORMAT_R8G8B8_UNORM:
        return 3;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
        return 4;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return 8;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return 16;
    default:
        return 0;
    }
}

inline std::uint32_t mip_dimension(std::uint32_t base, std::uint32_t level) noexcept {
    if (level >= 32) {
        return 1;
    }
    const std::uint32_t dim = base >> level;
    return dim == 0 ? 1 : dim;
}

// First memory type satisfying `required` property bits, or UINT32_MAX.
inline std::uint32_t memory_type(const VkPhysicalDeviceMemoryProperties& mem,
                                std::uint32_t type_bits,
                                VkMemoryPropertyFlags required) noexcept {
    for (std::uint32_t t = 0; t < mem.memoryTypeCount; ++t) {
        if ((type_bits & (1u << t)) != 0 &&
            (mem.memoryTypes[t].propertyFlags & required) == required) {
            return t;
        }
    }
    return UINT32_MAX;
}

inline VkFormat attrib_format(std::uint32_t components) noexcept {
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

// Maximum descriptor sets (one per draw call) allocatable per frame slot.
inline constexpr std::uint32_t kMaxUniformSetsPerFrame = 256;
// Descriptor set 0 bindings: 0 = uniform buffer, 1..N = texture/sampler pairs.
inline constexpr std::uint32_t kMaxDescriptorBindings = 1 + DrawCall::kMaxDrawTextures;

// Compiles GLSL source to SPIR-V via glslang. Returns false on failure with
// the log appended to `log_out`.
inline bool compile_spirv(ShaderStage stage, const std::string& source,
                          std::vector<std::uint32_t>& out_spirv, std::string& log_out) {
    const EShLanguage lang = shader_language(stage);
    glslang::TShader shader(lang);
    const char* text = source.c_str();
    shader.setStrings(&text, 1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    const EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
    if (!shader.parse(GetDefaultResources(), 460, false, messages)) {
        log_out = shader.getInfoLog();
        log_out += shader.getInfoDebugLog();
        return false;
    }
    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(messages)) {
        log_out = program.getInfoLog();
        log_out += program.getInfoDebugLog();
        return false;
    }
    program.mapIO();
    glslang::GlslangToSpv(*program.getIntermediate(lang), out_spirv);
    return !out_spirv.empty();
}

} // namespace vk
} // namespace poko
