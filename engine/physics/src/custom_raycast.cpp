#include "poko/physics/custom_world.h"

#include <cstddef>

#include "narrowphase.h"

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
        if (!body.alive || (body.collision_layer & layer_mask) == 0) {
            continue;
        }
        float t = 0.0f;
        math::Vec3 normal{};
        if (!physics::detail::ray_shape(
                ray.origin, direction, body.shape,
                physics::detail::Pose{body.state.position, body.state.rotation}, best, t, normal) ||
            t >= best) {
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
