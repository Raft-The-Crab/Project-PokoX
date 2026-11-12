#include "poko_test_harness.h"

#include <cmath>

#include "poko_camera_rig.h"

POKO_TEST(orbit_pose_orbits_target) {
    poko::camera::CameraRig rig({0.0f, 1.0f, 0.0f});
    rig.set_distance(4.0f);

    const auto p0 = rig.pose();
    // yaw=0,pitch=0.25: offset = (0, 4*sin(.25), 4*cos(.25)) ≈ (0,0.97,3.87)
    POKO_CHECK(p0.position.y > 1.0f);
    POKO_CHECK(p0.position.z > 1.0f);
    POKO_CHECK_EQ(p0.look_target.y, 1.0f);

    rig.orbit(1.5707963f, 0.0f); // +90 deg yaw
    const auto p1 = rig.pose();
    POKO_CHECK(p1.position.x > p0.position.x);
}

POKO_TEST(pitch_and_distance_clamped) {
    poko::camera::CameraRig rig;
    rig.orbit(0.0f, 10.0f);
    POKO_CHECK(rig.pose().position.y <=
               rig.target().y + rig.distance() * std::sin(poko::camera::CameraRig::kMaxPitch));
    rig.dolly(-1000.0f);
    POKO_CHECK_EQ(rig.distance(), poko::camera::CameraRig::kMinDistance);
}

POKO_TEST(topdown_looks_straight_down) {
    poko::camera::CameraRig rig({5.0f, 0.0f, 5.0f});
    rig.set_mode(poko::camera::CameraMode::TopDown);
    rig.set_distance(10.0f);
    const auto p = rig.pose();
    POKO_CHECK_EQ(p.position.y, 10.0f);
    POKO_CHECK_EQ(p.look_target.x, 5.0f);
}

POKO_TEST(fly_moves_and_looks) {
    poko::camera::CameraRig rig;
    rig.set_mode(poko::camera::CameraMode::Fly);
    rig.fly_move({0.0f, 0.0f, 2.0f}); // +z = along forward (-Z world)
    const auto p = rig.pose();
    POKO_CHECK(p.position.z < 0.0f);
    rig.fly_look(0.0f, 0.5f);
    POKO_CHECK(rig.pose().position.z < -1.0f);
}

POKO_TEST_MAIN()
