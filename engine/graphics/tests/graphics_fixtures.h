#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "poko/graphics/material.h"
#include "poko/graphics/mesh.h"

namespace poko::graphics::test {

inline MeshDesc make_mesh(bool with_indices = true) {
    MeshDesc desc;
    desc.vertices = std::vector<std::byte>(36, std::byte{0}); // 3 vertices x 12 bytes
    desc.layout.attribute_count = 1;
    desc.layout.attributes[0] = {0, 3, 12, 0};               // vec3, stride 12
    desc.index_type = IndexType::UInt32;
    if (with_indices) {
        desc.indices = std::vector<std::byte>(12, std::byte{0}); // 3 x uint32
    }
    return desc;
}

inline MaterialDesc make_material(std::uint32_t texture_count = 0) {
    MaterialDesc desc;
    desc.vertex_shader_source = "void main() {}";
    desc.fragment_shader_source = "void main() {}";
    desc.layout.attribute_count = 1;
    desc.layout.attributes[0] = {0, 3, 12, 0};
    for (std::uint32_t i = 0; i < texture_count; ++i) {
        TextureBinding binding;
        binding.texture.width = 4;
        binding.texture.height = 4;
        binding.texture.format = PixelFormat::RGBA8;
        binding.sampler.min_filter = FilterMode::Nearest;
        desc.textures.push_back(binding);
    }
    return desc;
}

} // namespace poko::graphics::test
