#include <algorithm>
#include <cmath>
#include <cstddef>

#include "custom_collide.h"
#include "poko/physics/custom_world.h"

namespace poko {
namespace {

using physics::detail::Aabb;
using physics::detail::Collider;
using physics::detail::Contact;

constexpr float kTouch = 1.0e-3f;
constexpr float kNormalMargin = 0.02f;

// Conservative advancement: walks `caster` along `dir` by the (lower-bound)
// separation until the shapes touch. Linear motion only.
bool time_of_impact(const Collider& caster, const Collider& target, const math::Vec3& dir,
                    float max_distance, float& out_t) noexcept {
    Collider moved = caster;
    float t = 0.0f;
    for (int i = 0; i < 64; ++i) {
        moved.position = caster.position + dir * t;
        const float gap = physics::detail::separation(moved, target);
        if (gap <= kTouch) {
            out_t = t;
            return true;
        }
        t += gap;
        if (t > max_distance) {
            return false;
        }
    }
    moved.position = caster.position + dir * t;
    out_t = t;
    return physics::detail::separation(moved, target) < 0.02f;
}

bool boxes_overlap(const Aabb& a, const Aabb& b) noexcept {
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

} // namespace

bool CustomPhysicsWorld::cast_locked(const Collider& caster, std::uint32_t caster_layer,
                                     std::uint32_t caster_mask, bool use_caster_mask,
                                     std::uint32_t layer_mask, const math::Vec3& direction,
                                     float max_distance, int skip_body, SweepHit& out) const {
    const Aabb start = physics::detail::bounds(caster);
    const math::Vec3 shift = direction * max_distance;
    constexpr float kPad = 0.05f;
    Aabb swept;
    swept.min = {std::min(start.min.x, start.min.x + shift.x) - kPad,
                 std::min(start.min.y, start.min.y + shift.y) - kPad,
                 std::min(start.min.z, start.min.z + shift.z) - kPad};
    swept.max = {std::max(start.max.x, start.max.x + shift.x) + kPad,
                 std::max(start.max.y, start.max.y + shift.y) + kPad,
                 std::max(start.max.z, start.max.z + shift.z) + kPad};

    float best = max_distance;
    bool found = false;
    SweepHit best_hit;
    Contact points[physics::detail::kMaxContacts];

    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        const Body& body = bodies_[i];
        if (!body.alive || body.trigger || static_cast<int>(i) == skip_body) {
            continue;
        }
        if (use_caster_mask) {
            if ((caster_layer & body.collision_mask) == 0 ||
                (body.collision_layer & caster_mask) == 0) {
                continue;
            }
        } else if ((body.collision_layer & layer_mask) == 0) {
            continue;
        }
        const Collider target =
            physics::detail::make_collider(body.shape, body.state.position, body.state.rotation);
        if (!boxes_overlap(swept, physics::detail::bounds(target))) {
            continue;
        }
        float t = 0.0f;
        if (!time_of_impact(caster, target, direction, best, t) ||
            (found && t >= best_hit.distance)) {
            continue;
        }
        Collider moved = caster;
        moved.position = caster.position + direction * t;
        const int n = physics::detail::collide(moved, target, points, kNormalMargin);
        SweepHit hit;
        hit.body = make_handle(static_cast<HandleIndex>(i), generations_[i]);
        hit.distance = t;
        if (n > 0) {
            int deepest = 0;
            for (int k = 1; k < n; ++k) {
                if (points[k].depth > points[deepest].depth) {
                    deepest = k;
                }
            }
            hit.normal = -points[deepest].normal;
            hit.point = points[deepest].point;
        } else {
            hit.normal = math::normalize(moved.position - target.position);
            hit.point = target.position;
        }
        best = t;
        found = true;
        best_hit = hit;
    }

    if (ground_enabled_) {
        const float gap = physics::detail::plane_gap(caster, ground_height_);
        float t = -1.0f;
        if (gap <= kTouch) {
            t = 0.0f;
        } else if (direction.y < -1.0e-6f) {
            t = gap / -direction.y;
        }
        if (t >= 0.0f && t <= best && (!found || t < best_hit.distance)) {
            Collider moved = caster;
            moved.position = caster.position + direction * t;
            const int n =
                physics::detail::collide_plane(moved, ground_height_, points, kNormalMargin);
            SweepHit hit;
            hit.ground = true;
            hit.distance = t;
            hit.normal = {0.0f, 1.0f, 0.0f};
            hit.point = n > 0 ? points[0].point
                              : math::Vec3{moved.position.x, ground_height_, moved.position.z};
            found = true;
            best_hit = hit;
        }
    }

    if (found) {
        out = best_hit;
    }
    return found;
}

bool CustomPhysicsWorld::shape_cast(const ShapeDesc& shape, const math::Vec3& position,
                                    const math::Quat& rotation, const math::Vec3& direction,
                                    float max_distance, std::uint32_t layer_mask,
                                    SweepHit& out) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const float length = math::length(direction);
    if (!initialized_ || !valid(shape) || !(length > 0.0f) || !(max_distance > 0.0f)) {
        return false;
    }
    const Collider caster = physics::detail::make_collider(shape, position, rotation);
    return cast_locked(caster, 0, 0, false, layer_mask, direction / length, max_distance, -1, out);
}

std::uint32_t CustomPhysicsWorld::overlap(const ShapeDesc& shape, const math::Vec3& position,
                                          const math::Quat& rotation, std::uint32_t layer_mask,
                                          BodyHandle* out, std::uint32_t capacity) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_ || !valid(shape)) {
        return 0;
    }
    const Collider probe = physics::detail::make_collider(shape, position, rotation);
    const Aabb probe_box = physics::detail::bounds(probe);
    Contact points[physics::detail::kMaxContacts];
    std::uint32_t total = 0;
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        const Body& body = bodies_[i];
        if (!body.alive || body.trigger || (body.collision_layer & layer_mask) == 0) {
            continue;
        }
        const Collider target =
            physics::detail::make_collider(body.shape, body.state.position, body.state.rotation);
        if (!boxes_overlap(probe_box, physics::detail::bounds(target)) ||
            physics::detail::collide(probe, target, points) == 0) {
            continue;
        }
        if (out != nullptr && total < capacity) {
            out[total] = make_handle(static_cast<HandleIndex>(i), generations_[i]);
        }
        ++total;
    }
    return total;
}

void CustomPhysicsWorld::update_triggers() {
    std::set<std::array<std::uint32_t, 4>> now;
    Contact points[physics::detail::kMaxContacts];
    for (std::size_t t = 0; t < bodies_.size(); ++t) {
        const Body& trigger = bodies_[t];
        if (!trigger.alive || !trigger.trigger) {
            continue;
        }
        const Collider volume = physics::detail::make_collider(
            trigger.shape, trigger.state.position, trigger.state.rotation);
        const Aabb volume_box = physics::detail::bounds(volume);
        for (std::size_t o = 0; o < bodies_.size(); ++o) {
            const Body& other = bodies_[o];
            if (!other.alive || other.trigger || other.type == BodyType::Static ||
                (trigger.collision_layer & other.collision_mask) == 0 ||
                (other.collision_layer & trigger.collision_mask) == 0) {
                continue;
            }
            const Collider shape = physics::detail::make_collider(other.shape, other.state.position,
                                                                  other.state.rotation);
            if (boxes_overlap(volume_box, physics::detail::bounds(shape)) &&
                physics::detail::collide(volume, shape, points) > 0) {
                now.insert(
                    {static_cast<std::uint32_t>(t), static_cast<std::uint32_t>(generations_[t]),
                     static_cast<std::uint32_t>(o), static_cast<std::uint32_t>(generations_[o])});
            }
        }
    }
    auto event = [](const std::array<std::uint32_t, 4>& key, bool entered) {
        return TriggerEvent{
            make_handle(static_cast<HandleIndex>(key[0]), static_cast<Generation>(key[1])),
            make_handle(static_cast<HandleIndex>(key[2]), static_cast<Generation>(key[3])),
            entered};
    };
    for (const auto& key : overlaps_) {
        if (now.find(key) == now.end()) {
            trigger_events_.push_back(event(key, false));
        }
    }
    for (const auto& key : now) {
        if (overlaps_.find(key) == overlaps_.end()) {
            trigger_events_.push_back(event(key, true));
        }
    }
    overlaps_ = std::move(now);
}

void CustomPhysicsWorld::drain_trigger_events(std::vector<TriggerEvent>& out) {
    std::lock_guard<std::mutex> guard(mutex_);
    out.insert(out.end(), trigger_events_.begin(), trigger_events_.end());
    trigger_events_.clear();
}

} // namespace poko
