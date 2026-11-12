#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "poko/core/api_seams.h"
#include "poko/core/render_contracts.h"
#include "poko/core/result.h"
#include "poko/core/types.h"

namespace poko::graphics {

using MaterialHandle = TypedHandle;

// One texture slot and the sampler that reads it. DrawCall pairs textures[i]
// with samplers[i], so a binding is always a (texture, sampler) pair.
struct TextureBinding {
    TextureDesc texture{};
    SamplerDesc sampler{};
};

// CPU-side material authored by game code: shader sources, pipeline state,
// texture bindings and optional uniform block data.
struct MaterialDesc {
    std::string vertex_shader_source;
    std::string fragment_shader_source;
    VertexLayout layout{};
    CullMode cull_mode = CullMode::Back;
    CompareOp depth_compare = CompareOp::LessOrEqual;
    bool depth_test = true;
    bool depth_write = true;
    std::vector<TextureBinding> textures; // <= DrawCall::kMaxDrawTextures
    std::vector<std::byte> uniform_data;  // empty => no uniform buffer
};

// Device-resident material: the pipeline, bound textures/samplers and optional
// uniform buffer consumed when building a DrawCall. The vertex layout is kept
// so a DrawCallBuilder can verify mesh/material compatibility.
struct GpuMaterial {
    RenderHandle pipeline;
    RenderHandle uniform_buffer; // invalid when no uniforms
    std::array<RenderHandle, DrawCall::kMaxDrawTextures> textures{};
    std::array<RenderHandle, DrawCall::kMaxDrawTextures> samplers{};
    std::uint32_t texture_count = 0;
    VertexLayout layout{};
};

inline bool valid(const MaterialDesc& desc) noexcept {
    if (desc.vertex_shader_source.empty() || desc.fragment_shader_source.empty()) {
        return false;
    }
    if (!::poko::valid(desc.layout)) {
        return false;
    }
    if (desc.textures.size() > DrawCall::kMaxDrawTextures) {
        return false;
    }
    for (const auto& binding : desc.textures) {
        if (!::poko::valid(binding.texture)) {
            return false;
        }
    }
    return true;
}

// Owns GPU material resources created through an injected IRenderDevice.
// All device handles (shaders, pipeline, textures, samplers, uniform buffer)
// are retained for the material's lifetime and released together.
// Thread-safety contract: synchronised; may be used from any thread.
class MaterialLibrary {
public:
    explicit MaterialLibrary(api::IRenderDevice& device) noexcept : device_(device) {}

    Result<MaterialHandle> create(const MaterialDesc& desc);
    Result<GpuMaterial> get(MaterialHandle handle) const;
    Status destroy(MaterialHandle handle);
    std::uint32_t count() const noexcept;

private:
    struct Slot {
        bool alive = false;
        GpuMaterial material{};
        std::vector<RenderHandle> owned;
    };

    bool live(MaterialHandle handle) const noexcept; // caller holds mutex_

    api::IRenderDevice& device_;
    mutable std::mutex mutex_;
    std::vector<Slot> slots_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
};

} // namespace poko::graphics
