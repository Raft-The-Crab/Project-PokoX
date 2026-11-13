#pragma once

#include "poko/core/physics_contracts.h"

namespace poko::physics::detail {

struct Pose {
    math::Vec3 position{};
    math::Quat rotation{};
};

// One contact: `normal` points from shape A toward shape B (the direction A
// must move to separate is -normal). `point` lies on the contact surface.
struct ContactPoint {
    math::Vec3 point{};
    math::Vec3 normal{0.0f, 1.0f, 0.0f};
    float depth = 0.0f;
};

inline constexpr int kMaxContacts = 8;

struct Manifold {
    ContactPoint points[kMaxContacts]{};
    int count = 0;

    void add(const ContactPoint& p) noexcept {
        if (count < kMaxContacts) {
            points[count++] = p;
        }
    }
};

// Exact narrow phase for sphere, capsule (local Y axis) and oriented box.
// Appends contacts to `out` and returns true when the shapes overlap.
bool collide(const ShapeDesc& a, const Pose& pa, const ShapeDesc& b, const Pose& pb,
             Manifold& out) noexcept;

// Contacts of a shape against the half-space y <= height. Normal points down
// (from the shape toward the plane).
bool collide_ground(const ShapeDesc& shape, const Pose& pose, float height, Manifold& out) noexcept;

// World-space half extents of the shape's axis-aligned bounding box.
math::Vec3 aabb_half_extents(const ShapeDesc& shape, const math::Quat& rotation) noexcept;

// Ray vs oriented shapes. `direction` must be normalized. On hit, writes the
// distance and world normal.
bool ray_shape(const math::Vec3& origin, const math::Vec3& direction, const ShapeDesc& shape,
               const Pose& pose, float max_distance, float& out_t, math::Vec3& out_normal) noexcept;

} // namespace poko::physics::detail
