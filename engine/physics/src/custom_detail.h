#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

#include "poko/core/physics_contracts.h"

namespace poko::physics::detail {

// Ray vs sphere: nearest t in [0, max_distance], or false. A ray starting
// inside reports the exit point.
inline bool ray_sphere(const math::Vec3& origin, const math::Vec3& direction,
                       const math::Vec3& center, float radius, float max_distance,
                       float& out_t) noexcept {
    const math::Vec3 oc = origin - center;
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

// Ray vs axis-aligned box centred at the origin (slab test). Callers rotate
// the ray into box-local space first.
inline bool ray_box_local(const math::Vec3& origin, const math::Vec3& direction,
                          const math::Vec3& half, float max_distance, float& out_t,
                          math::Vec3& out_normal) noexcept {
    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {direction.x, direction.y, direction.z};
    const float h[3] = {half.x, half.y, half.z};

    float t_min = 0.0f;
    float t_max = max_distance;
    int axis = -1;
    float sign = -1.0f;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(d[i]) < 1.0e-6f) {
            if (o[i] < -h[i] || o[i] > h[i]) {
                return false;
            }
            continue;
        }
        const float inv = 1.0f / d[i];
        float t1 = (-h[i] - o[i]) * inv;
        float t2 = (h[i] - o[i]) * inv;
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
    float n[3] = {0.0f, 0.0f, 0.0f};
    if (axis >= 0) {
        n[axis] = sign;
    }
    out_normal = {n[0], n[1], n[2]};
    return true;
}

// Ray vs capsule (segment p0-p1 swept by radius). Normal is written at the hit.
inline bool ray_capsule(const math::Vec3& origin, const math::Vec3& direction, const math::Vec3& p0,
                        const math::Vec3& p1, float radius, float max_distance, float& out_t,
                        math::Vec3& out_normal) noexcept {
    bool hit = false;
    float best = max_distance;

    auto consider_sphere = [&](const math::Vec3& center) {
        float t = 0.0f;
        if (ray_sphere(origin, direction, center, radius, best, t) && t <= best) {
            const math::Vec3 point = origin + direction * t;
            out_normal = math::normalize(point - center);
            best = t;
            hit = true;
        }
    };
    consider_sphere(p0);

    const math::Vec3 axis_vec = p1 - p0;
    const float length = math::length(axis_vec);
    if (length > 1.0e-6f) {
        consider_sphere(p1);
        const math::Vec3 u = axis_vec / length;
        const math::Vec3 oc = origin - p0;
        const math::Vec3 dp = direction - u * math::dot(direction, u);
        const math::Vec3 op = oc - u * math::dot(oc, u);
        const float a = math::dot(dp, dp);
        if (a > 1.0e-8f) {
            const float b = math::dot(dp, op);
            const float c = math::dot(op, op) - radius * radius;
            const float disc = b * b - a * c;
            if (disc >= 0.0f) {
                const float root = std::sqrt(disc);
                float t = (-b - root) / a;
                if (t < 0.0f) {
                    t = (-b + root) / a;
                }
                if (t >= 0.0f && t < best) {
                    const float s = math::dot(oc + direction * t, u);
                    if (s >= 0.0f && s <= length) {
                        const math::Vec3 point = origin + direction * t;
                        out_normal = math::normalize(point - (p0 + u * s));
                        best = t;
                        hit = true;
                    }
                }
            }
        }
    }
    if (hit) {
        out_t = best;
    }
    return hit;
}

} // namespace poko::physics::detail
