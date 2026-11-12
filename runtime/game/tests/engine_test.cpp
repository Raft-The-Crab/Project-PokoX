#include "poko_test_harness.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "poko/assets/filesystem_provider.h"
#include "poko/audio/mixer_device.h"
#include "poko/core/ownership.h"
#include "poko/graphics/material.h"
#include "poko/graphics/mesh.h"
#include "poko/network/channel_transport.h"
#include "poko/physics/custom_world.h"
#include "poko/renderer/render_device.h"
#include "poko/runtime/engine.h"
#include "poko/runtime/game_runtime.h"

namespace {
using poko::runtime::Engine;
using poko::runtime::EngineDesc;
using poko::runtime::RenderBackend;

std::filesystem::path temp_root(const char* tag) {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("pokox_engine_" + std::string(tag));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

EngineDesc desc_for(const std::filesystem::path& root) {
    EngineDesc desc;
    desc.asset_root = root;
    return desc;
}
} // namespace

POKO_TEST(initialize_validation_lifecycle_and_backend_chain) {
    const auto root = temp_root("init");
    poko::ScopeExit cleanup([root] { std::filesystem::remove_all(root); });

    Engine engine;
    POKO_CHECK(!engine.initialized());
    POKO_CHECK_EQ(std::string(engine.active_backend_name()), std::string("unknown"));

    EngineDesc bad = desc_for(root);
    bad.fixed_step_seconds = 0.0;
    POKO_CHECK(!engine.initialize(bad).ok());
    EngineDesc no_steps = desc_for(root);
    no_steps.max_steps_per_frame = 0;
    POKO_CHECK(!engine.initialize(no_steps).ok());

    POKO_CHECK(engine.initialize(desc_for(root)).ok());
    POKO_CHECK(engine.initialized());
    POKO_CHECK(!engine.initialize(desc_for(root)).ok()); // AlreadyExists
    POKO_CHECK_EQ(std::string(engine.active_backend_name()), std::string("null"));

    const auto stats = engine.stats();
    POKO_CHECK(stats.backend_chain.find("vulkan") != std::string::npos);
    POKO_CHECK(stats.backend_chain.find("opengl") != std::string::npos);
    POKO_CHECK(stats.backend_chain.find("null") != std::string::npos);

    POKO_CHECK(engine.shutdown().ok());
    POKO_CHECK(!engine.initialized());
    POKO_CHECK(engine.shutdown().ok()); // idempotent
    POKO_CHECK(engine.initialize(desc_for(root)).ok()); // re-initializable
    POKO_CHECK(engine.shutdown().ok());
}

POKO_TEST(production_backends_drive_a_real_frame) {
    const auto root = temp_root("frame");
    poko::ScopeExit cleanup([root] { std::filesystem::remove_all(root); });

    Engine engine;
    POKO_CHECK(engine.initialize(desc_for(root)).ok());

    // Every subsystem is the PRODUCTION implementation; build_demo_scene()
    // already created its physics body during initialize().
    POKO_CHECK_EQ(engine.physics().body_count(), 1u);
    POKO_CHECK_EQ(engine.assets().cache_size(), 0u);
    POKO_CHECK(engine.audio().stats().active_voices > 0u);
    POKO_CHECK_EQ(engine.network().endpoint_count(), 2u);
    POKO_CHECK_EQ(engine.meshes().count(), 1u);
    POKO_CHECK_EQ(engine.materials().count(), 1u);
    POKO_CHECK_EQ(engine.runtime().scene().node_count(), 4u);

    POKO_CHECK(engine.run(60, 1.0 / 60.0).ok());
    const auto stats = engine.stats();
    POKO_CHECK_EQ(stats.frames, std::uint64_t{60});
    POKO_CHECK_EQ(stats.simulation_steps, std::uint64_t{60});
    POKO_CHECK_EQ(stats.draw_calls, 2u); // batching merged the first two items
    POKO_CHECK_EQ(stats.triangles, std::uint64_t{4});
    POKO_CHECK_EQ(stats.physics_bodies, 1u);
    POKO_CHECK_EQ(stats.asset_cache_size, 1u); // real PKX1 file decoded from disk
    POKO_CHECK(stats.audio_active_voices > 0u);
    POKO_CHECK_EQ(stats.transport_framed, 60u); // one framed packet per tick
    POKO_CHECK_EQ(stats.resource_failures, 0u);
    POKO_CHECK_EQ(stats.skipped_draw_items, 0u);
    POKO_CHECK_EQ(stats.device_recoveries, 0u);
    POKO_CHECK(engine.shutdown().ok());
}

POKO_TEST(device_loss_is_detected_and_recovered_automatically) {
    const auto root = temp_root("loss");
    poko::ScopeExit cleanup([root] { std::filesystem::remove_all(root); });

    Engine engine;
    POKO_CHECK(engine.initialize(desc_for(root)).ok());
    POKO_CHECK(!engine.recover_device().ok()); // not lost

    engine.simulate_device_loss();
    POKO_CHECK(engine.tick(1.0 / 60.0).ok());
    const auto stats = engine.stats();
    POKO_CHECK_EQ(stats.device_recoveries, 1u);
    POKO_CHECK_EQ(stats.frames, std::uint64_t{1});
    POKO_CHECK_EQ(stats.draw_calls, 2u); // frame still rendered after recovery
    POKO_CHECK(engine.shutdown().ok());
}

POKO_TEST(resource_failure_degrades_instead_of_killing_the_loop) {
    const auto root = temp_root("failure");
    poko::ScopeExit cleanup([root] { std::filesystem::remove_all(root); });

    EngineDesc desc = desc_for(root);
    desc.inject_resource_failure = true;
    Engine engine;
    POKO_CHECK(engine.initialize(desc).ok());
    POKO_CHECK(engine.stats().resource_failures > 0u);

    POKO_CHECK(engine.run(5, 1.0 / 60.0).ok());
    const auto stats = engine.stats();
    POKO_CHECK_EQ(stats.frames, std::uint64_t{5});
    POKO_CHECK_EQ(stats.skipped_draw_items, 15u); // 3 items x 5 frames
    POKO_CHECK_EQ(stats.draw_calls, 0u);
    POKO_CHECK(engine.shutdown().ok());
}

POKO_TEST(explicit_gpu_backend_requires_an_injected_device) {
    const auto root = temp_root("inject");
    poko::ScopeExit cleanup([root] { std::filesystem::remove_all(root); });

    EngineDesc desc = desc_for(root);
    desc.render_backend = RenderBackend::Vulkan;
    Engine engine;
    const auto rejected = engine.initialize(desc);
    POKO_CHECK(!rejected.ok());
    POKO_CHECK(rejected.code() == poko::ErrorCode::Unsupported);

    engine.set_render_device(std::make_unique<poko::NullRenderDevice>());
    POKO_CHECK(engine.initialize(desc).ok()); // injected device wins
    POKO_CHECK_EQ(std::string(engine.active_backend_name()), std::string("null"));
    POKO_CHECK(engine.shutdown().ok());
}

POKO_TEST_MAIN()
