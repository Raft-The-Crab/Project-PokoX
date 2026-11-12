#pragma once

// Internal GL enum/format conversions shared by the backend's translation
// units. Not part of the public API.

#include <cstdint>
#include <string>

#include <GL/gl3w.h>

// windows.h (pulled in by glcorearb.h) #defines DeviceCapabilities to the
// Win32 DEVICE_CAPABILITIES struct name; it collides with poko's type.
#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif

namespace poko {
namespace gl {

inline std::uint32_t buffer_target(BufferUsage usage) noexcept {
    switch (usage) {
    case BufferUsage::Vertex:
        return GL_ARRAY_BUFFER;
    case BufferUsage::Index:
        return GL_ELEMENT_ARRAY_BUFFER;
    case BufferUsage::Uniform:
        return GL_UNIFORM_BUFFER;
    case BufferUsage::Storage:
        return GL_SHADER_STORAGE_BUFFER;
    case BufferUsage::Staging:
        return GL_PIXEL_UNPACK_BUFFER;
    }
    return GL_ARRAY_BUFFER;
}

inline std::uint32_t buffer_usage_hint(BufferUsage usage) noexcept {
    switch (usage) {
    case BufferUsage::Staging:
        return GL_STREAM_DRAW;
    default:
        return GL_DYNAMIC_DRAW;
    }
}

inline std::uint32_t shader_stage(ShaderStage stage) noexcept {
    switch (stage) {
    case ShaderStage::Vertex:
        return GL_VERTEX_SHADER;
    case ShaderStage::Fragment:
        return GL_FRAGMENT_SHADER;
    case ShaderStage::Compute:
        return GL_COMPUTE_SHADER;
    }
    return GL_VERTEX_SHADER;
}

inline std::uint32_t primitive_mode(PrimitiveTopology topology) noexcept {
    switch (topology) {
    case PrimitiveTopology::Triangles:
        return GL_TRIANGLES;
    case PrimitiveTopology::TriangleStrip:
        return GL_TRIANGLE_STRIP;
    case PrimitiveTopology::Lines:
        return GL_LINES;
    case PrimitiveTopology::Points:
        return GL_POINTS;
    }
    return GL_TRIANGLES;
}

inline std::uint32_t filter_mode(FilterMode mode) noexcept {
    return mode == FilterMode::Nearest ? GL_NEAREST : GL_LINEAR;
}

inline std::uint32_t wrap_mode(WrapMode mode) noexcept {
    switch (mode) {
    case WrapMode::Repeat:
        return GL_REPEAT;
    case WrapMode::ClampToEdge:
        return GL_CLAMP_TO_EDGE;
    case WrapMode::MirroredRepeat:
        return GL_MIRRORED_REPEAT;
    }
    return GL_REPEAT;
}

inline std::uint32_t compare_op(CompareOp op) noexcept {
    switch (op) {
    case CompareOp::Never:
        return GL_NEVER;
    case CompareOp::Less:
        return GL_LESS;
    case CompareOp::Equal:
        return GL_EQUAL;
    case CompareOp::LessOrEqual:
        return GL_LEQUAL;
    case CompareOp::Greater:
        return GL_GREATER;
    case CompareOp::NotEqual:
        return GL_NOTEQUAL;
    case CompareOp::GreaterOrEqual:
        return GL_GEQUAL;
    case CompareOp::Always:
        return GL_ALWAYS;
    }
    return GL_LEQUAL;
}

inline std::uint32_t cull_face(CullMode mode) noexcept {
    switch (mode) {
    case CullMode::None:
        return 0;
    case CullMode::Front:
        return GL_FRONT;
    case CullMode::Back:
        return GL_BACK;
    }
    return GL_BACK;
}

// Vertex attributes are 32-bit float tuples; the seam carries no element-type
// field yet (noted in DEC-004).
inline constexpr std::uint32_t kAttributeType = GL_FLOAT;

struct GlFormat {
    std::uint32_t internal_format;
    std::uint32_t format;
    std::uint32_t type;
    std::uint32_t bytes_per_pixel;
};

inline std::uint32_t mip_dimension(std::uint32_t base, std::uint32_t level) noexcept {
    if (level >= 32) {
        return 1;
    }
    const std::uint32_t dim = base >> level;
    return dim == 0 ? 1 : dim;
}

inline bool pixel_format(PixelFormat format, GlFormat& out) noexcept {
    switch (format) {
    case PixelFormat::RGBA8:
        out = {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4};
        return true;
    case PixelFormat::BGRA8:
        out = {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, 4};
        return true;
    case PixelFormat::RGB8:
        out = {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 3};
        return true;
    case PixelFormat::R8:
        out = {GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1};
        return true;
    case PixelFormat::RGBA16F:
        out = {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8};
        return true;
    case PixelFormat::RGBA32F:
        out = {GL_RGBA32F, GL_RGBA, GL_FLOAT, 16};
        return true;
    case PixelFormat::Depth24Stencil8:
        out = {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4};
        return true;
    case PixelFormat::Depth32F:
        out = {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 4};
        return true;
    case PixelFormat::Unknown:
        break;
    }
    return false;
}

inline const char* gl_string(std::uint32_t name) {
    const auto* value = glGetString(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : "";
}

} // namespace gl
} // namespace poko
