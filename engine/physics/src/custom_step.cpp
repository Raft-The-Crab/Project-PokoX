#include "poko/physics/custom_world.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "custom_collide.h"

namespace poko {

namespace physics::detail {

struct ContactConstraint {
    int a = -1; // body index, -1 = world (ground)
    int b = -1;
    math::Vec3 normal;
    math::Vec3 ra;
    math::Vec3 rb;
    math::Vec3 tangent1;
    math::Vec3 tangent2;
    float depth = 0.0f;
    float friction = 0.0f;
    float inv_a = 0.0f; // effective inverse masses (0 for static/sleeping)
    float inv_b = 0.0f;
    float bounce = 0.0f; // target separating speed
    float mass_n = 0.0f;
    float mass_t1 = 0.0f;
    float mass_t2 = 0.0f;
    float jn = 0.0f; // accumulated impulses
    float jt1 = 0.0f;
    float jt2 = 0.0f;
};

} // namespace physics::detail

namespace {

using physics::detail::ContactConstraint;

constexpr int kSolverIterations = 12;
constexpr int kProjectionPasses = 2;
constexpr float kPenetrationSlop = 0.005f;
constexpr float kProjectionPercent = 0.8f;
constexpr float kBounceThreshold = 1.0f; // m/s of approach below which bodies do not bounce
constexpr float kSleepLinear = 0.05f;
constexpr float kSleepAngular = 0.05f;
constexpr float kSleepDelay = 0.5f;
constexpr float kAngularDamping = 0.05f; // 1/s

math::Vec3 apply_inertia(const math::Quat& q, const math::Vec3& inv_diag, const math::Vec3& v) {
    const math::Vec3 local = math::rotate(math::conjugate(q), v);
    return math::rotate(q, {local.x * inv_diag.x, local.y * inv_diag.y, local.z * inv_diag.z});
}

} // namespace

bool CustomPhysicsWorld::moving(const Body& body) const noexcept {
    if (body.type == BodyType::Static || body.sleeping) {
        return false;
    }
    return math::length_squared(body.state.velocity) > kSleepLinear * kSleepLinear ||
           math::length_squared(body.state.angular_velocity) > kSleepAngular * kSleepAngular;
}

void CustomPhysicsWorld::build_contacts(std::vector<ContactConstraint>& out) {
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
        if (!body.alive) {
            continue;
        }
        colliders[i] =
            physics::detail::make_collider(body.shape, body.state.position, body.state.rotation);
        boxes[i] = physics::detail::bounds(colliders[i]);
        order.push_back(static_cast<std::uint32_t>(i));
    }

    auto effective_inv = [](const Body& body) {
        return body.type == BodyType::Dynamic && !body.sleeping ? body.inv_mass : 0.0f;
    };

    auto prepare = [&](ContactConstraint& c, const Contact& contact, int ia, int ib) {
        c.a = ia;
        c.b = ib;
        c.normal = contact.normal;
        c.depth = contact.depth;
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
        c.bounce = approach < -kBounceThreshold ? -restitution * approach : 0.0f;
    };

    Contact points[physics::detail::kMaxContacts];

    for (std::size_t oi = 0; oi < order.size(); ++oi) {
        const std::uint32_t i = order[oi];
        for (std::size_t oj = oi + 1; oj < order.size(); ++oj) {
            const std::uint32_t j = order[oj];
            if (boxes[j].min.x > boxes[i].max.x) {
                break;
            }
            if (boxes[j].min.y > boxes[i].max.y || boxes[j].max.y < boxes[i].min.y ||
                boxes[j].min.z > boxes[i].max.z || boxes[j].max.z < boxes[i].min.z) {
                continue;
            }
            Body& a = bodies_[i];
            Body& b = bodies_[j];
            if (a.type != BodyType::Dynamic && b.type != BodyType::Dynamic) {
                continue;
            }
            if ((a.collision_layer & b.collision_mask) == 0 ||
                (b.collision_layer & a.collision_mask) == 0) {
                continue;
            }
            const bool a_awake = a.type == BodyType::Dynamic && !a.sleeping;
            const bool b_awake = b.type == BodyType::Dynamic && !b.sleeping;
            if (!a_awake && !b_awake && !moving(a) && !moving(b)) {
                continue;
            }
            const int n = physics::detail::collide(colliders[i], colliders[j], points);
            if (n == 0) {
                continue;
            }
            if (a.type == BodyType::Dynamic && a.sleeping && moving(b)) {
                wake(a);
            }
            if (b.type == BodyType::Dynamic && b.sleeping && moving(a)) {
                wake(b);
            }
            for (int k = 0; k < n; ++k) {
                out.emplace_back();
                prepare(out.back(), points[k], static_cast<int>(i), static_cast<int>(j));
            }
        }
    }

    if (ground_enabled_) {
        for (std::uint32_t i : order) {
            const Body& body = bodies_[i];
            if (body.type != BodyType::Dynamic || body.sleeping) {
                continue;
            }
            const int n = physics::detail::collide_plane(colliders[i], ground_height_, points);
            for (int k = 0; k < n; ++k) {
                out.emplace_back();
                prepare(out.back(), points[k], -1, static_cast<int>(i));
            }
        }
    }
}

void CustomPhysicsWorld::solve_contacts(std::vector<ContactConstraint>& contacts) {
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

    for (int iteration = 0; iteration < kSolverIterations; ++iteration) {
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
            const float lambda = (c.bounce - vn) * c.mass_n;
            const float previous = c.jn;
            c.jn = std::max(previous + lambda, 0.0f);
            push(c, c.normal * (c.jn - previous));
        }
    }
}

void CustomPhysicsWorld::project_positions() {
    std::vector<ContactConstraint> contacts;
    for (int pass = 0; pass < kProjectionPasses; ++pass) {
        build_contacts(contacts);
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
        if (!body.alive || body.type != BodyType::Dynamic || body.sleeping) {
            continue;
        }
        if (moving(body)) {
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
    build_contacts(contacts);
    contacts_ += static_cast<std::uint32_t>(contacts.size());
    solve_contacts(contacts);

    for (auto& body : bodies_) {
        if (!body.alive || body.type == BodyType::Static || body.sleeping) {
            continue;
        }
        BodyState& state = body.state;
        state.position = state.position + state.velocity * h;
        const float spin = math::length(state.angular_velocity);
        if (spin > 1.0e-5f) {
            state.rotation =
                math::multiply(math::quat_from_axis_angle(state.angular_velocity / spin, spin * h),
                               state.rotation);
        }
    }

    project_positions();
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
}

} // namespace poko
