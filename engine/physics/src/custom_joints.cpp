#include <cmath>

#include "custom_math.h"
#include "poko/physics/custom_world.h"

namespace poko {

// Velocity-level joint constraints with Baumgarte position feedback. Called once
// per solver iteration, before the contact constraints.
void CustomPhysicsWorld::solve_joints(float h) {
    using namespace physics::detail;
    constexpr float kBeta = 0.2f;
    const float bias_scale = kBeta / h;
    const math::Vec3 zero{};

    for (auto& joint : joints_) {
        if (!joint.alive) {
            continue;
        }
        const JointDesc& desc = joint.desc;
        Body& a = bodies_[desc.body_a.index];
        Body* b = is_valid(desc.body_b) ? &bodies_[desc.body_b.index] : nullptr;
        const float inv_a = a.type == BodyType::Dynamic ? a.inv_mass : 0.0f;
        const float inv_b = (b != nullptr && b->type == BodyType::Dynamic) ? b->inv_mass : 0.0f;
        if (inv_a <= 0.0f && inv_b <= 0.0f) {
            continue;
        }
        const Mat3 ia =
            inv_a > 0.0f ? world_inverse_inertia(a.state.rotation, a.inv_inertia) : Mat3{};
        const Mat3 ib =
            inv_b > 0.0f ? world_inverse_inertia(b->state.rotation, b->inv_inertia) : Mat3{};
        const math::Quat qb = b != nullptr ? b->state.rotation : math::Quat{};
        const math::Vec3 ra = math::rotate(a.state.rotation, desc.anchor_a);
        const math::Vec3 rb = b != nullptr ? math::rotate(qb, desc.anchor_b) : zero;
        const math::Vec3 pa = a.state.position + ra;
        const math::Vec3 pb = b != nullptr ? b->state.position + rb : desc.anchor_b;

        auto anchor_velocity = [&]() {
            const math::Vec3 va = a.state.velocity + math::cross(a.state.angular_velocity, ra);
            const math::Vec3 vb =
                b != nullptr ? b->state.velocity + math::cross(b->state.angular_velocity, rb)
                             : zero;
            return vb - va;
        };
        auto angular_velocity = [&]() {
            return (b != nullptr ? b->state.angular_velocity : zero) - a.state.angular_velocity;
        };
        // Equal and opposite impulse applied at the anchors.
        auto push_linear = [&](const math::Vec3& impulse) {
            if (inv_a > 0.0f) {
                a.state.velocity = a.state.velocity - impulse * inv_a;
                a.state.angular_velocity = a.state.angular_velocity - ia * math::cross(ra, impulse);
            }
            if (inv_b > 0.0f) {
                b->state.velocity = b->state.velocity + impulse * inv_b;
                b->state.angular_velocity =
                    b->state.angular_velocity + ib * math::cross(rb, impulse);
            }
        };
        auto push_angular = [&](const math::Vec3& torque_impulse) {
            if (inv_a > 0.0f) {
                a.state.angular_velocity = a.state.angular_velocity - ia * torque_impulse;
            }
            if (inv_b > 0.0f) {
                b->state.angular_velocity = b->state.angular_velocity + ib * torque_impulse;
            }
        };
        // Drives relative angular velocity along `axis` toward -bias_scale * error.
        auto solve_angular_row = [&](const math::Vec3& axis, float error) {
            const float k = math::dot(axis, ia * axis) + math::dot(axis, ib * axis);
            if (k <= 0.0f) {
                return;
            }
            const float lambda = -(math::dot(angular_velocity(), axis) + bias_scale * error) / k;
            push_angular(axis * lambda);
        };

        if (desc.kind == JointKind::Distance) {
            const math::Vec3 delta = pb - pa;
            const float length = math::length(delta);
            const math::Vec3 n = length > 1.0e-6f ? delta / length : math::Vec3{0.0f, 1.0f, 0.0f};
            const math::Vec3 rna = math::cross(ra, n);
            const math::Vec3 rnb = math::cross(rb, n);
            const float k = inv_a + inv_b + math::dot(rna, ia * rna) + math::dot(rnb, ib * rnb);
            if (k > 0.0f) {
                const float error = length - joint.rest_length;
                const float lambda = -(math::dot(anchor_velocity(), n) + bias_scale * error) / k;
                push_linear(n * lambda);
            }
            continue;
        }

        // Point constraint shared by ball, hinge and fixed joints.
        Mat3 k = identity3() * (inv_a + inv_b);
        if (inv_a > 0.0f) {
            k = k - skew(ra) * ia * skew(ra);
        }
        if (inv_b > 0.0f) {
            k = k - skew(rb) * ib * skew(rb);
        }
        push_linear(inverse(k) * (-(anchor_velocity() + (pb - pa) * bias_scale)));

        if (desc.kind == JointKind::Hinge) {
            const math::Vec3 axis_a = math::normalize(math::rotate(a.state.rotation, desc.axis_a));
            const math::Vec3 axis_b =
                math::normalize(b != nullptr ? math::rotate(qb, desc.axis_b) : desc.axis_b);
            const math::Vec3 error = math::cross(axis_a, axis_b);
            const math::Vec3 helper = std::abs(axis_a.x) > 0.9f ? math::Vec3{0.0f, 1.0f, 0.0f}
                                                                : math::Vec3{1.0f, 0.0f, 0.0f};
            const math::Vec3 t1 = math::normalize(math::cross(axis_a, helper));
            const math::Vec3 t2 = math::cross(axis_a, t1);
            solve_angular_row(t1, math::dot(error, t1));
            solve_angular_row(t2, math::dot(error, t2));
        } else if (desc.kind == JointKind::Fixed) {
            const math::Quat desired = math::multiply(a.state.rotation, joint.reference);
            math::Quat diff = math::multiply(qb, math::conjugate(desired));
            if (diff.w < 0.0f) {
                diff = {-diff.x, -diff.y, -diff.z, -diff.w};
            }
            const math::Vec3 error{2.0f * diff.x, 2.0f * diff.y, 2.0f * diff.z};
            solve_angular_row({1.0f, 0.0f, 0.0f}, error.x);
            solve_angular_row({0.0f, 1.0f, 0.0f}, error.y);
            solve_angular_row({0.0f, 0.0f, 1.0f}, error.z);
        }
    }
}

} // namespace poko
