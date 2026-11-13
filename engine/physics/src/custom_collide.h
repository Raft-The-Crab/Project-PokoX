#pragma once

#include "poko/core/physics_contracts.h"

namespace poko::physics::detail {

constexpr int kMaxContacts = 8;

// Convex collider in world space. Spheres are capsules with half_height 0.
struct Collider {
    bool is_box = false;
    math::Vec3 position;
    math::Quat rotation;
    math::Vec3 half;          // box half extents
    float radius = 0.0f;      // capsule radius
    float half_height = 0.0f; // capsule mid-section half height (local Y axis)
};

struct Contact {
    math::Vec3 point;
    math::Vec3 normal; // unit, from the first collider toward the second
    float depth = 0.0f;
};

struct Aabb {
    math::Vec3 min;
    math::Vec3 max;
};

Collider make_collider(const ShapeDesc& shape, const math::Vec3& position,
                       const math::Quat& rotation) noexcept;
Aabb bounds(const Collider& collider) noexcept;

// Writes up to kMaxContacts contacts; returns the count (0 = separated).
// With margin > 0 both shapes are inflated by margin / 2, so near-miss pairs
// within `margin` produce contacts whose depth is margin larger than the true
// depth (callers subtract it to get a negative, speculative depth).
int collide(const Collider& a, const Collider& b, Contact* out, float margin = 0.0f) noexcept;

// Contacts against the half-space y <= height. The plane acts as the first
// collider, so normals point up (plane -> body). `margin` raises the plane.
int collide_plane(const Collider& collider, float height, Contact* out,
                  float margin = 0.0f) noexcept;

// Lower bound on the distance between two colliders (<= 0 when overlapping).
// Safe for conservative advancement: never larger than the true distance.
float separation(const Collider& a, const Collider& b) noexcept;

// Height of the collider's lowest point above the plane (negative = below).
float plane_gap(const Collider& collider, float height) noexcept;

} // namespace poko::physics::detail
