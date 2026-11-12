#include "poko_test_harness.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "poko/assets/provider.h"
#include "poko/core/physics_contracts.h"
#include "poko/physics/world.h"
#include "poko/runtime/demo_scene.h"
#include "poko/runtime/game_runtime.h"

namespace {
using poko::NullAssetProvider;
using poko::NullPhysicsWorld;
using poko::PhysicsWorldDesc;
using poko::Status;
using poko::runtime::FrameContext;
using poko::runtime::GameRuntime;
using poko::runtime::RenderItem;
} // namespace

POKO_TEST(fixed_step_is_deterministic) {
    NullPhysicsWorld physics;
    NullAssetProvider assets;
    POKO_CHECK(physics.initialize(PhysicsWorldDesc{}));
    POKO_CHECK(assets.initialize());

    GameRuntime game(physics, assets);
    POKO_CHECK(game.run(60, 1.0 / 60.0).ok());
    POKO_CHECK_EQ(game.frame_index(), 60u);
    POKO_CHECK_EQ(game.simulation_step_count(), 60u);
    POKO_CHECK(game.simulation_time() > 0.99 && game.simulation_time() < 1.01);
    POKO_CHECK_EQ(game.last_frame().frame_index, 59u);
    POKO_CHECK_EQ(game.diagnostics().counter("runtime.frames"), 60);
    POKO_CHECK_EQ(game.diagnostics().counter("runtime.steps"), 60);
}

POKO_TEST(render_fn_receives_one_stable_context_per_frame) {
    NullPhysicsWorld physics;
    NullAssetProvider assets;
    POKO_CHECK(physics.initialize(PhysicsWorldDesc{}));
    POKO_CHECK(assets.initialize());

    GameRuntime game(physics, assets);
    game.set_render_items({RenderItem{0, 0, 1, 7}});
    std::vector<FrameContext> frames;
    game.set_render_fn([&frames](const FrameContext& ctx) -> Status {
        frames.push_back(ctx);
        return Status::success();
    });

    POKO_CHECK(game.run(3, 1.0 / 60.0).ok());
    POKO_CHECK_EQ(frames.size(), std::size_t{3});
    POKO_CHECK_EQ(frames[0].frame_index, 0u);
    POKO_CHECK_EQ(frames[1].frame_index, 1u);
    POKO_CHECK_EQ(frames[2].frame_index, 2u);
    POKO_CHECK_EQ(frames[0].render_items.size(), std::size_t{1});
    POKO_CHECK_EQ(frames[0].render_items[0].sort_key, 7u);
    // Camera pose is identical across frames — no hidden nondeterminism.
    POKO_CHECK_EQ(frames[0].camera_position.x, frames[1].camera_position.x);
    POKO_CHECK_EQ(frames[0].camera_target.z, frames[2].camera_target.z);
}

POKO_TEST(render_failure_propagates_and_does_not_commit_frame) {
    NullPhysicsWorld physics;
    NullAssetProvider assets;
    POKO_CHECK(physics.initialize(PhysicsWorldDesc{}));
    POKO_CHECK(assets.initialize());

    GameRuntime game(physics, assets);
    game.set_render_fn([](const FrameContext&) -> Status {
        return Status::fail(poko::ErrorCode::Internal, "render failed");
    });
    POKO_CHECK(!game.tick(1.0 / 60.0).ok());
    POKO_CHECK_EQ(game.frame_index(), 0u);
}

POKO_TEST(runs_simulation_only_without_render_fn) {
    NullPhysicsWorld physics;
    NullAssetProvider assets;
    POKO_CHECK(physics.initialize(PhysicsWorldDesc{}));
    POKO_CHECK(assets.initialize());

    GameRuntime game(physics, assets);
    POKO_CHECK(game.run(10, 1.0 / 60.0).ok());
    POKO_CHECK_EQ(game.frame_index(), 10u);
    POKO_CHECK_EQ(game.simulation_step_count(), 10u);
}

POKO_TEST(tick_rejects_invalid_frame_time) {
    NullPhysicsWorld physics;
    NullAssetProvider assets;
    POKO_CHECK(physics.initialize(PhysicsWorldDesc{}));
    POKO_CHECK(assets.initialize());

    GameRuntime game(physics, assets);
    POKO_CHECK(!game.tick(0.0).ok());
    POKO_CHECK(!game.tick(-1.0).ok());
    POKO_CHECK_EQ(game.frame_index(), 0u);
}

POKO_TEST(demo_scene_populates_every_subsystem) {
    NullPhysicsWorld physics;
    NullAssetProvider assets;
    POKO_CHECK(physics.initialize(PhysicsWorldDesc{}));
    POKO_CHECK(assets.initialize());

    GameRuntime game(physics, assets);
    POKO_CHECK(poko::runtime::build_demo_scene(game).ok());
    POKO_CHECK_EQ(game.lights().count(), 2u);
    POKO_CHECK_EQ(game.postprocess().count(), 2u);
    POKO_CHECK_EQ(game.scene().node_count(), 4u);
    POKO_CHECK_EQ(game.entities().entity_count(), 3u);
    POKO_CHECK_EQ(game.physics().body_count(), 1u);

    POKO_CHECK(game.run(1, 1.0 / 60.0).ok());
    POKO_CHECK_EQ(game.last_frame().render_items.size(), std::size_t{3});
    POKO_CHECK_EQ(game.last_frame().light_count, 2u);
    POKO_CHECK_EQ(game.last_frame().postprocess_count, 2u);
}

POKO_TEST_MAIN()
