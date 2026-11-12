#include "poko_test_harness.h"

#include <cmath>

#include "poko/renderer/camera.h"

POKO_TEST(perspective_projection_is_right_handed) {
    poko::Camera camera;
    camera.set_perspective(poko::math::kHalfPi, 1.0f, 0.1f, 100.0f);

    const auto projection = camera.projection_matrix();
    POKO_CHECK(projection.at(0, 0) > 0.0f);
    POKO_CHECK(projection.at(1, 1) > 0.0f);
    POKO_CHECK(projection.at(3, 2) < 0.0f);

    const auto ndc = poko::math::transform(projection, poko::math::Vec3{0.0f, 0.0f, -1.0f}, 1.0f);
    POKO_CHECK(poko::math::nearly_equal(ndc.x, 0.0f));
    POKO_CHECK(poko::math::nearly_equal(ndc.y, 0.0f));
}

POKO_TEST(invalid_perspective_falls_back_to_safe_defaults) {
    poko::Camera camera;
    camera.set_perspective(0.0f, 0.0f, 0.0f, 0.0f);
    POKO_CHECK(camera.near_plane() > 0.0f);
    POKO_CHECK(camera.far_plane() > camera.near_plane());
    const auto projection = camera.projection_matrix();
    POKO_CHECK(projection.at(0, 0) > 0.0f);
    POKO_CHECK(std::isfinite(projection.at(2, 2)));
}

POKO_TEST(view_matrix_translates_the_world) {
    poko::Camera camera;
    camera.set_position(poko::math::Vec3{0.0f, 0.0f, 5.0f});
    camera.set_target(poko::math::Vec3{0.0f, 0.0f, 0.0f});
    camera.set_up(poko::math::Vec3{0.0f, 1.0f, 0.0f});

    const auto point =
        poko::math::transform_point(camera.view_matrix(), poko::math::Vec3{0.0f, 0.0f, 0.0f});
    POKO_CHECK(poko::math::nearly_equal(point.x, 0.0f));
    POKO_CHECK(poko::math::nearly_equal(point.y, 0.0f));
    POKO_CHECK(poko::math::nearly_equal(point.z, -5.0f, 1.0e-4f));
}

POKO_TEST(view_projection_composes) {
    poko::Camera camera;
    camera.set_perspective(poko::math::kHalfPi, 1.5f, 0.1f, 50.0f);
    camera.set_position(poko::math::Vec3{0.0f, 1.0f, 3.0f});
    const auto view_projection = camera.view_projection();
    POKO_CHECK(view_projection.at(0, 0) != 0.0f);
    POKO_CHECK(std::isfinite(view_projection.at(2, 3)));
}

POKO_TEST(orthographic_mode_has_no_perspective_divide) {
    poko::Camera camera;
    camera.set_orthographic(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
    POKO_CHECK(camera.orthographic());
    const auto projection = camera.projection_matrix();
    POKO_CHECK(poko::math::nearly_equal(projection.at(3, 3), 1.0f));
    POKO_CHECK_EQ(projection.at(3, 2), 0.0f);
}

POKO_TEST_MAIN()
