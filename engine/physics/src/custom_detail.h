#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

#include "poko/core/physics_contracts.h"

namespace poko::physics::detail {

// Conservative bounding radius for the narrow phase. Boxes use their
// circumscribed sphere; capsules their total half-height.
inline float bounding_radius(const ShapeDesc& shape) noexcept {
    switch (shape.kind) {
    case ShapeKind::Sphere:
        return shape.radius;
    case ShapeKind::Box:
        return math::length(shape.half_extents);
    case ShapeKind::Capsule:
        return shape.radius + shape.half_height;
    }
    return shape.radius;
}

struct Contact {
    math::Vec3 normal{}; // unit vector from A toward B
    float penetration = 0.0f;
};

inline bool overlap(const math::Vec3& a, float ra, const math::Vec3& b, float rb,
                    Contact& out) noexcept {
    const math::Vec3 delta = b - a;
    const float distance = math::length(delta);
    const float sum = ra + rb;
    if (distance >= sum) {
        return false;
    }
    out.normal = distance > 1.0e-5f ? delta / distance : math::Vec3{0.0f, 1.0f, 0.0f};
    out.penetration = sum - distance;
    return true;
}

// Ray vs sphere: nearest t in [0, max_distance], or false.
inline bool ray_sphere(const Ray& ray, const math::Vec3& center, float radius, float max_distance,
                       float& out_t) noexcept {
    const math::Vec3 direction = math::normalize(ray.direction);
    const math::Vec3 oc = ray.origin - center;
    const float b = math::dot(oc, direction);
    const float c = math::dot(oc, oc) - radius * radius;
    const float disc = b * b - c;
    if (disc < 0.0f) {
        return false;
    }
    const float root = std::sqrt(disc);
    float t = -b - root;
    if (t < 0.0f) {
        t = -b + root;
    }
    if (t < 0.0f || t > max_distance) {
        return false;
    }
    out_t = t;
    return true;
}

// Ray vs axis-aligned box (slab test), translation only; rotation is ignored.
inline bool ray_box(const Ray& ray, const math::Vec3& center, const math::Vec3& half,
                    float max_distance, float& out_t, math::Vec3& out_normal) noexcept {
    const math::Vec3 direction = math::normalize(ray.direction);
    const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float dir[3] = {direction.x, direction.y, direction.z};
    const float c[3] = {center.x, center.y, center.z};
    const float h[3] = {half.x, half.y, half.z};

    float t_min = 0.0f;
    float t_max = max_distance;
    int axis = 0;
    float sign = -1.0f;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(dir[i]) < 1.0e-6f) {
            if (origin[i] < c[i] - h[i] || origin[i] > c[i] + h[i]) {
                return false;
            }
            continue;
        }
        const float inv = 1.0f / dir[i];
        float t1 = (c[i] - h[i] - origin[i]) * inv;
        float t2 = (c[i] + h[i] - origin[i]) * inv;
        float s = -1.0f;
        if (t1 > t2) {
            std::swap(t1, t2);
            s = 1.0f;
        }
        if (t1 > t_min) {
            t_min = t1;
            axis = i;
            sign = s;
        }
        t_max = std::min(t_max, t2);
        if (t_min > t_max) {
            return false;
        }
    }
    out_t = t_min;
    out_normal = math::Vec3{};
    float* n[3] = {&out_normal.x, &out_normal.y, &out_normal.z};
    *n[axis] = sign;
    return true;
}

} // namespace poko::physics::detail
