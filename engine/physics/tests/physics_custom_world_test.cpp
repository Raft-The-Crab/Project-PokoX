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

POKO_TEST(tall_stack_settles_precisely_and_sleeps) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    std::vector<poko::BodyHandle> boxes;
    for (int i = 0; i < 5; ++i) {
        boxes.push_back(
            world.create_body(make_box(0.0f, 0.5f + 1.05f * static_cast<float>(i), 0.0f)).value());
    }
    run(world, 420);
    for (int i = 0; i < 5; ++i) {
        const auto s = world.get_body_state(boxes[static_cast<std::size_t>(i)]).value();
        POKO_CHECK(std::abs(s.position.y - (0.5f + static_cast<float>(i))) < 0.04f);
        POKO_CHECK(std::abs(s.position.x) < 0.05f && std::abs(s.position.z) < 0.05f);
        POKO_CHECK(world.is_sleeping(boxes[static_cast<std::size_t>(i)]));
    }
}

POKO_TEST(fast_bodies_do_not_tunnel) {
    PhysicsWorldDesc desc;
    desc.gravity = {0.0f, 0.0f, 0.0f};
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(desc));
    world.set_ground(false);
    BodyDesc wall = make_box(5.0f, 0.0f, 0.0f);
    wall.type = BodyType::Static;
    wall.shape.half_extents = {0.05f, 5.0f, 5.0f};
    POKO_CHECK(world.create_body(wall).ok());
    BodyDesc bullet = make_sphere(0.0f, 0.0f, 0.0f, 0.25f);
    bullet.velocity = {300.0f, 0.0f, 0.0f}; // 5 m per step, wall is 0.1 m thick
    auto handle = world.create_body(bullet);
    POKO_CHECK(handle.ok());
    run(world, 30);
    const auto s = world.get_body_state(handle.value()).value();
    POKO_CHECK(std::isfinite(s.position.x));
    POKO_CHECK(s.position.x < 5.0f); // stayed on the near side

    CustomPhysicsWorld falling;
    POKO_CHECK(falling.initialize(PhysicsWorldDesc{}));
    BodyDesc dropped = make_sphere(0.0f, 50.0f, 0.0f, 0.25f);
    dropped.velocity = {0.0f, -150.0f, 0.0f};
    auto drop = falling.create_body(dropped);
    run(falling, 30);
    POKO_CHECK(falling.get_body_state(drop.value()).value().position.y > 0.2f);
}

POKO_TEST(broadphase_finds_pairs_regardless_of_creation_order) {
    PhysicsWorldDesc desc;
    desc.gravity = {0.0f, 0.0f, 0.0f};
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(desc));
    world.set_ground(false);
    BodyDesc anchor = make_sphere(0.0f, 0.0f, 0.0f);
    anchor.type = BodyType::Static;
    POKO_CHECK(world.create_body(anchor).ok());
    POKO_CHECK(world.create_body(make_sphere(10.0f, 0.0f, 0.0f)).ok()); // far away, created between
    auto near = world.create_body(make_sphere(0.5f, 0.0f, 0.0f));       // overlaps the first body
    POKO_CHECK(near.ok());
    run(world, 3);
    POKO_CHECK(world.get_body_state(near.value()).value().position.x > 0.9f);
}

POKO_TEST(ball_joint_to_world_keeps_pendulum_length) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    world.set_ground(false);
    auto bob = world.create_body(make_sphere(2.0f, 5.0f, 0.0f, 0.25f));
    POKO_CHECK(bob.ok());
    poko::JointDesc joint;
    joint.kind = poko::JointKind::Ball;
    joint.body_a = bob.value();
    joint.anchor_a = {-2.0f, 0.0f, 0.0f};
    joint.anchor_b = {0.0f, 5.0f, 0.0f}; // world pivot
    auto handle = world.create_joint(joint);
    POKO_CHECK(handle.ok());
    POKO_CHECK_EQ(world.joint_count(), 1u);
    for (int i = 0; i < 240; ++i) {
        world.step(1.0f / 60.0f);
        const auto p = world.get_body_state(bob.value()).value().position;
        const float radius = std::sqrt(p.x * p.x + (p.y - 5.0f) * (p.y - 5.0f) + p.z * p.z);
        POKO_CHECK(std::abs(radius - 2.0f) < 0.08f);
    }
    // It really swung: gravity must have pulled it well below the pivot height.
    POKO_CHECK(world.get_body_state(bob.value()).value().position.y < 4.0f);
    POKO_CHECK(!world.is_sleeping(bob.value())); // jointed bodies never sleep
    POKO_CHECK(world.destroy_joint(handle.value()).ok());
    POKO_CHECK_EQ(world.joint_count(), 0u);
}

POKO_TEST(distance_joint_holds_a_rod_between_two_bodies) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    world.set_ground(false);
    BodyDesc top = make_sphere(0.0f, 10.0f, 0.0f, 0.2f);
    top.type = BodyType::Kinematic;
    auto a = world.create_body(top);
    auto b = world.create_body(make_sphere(1.5f, 10.0f, 0.0f, 0.2f));
    POKO_CHECK(a.ok() && b.ok());
    poko::JointDesc joint;
    joint.kind = poko::JointKind::Distance;
    joint.body_a = a.value();
    joint.body_b = b.value();
    joint.distance = 1.5f;
    POKO_CHECK(world.create_joint(joint).ok());
    run(world, 200);
    const auto pa = world.get_body_state(a.value()).value().position;
    const auto pb = world.get_body_state(b.value()).value().position;
    const float dx = pb.x - pa.x;
    const float dy = pb.y - pa.y;
    POKO_CHECK(std::abs(std::sqrt(dx * dx + dy * dy) - 1.5f) < 0.06f);
}

POKO_TEST(hinge_joint_only_rotates_about_its_axis) {
    PhysicsWorldDesc desc;
    desc.gravity = {0.0f, 0.0f, 0.0f};
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(desc));
    world.set_ground(false);
    BodyDesc door = make_box(1.0f, 0.0f, 0.0f);
    door.shape.half_extents = {1.0f, 1.0f, 0.1f};
    door.angular_velocity = {2.0f, 3.0f, 0.0f}; // only the Y part is allowed
    door.velocity = {0.0f, 0.0f, -3.0f};        // consistent with spinning about the pin
    auto handle = world.create_body(door);
    POKO_CHECK(handle.ok());
    poko::JointDesc joint;
    joint.kind = poko::JointKind::Hinge;
    joint.body_a = handle.value();
    joint.anchor_a = {-1.0f, 0.0f, 0.0f};
    joint.anchor_b = {0.0f, 0.0f, 0.0f};
    joint.axis_a = {0.0f, 1.0f, 0.0f};
    joint.axis_b = {0.0f, 1.0f, 0.0f};
    POKO_CHECK(world.create_joint(joint).ok());
    run(world, 120);
    const auto s = world.get_body_state(handle.value()).value();
    POKO_CHECK(std::abs(s.angular_velocity.x) < 0.2f);
    POKO_CHECK(std::abs(s.angular_velocity.z) < 0.2f);
    POKO_CHECK(s.angular_velocity.y > 2.5f);
    // The hinge pin stays at the world origin.
    const auto pin = s.position + poko::math::rotate(s.rotation, {-1.0f, 0.0f, 0.0f});
    POKO_CHECK(poko::math::length(pin) < 0.1f);
}

POKO_TEST(fixed_joint_holds_two_bodies_rigid_while_they_fall) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    auto a = world.create_body(make_box(0.0f, 3.0f, 0.0f));
    auto b = world.create_body(make_box(1.2f, 3.0f, 0.0f));
    POKO_CHECK(a.ok() && b.ok());
    poko::JointDesc joint;
    joint.kind = poko::JointKind::Fixed;
    joint.body_a = a.value();
    joint.body_b = b.value();
    joint.anchor_a = {0.6f, 0.0f, 0.0f};
    joint.anchor_b = {-0.6f, 0.0f, 0.0f};
    POKO_CHECK(world.create_joint(joint).ok());
    run(world, 240);
    const auto pa = world.get_body_state(a.value()).value();
    const auto pb = world.get_body_state(b.value()).value();
    POKO_CHECK(poko::math::distance(pa.position, pb.position) > 1.1f);
    POKO_CHECK(poko::math::distance(pa.position, pb.position) < 1.3f);
    POKO_CHECK(std::abs(pa.position.y - pb.position.y) < 0.1f); // did not droop
}

POKO_TEST(joints_are_removed_with_their_bodies_and_validated) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    auto a = world.create_body(make_sphere(0.0f, 5.0f, 0.0f));
    auto b = world.create_body(make_sphere(1.0f, 5.0f, 0.0f));
    BodyDesc fixed = make_sphere(5.0f, 5.0f, 0.0f);
    fixed.type = BodyType::Static;
    auto c = world.create_body(fixed);
    POKO_CHECK(a.ok() && b.ok() && c.ok());

    poko::JointDesc joint;
    joint.body_a = a.value();
    joint.body_b = a.value(); // same body twice
    POKO_CHECK(!world.create_joint(joint).ok());
    joint.body_b = c.value();
    joint.body_a = c.value(); // no dynamic body
    POKO_CHECK(!world.create_joint(joint).ok());
    joint.body_a = a.value();
    joint.body_b = b.value();
    POKO_CHECK(world.create_joint(joint).ok());
    POKO_CHECK_EQ(world.joint_count(), 1u);
    POKO_CHECK(world.destroy_body(b.value()).ok());
    POKO_CHECK_EQ(world.joint_count(), 0u);
}

POKO_TEST(triggers_report_enter_and_leave_without_colliding) {
    PhysicsWorldDesc desc;
    desc.gravity = {0.0f, 0.0f, 0.0f};
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(desc));
    world.set_ground(false);
    BodyDesc zone = make_box(0.0f, 0.0f, 0.0f);
    zone.type = BodyType::Static;
    zone.is_trigger = true;
    zone.shape.half_extents = {1.0f, 1.0f, 1.0f};
    auto trigger = world.create_body(zone);
    BodyDesc runner = make_sphere(-4.0f, 0.0f, 0.0f);
    runner.velocity = {4.0f, 0.0f, 0.0f};
    auto body = world.create_body(runner);
    POKO_CHECK(trigger.ok() && body.ok());

    std::vector<poko::TriggerEvent> events;
    for (int i = 0; i < 180; ++i) {
        world.step(1.0f / 60.0f);
        world.drain_trigger_events(events);
    }
    POKO_CHECK_EQ(events.size(), 2u);
    POKO_CHECK(events[0].entered && !events[1].entered);
    POKO_CHECK(poko::same_handle(events[0].trigger, trigger.value()));
    POKO_CHECK(poko::same_handle(events[0].other, body.value()));
    // Passing through never slowed the body: no collision response.
    POKO_CHECK(std::abs(world.get_body_state(body.value()).value().velocity.x - 4.0f) < 1e-4f);

    // Triggers are invisible to raycasts and cannot be dynamic.
    Ray ray;
    ray.origin = {0.0f, 5.0f, 0.0f};
    ray.direction = {0.0f, -1.0f, 0.0f};
    RayHit hit;
    POKO_CHECK(!world.raycast(ray, 20.0f, 0xFFFFFFFFu, hit));
    BodyDesc bad = zone;
    bad.type = BodyType::Dynamic;
    POKO_CHECK(!world.create_body(bad).ok());
}

POKO_TEST(shape_cast_and_overlap_queries) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    BodyDesc crate = make_box(0.0f, 0.5f, 0.0f);
    crate.type = BodyType::Static;
    crate.shape.half_extents = {1.0f, 0.5f, 1.0f};
    auto crate_handle = world.create_body(crate);
    POKO_CHECK(crate_handle.ok());

    poko::ShapeDesc ball;
    ball.kind = ShapeKind::Sphere;
    ball.radius = 0.5f;
    poko::SweepHit hit;
    POKO_CHECK(world.shape_cast(ball, {0.0f, 5.0f, 0.0f}, {}, {0.0f, -1.0f, 0.0f}, 10.0f,
                                0xFFFFFFFFu, hit));
    POKO_CHECK(!hit.ground);
    POKO_CHECK(std::abs(hit.distance - 3.5f) < 0.01f); // crate top at y = 1
    POKO_CHECK(std::abs(hit.normal.y - 1.0f) < 0.01f);
    POKO_CHECK(poko::same_handle(hit.body, crate_handle.value()));

    // Beside the crate the cast reaches the ground plane instead.
    POKO_CHECK(world.shape_cast(ball, {5.0f, 5.0f, 0.0f}, {}, {0.0f, -1.0f, 0.0f}, 10.0f,
                                0xFFFFFFFFu, hit));
    POKO_CHECK(hit.ground);
    POKO_CHECK(std::abs(hit.distance - 4.5f) < 0.01f);

    // Out of range, and a sideways cast that misses everything.
    POKO_CHECK(!world.shape_cast(ball, {0.0f, 5.0f, 0.0f}, {}, {0.0f, -1.0f, 0.0f}, 2.0f,
                                 0xFFFFFFFFu, hit));
    POKO_CHECK(!world.shape_cast(ball, {5.0f, 3.0f, 0.0f}, {}, {1.0f, 0.0f, 0.0f}, 20.0f,
                                 0xFFFFFFFFu, hit));

    // A sideways capsule cast into the crate's side face.
    poko::ShapeDesc capsule;
    capsule.kind = ShapeKind::Capsule;
    capsule.radius = 0.25f;
    capsule.half_height = 0.5f;
    POKO_CHECK(world.shape_cast(capsule, {-6.0f, 1.0f, 0.0f}, {}, {1.0f, 0.0f, 0.0f}, 10.0f,
                                0xFFFFFFFFu, hit));
    POKO_CHECK(std::abs(hit.distance - 4.75f) < 0.02f);
    POKO_CHECK(hit.normal.x < -0.9f);

    poko::BodyHandle found[4];
    POKO_CHECK_EQ(world.overlap(ball, {0.5f, 1.0f, 0.0f}, {}, 0xFFFFFFFFu, found, 4), 1u);
    POKO_CHECK(poko::same_handle(found[0], crate_handle.value()));
    POKO_CHECK_EQ(world.overlap(ball, {6.0f, 3.0f, 0.0f}, {}, 0xFFFFFFFFu, found, 4), 0u);
}

POKO_TEST_MAIN()
