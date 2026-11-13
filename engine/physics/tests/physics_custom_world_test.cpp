#include "poko_test_harness.h"

#include <cmath>
#include <cstdint>
#include <vector>

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

namespace {
BodyDesc make_box(float x, float y, float z, float half = 0.5f) {
    BodyDesc desc;
    desc.shape.kind = ShapeKind::Box;
    desc.shape.half_extents = {half, half, half};
    desc.position = {x, y, z};
    desc.mass = 1.0f;
    desc.friction = 0.6f;
    return desc;
}

void run(CustomPhysicsWorld& world, int steps) {
    for (int i = 0; i < steps; ++i) {
        world.step(1.0f / 60.0f);
    }
}
} // namespace

POKO_TEST(box_comes_to_rest_flat_on_ground_and_sleeps) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    BodyDesc desc = make_box(0.0f, 3.0f, 0.0f);
    desc.rotation = poko::math::quat_from_axis_angle({1.0f, 0.0f, 1.0f}, 0.4f);
    auto box = world.create_body(desc);
    POKO_CHECK(box.ok());
    run(world, 300);
    const auto state = world.get_body_state(box.value()).value();
    POKO_CHECK(std::abs(state.position.y - 0.5f) < 0.03f); // half extent, not circumscribed radius
    POKO_CHECK(world.is_sleeping(box.value()));
    POKO_CHECK_EQ(world.sleeping_count(), 1u);
    POKO_CHECK_EQ(state.velocity.y, 0.0f);

    BodyState nudged = state;
    nudged.velocity = {1.0f, 0.0f, 0.0f};
    POKO_CHECK(world.set_body_state(box.value(), nudged).ok());
    POKO_CHECK(!world.is_sleeping(box.value()));
}

POKO_TEST(box_stack_stays_stable) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    auto bottom = world.create_body(make_box(0.0f, 0.5f, 0.0f));
    auto middle = world.create_body(make_box(0.0f, 1.6f, 0.0f));
    auto top = world.create_body(make_box(0.0f, 2.7f, 0.0f));
    POKO_CHECK(bottom.ok() && middle.ok() && top.ok());
    run(world, 240);
    const auto t = world.get_body_state(top.value()).value();
    const auto m = world.get_body_state(middle.value()).value();
    POKO_CHECK(std::abs(m.position.y - 1.5f) < 0.06f);
    POKO_CHECK(std::abs(t.position.y - 2.5f) < 0.08f);
    POKO_CHECK(std::abs(t.position.x) < 0.1f && std::abs(t.position.z) < 0.1f);
}

POKO_TEST(capsule_lies_on_ground_at_its_radius) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    BodyDesc desc = make_sphere(0.0f, 2.0f, 0.0f, 0.4f);
    desc.shape.kind = ShapeKind::Capsule;
    desc.shape.half_height = 0.6f;
    // Rotate the capsule axis (local Y) onto world X so it falls on its side.
    desc.rotation = poko::math::quat_from_axis_angle({0.0f, 0.0f, 1.0f}, 1.5707963f);
    auto capsule = world.create_body(desc);
    POKO_CHECK(capsule.ok());
    run(world, 240);
    POKO_CHECK(std::abs(world.get_body_state(capsule.value()).value().position.y - 0.4f) < 0.03f);
}

POKO_TEST(box_rests_on_static_box_platform) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    BodyDesc platform = make_box(0.0f, 1.0f, 0.0f);
    platform.type = BodyType::Static;
    platform.shape.half_extents = {2.0f, 0.25f, 2.0f};
    POKO_CHECK(world.create_body(platform).ok());
    auto crate = world.create_body(make_box(0.5f, 3.0f, 0.0f));
    POKO_CHECK(crate.ok());
    run(world, 240);
    // Platform top is y = 1.25, crate half extent 0.5.
    POKO_CHECK(std::abs(world.get_body_state(crate.value()).value().position.y - 1.75f) < 0.03f);
}

POKO_TEST(kinematic_body_pushes_dynamic_body) {
    PhysicsWorldDesc desc;
    desc.gravity = {0.0f, 0.0f, 0.0f};
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(desc));
    world.set_ground(false);
    BodyDesc mover = make_box(-2.0f, 0.0f, 0.0f);
    mover.type = BodyType::Kinematic;
    mover.velocity = {4.0f, 0.0f, 0.0f};
    auto kinematic = world.create_body(mover);
    auto ball = world.create_body(make_sphere(0.0f, 0.0f, 0.0f));
    POKO_CHECK(kinematic.ok() && ball.ok());
    run(world, 90);
    POKO_CHECK(world.get_body_state(ball.value()).value().velocity.x > 1.0f);
    // The kinematic body is never deflected by the collision.
    POKO_CHECK(std::abs(world.get_body_state(kinematic.value()).value().velocity.x - 4.0f) < 1e-4f);
}

POKO_TEST(restitution_makes_ball_bounce_and_collisions_exchange_momentum) {
    PhysicsWorldDesc desc;
    desc.gravity = {0.0f, 0.0f, 0.0f};
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(desc));
    world.set_ground(false);
    BodyDesc left = make_sphere(-1.0f, 0.0f, 0.0f);
    left.velocity = {3.0f, 0.0f, 0.0f};
    left.restitution = 1.0f;
    BodyDesc right = make_sphere(1.0f, 0.0f, 0.0f);
    right.restitution = 1.0f;
    auto a = world.create_body(left);
    auto b = world.create_body(right);
    POKO_CHECK(a.ok() && b.ok());
    run(world, 60);
    const float va = world.get_body_state(a.value()).value().velocity.x;
    const float vb = world.get_body_state(b.value()).value().velocity.x;
    POKO_CHECK(std::abs(va + vb - 3.0f) < 0.05f); // momentum conserved
    POKO_CHECK(vb > va);                          // they separate
}

POKO_TEST(simulation_is_deterministic) {
    auto simulate = []() {
        CustomPhysicsWorld world;
        (void)world.initialize(PhysicsWorldDesc{});
        std::vector<poko::BodyHandle> handles;
        for (int i = 0; i < 12; ++i) {
            const float fi = static_cast<float>(i);
            BodyDesc d = i % 2 == 0 ? make_box(0.1f * fi, 1.0f + 1.1f * fi, 0.05f * fi)
                                    : make_sphere(0.05f * fi, 1.0f + 1.1f * fi, 0.0f);
            handles.push_back(world.create_body(d).value());
        }
        run(world, 180);
        std::vector<float> out;
        for (auto h : handles) {
            const auto s = world.get_body_state(h).value();
            out.push_back(s.position.x);
            out.push_back(s.position.y);
            out.push_back(s.position.z);
        }
        return out;
    };
    POKO_CHECK(simulate() == simulate());
}

POKO_TEST(many_bodies_stay_finite_and_above_ground) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    std::vector<poko::BodyHandle> handles;
    for (int i = 0; i < 200; ++i) {
        const float x = static_cast<float>(i % 8) * 1.2f;
        const float z = static_cast<float>((i / 8) % 5) * 1.2f;
        const float y = 1.0f + static_cast<float>(i / 40) * 1.5f;
        handles.push_back(
            world.create_body(i % 2 ? make_box(x, y, z) : make_sphere(x, y, z)).value());
    }
    run(world, 240);
    for (auto h : handles) {
        const auto s = world.get_body_state(h).value();
        POKO_CHECK(std::isfinite(s.position.x) && std::isfinite(s.position.y) &&
                   std::isfinite(s.position.z));
        POKO_CHECK(s.position.y > 0.3f);
    }
}

POKO_TEST(raycast_respects_rotation_and_capsule_shape) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    world.set_ground(false);

    BodyDesc box = make_box(0.0f, 0.0f, -5.0f);
    box.type = BodyType::Static;
    box.rotation = poko::math::quat_from_axis_angle({0.0f, 1.0f, 0.0f}, 0.7853982f);
    POKO_CHECK(world.create_body(box).ok());

    Ray ray;
    ray.origin = {0.0f, 0.0f, 0.0f};
    ray.direction = {0.0f, 0.0f, -1.0f};
    RayHit hit;
    POKO_CHECK(world.raycast(ray, 20.0f, 0xFFFFFFFFu, hit));
    // A box turned 45 degrees presents its corner: 5 - 0.5 * sqrt(2).
    POKO_CHECK(std::abs(hit.distance - 4.2929f) < 0.01f);

    CustomPhysicsWorld capsule_world;
    POKO_CHECK(capsule_world.initialize(PhysicsWorldDesc{}));
    capsule_world.set_ground(false);
    BodyDesc capsule = make_sphere(0.0f, 0.0f, -5.0f, 0.5f);
    capsule.type = BodyType::Static;
    capsule.shape.kind = ShapeKind::Capsule;
    capsule.shape.half_height = 1.0f;
    POKO_CHECK(capsule_world.create_body(capsule).ok());
    ray.origin = {0.0f, 0.8f, 0.0f}; // alongside the cylinder section
    POKO_CHECK(capsule_world.raycast(ray, 20.0f, 0xFFFFFFFFu, hit));
    POKO_CHECK(std::abs(hit.distance - 4.5f) < 0.01f);
    ray.origin = {0.0f, 1.6f, 0.0f}; // above the top cap (cap reaches y = 1.5)
    POKO_CHECK(!capsule_world.raycast(ray, 20.0f, 0xFFFFFFFFu, hit));
}

POKO_TEST_MAIN()
