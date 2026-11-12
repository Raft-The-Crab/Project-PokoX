#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "poko/assets/filesystem_provider.h"
#include "poko/audio/mixer_device.h"
#include "poko/core/api_seams.h"
#include "poko/core/asset_contracts.h"
#include "poko/core/audio_contracts.h"
#include "poko/core/net_contracts.h"
#include "poko/core/render_contracts.h"
#include "poko/graphics/draw_call.h"
#include "poko/graphics/material.h"
#include "poko/graphics/mesh.h"
#include "poko/network/channel_transport.h"
#include "poko/physics/custom_world.h"
#include "poko/platform/fs.h"
#include "poko/renderer/render_device.h"
#include "poko/renderer/renderer.h"
#include "poko/runtime/demo_scene.h"
#include "poko/runtime/engine.h"
#include "poko/runtime/game_runtime.h"

namespace poko::runtime {
namespace detail {

inline graphics::MeshDesc make_demo_mesh() {
    graphics::MeshDesc desc;
    // Three vec3 positions (12-byte stride) and a uint32 index triple.
    const float vertices[9] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f,
    };
    desc.vertices.resize(sizeof(vertices));
    std::memcpy(desc.vertices.data(), vertices, sizeof(vertices));
    desc.layout.attribute_count = 1;
    desc.layout.attributes[0] = {0, 3, 12, 0};
    desc.index_type = IndexType::UInt32;
    const std::uint32_t indices[3] = {0, 1, 2};
    desc.indices.resize(sizeof(indices));
    std::memcpy(desc.indices.data(), indices, sizeof(indices));
    return desc;
}

inline graphics::MaterialDesc make_demo_material(bool inject_failure) {
    graphics::MaterialDesc desc;
    // An empty shader source is rejected by MaterialLibrary::valid(); this is
    // the deterministic resource-failure injection used by the engine tests.
    desc.vertex_shader_source = inject_failure ? "" : "void main() {}";
    desc.fragment_shader_source = inject_failure ? "" : "void main() {}";
    desc.layout.attribute_count = 1;
    desc.layout.attributes[0] = {0, 3, 12, 0};

    graphics::TextureBinding binding;
    binding.texture.width = 2;
    binding.texture.height = 2;
    binding.texture.format = PixelFormat::RGBA8;
    binding.sampler.min_filter = FilterMode::Linear;
    desc.textures.push_back(binding);
    desc.uniform_data = std::vector<std::byte>(16, std::byte{0});
    return desc;
}

struct PkxHeader {
    char magic[4];
    std::uint32_t kind;
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
};

// Materializes the sample scene's mesh asset so the PRODUCTION
// FilesystemAssetProvider decodes it from a real file during the frame loop.
inline bool write_demo_asset(const std::filesystem::path& root) {
    std::error_code ec;
    std::filesystem::create_directories(root / "models", ec);
    if (ec) {
        return false;
    }
    PkxHeader header{};
    std::memcpy(header.magic, "PKX1", 4);
    header.kind = static_cast<std::uint32_t>(AssetKind::Mesh);
    header.a = 3; // vertex_count
    header.b = 3; // index_count
    header.c = 1; // submesh_count
    std::ofstream out(root / "models" / "demo.mesh", std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    const std::vector<char> payload(36, 0);
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    return out.good();
}

inline std::string backend_chain_string() {
    return std::string(to_string(preferred_backend())) + " -> " +
           to_string(fallback_backend()) + " -> " + to_string(last_resort_backend()) +
           " (last available selected offline)";
}

inline std::filesystem::path default_asset_root() {
    return std::filesystem::temp_directory_path() / "pokox_engine_assets";
}

} // namespace detail

// Private implementation, shared across engine.cpp / engine_backends.cpp /
// engine_render.cpp. Owning order matters: GameRuntime holds references to the
// physics and asset backends, and the graphics libraries hold a reference to
// Renderer::device(), so shutdown tears down in reverse.
struct Engine::Impl {
    EngineDesc desc{};
    bool initialized = false;

    std::unique_ptr<api::IRenderDevice> injected_device;

    std::unique_ptr<LocalFileSystem> filesystem;
    std::unique_ptr<CustomPhysicsWorld> physics;
    std::unique_ptr<FilesystemAssetProvider> assets;
    std::unique_ptr<MixerAudioDevice> audio;
    std::unique_ptr<ChannelNetworkTransport> network;

    std::unique_ptr<Renderer> renderer;
    NullRenderDevice* null_device = nullptr; // test hook, non-owning
    std::unique_ptr<graphics::MeshLibrary> meshes;
    std::unique_ptr<graphics::MaterialLibrary> materials;
    std::unique_ptr<graphics::DrawCallBuilder> draw_builder;

    std::unique_ptr<GameRuntime> runtime;

    std::vector<graphics::MeshHandle> mesh_handles;
    std::vector<graphics::MaterialHandle> material_handles;
    std::vector<float> audio_buffer;
    SoundHandle ambient_sound{};
    EndpointId listen_endpoint{};
    EndpointId client_endpoint{};
    std::array<std::byte, 4> state_packet{};

    std::uint32_t device_recoveries = 0;
    std::uint32_t resource_failures = 0;
    std::uint32_t skipped_draw_items = 0;
    std::string active_backend = "unknown";
    std::string backend_chain = "unknown";

    Status setup_backends();
    Status select_render_device();
    Status upload_resources();
    Status render_frame(const FrameContext& ctx);
};

} // namespace poko::runtime
