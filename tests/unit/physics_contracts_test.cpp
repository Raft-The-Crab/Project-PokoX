#include "poko_test_harness.h"

#include "poko_physics_contracts.h"

POKO_TEST(shape_descriptors_validate) {
    poko::ShapeDesc sphere;
    POKO_CHECK(poko::valid(sphere));
    sphere.radius = 0.0f;
    POKO_CHECK(!poko::valid(sphere));

    sphere.kind = poko::ShapeKind::Box;
    sphere.radius = 0.5f;
    POKO_CHECK(poko::valid(sphere));
    sphere.half_extents = {0.5f, 0.0f, 0.5f};
    POKO_CHECK(!poko::valid(sphere));

    sphere.kind = poko::ShapeKind::Capsule;
    sphere.half_extents = {0.5f, 0.5f, 0.5f};
    POKO_CHECK(poko::valid(sphere));
    sphere.half_height = -1.0f;
    POKO_CHECK(!poko::valid(sphere));
}

POKO_TEST(body_descriptors_validate) {
    poko::BodyDesc body; // defaults are a valid 1 kg sphere
    POKO_CHECK(poko::valid(body));
    body.mass = 0.0f;
    POKO_CHECK(!poko::valid(body));
    body.mass = 1.0f;
    body.friction = 1.5f;
    POKO_CHECK(!poko::valid(body));
}

POKO_TEST(rays_validate) {
    poko::Ray ray;
    POKO_CHECK(!poko::valid(ray));
    ray.direction = {0.0f, 0.0f, -1.0f};
    POKO_CHECK(poko::valid(ray));
}

POKO_TEST(default_handles_are_invalid) {
    POKO_CHECK(!poko::is_valid(poko::BodyHandle{}));
}

POKO_TEST_MAIN()
