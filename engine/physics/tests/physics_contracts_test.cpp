#include "poko_test_harness.h"

#include "poko/core/physics_contracts.h"

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

    // Mass is ignored for static bodies, so zero mass is legal there only.
    poko::BodyDesc fixed;
    fixed.type = poko::BodyType::Static;
    fixed.mass = 0.0f;
    POKO_CHECK(poko::valid(fixed));
    fixed.type = poko::BodyType::Kinematic;
    POKO_CHECK(!poko::valid(fixed));

    // Triggers are non-colliding volumes: never dynamic.
    poko::BodyDesc trigger;
    trigger.is_trigger = true;
    POKO_CHECK(!poko::valid(trigger));
    trigger.type = poko::BodyType::Static;
    POKO_CHECK(poko::valid(trigger));
}

POKO_TEST(joint_descriptors_validate) {
    poko::JointDesc joint;
    POKO_CHECK(!poko::valid(joint)); // body_a is unset
    joint.body_a = poko::make_handle(0, 0);
    POKO_CHECK(poko::valid(joint)); // world-anchored ball joint
    joint.body_b = poko::make_handle(0, 0);
    POKO_CHECK(!poko::valid(joint)); // same body twice
    joint.body_b = poko::make_handle(1, 0);
    POKO_CHECK(poko::valid(joint));
    joint.kind = poko::JointKind::Hinge;
    joint.axis_a = {0.0f, 0.0f, 0.0f};
    POKO_CHECK(!poko::valid(joint));
    joint.axis_a = {0.0f, 1.0f, 0.0f};
    POKO_CHECK(poko::valid(joint));
    joint.kind = poko::JointKind::Distance;
    joint.distance = 0.0f;
    POKO_CHECK(!poko::valid(joint));
    joint.distance = -1.0f; // "use current separation"
    POKO_CHECK(poko::valid(joint));
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
