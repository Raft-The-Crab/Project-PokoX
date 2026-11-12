#include "poko_test_harness.h"

#include "poko_physics_world.h"

POKO_TEST(world_requires_initialize) {
    poko::NullPhysicsWorld world;
    poko::BodyDesc desc;
    POKO_CHECK(!world.create_body(desc).ok());
    POKO_CHECK(world.initialize(poko::PhysicsWorldDesc{}));
    POKO_CHECK(world.create_body(desc).ok());
}

POKO_TEST(initialize_rejects_bad_desc) {
    poko::NullPhysicsWorld world;
    auto bad = poko::PhysicsWorldDesc{};
    bad.fixed_delta_seconds = 0.0f;
    POKO_CHECK(!world.initialize(bad));
}

POKO_TEST(body_lifecycle_and_state) {
    poko::NullPhysicsWorld world;
    POKO_CHECK(world.initialize(poko::PhysicsWorldDesc{}));

    poko::BodyDesc desc;
    desc.type = poko::BodyType::Static;
    desc.position = {0.0f, 5.0f, 0.0f};
    desc.shape.radius = 1.0f;
    const auto created = world.create_body(desc);
    POKO_CHECK(created.ok());
    POKO_CHECK_EQ(world.body_count(), 1u);

    const auto state = world.get_body_state(created.value());
    POKO_CHECK(state.ok());
    POKO_CHECK_EQ(state.value().position.y, 5.0f);

    poko::BodyState moved = state.value();
    moved.position = {1.0f, 2.0f, 3.0f};
    POKO_CHECK(world.set_body_state(created.value(), moved).ok());
    const auto after = world.get_body_state(created.value());
    POKO_CHECK(after.ok());
    POKO_CHECK_EQ(after.value().position.x, 1.0f);

    POKO_CHECK(world.destroy_body(created.value()).ok());
    POKO_CHECK_EQ(world.body_count(), 0u);
    POKO_CHECK(!world.get_body_state(created.value()).ok());
    POKO_CHECK(!world.destroy_body(created.value()).ok());
}

POKO_TEST(step_integrates_dynamic_bodies_only) {
    poko::NullPhysicsWorld world;
    auto desc = poko::PhysicsWorldDesc{};
    desc.gravity = {0.0f, -10.0f, 0.0f};
    POKO_CHECK(world.initialize(desc));

    poko::BodyDesc dyn;
    dyn.type = poko::BodyType::Dynamic;
    dyn.velocity = {0.0f, 0.0f, 2.0f};
    const auto body = world.create_body(dyn);
    POKO_CHECK(body.ok());

    world.step(0.5f);
    const auto state = world.get_body_state(body.value());
    POKO_CHECK(state.ok());
    // y: 0 + (-10 * 0.5) * 0.5 = -2.5 (semi-implicit Euler)
    POKO_CHECK_EQ(state.value().position.y, -2.5f);
    POKO_CHECK_EQ(state.value().velocity.y, -5.0f);
    POKO_CHECK_EQ(state.value().position.z, 1.0f);
}

POKO_TEST(raycast_hits_sphere_and_respects_mask) {
    poko::NullPhysicsWorld world;
    POKO_CHECK(world.initialize(poko::PhysicsWorldDesc{}));

    poko::BodyDesc desc;
    desc.type = poko::BodyType::Static;
    desc.position = {0.0f, 0.0f, -5.0f};
    desc.shape.radius = 1.0f;
    desc.collision_layer = 2;
    const auto body = world.create_body(desc);
    POKO_CHECK(body.ok());

    poko::Ray ray{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};
    poko::RayHit hit{};
    POKO_CHECK(world.raycast(ray, 100.0f, 0xFFFFFFFFu, hit));
    POKO_CHECK_EQ(hit.distance, 4.0f);
    POKO_CHECK_EQ(hit.normal.z, 1.0f);

    // Mask that excludes layer 2 misses.
    POKO_CHECK(!world.raycast(ray, 100.0f, 1u, hit));
}

POKO_TEST(raycast_hits_axis_aligned_box) {
    poko::NullPhysicsWorld world;
    POKO_CHECK(world.initialize(poko::PhysicsWorldDesc{}));

    poko::BodyDesc desc;
    desc.type = poko::BodyType::Static;
    desc.position = {0.0f, 0.0f, -4.0f};
    desc.shape.kind = poko::ShapeKind::Box;
    desc.shape.half_extents = {1.0f, 1.0f, 1.0f};
    POKO_CHECK(world.create_body(desc).ok());

    poko::RayHit hit{};
    const poko::Ray ray{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};
    POKO_CHECK(world.raycast(ray, 100.0f, 0xFFFFFFFFu, hit));
    POKO_CHECK_EQ(hit.distance, 3.0f);
    POKO_CHECK_EQ(hit.normal.z, 1.0f);
}

POKO_TEST_MAIN()
