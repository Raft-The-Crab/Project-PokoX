#include "poko_camera.h"

#include <cmath>

namespace poko {

void Camera::set_perspective(float fov_y_radians, float aspect, float near_plane,
                             float far_plane) noexcept {
    // Invalid values fall back to safe defaults rather than producing NaN.
    fov_y_ = fov_y_radians > 0.0f && fov_y_radians < math::kPi
                 ? fov_y_radians
                 : math::kHalfPi;
    aspect_ = aspect > 0.0f ? aspect : 1.0f;
    near_plane_ = near_plane > 0.0f ? near_plane : 0.1f;
    far_plane_ = far_plane > near_plane_ ? far_plane : near_plane_ + 100.0f;
    orthographic_ = false;
}

void Camera::set_orthographic(float left, float right, float bottom, float top,
                              float near_plane, float far_plane) noexcept {
    ortho_left_ = left;
    ortho_right_ = right > left ? right : left + 1.0f;
    ortho_bottom_ = bottom;
    ortho_top_ = top > bottom ? top : bottom + 1.0f;
    near_plane_ = near_plane > 0.0f ? near_plane : 0.1f;
    far_plane_ = far_plane > near_plane_ ? far_plane : near_plane_ + 100.0f;
    orthographic_ = true;
}

math::Mat4 Camera::view_matrix() const {
    const math::Vec3 forward = math::normalize(target_ - position_);
    const math::Vec3 right = math::normalize(math::cross(forward, up_));
    const math::Vec3 true_up = math::cross(right, forward);

    math::Mat4 view = math::Mat4::identity();
    view.at(0, 0) = right.x;
    view.at(0, 1) = right.y;
    view.at(0, 2) = right.z;
    view.at(0, 3) = -math::dot(right, position_);

    view.at(1, 0) = true_up.x;
    view.at(1, 1) = true_up.y;
    view.at(1, 2) = true_up.z;
    view.at(1, 3) = -math::dot(true_up, position_);

    view.at(2, 0) = -forward.x;
    view.at(2, 1) = -forward.y;
    view.at(2, 2) = -forward.z;
    view.at(2, 3) = math::dot(forward, position_);
    return view;
}

math::Mat4 Camera::projection_matrix() const {
    math::Mat4 projection;
    if (orthographic_) {
        const float width = ortho_right_ - ortho_left_;
        const float height = ortho_top_ - ortho_bottom_;
        const float depth = far_plane_ - near_plane_;
        projection.at(0, 0) = 2.0f / width;
        projection.at(0, 3) = -(ortho_right_ + ortho_left_) / width;
        projection.at(1, 1) = 2.0f / height;
        projection.at(1, 3) = -(ortho_top_ + ortho_bottom_) / height;
        projection.at(2, 2) = -2.0f / depth;
        projection.at(2, 3) = -(far_plane_ + near_plane_) / depth;
        projection.at(3, 3) = 1.0f;
        return projection;
    }

    const float focal = 1.0f / std::tan(fov_y_ * 0.5f);
    const float depth = near_plane_ - far_plane_;
    projection.at(0, 0) = focal / aspect_;
    projection.at(1, 1) = focal;
    projection.at(2, 2) = (far_plane_ + near_plane_) / depth;
    projection.at(2, 3) = (2.0f * far_plane_ * near_plane_) / depth;
    projection.at(3, 2) = -1.0f;
    return projection;
}

math::Mat4 Camera::view_projection() const {
    return math::multiply(projection_matrix(), view_matrix());
}

}  // namespace poko
