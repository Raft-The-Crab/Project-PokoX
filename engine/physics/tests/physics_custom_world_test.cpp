#include "poko_test_harness.h"

#include <cmath>
#include <cstdint>

#include "poko/core/physics_contracts.h"
#include "poko/physics/custom_world.h"

namespace {
using poko::BodyDesc;
using poko::BodyState;
using poko::BodyType;
using poko::CustomPhysicsWorld;
using poko::PhysicsWorldDesc;
using poko::Ray;
using poko::RayHit;
using poko::ShapeKind;

BodyDesc make_sphere(float x, float y, float z, float radius = 0.5f) {
    BodyDesc desc;
    desc.type = BodyType::Dynamic;
    desc.shape.kind = ShapeKind::Sphere;
    desc.shape.radius = radius;
    desc.position = {x, y, z};
    desc.mass = 1.0f;
    desc.friction = 0.5f;
    return desc;
}
} // namespace

POKO_TEST(initialize_validates_descriptor_and_is_idempotent) {
    CustomPhysicsWorld world;
    PhysicsWorldDesc bad;
    bad.fixed_delta_seconds = 0.0f;
    POKO_CHECK(!world.initialize(bad));
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    POKO_CHECK(world.initialize(PhysicsWorldDesc{})); // idempotent
}

POKO_TEST(create_destroy_and_generation_checked_handles) {
    CustomPhysicsWorld world;
    POKO_CHECK(!world.create_body(make_sphere(0, 0, 0)).ok()); // not initialized
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    BodyDesc invalid = make_sphere(0, 0, 0);
    invalid.mass = 0.0f; // invalid descriptor
    POKO_CHECK(!world.create_body(invalid).ok());

    auto first = world.create_body(make_sphere(0, 2, 0));
    POKO_CHECK(first.ok());
    POKO_CHECK_EQ(world.body_count(), 1u);

    const auto state = world.get_body_state(first.value());
    POKO_CHECK(state.ok());
    POKO_CHECK_EQ(state.value().position.y, 2.0f);

    BodyState moved;
    moved.position = {1.0f, 3.0f, 0.0f};
    POKO_CHECK(world.set_body_state(first.value(), moved).ok());
    POKO_CHECK_EQ(world.get_body_state(first.value()).value().position.x, 1.0f);

    POKO_CHECK(world.destroy_body(first.value()).ok());
    POKO_CHECK(!world.destroy_body(first.value()).ok());
    POKO_CHECK(!world.get_body_state(first.value()).ok());
    POKO_CHECK_EQ(world.body_count(), 0u);

    auto second = world.create_body(make_sphere(0, 0, 0));
    POKO_CHECK(second.ok());
    POKO_CHECK(!poko::same_handle(first.value(), second.value()));
}

POKO_TEST(gravity_integration_and_ground_resolution) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    auto body = world.create_body(make_sphere(0.0f, 5.0f, 0.0f));
    POKO_CHECK(body.ok());

    for (std::uint32_t i = 0; i < 60; ++i) {
        world.step(1.0f / 60.0f);
    }
    const auto state = world.get_body_state(body.value());
    POKO_CHECK(state.ok());
    POKO_CHECK(std::abs(state.value().position.y - 0.5f) < 0.02f); // resting on ground
    POKO_CHECK(std::abs(state.value().velocity.y) < 0.2f);
    POKO_CHECK_EQ(world.substep_count(), 60u);
    POKO_CHECK(world.contact_count() > 0u);
}

POKO_TEST(collision_response_separates_overlapping_bodies) {
    PhysicsWorldDesc desc;
    desc.gravity = {0.0f, 0.0f, 0.0f};
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(desc));
    world.set_ground(false);

    BodyDesc static_body = make_sphere(0.0f, 0.0f, 0.0f);
    static_body.type = BodyType::Static;
    auto anchor = world.create_body(static_body);
    POKO_CHECK(anchor.ok());

    auto moving = world.create_body(make_sphere(0.6f, 0.0f, 0.0f)); // overlapping
    POKO_CHECK(moving.ok());
    world.step(1.0f / 60.0f);

    const auto state = world.get_body_state(moving.value());
    POKO_CHECK(state.ok());
    POKO_CHECK(state.value().position.x > 0.9f); // pushed out of penetration
    POKO_CHECK(world.contact_count() > 0u);
}

POKO_TEST(raycast_hits_analytic_shapes_and_respects_layer_mask) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));

    auto sphere = world.create_body(make_sphere(0.0f, 0.0f, -5.0f, 1.0f));
    POKO_CHECK(sphere.ok());

    Ray ray;
    ray.origin = {0.0f, 0.0f, 0.0f};
    ray.direction = {0.0f, 0.0f, -1.0f};
    RayHit hit;
    POKO_CHECK(world.raycast(ray, 20.0f, 0xFFFFFFFFu, hit));
    POKO_CHECK(std::abs(hit.distance - 4.0f) < 0.01f);
    POKO_CHECK(poko::same_handle(hit.body, sphere.value()));

    BodyDesc box = make_sphere(0.0f, 0.0f, -10.0f);
    box.shape.kind = ShapeKind::Box;
    box.shape.half_extents = {0.5f, 0.5f, 0.5f};
    auto box_handle = world.create_body(box);
    POKO_CHECK(box_handle.ok());

    // Layer masking: a body on layer 2 is invisible to a mask of 1.
    BodyDesc layered = make_sphere(0.0f, 0.0f, -2.0f);
    layered.collision_layer = 2;
    auto hidden = world.create_body(layered);
    POKO_CHECK(hidden.ok());
    RayHit masked;
    POKO_CHECK(!world.raycast(ray, 1.5f, 1u, masked));
    POKO_CHECK(world.raycast(ray, 3.0f, 2u, masked));
    POKO_CHECK(poko::same_handle(masked.body, hidden.value()));
}

POKO_TEST_MAIN()
