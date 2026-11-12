#include "poko/physics/world.h"

#include <cmath>
#include <limits>

namespace poko {

using math::Vec3;

namespace {

// Ray vs sphere (center = body position, radius = shape.radius).
bool ray_sphere(const Ray& ray, const Vec3& center, float radius, float max_distance, float& t_out,
                Vec3& normal_out) {
    const Vec3 oc = ray.origin - center;
    const float b = math::dot(oc, ray.direction);
    const float c = math::dot(oc, oc) - radius * radius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f) {
        return false;
    }
    const float root = std::sqrt(discriminant);
    float t = -b - root;
    if (t < 0.0f) {
        t = -b + root; // origin inside the sphere
    }
    if (t < 0.0f || t > max_distance) {
        return false;
    }
    t_out = t;
    normal_out = math::normalize(ray.origin + ray.direction * t - center);
    return true;
}

// Ray vs axis-aligned box centered at origin with given half extents, in the
// ray's local frame (MOCK ignores body rotation — see header comment).
bool ray_box(const Ray& ray, const Vec3& half, float max_distance, float& t_out, Vec3& normal_out) {
    float t_min = 0.0f;
    float t_max = max_distance;
    int axis = -1;
    float sign = 0.0f;
    const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float dir[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float he[3] = {half.x, half.y, half.z};
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(dir[i]) < 1e-9f) {
            if (origin[i] < -he[i] || origin[i] > he[i]) {
                return false;
            }
            continue;
        }
        float t1 = (-he[i] - origin[i]) / dir[i];
        float t2 = (he[i] - origin[i]) / dir[i];
        float s = -1.0f;
        if (t1 > t2) {
            const float tmp = t1;
            t1 = t2;
            t2 = tmp;
            s = 1.0f;
        }
        if (t1 > t_min) {
            t_min = t1;
            axis = i;
            sign = s;
        }
        t_max = t_max < t2 ? t_max : t2;
        if (t_min > t_max) {
            return false;
        }
    }
    if (axis < 0 || t_min > max_distance) {
        return false;
    }
    t_out = t_min;
    normal_out = Vec3{};
    if (axis == 0) {
        normal_out.x = sign;
    } else if (axis == 1) {
        normal_out.y = sign;
    } else {
        normal_out.z = sign;
    }
    return true;
}

} // namespace

bool NullPhysicsWorld::raycast(const Ray& ray, float max_distance, std::uint32_t layer_mask,
                               RayHit& out) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_ || !valid(ray) || max_distance <= 0.0f) {
        return false;
    }
    const Vec3 dir = math::normalize(ray.direction);
    const Ray unit{ray.origin, dir};
    bool hit = false;
    float best_t = max_distance;
    RayHit best{};
    for (const auto& body : bodies_) {
        if (!body.alive || (body.collision_layer & layer_mask) == 0) {
            continue;
        }
        float t = 0.0f;
        Vec3 normal{};
        bool shape_hit = false;
        if (body.shape.kind == ShapeKind::Sphere) {
            shape_hit = ray_sphere(unit, body.state.position, body.shape.radius, best_t, t, normal);
        } else if (body.shape.kind == ShapeKind::Box) {
            // MOCK: box test ignores rotation; position is the box center.
            const Ray local{unit.origin - body.state.position, unit.direction};
            shape_hit = ray_box(local, body.shape.half_extents, best_t, t, normal);
        }
        // Capsule unsupported in MOCK raycast (documented limitation).
        if (shape_hit && t < best_t) {
            best_t = t;
            best.body = make_handle(static_cast<HandleIndex>(&body - bodies_.data()),
                                    generations_[static_cast<HandleIndex>(&body - bodies_.data())]);
            best.distance = t;
            best.point = unit.origin + unit.direction * t;
            best.normal = normal;
            hit = true;
        }
    }
    if (hit) {
        out = best;
    }
    return hit;
}

} // namespace poko
