#pragma once

#include "poko_math.h"

namespace poko {

// Camera and transform math (systemplan/04 step 6). Uses the core coordinate
// system: right-handed, +Y up, -Z forward, row-major matrices, column vectors.
// Perspective projection maps depth to [-1, 1] (OpenGL-style); a Vulkan
// backend will apply its own depth-range conversion without changing this API.
//
// Thread-safety contract: a Camera is owned by one renderer and is not
// synchronised; callers must not mutate it while another thread reads it.
class Camera {
public:
    void set_position(const math::Vec3& position) noexcept {
        position_ = position;
    }
    void set_target(const math::Vec3& target) noexcept {
        target_ = target;
    }
    void set_up(const math::Vec3& up) noexcept {
        up_ = up;
    }

    void set_perspective(float fov_y_radians, float aspect, float near_plane,
                         float far_plane) noexcept;
    void set_orthographic(float left, float right, float bottom, float top, float near_plane,
                          float far_plane) noexcept;

    math::Mat4 view_matrix() const;
    math::Mat4 projection_matrix() const;
    math::Mat4 view_projection() const;

    const math::Vec3& position() const noexcept {
        return position_;
    }
    const math::Vec3& target() const noexcept {
        return target_;
    }
    const math::Vec3& up() const noexcept {
        return up_;
    }
    float near_plane() const noexcept {
        return near_plane_;
    }
    float far_plane() const noexcept {
        return far_plane_;
    }
    bool orthographic() const noexcept {
        return orthographic_;
    }

private:
    math::Vec3 position_{0.0f, 0.0f, 0.0f};
    math::Vec3 target_{0.0f, 0.0f, -1.0f};
    math::Vec3 up_{0.0f, 1.0f, 0.0f};
    float fov_y_ = math::kHalfPi;
    float aspect_ = 1.0f;
    float near_plane_ = 0.1f;
    float far_plane_ = 100.0f;
    float ortho_left_ = -1.0f;
    float ortho_right_ = 1.0f;
    float ortho_bottom_ = -1.0f;
    float ortho_top_ = 1.0f;
    bool orthographic_ = false;
};

} // namespace poko
