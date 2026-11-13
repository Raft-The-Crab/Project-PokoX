#include "poko_test_harness.h"

#include <cmath>
#include <cstdint>

#include "poko/core/physics_contracts.h"
#include "poko/physics/custom_world.h"

namespace {
using namespace poko;

BodyDesc box(float x, float y, float z, float half = 0.5f) {
    BodyDesc d;
    d.shape.kind = ShapeKind::Box;
    d.shape.half_extents = {half, half, half};
    d.position = {x, y, z};
    d.friction = 0.6f;
    return d;
}

BodyDesc sphere(float x, float y, float z, float r = 0.5f) {
    BodyDesc d;
    d.shape.kind = ShapeKind::Sphere;
    d.shape.radius = r;
    d.position = {x, y, z};
    return d;
}

BodyDesc capsule(float x, float y, float z, float r, float hh) {
    BodyDesc d;
    d.shape.kind = ShapeKind::Capsule;
    d.shape.radius = r;
    d.shape.half_height = hh;
    d.position = {x, y, z};
    return d;
}

void run(CustomPhysicsWorld& w, float seconds) {
    const int steps = static_cast<int>(seconds * 60.0f);
    for (int i = 0; i < steps; ++i) w.step(1.0f / 60.0f);
}

BodyState state(const CustomPhysicsWorld& w, BodyHandle h) {
    return w.get_body_state(h).value();
}

float up_y(const math::Quat& q) {
    return math::rotate(q, math::Vec3{0, 1, 0}).y;
}

// Non-movable world fixture; w-> forwards to the simulation.
struct World {
    CustomPhysicsWorld sim;
    World() { POKO_CHECK(sim.initialize(PhysicsWorldDesc{})); }
    CustomPhysicsWorld* operator->() { return &sim; }
    operator CustomPhysicsWorld&() { return sim; }
};
} // namespace

POKO_TEST(box_rests_flat_on_ground_and_falls_asleep) {
    World w;
    const auto b = w->create_body(box(0, 0.6f, 0)).value();
    run(w, 3.0f);
    const auto s = state(w, b);
    POKO_CHECK(std::abs(s.position.y - 0.5f) < 0.02f);
    POKO_CHECK(up_y(s.rotation) > 0.999f);
    POKO_CHECK(w->is_sleeping(b));
    POKO_CHECK_EQ(w->sleeping_body_count(), 1u);
}

POKO_TEST(tilted_box_settles_on_a_face) {
    World w;
    auto d = box(0, 2.0f, 0);
    d.rotation = math::quat_from_axis_angle({0.3f, 0.0f, 1.0f}, 0.6f);
    const auto b = w->create_body(d).value();
    run(w, 5.0f);
    const auto s = state(w, b);
    POKO_CHECK(std::abs(s.position.y - 0.5f) < 0.03f);
    POKO_CHECK(up_y(s.rotation) > 0.995f);
}

POKO_TEST(box_stack_stays_standing) {
    World w;
    BodyHandle top{};
    for (int i = 0; i < 3; ++i) {
        top = w->create_body(box(0, 0.5f + static_cast<float>(i) * 1.05f, 0)).value();
    }
    run(w, 5.0f);
    const auto s = state(w, top);
    POKO_CHECK(std::abs(s.position.y - 2.5f) < 0.1f);
    POKO_CHECK(std::abs(s.position.x) < 0.1f && std::abs(s.position.z) < 0.1f);
}

POKO_TEST(ten_box_stack_with_offsets_stays_standing_and_sleeps) {
    World w;
    BodyHandle top{};
    for (int i = 0; i < 10; ++i) {
        const float jitter = (i % 2 == 0 ? 0.02f : -0.02f); // slightly off-centre
        top = w->create_body(box(jitter, 0.5f + static_cast<float>(i) * 1.0f, 0)).value();
    }
    run(w.sim, 8.0f);
    const auto s = state(w.sim, top);
    POKO_CHECK(std::abs(s.position.y - 9.5f) < 0.15f);
    POKO_CHECK(std::abs(s.position.x) < 0.15f);
    POKO_CHECK(w->sleeping_body_count() >= 9u);
}

POKO_TEST(heavy_box_rests_on_light_box_without_squashing_it) {
    World w;
    auto light = box(0, 0.5f, 0);
    light.mass = 1.0f;
    w->create_body(light);
    auto heavy = box(0, 1.6f, 0);
    heavy.mass = 50.0f;
    const auto top = w->create_body(heavy).value();
    run(w.sim, 4.0f);
    POKO_CHECK(std::abs(state(w.sim, top).position.y - 1.5f) < 0.08f);
}

POKO_TEST(box_lands_on_static_platform) {
    World w;
    auto platform = box(0, 1.0f, 0, 1.0f);
    platform.type = BodyType::Static;
    w->create_body(platform);
    const auto b = w->create_body(box(0.3f, 4.0f, 0.2f)).value();
    run(w, 4.0f);
    const auto s = state(w, b);
    POKO_CHECK(std::abs(s.position.y - 2.5f) < 0.03f); // platform top 2.0 + half 0.5
}

POKO_TEST(capsules_rest_upright_and_lying) {
    World w;
    const auto standing = w->create_body(capsule(0, 2.0f, 0, 0.3f, 0.5f)).value();
    auto lying_desc = capsule(5, 2.0f, 0, 0.3f, 0.5f);
    lying_desc.rotation = math::quat_from_axis_angle({0, 0, 1}, math::kHalfPi);
    const auto lying = w->create_body(lying_desc).value();
    run(w, 4.0f);
    POKO_CHECK(std::abs(state(w, standing).position.y - 0.8f) < 0.03f);
    POKO_CHECK(std::abs(state(w, lying).position.y - 0.3f) < 0.03f);
}

POKO_TEST(restitution_bounces_and_friction_stops_sliding) {
    World w;
    auto ball = sphere(0, 5.0f, 0);
    ball.restitution = 0.8f;
    const auto b = w->create_body(ball).value();
    float peak = 0.0f;
    bool bounced = false;
    for (int i = 0; i < 240; ++i) {
        w->step(1.0f / 60.0f);
        const auto s = state(w, b);
        if (s.velocity.y > 1.0f) bounced = true;
        if (bounced) peak = std::max(peak, s.position.y);
    }
    POKO_CHECK(bounced);
    POKO_CHECK(peak > 2.0f && peak < 4.0f);

    auto grippy = box(10, 0.5f, 0);
    grippy.velocity = {5.0f, 0.0f, 0.0f};
    auto slick = box(10, 0.5f, 5);
    slick.velocity = {5.0f, 0.0f, 0.0f};
    slick.friction = 0.0f;
    const auto g = w->create_body(grippy).value();
    const auto s = w->create_body(slick).value();
    run(w, 2.0f);
    POKO_CHECK(state(w, g).position.x < 10.0f + 2.5f);
    POKO_CHECK(state(w, s).position.x > 10.0f + 8.0f);
}

POKO_TEST(sleeping_body_wakes_when_hit_or_modified) {
    World w;
    const auto rest = w->create_body(box(0, 0.5f, 0)).value();
    run(w, 2.0f);
    POKO_CHECK(w->is_sleeping(rest));

    const auto ball = w->create_body(sphere(0.0f, 4.0f, 0.0f, 0.3f)).value();
    run(w, 1.5f);
    // The falling sphere strikes the box and wakes it; both settle again.
    POKO_CHECK(std::abs(state(w, rest).position.y - 0.5f) < 0.03f);
    (void)ball;

    POKO_CHECK(w->wake(rest).ok());
    POKO_CHECK(!w->is_sleeping(rest));
    run(w, 2.0f);
    POKO_CHECK(w->is_sleeping(rest));
    BodyState moved = state(w, rest);
    moved.position.x = 3.0f;
    POKO_CHECK(w->set_body_state(rest, moved).ok());
    POKO_CHECK(!w->is_sleeping(rest));
}

POKO_TEST(simulation_is_deterministic) {
    const auto simulate = [] {
        World w;
        auto d = box(0, 3.0f, 0);
        d.rotation = math::quat_from_axis_angle({1, 0, 0.5f}, 0.4f);
        const auto a = w->create_body(d).value();
        w->create_body(sphere(0.2f, 6.0f, 0.1f));
        run(w, 3.0f);
        return state(w, a);
    };
    const auto first = simulate();
    const auto second = simulate();
    POKO_CHECK(first.position.x == second.position.x && first.position.y == second.position.y &&
               first.rotation.w == second.rotation.w);
}

POKO_TEST(raycast_respects_rotation_and_capsule_shape) {
    World w;
    auto d = box(0, 0, -5);
    d.rotation = math::quat_from_axis_angle({0, 1, 0}, math::kPi / 4.0f);
    d.type = BodyType::Static;
    w->create_body(d);
    Ray ray{{0.6f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};
    RayHit hit;
    // A diamond reaches x = 0.707, so this ray hits only when rotation is honoured.
    POKO_CHECK(w->raycast(ray, 20.0f, 0xFFFFFFFFu, hit));
    POKO_CHECK(std::abs(hit.distance - 4.893f) < 0.01f);
    POKO_CHECK(hit.normal.z > 0.5f);

    auto cap = capsule(0, 0, -20, 0.5f, 1.0f);
    cap.type = BodyType::Static;
    w->create_body(cap);
    Ray side{{0.0f, 0.9f, 0.0f}, {0.0f, 0.0f, -1.0f}};
    RayHit cap_hit;
    // Side of the cylinder section (|y| <= 1) at z = -19.5, behind the box.
    POKO_CHECK(w->raycast(side, 40.0f, 0xFFFFFFFFu, cap_hit));
    POKO_CHECK(cap_hit.distance > 4.0f); // still the box first
    Ray miss{{0.0f, 1.8f, 0.0f}, {0.0f, 0.0f, -1.0f}};
    RayHit none;
    POKO_CHECK(!w->raycast(miss, 40.0f, 0xFFFFFFFFu, none)); // above the capsule cap
}

POKO_TEST_MAIN()
