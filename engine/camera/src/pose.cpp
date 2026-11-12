#include "poko/camera/rig.h"

#include <cmath>

namespace poko::camera {
namespace {

math::Vec3 spherical_offset(float yaw, float pitch, float distance) {
    const float cp = std::cos(pitch);
    return {distance * cp * std::sin(yaw), distance * std::sin(pitch),
            distance * cp * std::cos(yaw)};
}

} // namespace

math::Vec3 CameraRig::forward() const noexcept {
    const float cp = std::cos(pitch_);
    return {-cp * std::sin(yaw_), -std::sin(pitch_), -cp * std::cos(yaw_)};
}

CameraPose CameraRig::pose() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    CameraPose out;
    out.up = {0.0f, 1.0f, 0.0f};
    switch (mode_) {
    case CameraMode::Orbit:
        out.position = target_ + spherical_offset(yaw_, pitch_, distance_);
        out.look_target = target_;
        break;
    case CameraMode::TopDown:
        out.position = {target_.x, target_.y + distance_, target_.z};
        out.look_target = target_;
        break;
    case CameraMode::Fly:
        out.position = fly_position_;
        out.look_target = fly_position_ + forward();
        break;
    }
    return out;
}

} // namespace poko::camera
