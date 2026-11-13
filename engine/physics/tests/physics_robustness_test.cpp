#include "poko_test_harness.h"

#include <cmath>
#include <limits>

#include "poko/core/physics_contracts.h"
#include "poko/physics/custom_world.h"

namespace {
using namespace poko;

BodyDesc box(math::Vec3 pos, float half = 0.5f) {
    BodyDesc d;
    d.shape.kind = ShapeKind::Box;
    d.shape.half_extents = {half, half, half};
    d.position = pos;
    return d;
}
} // namespace

POKO_TEST(non_finite_and_degenerate_input_is_rejected_or_sanitized) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    auto bad_pos = box({nan, 0, 0});
    POKO_CHECK(!world.create_body(bad_pos).ok());
    auto bad_vel = box({0, 5, 0});
    bad_vel.velocity = {0, inf, 0};
    POKO_CHECK(!world.create_body(bad_vel).ok());
    auto bad_mass = box({0, 5, 0});
    bad_mass.mass = inf;
    POKO_CHECK(!world.create_body(bad_mass).ok());

    // A zero quaternion becomes the identity rather than poisoning the body.
    auto zero_q = box({0, 5, 0});
    zero_q.rotation = {0, 0, 0, 0};
    const auto h = world.create_body(zero_q);
    POKO_CHECK(h.ok());
    const auto q = world.get_body_state(h.value()).value().rotation;
    POKO_CHECK(std::abs(q.w - 1.0f) < 1e-6f);

    BodyState bad = world.get_body_state(h.value()).value();
    bad.angular_velocity.x = nan;
    POKO_CHECK(!world.set_body_state(h.value(), bad).ok());
    BodyState scaled = world.get_body_state(h.value()).value();
    scaled.rotation = {0, 0, 0, 10.0f}; // non-unit
    POKO_CHECK(world.set_body_state(h.value(), scaled).ok());
    POKO_CHECK(std::abs(world.get_body_state(h.value()).value().rotation.w - 1.0f) < 1e-6f);
    for (int i = 0; i < 60; ++i) world.step(1.0f / 60.0f);
    POKO_CHECK_EQ(world.nonfinite_recoveries(), 0u);
}

POKO_TEST(timestep_bounds_are_enforced) {
    for (const float bad : {0.0f, -1.0f, 5.0e-5f, 0.5f, 10.0f}) {
        CustomPhysicsWorld world;
        PhysicsWorldDesc d;
        d.fixed_delta_seconds = bad;
        POKO_CHECK(!world.initialize(d));
    }
    CustomPhysicsWorld ok;
    PhysicsWorldDesc d;
    d.fixed_delta_seconds = 1.0f / 240.0f;
    POKO_CHECK(ok.initialize(d));
}

POKO_TEST(long_frames_do_not_build_an_unbounded_backlog) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    world.create_body(box({0, 5, 0}));
    for (int i = 0; i < 40; ++i) world.step(0.25f); // far more time than 8 substeps cover
    const auto before = world.substep_count();
    world.step(1.0f / 60.0f); // one normal frame afterwards
    POKO_CHECK(world.substep_count() - before <= 2u);
}

POKO_TEST(moving_kinematic_platform_wakes_and_pushes_a_sleeping_body) {
    CustomPhysicsWorld world;
    POKO_CHECK(world.initialize(PhysicsWorldDesc{}));
    const auto crate = world.create_body(box({0, 0.5f, 0})).value();
    for (int i = 0; i < 180; ++i) world.step(1.0f / 60.0f);
    POKO_CHECK(world.is_sleeping(crate));

    BodyDesc pusher = box({-3.0f, 0.5f, 0}, 0.5f);
    pusher.type = BodyType::Kinematic;
    const auto kin = world.create_body(pusher).value();
    float x = -3.0f;
    for (int i = 0; i < 120; ++i) { // slides in from the left at 2 m/s
        x += 2.0f / 60.0f;
        BodyState s = world.get_body_state(kin).value();
        s.position.x = x;
        s.velocity = {2.0f, 0, 0};
        POKO_CHECK(world.set_body_state(kin, s).ok());
        world.step(1.0f / 60.0f);
    }
    POKO_CHECK(world.get_body_state(crate).value().position.x > 0.5f); // it was shoved along
}

POKO_TEST(extreme_speeds_are_clamped_and_deep_overlap_does_not_teleport) {
    CustomPhysicsWorld world;
    PhysicsWorldDesc zero_g;
    zero_g.gravity = {0, 0, 0};
    POKO_CHECK(world.initialize(zero_g));
    world.set_ground(false);
    auto rocket = box({0, 0, 0});
    rocket.velocity = {1.0e9f, 0, 0};
    rocket.angular_velocity = {0, 1.0e9f, 0};
    const auto r = world.create_body(rocket).value();
    world.step(1.0f / 60.0f);
    const auto s = world.get_body_state(r).value();
    POKO_CHECK(math::length(s.velocity) <= 1000.5f);
    POKO_CHECK(math::length(s.angular_velocity) <= 300.5f);
    POKO_CHECK(std::isfinite(s.position.x));
    POKO_CHECK_EQ(world.nonfinite_recoveries(), 0u);

    // Two boxes overlapping by 0.9 are pushed apart gradually, not teleported.
    CustomPhysicsWorld deep;
    POKO_CHECK(deep.initialize(zero_g));
    deep.set_ground(false);
    BodyDesc anchor = box({0, 0, 0});
    anchor.type = BodyType::Static;
    deep.create_body(anchor);
    const auto mover = deep.create_body(box({0.1f, 0, 0})).value();
    deep.step(1.0f / 60.0f);
    POKO_CHECK(deep.get_body_state(mover).value().position.x < 0.1f + 0.25f);
}

POKO_TEST_MAIN()
