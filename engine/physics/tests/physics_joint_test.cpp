#include "poko_test_harness.h"

#include <cmath>

#include "poko/core/physics_contracts.h"
#include "poko/physics/custom_world.h"

namespace {
using namespace poko;

struct World {
    CustomPhysicsWorld sim;
    World() { POKO_CHECK(sim.initialize(PhysicsWorldDesc{})); }
    CustomPhysicsWorld* operator->() { return &sim; }

    BodyHandle body(ShapeKind kind, math::Vec3 pos, BodyType type = BodyType::Dynamic,
                    float half = 0.25f) {
        BodyDesc d;
        d.type = type;
        d.shape.kind = kind;
        d.shape.radius = half;
        d.shape.half_extents = {half, half, half};
        d.position = pos;
        return sim.create_body(d).value();
    }
    math::Vec3 pos(BodyHandle h) { return sim.get_body_state(h).value().position; }
    void run(float seconds) {
        for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i) sim.step(1.0f / 60.0f);
    }
};

float dist(const math::Vec3& a, const math::Vec3& b) {
    return math::length(a - b);
}
} // namespace

POKO_TEST(ball_joint_pendulum_keeps_its_length) {
    World w;
    const auto anchor = w.body(ShapeKind::Box, {0, 10, 0}, BodyType::Static);
    const auto bob = w.body(ShapeKind::Sphere, {2, 10, 0});
    JointDesc d;
    d.type = JointType::Ball;
    d.body_a = anchor;
    d.body_b = bob;
    d.anchor = {0, 10, 0};
    POKO_CHECK(w->create_joint(d).ok());
    float worst = 0.0f;
    for (int i = 0; i < 300; ++i) {
        w->step(1.0f / 60.0f);
        worst = std::max(worst, std::abs(dist(w.pos(bob), {0, 10, 0}) - 2.0f));
    }
    POKO_CHECK(worst < 0.05f);
    POKO_CHECK(w.pos(bob).y < 10.0f); // it swung down
}

POKO_TEST(world_anchored_ball_joint_and_collide_connected) {
    World w;
    const auto bob = w.body(ShapeKind::Sphere, {1.5f, 5, 0});
    JointDesc d;
    d.type = JointType::Ball;
    d.body_a = bob;
    d.anchor = {0, 5, 0}; // body_b invalid: world anchor
    POKO_CHECK(w->create_joint(d).ok());
    w.run(3.0f);
    POKO_CHECK(std::abs(dist(w.pos(bob), {0, 5, 0}) - 1.5f) < 0.05f);

    // Connected bodies pass through each other by default.
    World v;
    const auto a = v.body(ShapeKind::Sphere, {0, 8, 0}, BodyType::Static);
    const auto b = v.body(ShapeKind::Sphere, {0.1f, 8, 0}, BodyType::Dynamic, 0.25f);
    JointDesc j;
    j.type = JointType::Ball;
    j.body_a = a;
    j.body_b = b;
    j.anchor = {0.05f, 8, 0};
    POKO_CHECK(v->create_joint(j).ok());
    v.run(1.0f);
    POKO_CHECK(dist(v.pos(b), {0.1f, 8, 0}) < 0.1f); // not shoved apart by contact
}

POKO_TEST(fixed_joint_welds_bodies_together) {
    World w;
    const auto a = w.body(ShapeKind::Box, {0, 6, 0});
    const auto b = w.body(ShapeKind::Box, {1.0f, 6, 0});
    JointDesc d;
    d.type = JointType::Fixed;
    d.body_a = a;
    d.body_b = b;
    d.anchor = {0.5f, 6, 0};
    POKO_CHECK(w->create_joint(d).ok());
    // Kick one body sideways while they fall.
    auto s = w->get_body_state(b).value();
    s.velocity = {0, 0, 4};
    s.angular_velocity = {0, 2, 0};
    POKO_CHECK(w->set_body_state(b, s).ok());
    w.run(1.0f);
    const auto qa = w->get_body_state(a).value().rotation;
    const auto qb = w->get_body_state(b).value().rotation;
    POKO_CHECK(std::abs(dist(w.pos(a), w.pos(b)) - 1.0f) < 0.05f);
    POKO_CHECK(std::abs(std::abs(qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w) - 1.0f) < 0.01f);
}

POKO_TEST(hinge_door_swings_about_its_axis_and_respects_limits) {
    PhysicsWorldDesc zero_g;
    zero_g.gravity = {0, 0, 0};
    CustomPhysicsWorld sim;
    POKO_CHECK(sim.initialize(zero_g));
    sim.set_ground(false);
    BodyDesc frame;
    frame.type = BodyType::Static;
    frame.position = {0, 2, 0};
    const auto fh = sim.create_body(frame).value();
    BodyDesc door;
    door.shape.kind = ShapeKind::Box;
    door.shape.half_extents = {0.5f, 1.0f, 0.05f};
    door.position = {0.5f, 2, 0};
    const auto dh = sim.create_body(door).value();
    JointDesc d;
    d.type = JointType::Hinge;
    d.body_a = fh;
    d.body_b = dh;
    d.anchor = {0, 2, 0};
    d.axis = {0, 1, 0};
    d.limit_enabled = true;
    d.lower = -0.5f;
    d.upper = 1.2f;
    const auto joint = sim.create_joint(d).value();

    auto s = sim.get_body_state(dh).value();
    s.angular_velocity = {0, 3.0f, 0};
    POKO_CHECK(sim.set_body_state(dh, s).ok());
    for (int i = 0; i < 120; ++i) sim.step(1.0f / 60.0f);
    const float angle = sim.joint_position(joint).value();
    POKO_CHECK(angle < 1.2f + 0.05f && angle > 1.0f); // stopped at the upper limit
    const auto p = sim.get_body_state(dh).value().position;
    POKO_CHECK(std::abs(p.y - 2.0f) < 0.02f);                       // stays in the hinge plane
    POKO_CHECK(std::abs(std::hypot(p.x, p.z) - 0.5f) < 0.03f);       // stays on its arc
}

POKO_TEST(hinge_motor_reaches_target_speed_and_is_force_limited) {
    PhysicsWorldDesc zero_g;
    zero_g.gravity = {0, 0, 0};
    CustomPhysicsWorld sim;
    POKO_CHECK(sim.initialize(zero_g));
    sim.set_ground(false);
    BodyDesc base;
    base.type = BodyType::Static;
    const auto bh = sim.create_body(base).value();
    BodyDesc wheel;
    wheel.shape.kind = ShapeKind::Box;
    wheel.shape.half_extents = {0.5f, 0.5f, 0.1f};
    const auto wh = sim.create_body(wheel).value();
    JointDesc d;
    d.type = JointType::Hinge;
    d.body_a = bh;
    d.body_b = wh;
    d.anchor = {0, 0, 0};
    d.axis = {0, 0, 1};
    const auto joint = sim.create_joint(d).value();
    POKO_CHECK(!sim.set_joint_motor(joint, 1.0f, -1.0f).ok());
    POKO_CHECK(sim.set_joint_motor(joint, 4.0f, 1000.0f).ok());
    for (int i = 0; i < 60; ++i) sim.step(1.0f / 60.0f);
    POKO_CHECK(std::abs(sim.get_body_state(wh).value().angular_velocity.z - 4.0f) < 0.1f);

    // A weak motor cannot reach the same speed within the same time.
    POKO_CHECK(sim.set_joint_motor(joint, -4.0f, 0.001f).ok());
    for (int i = 0; i < 10; ++i) sim.step(1.0f / 60.0f);
    POKO_CHECK(sim.get_body_state(wh).value().angular_velocity.z > 3.0f);
    POKO_CHECK(sim.set_joint_motor(joint, 0.0f, 0.0f).ok());
}

POKO_TEST(slider_moves_along_axis_with_limits_and_motor) {
    PhysicsWorldDesc zero_g;
    zero_g.gravity = {0, 0, 0};
    CustomPhysicsWorld sim;
    POKO_CHECK(sim.initialize(zero_g));
    sim.set_ground(false);
    BodyDesc rail;
    rail.type = BodyType::Static;
    const auto rh = sim.create_body(rail).value();
    BodyDesc cart;
    cart.shape.kind = ShapeKind::Box;
    const auto ch = sim.create_body(cart).value();
    JointDesc d;
    d.type = JointType::Slider;
    d.body_a = rh;
    d.body_b = ch;
    d.anchor = {0, 0, 0};
    d.axis = {1, 0, 0};
    d.limit_enabled = true;
    d.lower = -1.0f;
    d.upper = 2.0f;
    const auto joint = sim.create_joint(d).value();
    POKO_CHECK(sim.set_joint_motor(joint, 2.0f, 500.0f).ok());
    // Disturb off-axis: the slider must hold the cart on the rail.
    auto s = sim.get_body_state(ch).value();
    s.velocity = {0, 3, 0};
    s.angular_velocity = {1, 1, 1};
    POKO_CHECK(sim.set_body_state(ch, s).ok());
    for (int i = 0; i < 180; ++i) sim.step(1.0f / 60.0f);
    const auto st = sim.get_body_state(ch).value();
    POKO_CHECK(std::abs(sim.joint_position(joint).value() - 2.0f) < 0.06f); // pinned at the upper limit
    POKO_CHECK(std::abs(st.position.y) < 0.05f && std::abs(st.position.z) < 0.05f);
    POKO_CHECK(std::abs(st.rotation.w) > 0.99f); // orientation locked
}

POKO_TEST(joint_validation_and_lifetime) {
    World w;
    const auto a = w.body(ShapeKind::Box, {0, 3, 0});
    const auto b = w.body(ShapeKind::Box, {1, 3, 0});
    JointDesc d;
    d.body_a = a;
    d.body_b = a;
    POKO_CHECK(!w->create_joint(d).ok()); // identical bodies
    d.body_b = b;
    d.type = JointType::Hinge;
    d.axis = {0, 0, 0};
    POKO_CHECK(!w->create_joint(d).ok()); // zero axis
    d.axis = {0, 1, 0};
    d.limit_enabled = true;
    d.lower = 1.0f;
    d.upper = 0.0f;
    POKO_CHECK(!w->create_joint(d).ok()); // lower > upper
    d.lower = -1.0f;
    d.upper = 1.0f;
    const auto joint = w->create_joint(d);
    POKO_CHECK(joint.ok());
    POKO_CHECK_EQ(w->joint_count(), 1u);

    POKO_CHECK(w->destroy_body(b).ok()); // takes the joint with it
    POKO_CHECK_EQ(w->joint_count(), 0u);
    POKO_CHECK(!w->destroy_joint(joint.value()).ok());
    POKO_CHECK(!w->joint_position(joint.value()).ok());
    POKO_CHECK(!w->set_joint_motor(joint.value(), 1.0f, 1.0f).ok());

    JointDesc ball;
    ball.type = JointType::Ball;
    ball.body_a = a;
    ball.anchor = {0, 3, 0};
    const auto j2 = w->create_joint(ball).value();
    POKO_CHECK(!w->set_joint_motor(j2, 1.0f, 1.0f).ok()); // balls have no motor
    POKO_CHECK(w->destroy_joint(j2).ok());
}

POKO_TEST(jointed_chain_sleeps_and_wakes_together) {
    World w;
    const auto a = w.body(ShapeKind::Box, {0, 0.25f, 0});
    const auto b = w.body(ShapeKind::Box, {0.5f, 0.25f, 0});
    JointDesc d;
    d.type = JointType::Fixed;
    d.body_a = a;
    d.body_b = b;
    d.anchor = {0.25f, 0.25f, 0};
    POKO_CHECK(w->create_joint(d).ok());
    w.run(3.0f);
    POKO_CHECK(w->is_sleeping(a) && w->is_sleeping(b));
    POKO_CHECK(w->wake(a).ok());
    w.run(1.0f / 30.0f);
    POKO_CHECK(!w->is_sleeping(b)); // the weld woke its partner
}

POKO_TEST_MAIN()
