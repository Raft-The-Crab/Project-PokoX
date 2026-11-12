#include "poko/camera/rig.h"

namespace poko::camera {

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

} // namespace poko::camera
