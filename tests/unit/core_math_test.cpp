#include "poko_test_harness.h"

#include <cmath>

#include "poko_math.h"

using namespace poko::math;

POKO_TEST(constants_and_conversions) {
    POKO_CHECK(nearly_equal(to_radians(180.0f), kPi));
    POKO_CHECK(nearly_equal(to_degrees(kPi), 180.0f));
    POKO_CHECK_EQ(clamp(2.0f, 0.0f, 1.0f), 1.0f);
    POKO_CHECK_EQ(clamp(-1.0f, 0.0f, 1.0f), 0.0f);
    POKO_CHECK_EQ(lerp(0.0f, 10.0f, 0.25f), 2.5f);
    POKO_CHECK(nearly_equal(1.0f, 1.0f + kEpsilon / 2.0f));
    POKO_CHECK(!nearly_equal(1.0f, 1.1f));
}

POKO_TEST(vector_operations) {
    const Vec3 a{1.0f, 2.0f, 3.0f};
    const Vec3 b{4.0f, 5.0f, 6.0f};
    POKO_CHECK_EQ(dot(a, b), 32.0f);

    const Vec3 c = cross(Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});
    POKO_CHECK(nearly_equal(c.x, 0.0f));
    POKO_CHECK(nearly_equal(c.y, 0.0f));
    POKO_CHECK(nearly_equal(c.z, 1.0f));

    POKO_CHECK(nearly_equal(length(Vec3{3.0f, 4.0f, 0.0f}), 5.0f));
    POKO_CHECK_EQ(length_squared(Vec3{3.0f, 4.0f, 0.0f}), 25.0f);
    POKO_CHECK(nearly_equal(distance(Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 5.0f}), 5.0f));
    POKO_CHECK(nearly_equal(length(normalize(Vec3{0.0f, 0.0f, 5.0f})), 1.0f));
    POKO_CHECK_EQ(length(normalize(Vec3{})), 0.0f);
}

POKO_TEST(matrices_are_row_major_and_compose) {
    const Mat4 identity = Mat4::identity();
    POKO_CHECK_EQ(identity.at(0, 0), 1.0f);
    POKO_CHECK_EQ(identity.at(0, 1), 0.0f);
    POKO_CHECK_EQ(identity.at(3, 3), 1.0f);

    const Vec3 p{1.0f, 2.0f, 3.0f};
    const Mat4 translation = Mat4::translation(Vec3{5.0f, 6.0f, 7.0f});
    const Vec3 moved = transform_point(translation, p);
    POKO_CHECK(nearly_equal(moved.x, 6.0f));
    POKO_CHECK(nearly_equal(moved.y, 8.0f));
    POKO_CHECK(nearly_equal(moved.z, 10.0f));

    const Vec3 direction = transform_direction(translation, p);
    POKO_CHECK(nearly_equal(direction.x, 1.0f));
    POKO_CHECK(nearly_equal(direction.y, 2.0f));
    POKO_CHECK(nearly_equal(direction.z, 3.0f));

    const Mat4 combined =
        multiply(Mat4::translation(Vec3{1.0f, 0.0f, 0.0f}),
                 Mat4::scaling(Vec3{2.0f, 2.0f, 2.0f}));
    const Vec3 q = transform_point(combined, Vec3{1.0f, 1.0f, 1.0f});
    POKO_CHECK(nearly_equal(q.x, 3.0f));
    POKO_CHECK(nearly_equal(q.y, 2.0f));
    POKO_CHECK(nearly_equal(q.z, 2.0f));
}

POKO_TEST(rotation_and_quaternions) {
    const Vec3 rotated_x = transform_direction(Mat4::rotation_y(kHalfPi),
                                               Vec3{1.0f, 0.0f, 0.0f});
    POKO_CHECK(std::fabs(rotated_x.x) < 1.0e-4f);
    POKO_CHECK(nearly_equal(rotated_x.z, -1.0f, 1.0e-4f));

    const Quat q = quat_from_axis_angle(Vec3{0.0f, 1.0f, 0.0f}, kHalfPi);
    POKO_CHECK(nearly_equal(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w, 1.0f));
    const Vec3 quat_rotated = rotate(q, Vec3{1.0f, 0.0f, 0.0f});
    POKO_CHECK(std::fabs(quat_rotated.x) < 1.0e-4f);
    POKO_CHECK(nearly_equal(quat_rotated.z, -1.0f, 1.0e-4f));

    const Quat inverse = conjugate(q);
    const Vec3 restored = rotate(inverse, quat_rotated);
    POKO_CHECK(nearly_equal(restored.x, 1.0f, 1.0e-4f));
    POKO_CHECK(nearly_equal(restored.y, 0.0f, 1.0e-4f));
    POKO_CHECK(nearly_equal(restored.z, 0.0f, 1.0e-4f));
}

POKO_TEST_MAIN()
