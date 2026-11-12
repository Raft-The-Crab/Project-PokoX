#pragma once

#include <mutex>

#include "poko/core/math.h"
#include "poko/core/result.h"

namespace poko::camera {

// Gameplay camera rig skeleton (Phase 1). Produces eye position + look-at
// target for the renderer's poko::Camera; does NOT own projection or GPU
// state. Modes: orbit around a target, top-down follow, and free-fly.

enum class CameraMode : std::uint8_t {
    Orbit = 0, // yaw/pitch/distance around target
    TopDown,   // fixed pitch looking down at target
    Fly,       // free position + yaw/pitch orientation
};

struct CameraPose {
    math::Vec3 position;
    math::Vec3 look_target; // point the camera looks at
    math::Vec3 up{0.0f, 1.0f, 0.0f};
};

class CameraRig {
public:
    explicit CameraRig(const math::Vec3& target = {}) noexcept;

    void set_mode(CameraMode mode) noexcept;
    CameraMode mode() const noexcept;

    void set_target(const math::Vec3& target) noexcept;
    math::Vec3 target() const noexcept;

    // Orbit controls.
    void orbit(float yaw_delta_radians, float pitch_delta_radians) noexcept;
    void dolly(float distance_delta) noexcept; // metres; clamped
    void set_distance(float distance) noexcept;
    float distance() const noexcept;

    // Fly controls: move/rotate the eye directly.
    void fly_move(const math::Vec3& delta) noexcept;
    void fly_look(float yaw_delta_radians, float pitch_delta_radians) noexcept;

    // World-space basis for the current pose (fly mode uses yaw/pitch).
    CameraPose pose() const noexcept;

    static constexpr float kMinPitch = -1.45f; // ~ -83 deg
    static constexpr float kMaxPitch = 1.45f;  // ~ +83 deg
    static constexpr float kMinDistance = 0.5f;
    static constexpr float kMaxDistance = 200.0f;

private:
    math::Vec3 forward() const noexcept;

    mutable std::mutex mutex_;
    CameraMode mode_ = CameraMode::Orbit;
    math::Vec3 target_{};
    float yaw_ = 0.0f;
    float pitch_ = 0.25f;
    float distance_ = 5.0f;
    math::Vec3 fly_position_{};
};

} // namespace poko::camera
