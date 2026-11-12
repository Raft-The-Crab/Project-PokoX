#include "poko/physics/custom_world.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "custom_detail.h"

namespace poko {

void CustomPhysicsWorld::resolve_ground() {
    for (auto& body : bodies_) {
        if (!body.alive || body.type != BodyType::Dynamic) {
            continue;
        }
        const float radius = physics::detail::bounding_radius(body.shape);
        const float floor = ground_height_ + radius;
        BodyState& state = body.state;
        if (state.position.y >= floor) {
            continue;
        }
        state.position.y = floor;
        if (state.velocity.y < 0.0f) {
            state.velocity.y = -state.velocity.y * body.restitution;
        }
        // Simple Coulomb-style ground damping on the tangent plane.
        const float damp = std::max(0.0f, 1.0f - body.friction * 4.0f * desc_.fixed_delta_seconds);
        state.velocity.x *= damp;
        state.velocity.z *= damp;
        ++contacts_;
    }
}

void CustomPhysicsWorld::resolve_contacts() {
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        if (!bodies_[i].alive) {
            continue;
        }
        for (std::size_t j = i + 1; j < bodies_.size(); ++j) {
            if (!bodies_[j].alive) {
                continue;
            }
            Body& a = bodies_[i];
            Body& b = bodies_[j];
            // At least one participant must be dynamic.
            if (a.type != BodyType::Dynamic && b.type != BodyType::Dynamic) {
                continue;
            }
            if ((a.collision_layer & b.collision_mask) == 0 ||
                (b.collision_layer & a.collision_mask) == 0) {
                continue;
            }
            physics::detail::Contact contact;
            if (!physics::detail::overlap(a.state.position, physics::detail::bounding_radius(a.shape),
                                 b.state.position, physics::detail::bounding_radius(b.shape), contact)) {
                continue;
            }
            ++contacts_;

            const float inv_a =
                (a.type == BodyType::Dynamic && a.mass > 0.0f) ? 1.0f / a.mass : 0.0f;
            const float inv_b =
                (b.type == BodyType::Dynamic && b.mass > 0.0f) ? 1.0f / b.mass : 0.0f;
            const float inv_sum = inv_a + inv_b;
            if (inv_sum <= 0.0f) {
                continue;
            }

            // Positional correction split by inverse mass.
            const float correction = contact.penetration / inv_sum;
            if (inv_a > 0.0f) {
                a.state.position = a.state.position - contact.normal * (correction * inv_a);
            }
            if (inv_b > 0.0f) {
                b.state.position = b.state.position + contact.normal * (correction * inv_b);
            }

            const math::Vec3 relative = b.state.velocity - a.state.velocity;
            const float normal_speed = math::dot(relative, contact.normal);
            if (normal_speed >= 0.0f) {
                continue; // separating
            }
            const float restitution = std::min(a.restitution, b.restitution);
            const float impulse_magnitude = -(1.0f + restitution) * normal_speed / inv_sum;
            const math::Vec3 impulse = contact.normal * impulse_magnitude;
            if (inv_a > 0.0f) {
                a.state.velocity = a.state.velocity - impulse * inv_a;
            }
            if (inv_b > 0.0f) {
                b.state.velocity = b.state.velocity + impulse * inv_b;
            }

            // Tangential friction impulse, clamped to the Coulomb cone.
            const math::Vec3 tangent = relative - contact.normal * normal_speed;
            const float tangent_length = math::length(tangent);
            if (tangent_length <= 1.0e-5f) {
                continue;
            }
            const math::Vec3 tangent_dir = tangent / tangent_length;
            float friction_impulse = -math::dot(relative, tangent_dir) / inv_sum;
            const float mu = std::sqrt(a.friction * b.friction);
            friction_impulse =
                math::clamp(friction_impulse, -impulse_magnitude * mu, impulse_magnitude * mu);
            const math::Vec3 friction_vec = tangent_dir * friction_impulse;
            if (inv_a > 0.0f) {
                a.state.velocity = a.state.velocity - friction_vec * inv_a;
            }
            if (inv_b > 0.0f) {
                b.state.velocity = b.state.velocity + friction_vec * inv_b;
            }
        }
    }
}

void CustomPhysicsWorld::substep(float h) {
    // Semi-implicit Euler for dynamics; angular velocity integrated as a
    // rotation quaternion about its own axis.
    for (auto& body : bodies_) {
        if (!body.alive || body.type != BodyType::Dynamic) {
            continue;
        }
        BodyState& state = body.state;
        state.velocity = state.velocity + desc_.gravity * h;
        state.position = state.position + state.velocity * h;
        const float spin = math::length(state.angular_velocity);
        if (spin > 1.0e-5f) {
            const math::Vec3 axis = state.angular_velocity / spin;
            state.rotation =
                math::multiply(math::quat_from_axis_angle(axis, spin * h), state.rotation);
        }
    }

    if (ground_enabled_) {
        resolve_ground();
    }
    resolve_contacts();
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
