#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <vector>

#include "poko/assets/provider.h"
#include "poko/core/api_seams.h"
#include "poko/core/physics_contracts.h"
#include "poko/core/render_contracts.h"
#include "poko/graphics/draw_call.h"
#include "poko/graphics/material.h"
#include "poko/graphics/mesh.h"
#include "poko/physics/world.h"
#include "poko/renderer/render_device.h"
#include "poko/renderer/renderer.h"
#include "poko/runtime/demo_scene.h"
#include "poko/runtime/game_runtime.h"

namespace {

constexpr std::uint32_t kFrames = 60;
constexpr double kFrameSeconds = 1.0 / 60.0;

poko::graphics::MeshDesc make_demo_mesh() {
    poko::graphics::MeshDesc desc;
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
    desc.index_type = poko::IndexType::UInt32;
    const std::uint32_t indices[3] = {0, 1, 2};
    desc.indices.resize(sizeof(indices));
    std::memcpy(desc.indices.data(), indices, sizeof(indices));
    return desc;
}

poko::graphics::MaterialDesc make_demo_material() {
    poko::graphics::MaterialDesc desc;
    desc.vertex_shader_source = "void main() {}";
    desc.fragment_shader_source = "void main() {}";
    desc.layout.attribute_count = 1;
    desc.layout.attributes[0] = {0, 3, 12, 0};

    poko::graphics::TextureBinding binding;
    binding.texture.width = 2;
    binding.texture.height = 2;
    binding.texture.format = poko::PixelFormat::RGBA8;
    binding.sampler.min_filter = poko::FilterMode::Linear;
    desc.textures.push_back(binding);

    desc.uniform_data = std::vector<std::byte>(16, std::byte{0});
    return desc;
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "integrated runtime demo: " << message << '\n';
    }
    return condition;
}

int run_demo() {
    auto device = std::make_unique<poko::NullRenderDevice>();
    poko::NullRenderDevice* null_device = device.get();
    poko::Renderer renderer(std::move(device));
    poko::api::IRenderDevice& gpu = renderer.device();

    // The graphics layer is built directly on the renderer's device seam.
    poko::graphics::MeshLibrary meshes(gpu);
    poko::graphics::MaterialLibrary materials(gpu);
    poko::graphics::DrawCallBuilder builder(meshes, materials);

    auto mesh = meshes.create(make_demo_mesh());
    auto material = materials.create(make_demo_material());
    if (!mesh.ok() || !material.ok()) {
        std::cerr << "integrated runtime demo: failed to upload demo resources\n";
        return 1;
    }
    const std::vector<poko::graphics::MeshHandle> mesh_handles{mesh.value()};
    const std::vector<poko::graphics::MaterialHandle> material_handles{material.value()};

    poko::NullPhysicsWorld physics;
    poko::NullAssetProvider assets;
    if (!physics.initialize(poko::PhysicsWorldDesc{}) || !assets.initialize()) {
        std::cerr << "integrated runtime demo: failed to initialize MOCK backends\n";
        return 1;
    }

    poko::runtime::GameRuntime game(physics, assets);
    if (!poko::runtime::build_demo_scene(game).ok()) {
        std::cerr << "integrated runtime demo: failed to build demo scene\n";
        return 1;
    }

    renderer.camera().set_perspective(poko::math::kHalfPi, 16.0f / 9.0f, 0.1f, 100.0f);

    // The render step translates the runtime's FrameContext into real GPU work:
    // camera -> graphics DrawCalls -> SceneSubmission -> Renderer::render_frame.
    game.set_render_fn([&](const poko::runtime::FrameContext& ctx) -> poko::Status {
        renderer.camera().set_position(ctx.camera_position);
        renderer.camera().set_target(ctx.camera_target);
        renderer.camera().set_up(ctx.camera_up);

        renderer.submission().clear();
        for (const auto& item : ctx.render_items) {
            if (item.mesh_id >= mesh_handles.size() ||
                item.material_id >= material_handles.size()) {
                return poko::Status::fail(poko::ErrorCode::OutOfRange,
                                          "render item references a missing resource");
            }
            poko::graphics::DrawOptions options;
            options.instance_count = item.instance_count;
            options.sort_key = item.sort_key;
            auto call = builder.build(mesh_handles[item.mesh_id],
                                      material_handles[item.material_id], options);
            if (!call.ok()) {
                return poko::Status::fail(call.error().code, call.error().message);
            }
            renderer.submission().submit(call.value());
        }
        renderer.submission().batch();
        return renderer.render_frame(poko::ClearValue{0.08f, 0.09f, 0.12f, 1.0f});
    });

    if (!game.run(kFrames, kFrameSeconds).ok()) {
        std::cerr << "integrated runtime demo: frame loop failed\n";
        return 1;
    }

    std::cout << "integrated runtime demo: frames=" << game.frame_index()
              << " sim_steps=" << game.simulation_step_count()
              << " draws_per_frame=" << renderer.stats().draw_calls()
              << " triangles_per_frame=" << renderer.stats().triangles()
              << " recorded_draws=" << null_device->recorded_draws().size()
              << " gpu_resources=" << null_device->resource_count()
              << " lights=" << game.lights().count()
              << " postprocess=" << game.postprocess().count()
              << " nodes=" << game.scene().node_count()
              << " entities=" << game.entities().entity_count()
              << " physics_bodies=" << physics.body_count() << '\n';

    // Deterministic end-to-end assertions: the loop really drove every layer.
    bool ok = true;
    ok &= expect(game.frame_index() == kFrames, "frame count mismatch");
    ok &= expect(game.simulation_step_count() == kFrames, "simulation step count mismatch");
    ok &= expect(renderer.stats().frame_index() == kFrames, "renderer frame count mismatch");
    ok &= expect(renderer.stats().draw_calls() == 2, "batched draw count mismatch");
    ok &= expect(null_device->frame_count() == kFrames, "device frame count mismatch");
    ok &= expect(null_device->recorded_draws().size() ==
                     static_cast<std::size_t>(kFrames) * 2u,
                 "recorded draw count mismatch");
    ok &= expect(null_device->resource_count() == 8, "gpu resource count mismatch");
    ok &= expect(null_device->buffer_update_count() == 3, "buffer upload count mismatch");
    ok &= expect(game.lights().count() == 2, "light count mismatch");
    ok &= expect(game.postprocess().count() == 2, "post-process count mismatch");
    ok &= expect(game.scene().node_count() == 4, "scene node count mismatch");
    ok &= expect(game.entities().entity_count() == 3, "entity count mismatch");
    ok &= expect(physics.body_count() == 1, "physics body count mismatch");

    std::cout << "integrated runtime demo: " << (ok ? "OK" : "FAILED") << '\n';
    return ok ? 0 : 1;
}

} // namespace

int main() {
    try {
        return run_demo();
    } catch (const std::exception& error) {
        std::cerr << "integrated runtime demo: exception: " << error.what() << '\n';
        return 1;
    }
}
