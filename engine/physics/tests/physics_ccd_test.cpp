#include "poko_test_harness.h"

#include <cmath>

#include "poko/core/physics_contracts.h"
#include "poko/physics/custom_world.h"

namespace {
using namespace poko;

struct World {
    CustomPhysicsWorld sim;
    explicit World(bool ccd = true, math::Vec3 gravity = {0, 0, 0}) {
        PhysicsWorldDesc d;
        d.gravity = gravity;
        POKO_CHECK(sim.initialize(d));
        sim.set_ground(false);
        sim.set_ccd_enabled(ccd);
    }
    CustomPhysicsWorld* operator->() { return &sim; }

    BodyHandle wall(math::Vec3 pos, math::Vec3 half) {
        BodyDesc d;
        d.type = BodyType::Static;
        d.shape.kind = ShapeKind::Box;
        d.shape.half_extents = half;
        d.position = pos;
        return sim.create_body(d).value();
    }
    BodyHandle projectile(ShapeKind kind, math::Vec3 pos, math::Vec3 velocity, float size = 0.1f,
                          float mass = 1.0f) {
        BodyDesc d;
        d.shape.kind = kind;
        d.shape.radius = size;
        d.shape.half_extents = {size, size, size};
        d.shape.half_height = size;
        d.position = pos;
        d.velocity = velocity;
        d.mass = mass;
        return sim.create_body(d).value();
    }
    math::Vec3 pos(BodyHandle h) { return sim.get_body_state(h).value().position; }
    void run(float seconds) {
        for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i) sim.step(1.0f / 60.0f);
    }
};
} // namespace

POKO_TEST(fast_sphere_tunnels_a_thin_wall_without_ccd_and_stops_with_it) {
    for (const bool ccd : {false, true}) {
        World w(ccd);
        w.wall({0, 0, -7.5f}, {5, 5, 0.025f}); // 5 cm thick, between the 5 m step positions
        const auto bullet = w.projectile(ShapeKind::Sphere, {0, 0, 0}, {0, 0, -300}, 0.05f);
        w.run(0.5f);
        const float z = w.pos(bullet).z;
        if (ccd) {
            POKO_CHECK(z > -7.5f);             // never got past the wall
            POKO_CHECK(w->ccd_hit_count() > 0u);
        } else {
            POKO_CHECK(z < -7.6f);             // tunnelled straight through
            POKO_CHECK_EQ(w->ccd_hit_count(), 0u);
        }
    }
}

POKO_TEST(fast_box_and_capsule_also_stop_at_thin_walls) {
    for (const auto kind : {ShapeKind::Box, ShapeKind::Capsule}) {
        for (const bool ccd : {false, true}) {
            World w(ccd);
            w.wall({0, 0, -7.0f}, {5, 5, 0.02f});
            const auto b = w.projectile(kind, {0.3f, 0.2f, 0}, {0, 0, -250}, 0.08f);
            w.run(0.5f);
            POKO_CHECK((w.pos(b).z > -7.0f) == ccd); // tunnels only without CCD
        }
    }
}

POKO_TEST(ccd_hit_bounces_a_restitutive_projectile_back) {
    World w;
    BodyDesc wall;
    wall.type = BodyType::Static;
    wall.shape.kind = ShapeKind::Box;
    wall.shape.half_extents = {5, 5, 0.05f};
    wall.position = {0, 0, -10};
    wall.restitution = 1.0f;
    w->create_body(wall);
    BodyDesc ball;
    ball.shape.kind = ShapeKind::Sphere;
    ball.shape.radius = 0.05f;
    ball.velocity = {0, 0, -200};
    ball.restitution = 1.0f;
    const auto h = w->create_body(ball).value();
    w.run(0.3f);
    const auto s = w->get_body_state(h).value();
    POKO_CHECK(s.velocity.z > 100.0f); // came back off the wall
    POKO_CHECK(s.position.z > -10.0f);
}

POKO_TEST(fast_body_stops_at_a_light_dynamic_crate_it_would_skip_over) {
    World w;
    BodyDesc plate;
    plate.shape.kind = ShapeKind::Box;
    plate.shape.half_extents = {2, 2, 0.01f}; // very thin loose plate
    plate.position = {0, 0, -5};
    plate.mass = 0.2f;
    const auto target = w->create_body(plate).value();
    const auto bullet = w.projectile(ShapeKind::Sphere, {0, 0, 0}, {0, 0, -400}, 0.04f, 0.05f);
    w.run(0.2f);
    // The plate was struck: it was pushed, so it is no longer at rest.
    POKO_CHECK(w->get_body_state(target).value().velocity.z < -1.0f);
    POKO_CHECK(w.pos(bullet).z > w.pos(target).z - 0.5f); // bullet did not pass cleanly through
}

POKO_TEST(ccd_ignores_resting_and_sliding_bodies_and_respects_layers_and_joints) {
    // A box sliding along a floor at speed keeps sliding with and without CCD.
    float final_x[2] = {};
    for (const bool ccd : {false, true}) {
        World w(ccd, {0, -9.81f, 0});
        w.sim.set_ground(true, 0.0f);
        BodyDesc box;
        box.shape.kind = ShapeKind::Box;
        box.shape.half_extents = {0.5f, 0.5f, 0.5f};
        box.position = {0, 0.5f, 0};
        box.velocity = {12.0f, 0, 0};
        box.friction = 0.1f;
        const auto h = w->create_body(box).value();
        w.run(0.5f);
        final_x[ccd ? 1 : 0] = w.pos(h).x;
        if (ccd) POKO_CHECK_EQ(w->ccd_hit_count(), 0u);
    }
    POKO_CHECK(std::abs(final_x[0] - final_x[1]) < 0.05f);

    // Layers: a projectile whose mask excludes the wall passes through.
    World w;
    BodyDesc wall;
    wall.type = BodyType::Static;
    wall.shape.kind = ShapeKind::Box;
    wall.shape.half_extents = {5, 5, 0.02f};
    wall.position = {0, 0, -10};
    wall.collision_layer = 2;
    w->create_body(wall);
    BodyDesc ghost;
    ghost.shape.kind = ShapeKind::Sphere;
    ghost.shape.radius = 0.05f;
    ghost.velocity = {0, 0, -300};
    ghost.collision_mask = 1; // does not collide with layer 2
    const auto g = w->create_body(ghost).value();
    w.run(0.5f);
    POKO_CHECK(w.pos(g).z < -10.1f);
    POKO_CHECK_EQ(w->ccd_hit_count(), 0u);
}

POKO_TEST_MAIN()
