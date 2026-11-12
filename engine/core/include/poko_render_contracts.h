#pragma once

// Renderer contracts (systemplan/01 dependency-map seam). These are POD-only
// descriptors and opaque handles in engine core so the seam can be declared
// without any graphics vendor dependency. Concrete backends live in
// engine/renderer/. See DEC-003.

#include <array>
#include <cstdint>
#include <string_view>

#include "poko_types.h"

namespace poko {

// GPU objects are referred to by generation-checked opaque handles.
using RenderHandle = TypedHandle;

enum class GraphicsBackend : std::uint8_t {
    Unknown = 0,
    Vulkan,
    OpenGL,
    Null, // MOCK / headless
};

inline constexpr const char* to_string(GraphicsBackend backend) noexcept {
    switch (backend) {
    case GraphicsBackend::Vulkan:
        return "vulkan";
    case GraphicsBackend::OpenGL:
        return "opengl";
    case GraphicsBackend::Null:
        return "null";
    case GraphicsBackend::Unknown:
        return "unknown";
    }
    return "unknown";
}

enum class PixelFormat : std::uint8_t {
    Unknown = 0,
    RGBA8,
    BGRA8,
    RGB8,
    R8,
    RGBA16F,
    RGBA32F,
    Depth24Stencil8,
    Depth32F,
};

enum class BufferUsage : std::uint8_t {
    Vertex,
    Index,
    Uniform,
    Storage,
    Staging,
};

enum class ShaderStage : std::uint8_t {
    Vertex,
    Fragment,
    Compute,
};

enum class PrimitiveTopology : std::uint8_t {
    Triangles,
    TriangleStrip,
    Lines,
    Points,
};

enum class CullMode : std::uint8_t {
    None,
    Front,
    Back,
};

enum class CompareOp : std::uint8_t {
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always,
};

struct BufferDesc {
    std::uint64_t size_bytes = 0;
    BufferUsage usage = BufferUsage::Vertex;
    bool host_visible = false;
    std::string_view debug_name;
};

struct TextureDesc {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t depth = 1;
    std::uint32_t mip_levels = 1;
    std::uint32_t array_layers = 1;
    PixelFormat format = PixelFormat::RGBA8;
    std::string_view debug_name;
};

struct ShaderDesc {
    ShaderStage stage = ShaderStage::Vertex;
    std::string_view source;
    std::string_view entry_point = "main";
    std::string_view debug_name;
};

struct VertexAttribute {
    std::uint32_t location = 0;
    std::uint32_t components = 3; // 1..4
    std::uint32_t stride_bytes = 0;
    std::uint32_t offset_bytes = 0;
};

inline constexpr std::size_t kMaxVertexAttributes = 8;

struct VertexLayout {
    std::array<VertexAttribute, kMaxVertexAttributes> attributes{};
    std::uint32_t attribute_count = 0;
    PrimitiveTopology topology = PrimitiveTopology::Triangles;
};

struct PipelineDesc {
    RenderHandle vertex_shader;
    RenderHandle fragment_shader;
    VertexLayout layout;
    CullMode cull_mode = CullMode::Back;
    CompareOp depth_compare = CompareOp::LessOrEqual;
    bool depth_test = true;
    bool depth_write = true;
    PixelFormat color_format = PixelFormat::RGBA8;
    PixelFormat depth_format = PixelFormat::Depth24Stencil8;
    std::string_view debug_name;
};

struct ClearValue {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
    float depth = 1.0f;
    std::uint8_t stencil = 0;
};

struct Viewport {
    float x = 0.0f;
    float y = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
    float min_depth = 0.0f;
    float max_depth = 1.0f;
};

struct Rect2D {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct DrawCall {
    RenderHandle pipeline;
    RenderHandle vertex_buffer;
    RenderHandle index_buffer;
    // Optional uniform buffer (BufferUsage::Uniform) bound for this draw.
    // Shaders consume it through a std140 uniform block; an invalid handle
    // means no uniforms are bound.
    RenderHandle uniform_buffer;
    std::uint64_t uniform_offset = 0;
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
    std::uint32_t instance_count = 1;
    std::uint32_t sort_key = 0;
};

struct DeviceCapabilities {
    GraphicsBackend backend = GraphicsBackend::Unknown;
    const char* device_name = "unknown";
    std::uint32_t max_texture_size = 0;
    std::uint64_t max_buffer_bytes = 0;
    bool supports_compute = false;
    bool supports_wireframe = false;
    bool supports_shader_reload = false;
};

// Input validation at the module boundary. Backends must reject invalid
// descriptors before allocating anything (systemplan/04 step 10).
inline bool valid(const BufferDesc& desc) noexcept {
    return desc.size_bytes > 0;
}

inline bool valid(const TextureDesc& desc) noexcept {
    return desc.width > 0 && desc.height > 0 && desc.depth > 0 && desc.mip_levels > 0 &&
           desc.array_layers > 0 && desc.format != PixelFormat::Unknown;
}

inline bool valid(const ShaderDesc& desc) noexcept {
    return !desc.source.empty() && !desc.entry_point.empty();
}

inline bool valid(const VertexLayout& layout) noexcept {
    if (layout.attribute_count == 0 || layout.attribute_count > kMaxVertexAttributes) {
        return false;
    }
    for (std::uint32_t i = 0; i < layout.attribute_count; ++i) {
        const auto& attribute = layout.attributes[i];
        if (attribute.components == 0 || attribute.components > 4) {
            return false;
        }
    }
    return true;
}

inline bool valid(const DrawCall& call) noexcept {
    return is_valid(call.pipeline) && call.instance_count > 0 &&
           (call.vertex_count > 0 || call.index_count > 0);
}

} // namespace poko
