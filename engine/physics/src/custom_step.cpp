#include "poko/physics/custom_world.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "custom_collide.h"
#include "custom_math.h"

namespace poko {

namespace physics::detail {

struct ContactConstraint {
    int a = -1; // body index, -1 = world (ground)
    int b = -1;
    math::Vec3 normal;
    math::Vec3 point;
    math::Vec3 ra;
    math::Vec3 rb;
    math::Vec3 tangent1;
    math::Vec3 tangent2;
    float depth = 0.0f; // negative = speculative (not yet touching)
    float friction = 0.0f;
    float inv_a = 0.0f; // effective inverse masses (0 for static/sleeping)
    float inv_b = 0.0f;
    float bounce = 0.0f; // separating speed to reach on impact
    float target = 0.0f; // speculative: most negative allowed approach speed
    float mass_n = 0.0f;
    float mass_t1 = 0.0f;
    float mass_t2 = 0.0f;
    float jn = 0.0f; // accumulated impulses
    float jt1 = 0.0f;
    float jt2 = 0.0f;
};

} // namespace physics::detail

namespace {

using physics::detail::apply_inertia;
using physics::detail::ContactConstraint;

constexpr int kSolverIterations = 12;
constexpr int kProjectionPasses = 2;
constexpr float kSpeculativeMargin = 0.04f;
constexpr float kPenetrationSlop = 0.005f;
constexpr float kProjectionPercent = 0.8f;
constexpr float kBounceThreshold = 1.0f; // m/s of approach below which bodies do not bounce
constexpr float kSleepLinear = 0.05f;
constexpr float kSleepAngular = 0.05f;
constexpr float kSleepDelay = 0.5f;
constexpr float kAngularDamping = 0.05f; // 1/s
constexpr float kCcdGap = 0.01f;         // left between a swept body and what it hit
constexpr float kWarmStartMatch = 0.05f; // m, max point drift to reuse cached impulses

float smallest_dimension(const ShapeDesc& shape) noexcept {
    switch (shape.kind) {
    case ShapeKind::Sphere:
    case ShapeKind::Capsule:
        return shape.radius;
    case ShapeKind::Box:
        return std::min({shape.half_extents.x, shape.half_extents.y, shape.half_extents.z});
    }
    return shape.radius;
}

} // namespace

bool CustomPhysicsWorld::moving(const Body& body) const noexcept {
    if (body.type == BodyType::Static || body.sleeping) {
        return false;
    }
    return math::length_squared(body.state.velocity) > kSleepLinear * kSleepLinear ||
           math::length_squared(body.state.angular_velocity) > kSleepAngular * kSleepAngular;
}

void CustomPhysicsWorld::build_contacts(std::vector<ContactConstraint>& out, float h,
                                        bool warm_start) {
    using physics::detail::Aabb;
    using physics::detail::Collider;
    using physics::detail::Contact;

    out.clear();
    const std::size_t count = bodies_.size();
    std::vector<Collider> colliders(count);
    std::vector<Aabb> boxes(count);
    std::vector<std::uint32_t> order;
    order.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Body& body = bodies_[i];
        if (!body.alive || body.trigger) {
            continue;
        }
        colliders[i] =
            physics::detail::make_collider(body.shape, body.state.position, body.state.rotation);
        boxes[i] = physics::detail::bounds(colliders[i]);
        order.push_back(static_cast<std::uint32_t>(i));
    }
    std::sort(order.begin(), order.end(), [&](std::uint32_t l, std::uint32_t r) {
        return boxes[l].min.x < boxes[r].min.x || (boxes[l].min.x == boxes[r].min.x && l < r);
    });

    auto effective_inv = [](const Body& body) {
        return body.type == BodyType::Dynamic && !body.sleeping ? body.inv_mass : 0.0f;
    };

    auto cached_for = [&](ContactConstraint& c) {
        const auto key_less = [](const CachedImpulse& e, const std::pair<int, int>& k) {
            return e.a < k.first || (e.a == k.first && e.b < k.second);
        };
        auto it =
            std::lower_bound(cache_.begin(), cache_.end(), std::make_pair(c.a, c.b), key_less);
        const CachedImpulse* best = nullptr;
        float best_d2 = kWarmStartMatch * kWarmStartMatch;
        for (; it != cache_.end() && it->a == c.a && it->b == c.b; ++it) {
            const float d2 = math::length_squared(it->point - c.point);
            if (d2 < best_d2) {
                best_d2 = d2;
                best = &*it;
            }
        }
        if (best != nullptr) {
            c.jn = best->jn;
            c.jt1 = best->jt1;
            c.jt2 = best->jt2;
        }
    };

    auto prepare = [&](ContactConstraint& c, const Contact& contact, int ia, int ib) {
        c.a = ia;
        c.b = ib;
        c.normal = contact.normal;
        c.point = contact.point;
        c.depth = contact.depth - kSpeculativeMargin;
        const Body* a = ia >= 0 ? &bodies_[static_cast<std::size_t>(ia)] : nullptr;
        const Body* b = &bodies_[static_cast<std::size_t>(ib)];
        c.inv_a = a != nullptr ? effective_inv(*a) : 0.0f;
        c.inv_b = effective_inv(*b);
        c.ra = a != nullptr ? contact.point - a->state.position : math::Vec3{};
        c.rb = contact.point - b->state.position;
        const float friction_a = a != nullptr ? a->friction : b->friction;
        const float restitution_a = a != nullptr ? a->restitution : b->restitution;
        c.friction = std::sqrt(friction_a * b->friction);

        const math::Vec3 n = c.normal;
        c.tangent1 = math::normalize(math::cross(
            n, std::abs(n.x) > 0.9f ? math::Vec3{0.0f, 1.0f, 0.0f} : math::Vec3{1.0f, 0.0f, 0.0f}));
        c.tangent2 = math::cross(n, c.tangent1);

        auto effective_mass = [&](const math::Vec3& axis) {
            float k = c.inv_a + c.inv_b;
            if (c.inv_a > 0.0f) {
                const math::Vec3 rn = math::cross(c.ra, axis);
                k += math::dot(
                    math::cross(apply_inertia(a->state.rotation, a->inv_inertia, rn), c.ra), axis);
            }
            if (c.inv_b > 0.0f) {
                const math::Vec3 rn = math::cross(c.rb, axis);
                k += math::dot(
                    math::cross(apply_inertia(b->state.rotation, b->inv_inertia, rn), c.rb), axis);
            }
            return k > 0.0f ? 1.0f / k : 0.0f;
        };
        c.mass_n = effective_mass(n);
        c.mass_t1 = effective_mass(c.tangent1);
        c.mass_t2 = effective_mass(c.tangent2);

        const math::Vec3 va = a != nullptr
                                  ? a->state.velocity + math::cross(a->state.angular_velocity, c.ra)
                                  : math::Vec3{};
        const math::Vec3 vb = b->state.velocity + math::cross(b->state.angular_velocity, c.rb);
        const float approach = math::dot(vb - va, n);
        const float restitution = std::min(restitution_a, b->restitution);
        // Speculative contacts may close at most the remaining gap per step.
        c.target = c.depth < 0.0f ? c.depth / h : 0.0f;
        if (restitution > 0.0f && approach < -kBounceThreshold && approach * h < c.depth) {
            c.bounce = -restitution * approach;
        }
        if (warm_start && c.bounce == 0.0f) {
            cached_for(c);
        }
    };

    Contact points[physics::detail::kMaxContacts];

    for (std::size_t oi = 0; oi < order.size(); ++oi) {
        const std::uint32_t i = order[oi];
        for (std::size_t oj = oi + 1; oj < order.size(); ++oj) {
            const std::uint32_t j = order[oj];
            if (boxes[j].min.x > boxes[i].max.x + kSpeculativeMargin) {
                break;
            }
            if (boxes[j].min.y > boxes[i].max.y + kSpeculativeMargin ||
                boxes[j].max.y < boxes[i].min.y - kSpeculativeMargin ||
                boxes[j].min.z > boxes[i].max.z + kSpeculativeMargin ||
                boxes[j].max.z < boxes[i].min.z - kSpeculativeMargin) {
                continue;
            }
            // Keep the lower index first so contacts of a pair have a stable key.
            const std::uint32_t lo = std::min(i, j);
            const std::uint32_t hi = std::max(i, j);
            Body& a = bodies_[lo];
            Body& b = bodies_[hi];
            if (a.type != BodyType::Dynamic && b.type != BodyType::Dynamic) {
                continue;
            }
            if ((a.collision_layer & b.collision_mask) == 0 ||
                (b.collision_layer & a.collision_mask) == 0) {
                continue;
            }
            const bool a_awake = a.type == BodyType::Dynamic && !a.sleeping;
            const bool b_awake = b.type == BodyType::Dynamic && !b.sleeping;
            if (!a_awake && !b_awake && !a.in_motion && !b.in_motion) {
                continue;
            }
            const int n =
                physics::detail::collide(colliders[lo], colliders[hi], points, kSpeculativeMargin);
            if (n == 0) {
                continue;
            }
            // in_motion comes from the previous substep's settled velocities; the
            // current ones still contain this step's gravity.
            if (a.type == BodyType::Dynamic && a.sleeping && b.in_motion) {
                wake(a);
            }
            if (b.type == BodyType::Dynamic && b.sleeping && a.in_motion) {
                wake(b);
            }
            for (int k = 0; k < n; ++k) {
                out.emplace_back();
                prepare(out.back(), points[k], static_cast<int>(lo), static_cast<int>(hi));
            }
        }
    }

    if (ground_enabled_) {
        for (std::uint32_t i : order) {
            const Body& body = bodies_[i];
            if (body.type != BodyType::Dynamic || body.sleeping) {
                continue;
            }
            const int n = physics::detail::collide_plane(colliders[i], ground_height_, points,
                                                         kSpeculativeMargin);
            for (int k = 0; k < n; ++k) {
                out.emplace_back();
                prepare(out.back(), points[k], -1, static_cast<int>(i));
            }
        }
    }
}

void CustomPhysicsWorld::solve_contacts(std::vector<ContactConstraint>& contacts, float h) {
    auto velocity_at = [&](int index, const math::Vec3& r) {
        if (index < 0) {
            return math::Vec3{};
        }
        const BodyState& s = bodies_[static_cast<std::size_t>(index)].state;
        return s.velocity + math::cross(s.angular_velocity, r);
    };
    auto apply = [&](int index, float inv, const math::Vec3& r, const math::Vec3& impulse,
                     float sign) {
        if (index < 0 || inv <= 0.0f) {
            return;
        }
        Body& body = bodies_[static_cast<std::size_t>(index)];
        body.state.velocity = body.state.velocity + impulse * (sign * inv);
        body.state.angular_velocity =
            body.state.angular_velocity +
            apply_inertia(body.state.rotation, body.inv_inertia, math::cross(r, impulse)) * sign;
    };
    auto push = [&](ContactConstraint& c, const math::Vec3& impulse) {
        apply(c.a, c.inv_a, c.ra, impulse, -1.0f);
        apply(c.b, c.inv_b, c.rb, impulse, 1.0f);
    };

    for (auto& c : contacts) {
        if (c.jn != 0.0f || c.jt1 != 0.0f || c.jt2 != 0.0f) {
            push(c, c.normal * c.jn + c.tangent1 * c.jt1 + c.tangent2 * c.jt2);
        }
    }

    for (int iteration = 0; iteration < kSolverIterations; ++iteration) {
        solve_joints(h);
        for (auto& c : contacts) {
            const float limit = c.friction * c.jn;
            const math::Vec3 tangents[2] = {c.tangent1, c.tangent2};
            const float masses[2] = {c.mass_t1, c.mass_t2};
            float* accumulated[2] = {&c.jt1, &c.jt2};
            for (int t = 0; t < 2; ++t) {
                const math::Vec3 rv = velocity_at(c.b, c.rb) - velocity_at(c.a, c.ra);
                const float lambda = -math::dot(rv, tangents[t]) * masses[t];
                const float previous = *accumulated[t];
                *accumulated[t] = math::clamp(previous + lambda, -limit, limit);
                push(c, tangents[t] * (*accumulated[t] - previous));
            }

            const math::Vec3 rv = velocity_at(c.b, c.rb) - velocity_at(c.a, c.ra);
            const float vn = math::dot(rv, c.normal);
            const float desired = c.bounce > 0.0f ? c.bounce : c.target;
            const float lambda = (desired - vn) * c.mass_n;
            const float previous = c.jn;
            c.jn = std::max(previous + lambda, 0.0f);
            push(c, c.normal * (c.jn - previous));
        }
    }
}

void CustomPhysicsWorld::project_positions(float h) {
    std::vector<ContactConstraint> contacts;
    for (int pass = 0; pass < kProjectionPasses; ++pass) {
        build_contacts(contacts, h, false);
        // Contacts of one pair are emitted consecutively; correct once per pair by
        // its deepest point so a face contact is not pushed out once per corner.
        for (std::size_t begin = 0; begin < contacts.size();) {
            const ContactConstraint& c = contacts[begin];
            const ContactConstraint* deepest = &c;
            std::size_t end = begin + 1;
            while (end < contacts.size() && contacts[end].a == c.a && contacts[end].b == c.b) {
                if (contacts[end].depth > deepest->depth) {
                    deepest = &contacts[end];
                }
                ++end;
            }
            begin = end;
            const float depth = deepest->depth;
            const math::Vec3 normal = deepest->normal;
            const float inv_sum = c.inv_a + c.inv_b;
            if (depth <= kPenetrationSlop || inv_sum <= 0.0f) {
                continue;
            }
            const float correction = kProjectionPercent * (depth - kPenetrationSlop) / inv_sum;
            if (c.a >= 0 && c.inv_a > 0.0f) {
                Body& body = bodies_[static_cast<std::size_t>(c.a)];
                body.state.position = body.state.position - normal * (correction * c.inv_a);
            }
            if (c.b >= 0 && c.inv_b > 0.0f) {
                Body& body = bodies_[static_cast<std::size_t>(c.b)];
                body.state.position = body.state.position + normal * (correction * c.inv_b);
            }
        }
    }
}

void CustomPhysicsWorld::update_sleep(float h) {
    for (auto& body : bodies_) {
        body.jointed = false;
        body.in_motion = body.alive && moving(body);
    }
    for (const auto& joint : joints_) {
        if (!joint.alive) {
            continue;
        }
        bodies_[joint.desc.body_a.index].jointed = true;
        if (is_valid(joint.desc.body_b)) {
            bodies_[joint.desc.body_b.index].jointed = true;
        }
    }
    for (auto& body : bodies_) {
        if (!body.alive || body.type != BodyType::Dynamic || body.sleeping) {
            continue;
        }
        if (body.jointed || moving(body)) {
            body.sleep_time = 0.0f;
            continue;
        }
        body.sleep_time += h;
        if (body.sleep_time >= kSleepDelay) {
            body.sleeping = true;
            body.state.velocity = {};
            body.state.angular_velocity = {};
        }
    }
}

void CustomPhysicsWorld::substep(float h) {
    const float damping = 1.0f / (1.0f + kAngularDamping * h);
    for (auto& body : bodies_) {
        if (!body.alive || body.type != BodyType::Dynamic || body.sleeping) {
            continue;
        }
        body.state.velocity = body.state.velocity + desc_.gravity * h;
        body.state.angular_velocity = body.state.angular_velocity * damping;
    }

    std::vector<ContactConstraint> contacts;
    build_contacts(contacts, h, true);
    contacts_ += static_cast<std::uint32_t>(contacts.size());
    solve_contacts(contacts, h);

    cache_.clear();
    cache_.reserve(contacts.size());
    for (const auto& c : contacts) {
        cache_.push_back({c.a, c.b, c.point, c.jn, c.jt1, c.jt2});
    }
    std::stable_sort(cache_.begin(), cache_.end(),
                     [](const CachedImpulse& l, const CachedImpulse& r) {
                         return l.a < r.a || (l.a == r.a && l.b < r.b);
                     });

    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        Body& body = bodies_[i];
        if (!body.alive || body.type == BodyType::Static || body.sleeping) {
            continue;
        }
        BodyState& state = body.state;
        const math::Vec3 motion = state.velocity * h;
        const float travel = math::length(motion);
        bool swept = false;
        if (body.type == BodyType::Dynamic && !body.trigger &&
            travel > 0.5f * smallest_dimension(body.shape)) {
            // Fast body: stop it just short of anything it would pass through.
            const physics::detail::Collider caster =
                physics::detail::make_collider(body.shape, state.position, state.rotation);
            const math::Vec3 dir = motion / travel;
            SweepHit hit;
            if (cast_locked(caster, body.collision_layer, body.collision_mask, true, 0, dir, travel,
                            static_cast<int>(i), hit) &&
                math::dot(dir, hit.normal) < -0.05f) {
                state.position = state.position + dir * std::max(hit.distance - kCcdGap, 0.0f);
                swept = true;
            }
        }
        if (!swept) {
            state.position = state.position + motion;
        }
        const float spin = math::length(state.angular_velocity);
        if (spin > 1.0e-5f) {
            state.rotation =
                math::multiply(math::quat_from_axis_angle(state.angular_velocity / spin, spin * h),
                               state.rotation);
        }
    }

    project_positions(h);
    update_sleep(h);
    ++substeps_;
}

void CustomPhysicsWorld::step(float delta_seconds) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_ || !(delta_seconds > 0.0f) || !std::isfinite(delta_seconds)) {
        return;
    }
    contacts_ = 0;
    const float h = desc_.fixed_delta_seconds;
    accumulator_ += std::min(delta_seconds, 0.25f);
    std::uint32_t iterations = 0;
    while (accumulator_ >= h && iterations < 8u) {
        substep(h);
        accumulator_ -= h;
        ++iterations;
    }
    if (iterations > 0) {
        update_triggers();
    }
}

} // namespace poko
