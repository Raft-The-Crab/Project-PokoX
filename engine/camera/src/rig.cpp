#include "poko/camera/rig.h"

#include <cmath>

namespace poko::camera {
namespace {

math::Vec3 spherical_offset(float yaw, float pitch, float distance) {
    const float cp = std::cos(pitch);
    return {distance * cp * std::sin(yaw), distance * std::sin(pitch),
            distance * cp * std::cos(yaw)};
}

math::Vec3 clamp_vec(const math::Vec3& v, float lo, float hi) {
    return {math::clamp(v.x, lo, hi), math::clamp(v.y, lo, hi), math::clamp(v.z, lo, hi)};
}

} // namespace

CameraRig::CameraRig(const math::Vec3& target) noexcept : target_(target) {}

void CameraRig::set_mode(CameraMode mode) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    mode_ = mode;
}

CameraMode CameraRig::mode() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return mode_;
}

void CameraRig::set_target(const math::Vec3& target) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    target_ = target;
}

math::Vec3 CameraRig::target() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return target_;
}

void CameraRig::orbit(float yaw_delta, float pitch_delta) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    yaw_ += yaw_delta;
    pitch_ = math::clamp(pitch_ + pitch_delta, kMinPitch, kMaxPitch);
}

void CameraRig::dolly(float delta) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    distance_ = math::clamp(distance_ + delta, kMinDistance, kMaxDistance);
}

void CameraRig::set_distance(float distance) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    distance_ = math::clamp(distance, kMinDistance, kMaxDistance);
}

float CameraRig::distance() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return distance_;
}

void CameraRig::fly_move(const math::Vec3& delta) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    // Move in the camera's local basis: forward + right + up.
    const math::Vec3 fwd = forward();
    const math::Vec3 right = math::normalize(math::cross(fwd, math::Vec3{0.0f, 1.0f, 0.0f}));
    fly_position_ =
        fly_position_ + fwd * delta.z + right * delta.x + math::Vec3{0.0f, 1.0f, 0.0f} * delta.y;
}

void CameraRig::fly_look(float yaw_delta, float pitch_delta) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    yaw_ += yaw_delta;
    pitch_ = math::clamp(pitch_ + pitch_delta, kMinPitch, kMaxPitch);
}

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
