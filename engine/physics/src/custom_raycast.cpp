#include "poko/physics/custom_world.h"

#include <cstddef>

#include "custom_detail.h"

namespace poko {

bool CustomPhysicsWorld::raycast(const Ray& ray, float max_distance, std::uint32_t layer_mask,
                                 RayHit& out) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_ || !valid(ray) || !(max_distance > 0.0f)) {
        return false;
    }
    const math::Vec3 direction = math::normalize(ray.direction);
    float best = max_distance;
    bool hit = false;
    RayHit candidate{};

    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        const Body& body = bodies_[i];
        if (!body.alive || body.trigger || (body.collision_layer & layer_mask) == 0) {
            continue;
        }
        float t = 0.0f;
        math::Vec3 normal{};
        bool found = false;
        const math::Vec3& position = body.state.position;
        switch (body.shape.kind) {
        case ShapeKind::Sphere:
            found = physics::detail::ray_sphere(ray.origin, direction, position, body.shape.radius,
                                                best, t);
            if (found) {
                normal = math::normalize((ray.origin + direction * t) - position);
            }
            break;
        case ShapeKind::Box: {
            const math::Quat inverse = math::conjugate(body.state.rotation);
            math::Vec3 local_normal{};
            found = physics::detail::ray_box_local(math::rotate(inverse, ray.origin - position),
                                                   math::rotate(inverse, direction),
                                                   body.shape.half_extents, best, t, local_normal);
            if (found) {
                normal = math::rotate(body.state.rotation, local_normal);
            }
            break;
        }
        case ShapeKind::Capsule: {
            const math::Vec3 axis =
                math::rotate(body.state.rotation, {0.0f, 1.0f, 0.0f}) * body.shape.half_height;
            found =
                physics::detail::ray_capsule(ray.origin, direction, position - axis,
                                             position + axis, body.shape.radius, best, t, normal);
            break;
        }
        }
        if (!found || t >= best) {
            continue;
        }
        best = t;
        hit = true;
        candidate.body = make_handle(static_cast<HandleIndex>(i), generations_[i]);
        candidate.distance = t;
        candidate.point = ray.origin + direction * t;
        candidate.normal = normal;
    }

    if (hit) {
        out = candidate;
    }
    return hit;
}

} // namespace poko
