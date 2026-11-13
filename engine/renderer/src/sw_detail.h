#pragma once

#include <string>
#include <vector>

#include "poko/renderer/software_device.h"

namespace poko {

enum class SwKind : std::uint8_t { Buffer, Texture, Shader, Pipeline, Sampler };
enum class SwProgram : std::uint8_t { Unlit, VertexColor, Lambert, Textured };

struct SoftwareRenderDevice::Slot {
    SwKind kind = SwKind::Buffer;
    bool alive = false;
    std::vector<std::byte> bytes; // buffer contents or texture level 0
    TextureDesc texture{};
    SamplerDesc sampler{};
    ShaderStage stage = ShaderStage::Vertex;
    std::string source;
    PipelineDesc pipeline{};
    SwProgram program = SwProgram::Unlit;
};

namespace swdetail {

inline std::uint32_t bytes_per_pixel(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::R8:
        return 1;
    case PixelFormat::RGB8:
        return 3;
    case PixelFormat::RGBA8:
    case PixelFormat::BGRA8:
    case PixelFormat::Depth24Stencil8:
    case PixelFormat::Depth32F:
        return 4;
    case PixelFormat::RGBA16F:
        return 8;
    case PixelFormat::RGBA32F:
        return 16;
    case PixelFormat::Unknown:
        return 0;
    }
    return 0;
}

} // namespace swdetail
} // namespace poko
