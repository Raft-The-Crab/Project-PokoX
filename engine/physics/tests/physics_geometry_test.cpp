#include "poko_test_harness.h"

#include <cmath>
#include <limits>
#include <vector>

#include "poko/core/physics_contracts.h"
#include "poko/physics/custom_world.h"

namespace {
using namespace poko;
using math::Vec3;

std::vector<Vec3> cube_points(float h = 0.5f) {
    std::vector<Vec3> p;
    for (int i = 0; i < 8; ++i) {
        p.push_back({(i & 1) ? h : -h, (i & 2) ? h : -h, (i & 4) ? h : -h});
    }
    return p;
}

// Square pyramid whose volume centroid is the origin: base at y = -0.25, apex at y = 0.75.
std::vector<Vec3> pyramid_points() {
    return {{-0.5f, -0.25f, -0.5f}, {0.5f, -0.25f, -0.5f}, {0.5f, -0.25f, 0.5f},
            {-0.5f, -0.25f, 0.5f},  {0.0f, 0.75f, 0.0f}};
}

struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<std::uint32_t> indices;
};

// A flat grid on y = 0 spanning [-n, n], `cells` x `cells` squares.
Mesh floor_mesh(float n, int cells, float tilt_z = 0.0f) {
    Mesh m;
    for (int z = 0; z <= cells; ++z) {
        for (int x = 0; x <= cells; ++x) {
            const float fx = -n + 2.0f * n * static_cast<float>(x) / static_cast<float>(cells);
            const float fz = -n + 2.0f * n * static_cast<float>(z) / static_cast<float>(cells);
            m.vertices.push_back({fx, -fx * tilt_z, fz}); // slope falls toward +x when tilt_z > 0
        }
    }
    const auto id = [&](int x, int z) { return static_cast<std::uint32_t>(z * (cells + 1) + x); };
    for (int z = 0; z < cells; ++z) {
        for (int x = 0; x < cells; ++x) {
            m.indices.insert(m.indices.end(), {id(x, z), id(x, z + 1), id(x + 1, z)});
            m.indices.insert(m.indices.end(), {id(x + 1, z), id(x, z + 1), id(x + 1, z + 1)});
        }
    }
    return m;
}

struct World {
    CustomPhysicsWorld sim;
    World() { POKO_CHECK(sim.initialize(PhysicsWorldDesc{})); }
    CustomPhysicsWorld* operator->() { return &sim; }

    GeometryHandle hull(const std::vector<Vec3>& pts) {
        const auto g = sim.create_convex_hull(pts);
        POKO_CHECK(g.ok());
        return g.value();
    }
    BodyHandle hull_body(GeometryHandle g, Vec3 pos, BodyType type = BodyType::Dynamic,
                         math::Quat rot = math::quat_identity()) {
        BodyDesc d;
        d.type = type;
        d.shape.kind = ShapeKind::ConvexHull;
        d.shape.geometry = g;
        d.position = pos;
        d.rotation = rot;
        d.friction = 0.6f;
        const auto b = sim.create_body(d);
        POKO_CHECK(b.ok());
        return b.value();
    }
    BodyHandle mesh_body(const Mesh& m, Vec3 pos = {}) {
        const auto g = sim.create_triangle_mesh(m.vertices, m.indices);
        POKO_CHECK(g.ok());
        BodyDesc d;
        d.type = BodyType::Static;
        d.shape.kind = ShapeKind::TriangleMesh;
        d.shape.geometry = g.value();
        d.position = pos;
        d.friction = 0.6f;
        const auto b = sim.create_body(d);
        POKO_CHECK(b.ok());
        return b.value();
    }
    BodyHandle primitive(ShapeKind kind, Vec3 pos, float size = 0.5f) {
        BodyDesc d;
        d.shape.kind = kind;
        d.shape.radius = size;
        d.shape.half_extents = {size, size, size};
        d.shape.half_height = size;
        d.position = pos;
        d.friction = 0.6f;
        return sim.create_body(d).value();
    }
    BodyState state(BodyHandle h) { return sim.get_body_state(h).value(); }
    void run(float seconds) {
        for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i) sim.step(1.0f / 60.0f);
    }
};

float up_y(const math::Quat& q) {
    return math::rotate(q, Vec3{0, 1, 0}).y;
}
} // namespace

POKO_TEST(hull_construction_validates_input) {
    CustomPhysicsWorld sim;
    POKO_CHECK(sim.initialize(PhysicsWorldDesc{}));
    const float nan = std::numeric_limits<float>::quiet_NaN();

    POKO_CHECK(sim.create_convex_hull(cube_points()).ok());
    // Interior and duplicate points are fine.
    auto padded = cube_points();
    padded.push_back({0, 0, 0});
    padded.push_back({0.1f, 0.2f, -0.1f});
    padded.push_back(padded[0]);
    POKO_CHECK(sim.create_convex_hull(padded).ok());

    POKO_CHECK(!sim.create_convex_hull(std::vector<Vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}).ok());    // too few
    POKO_CHECK(!sim.create_convex_hull(std::vector<Vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}}).ok()); // planar
    POKO_CHECK(!sim.create_convex_hull(std::vector<Vec3>{{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}}).ok()); // line
    auto bad = cube_points();
    bad[3].y = nan;
    POKO_CHECK(!sim.create_convex_hull(bad).ok());
    POKO_CHECK(!sim.create_convex_hull(std::vector<Vec3>(300, Vec3{1, 2, 3})).ok()); // too many / identical
}

POKO_TEST(hull_cube_behaves_like_a_box) {
    World w;
    const auto cube = w.hull(cube_points());
    const auto body = w.hull_body(cube, {0, 2.0f, 0}, BodyType::Dynamic,
                                  math::quat_from_axis_angle({0.3f, 0, 1}, 0.5f));
    w.run(4.0f);
    const auto s = w.state(body);
    POKO_CHECK(std::abs(s.position.y - 0.5f) < 0.03f);
    POKO_CHECK(up_y(s.rotation) > 0.995f);
    POKO_CHECK(w->is_sleeping(body));
}

POKO_TEST(hull_stacks_and_collides_with_every_primitive) {
    World w;
    const auto cube = w.hull(cube_points());
    const auto bottom = w.hull_body(cube, {0, 0.5f, 0});
    const auto top = w.hull_body(cube, {0, 1.6f, 0});
    const auto ball = w.primitive(ShapeKind::Sphere, {3, 3, 0}, 0.4f);
    const auto box = w.primitive(ShapeKind::Box, {-3, 3, 0}, 0.4f);
    const auto pill = w.primitive(ShapeKind::Capsule, {0, 6, 3}, 0.3f);
    // Static hull platform the primitives land on.
    const auto platform = w.hull(cube_points(1.0f));
    w.hull_body(platform, {3, 1.0f, 0}, BodyType::Static);
    w.hull_body(platform, {-3, 1.0f, 0}, BodyType::Static);
    w.run(5.0f);
    POKO_CHECK(std::abs(w.state(top).position.y - 1.5f) < 0.06f);
    POKO_CHECK(std::abs(w.state(bottom).position.y - 0.5f) < 0.04f);
    POKO_CHECK(std::abs(w.state(ball).position.y - 2.4f) < 0.04f);  // platform top 2.0 + radius
    POKO_CHECK(std::abs(w.state(box).position.y - 2.4f) < 0.04f);
    // The capsule fell on the ground (not on the platform) and lies somewhere sane.
    POKO_CHECK(w.state(pill).position.y < 1.5f);
}

POKO_TEST(non_box_hull_settles_on_a_face) {
    World w;
    const auto pyramid = w.hull(pyramid_points());
    const auto body = w.hull_body(pyramid, {0, 3.0f, 0}, BodyType::Dynamic,
                                  math::quat_from_axis_angle({1, 0, 0.3f}, 0.4f));
    w.run(6.0f);
    const auto s = w.state(body);
    // Base-down: origin 0.25 above the ground, apex straight up.
    POKO_CHECK(std::abs(s.position.y - 0.25f) < 0.04f);
    POKO_CHECK(up_y(s.rotation) > 0.98f);
    POKO_CHECK(w->is_sleeping(body));
}

POKO_TEST(hull_raycast_hits_faces_and_misses) {
    World w;
    const auto cube = w.hull(cube_points());
    w.hull_body(cube, {0, 0, -5}, BodyType::Static, math::quat_from_axis_angle({0, 1, 0}, 0.3f));
    RayHit hit;
    POKO_CHECK(w->raycast(Ray{{0, 0, 0}, {0, 0, -1}}, 20.0f, 0xFFFFFFFFu, hit));
    // A cube turned 0.3 rad about Y: the face nearest the ray origin is at distance ~4.5 / cos-ish.
    POKO_CHECK(hit.distance > 4.0f && hit.distance < 5.0f);
    POKO_CHECK(hit.normal.z > 0.8f);
    POKO_CHECK(!w->raycast(Ray{{2, 0, 0}, {0, 0, -1}}, 20.0f, 0xFFFFFFFFu, hit));
}

POKO_TEST(mesh_floor_supports_every_shape) {
    World w;
    w.mesh_body(floor_mesh(10.0f, 10));
    w->set_ground(false); // only the mesh holds things up
    const auto ball = w.primitive(ShapeKind::Sphere, {-4, 2, 0}, 0.4f);
    const auto box = w.primitive(ShapeKind::Box, {-2, 2, 0}, 0.5f);
    const auto pill = w.primitive(ShapeKind::Capsule, {0, 2, 0}, 0.3f);
    const auto hullbody = w.hull_body(w.hull(pyramid_points()), {3, 2, 0});
    w.run(5.0f);
    POKO_CHECK(std::abs(w.state(ball).position.y - 0.4f) < 0.03f);
    POKO_CHECK(std::abs(w.state(box).position.y - 0.5f) < 0.03f);
    POKO_CHECK(std::abs(w.state(pill).position.y - 0.6f) < 0.12f); // upright or lying
    POKO_CHECK(std::abs(w.state(hullbody).position.y - 0.25f) < 0.05f);
    POKO_CHECK(w->is_sleeping(box));
}

POKO_TEST(mesh_ramp_slides_a_sphere_downhill_and_ignores_the_far_side) {
    World w;
    w->set_ground(false);
    w.mesh_body(floor_mesh(10.0f, 20, 0.35f)); // falls toward +x
    BodyDesc d;
    d.shape.kind = ShapeKind::Sphere;
    d.shape.radius = 0.3f;
    d.position = {-2.0f, 1.2f, 0};
    d.friction = 0.05f;
    const auto ball = w->create_body(d).value();
    w.run(2.0f);
    const auto s = w.state(ball);
    POKO_CHECK(s.position.x > 0.5f);  // rolled/slid down the slope
    POKO_CHECK(s.position.y < 1.0f);
    POKO_CHECK(std::abs(s.position.z) < 0.2f);
}

POKO_TEST(ccd_stops_fast_bodies_at_a_thin_mesh) {
    for (const bool ccd : {false, true}) {
        World w;
        w->set_ground(false);
        w->set_ccd_enabled(ccd);
        PhysicsWorldDesc dummy;
        (void)dummy;
        // One quad standing at z = -7.5, facing +z (single sided geometry, two triangles).
        Mesh wall;
        wall.vertices = {{-5, -5, -7.5f}, {5, -5, -7.5f}, {5, 5, -7.5f}, {-5, 5, -7.5f}};
        wall.indices = {0, 1, 2, 0, 2, 3};
        w.mesh_body(wall);
        BodyDesc d;
        d.shape.kind = ShapeKind::Sphere;
        d.shape.radius = 0.05f;
        d.velocity = {0, 0, -300};
        const auto b = w->create_body(d).value();
        // zero gravity so the shot flies straight
        auto s = w.state(b);
        s.velocity = {0, 0, -300};
        POKO_CHECK(w->set_body_state(b, s).ok());
        for (int i = 0; i < 20; ++i) {
            auto cur = w.state(b);
            cur.velocity.y = 0.0f; // cancel gravity's pull for a clean shot
            w->set_body_state(b, cur);
            w->step(1.0f / 60.0f);
        }
        const float z = w.state(b).position.z;
        if (ccd) {
            POKO_CHECK(z > -7.5f);
            POKO_CHECK(w->ccd_hit_count() > 0u);
        } else {
            POKO_CHECK(z < -7.6f);
        }
    }
}

POKO_TEST(mesh_raycast_and_big_mesh_performance_shape) {
    World w;
    const auto big = floor_mesh(50.0f, 60); // 7200 triangles
    w.mesh_body(big);
    RayHit hit;
    POKO_CHECK(w->raycast(Ray{{3.3f, 5.0f, -2.2f}, {0, -1, 0}}, 20.0f, 0xFFFFFFFFu, hit));
    POKO_CHECK(std::abs(hit.distance - 5.0f) < 1e-3f);
    POKO_CHECK(hit.normal.y > 0.99f);
    POKO_CHECK(!w->raycast(Ray{{80, 5, 0}, {0, -1, 0}}, 20.0f, 0xFFFFFFFFu, hit)); // off the mesh
    // A box on this mesh only tests triangles under it and still rests correctly.
    w->set_ground(false);
    const auto box = w.primitive(ShapeKind::Box, {10, 1, 10}, 0.5f);
    w.run(3.0f);
    POKO_CHECK(std::abs(w.state(box).position.y - 0.5f) < 0.03f);
}

POKO_TEST(geometry_lifetime_and_misuse_are_rejected) {
    World w;
    const auto g = w.hull(cube_points());
    const auto b = w.hull_body(g, {0, 3, 0});
    POKO_CHECK(!w->destroy_geometry(g).ok());      // still in use
    POKO_CHECK(w->destroy_body(b).ok());
    POKO_CHECK(w->destroy_geometry(g).ok());
    POKO_CHECK(!w->destroy_geometry(g).ok());      // already gone

    BodyDesc stale;
    stale.shape.kind = ShapeKind::ConvexHull;
    stale.shape.geometry = g;                      // stale handle
    POKO_CHECK(!w->create_body(stale).ok());

    const auto mesh = w->create_triangle_mesh(floor_mesh(1, 1).vertices, floor_mesh(1, 1).indices);
    POKO_CHECK(mesh.ok());
    BodyDesc dynamic_mesh;
    dynamic_mesh.shape.kind = ShapeKind::TriangleMesh;
    dynamic_mesh.shape.geometry = mesh.value();
    POKO_CHECK(!w->create_body(dynamic_mesh).ok()); // meshes are static only
    BodyDesc wrong_kind;
    wrong_kind.type = BodyType::Static;
    wrong_kind.shape.kind = ShapeKind::ConvexHull;
    wrong_kind.shape.geometry = mesh.value();       // a mesh used as a hull
    POKO_CHECK(!w->create_body(wrong_kind).ok());

    POKO_CHECK(!w->create_triangle_mesh(std::vector<Vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}},
                                        std::vector<std::uint32_t>{0, 1, 9}).ok()); // bad index
    POKO_CHECK(!w->create_triangle_mesh(std::vector<Vec3>{{0, 0, 0}, {1, 0, 0}, {2, 0, 0}},
                                        std::vector<std::uint32_t>{0, 1, 2}).ok()); // all degenerate
}

POKO_TEST(random_hull_pile_stays_finite_contained_and_settles) {
    World w;
    // Arena: ground plus four static walls.
    const auto wall = [&](Vec3 pos, Vec3 half) {
        BodyDesc d;
        d.type = BodyType::Static;
        d.shape.kind = ShapeKind::Box;
        d.shape.half_extents = half;
        d.position = pos;
        w->create_body(d);
    };
    wall({4, 2, 0}, {0.25f, 2, 4});
    wall({-4, 2, 0}, {0.25f, 2, 4});
    wall({0, 2, 4}, {4, 2, 0.25f});
    wall({0, 2, -4}, {4, 2, 0.25f});

    std::uint32_t seed = 12345u;
    const auto rnd = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f;
    };
    std::vector<BodyHandle> bodies;
    for (int i = 0; i < 40; ++i) {
        std::vector<Vec3> pts;
        const int n = 6 + static_cast<int>(rnd() * 6.0f);
        for (int k = 0; k < n; ++k) {
            pts.push_back({(rnd() - 0.5f) * 0.7f, (rnd() - 0.5f) * 0.7f, (rnd() - 0.5f) * 0.7f});
        }
        const auto g = w->create_convex_hull(pts);
        if (!g.ok()) continue; // an unlucky near-planar cloud is a legitimate rejection
        const auto q = math::quat_from_axis_angle({rnd() + 0.1f, rnd(), rnd()}, rnd() * 6.0f);
        bodies.push_back(w.hull_body(g.value(), {(rnd() - 0.5f) * 5.0f, 1.0f + 0.9f * static_cast<float>(i),
                                                (rnd() - 0.5f) * 5.0f}, BodyType::Dynamic, q));
    }
    POKO_CHECK(bodies.size() >= 30u);
    w.run(25.0f);
    POKO_CHECK_EQ(w->nonfinite_recoveries(), 0u);
    std::size_t sleeping = 0;
    for (const auto b : bodies) {
        const auto s = w.state(b);
        POKO_CHECK(std::isfinite(s.position.x + s.position.y + s.position.z));
        // The origin may lie outside a random cloud, so allow its extent below ground.
        POKO_CHECK(s.position.y > -0.4f && s.position.y < 6.0f);
        POKO_CHECK(std::abs(s.position.x) < 4.0f && std::abs(s.position.z) < 4.0f);
        if (w->is_sleeping(b)) ++sleeping;
    }
    POKO_CHECK(sleeping * 4 >= bodies.size() * 3); // at least three quarters have come to rest
}

POKO_TEST_MAIN()
