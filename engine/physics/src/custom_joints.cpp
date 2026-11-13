#include <cmath>

#include "joint_rows.h"
#include "poko/physics/custom_world.h"

namespace poko {
namespace {

using math::Vec3;
constexpr float kJointBeta = 0.2f;
constexpr float kInf = 1.0e30f;

Vec3 perpendicular(const Vec3& n) noexcept {
    const Vec3 helper = std::abs(n.x) < 0.57f ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
    return math::normalize(math::cross(n, helper));
}

bool finite(const Vec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// Rotation vector (world frame) that takes the expected orientation of b to its
// actual one; small-angle error used for orientation locks.
Vec3 orientation_error(const math::Quat& qa, const math::Quat& qb, const math::Quat& rel0) noexcept {
    const math::Quat expected = math::multiply(qa, rel0);
    const math::Quat e = math::multiply(qb, math::conjugate(expected));
    const float s = e.w < 0.0f ? -2.0f : 2.0f;
    return {e.x * s, e.y * s, e.z * s};
}

} // namespace

bool CustomPhysicsWorld::joint_live(JointHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < joints_.size() &&
           joint_generations_[handle.index] == handle.generation && joints_[handle.index].alive;
}

Result<JointHandle> CustomPhysicsWorld::create_joint(const JointDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<JointHandle>::fail(ErrorCode::FailedPrecondition, "physics world is not initialized");
    }
    const bool world_b = !is_valid(desc.body_b);
    if (!handle_live(desc.body_a) || (!world_b && !handle_live(desc.body_b)) ||
        (!world_b && desc.body_a.index == desc.body_b.index)) {
        return Result<JointHandle>::fail(ErrorCode::InvalidArgument, "joint bodies are not live or identical");
    }
    if (!finite(desc.anchor) || !finite(desc.axis)) {
        return Result<JointHandle>::fail(ErrorCode::InvalidArgument, "non-finite joint anchor or axis");
    }
    const bool needs_axis = desc.type == JointType::Hinge || desc.type == JointType::Slider;
    if (needs_axis && math::length_squared(desc.axis) < 1.0e-8f) {
        return Result<JointHandle>::fail(ErrorCode::InvalidArgument, "joint axis must be non-zero");
    }
    if (desc.limit_enabled) {
        const bool ok = std::isfinite(desc.lower) && std::isfinite(desc.upper) &&
                        desc.lower <= desc.upper && desc.type != JointType::Ball &&
                        desc.type != JointType::Fixed &&
                        (desc.type != JointType::Hinge ||
                         (desc.lower >= -math::kPi && desc.upper <= math::kPi));
        if (!ok) {
            return Result<JointHandle>::fail(ErrorCode::InvalidArgument, "invalid joint limits");
        }
    }

    const BodyState& sa = bodies_[desc.body_a.index].state;
    const BodyState* sb = world_b ? nullptr : &bodies_[desc.body_b.index].state;
    const math::Quat qa = sa.rotation;
    const math::Quat qb = sb != nullptr ? sb->rotation : math::quat_identity();
    const Vec3 pb = sb != nullptr ? sb->position : Vec3{};
    const math::Quat inv_a = math::conjugate(qa);
    const math::Quat inv_b = math::conjugate(qb);

    Joint joint;
    joint.alive = true;
    joint.type = desc.type;
    joint.a = static_cast<int>(desc.body_a.index);
    joint.b = world_b ? -1 : static_cast<int>(desc.body_b.index);
    joint.local_a = math::rotate(inv_a, desc.anchor - sa.position);
    joint.local_b = math::rotate(inv_b, desc.anchor - pb);
    if (needs_axis) {
        const Vec3 axis = math::normalize(desc.axis);
        const Vec3 ref = perpendicular(axis);
        joint.axis_a = math::rotate(inv_a, axis);
        joint.axis_b = math::rotate(inv_b, axis);
        joint.ref_a = math::rotate(inv_a, ref);
        joint.ref_b = math::rotate(inv_b, ref);
    }
    joint.rel0 = math::multiply(inv_a, qb);
    joint.limit_enabled = desc.limit_enabled;
    joint.lower = desc.lower;
    joint.upper = desc.upper;
    joint.collide_connected = desc.collide_connected;

    HandleIndex index;
    if (!joint_free_.empty()) {
        index = joint_free_.back();
        joint_free_.pop_back();
        joints_[index] = joint;
    } else {
        index = static_cast<HandleIndex>(joints_.size());
        joints_.push_back(joint);
        joint_generations_.push_back(0);
    }
    // Wake both bodies so the new constraint takes effect immediately.
    bodies_[desc.body_a.index].sleeping = false;
    bodies_[desc.body_a.index].sleep_timer = 0.0f;
    if (!world_b) {
        bodies_[desc.body_b.index].sleeping = false;
        bodies_[desc.body_b.index].sleep_timer = 0.0f;
    }
    return Result<JointHandle>::success(make_handle(index, joint_generations_[index]));
}

Status CustomPhysicsWorld::destroy_joint(JointHandle joint) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!joint_live(joint)) {
        return Status::fail(ErrorCode::NotFound, "joint handle is not live");
    }
    Joint& j = joints_[joint.index];
    for (const int body : {j.a, j.b}) {
        if (body >= 0) {
            bodies_[static_cast<std::size_t>(body)].sleeping = false;
            bodies_[static_cast<std::size_t>(body)].sleep_timer = 0.0f;
        }
    }
    j.alive = false;
    ++joint_generations_[joint.index];
    joint_free_.push_back(joint.index);
    return Status::success();
}

Status CustomPhysicsWorld::set_joint_motor(JointHandle joint, float target_speed, float max_force) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!joint_live(joint)) {
        return Status::fail(ErrorCode::NotFound, "joint handle is not live");
    }
    Joint& j = joints_[joint.index];
    if (j.type != JointType::Hinge && j.type != JointType::Slider) {
        return Status::fail(ErrorCode::Unsupported, "only hinge and slider joints have motors");
    }
    if (!std::isfinite(target_speed) || !std::isfinite(max_force) || max_force < 0.0f) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid motor parameters");
    }
    j.motor_speed = target_speed;
    j.motor_max_force = max_force;
    for (const int body : {j.a, j.b}) {
        if (body >= 0) bodies_[static_cast<std::size_t>(body)].sleeping = false;
    }
    return Status::success();
}

std::uint32_t CustomPhysicsWorld::joint_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t count = 0;
    for (const auto& j : joints_) {
        if (j.alive) ++count;
    }
    return count;
}

float CustomPhysicsWorld::joint_position_unlocked(const Joint& j) const noexcept {
    const BodyState& sa = bodies_[static_cast<std::size_t>(j.a)].state;
    const bool world_b = j.b < 0;
    const math::Quat qb = world_b ? math::quat_identity() : bodies_[static_cast<std::size_t>(j.b)].state.rotation;
    const Vec3 pb = world_b ? Vec3{} : bodies_[static_cast<std::size_t>(j.b)].state.position;
    if (j.type == JointType::Hinge) {
        const Vec3 axis = math::rotate(sa.rotation, j.axis_a);
        const Vec3 ra = math::rotate(sa.rotation, j.ref_a);
        const Vec3 rb = math::rotate(qb, j.ref_b);
        return std::atan2(math::dot(axis, math::cross(ra, rb)), math::dot(ra, rb));
    }
    if (j.type == JointType::Slider) {
        const Vec3 axis = math::rotate(sa.rotation, j.axis_a);
        const Vec3 d = (pb + math::rotate(qb, j.local_b)) -
                       (sa.position + math::rotate(sa.rotation, j.local_a));
        return math::dot(d, axis);
    }
    return 0.0f;
}

Result<float> CustomPhysicsWorld::joint_position(JointHandle joint) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!joint_live(joint)) {
        return Result<float>::fail(ErrorCode::NotFound, "joint handle is not live");
    }
    return Result<float>::success(joint_position_unlocked(joints_[joint.index]));
}

void CustomPhysicsWorld::build_joint_rows(float h, const std::vector<float>& inv_m,
                                          std::vector<physics::detail::JointRow>& out) {
    using physics::detail::JointRow;
    const float beta = kJointBeta / h;
    for (std::size_t index = 0; index < joints_.size(); ++index) {
        Joint& j = joints_[index];
        if (!j.alive) continue;
        const std::size_t ia = static_cast<std::size_t>(j.a);
        const bool world_b = j.b < 0;
        if (inv_m[ia] == 0.0f && (world_b || inv_m[static_cast<std::size_t>(j.b)] == 0.0f)) {
            for (float& v : j.impulses) v = 0.0f;
            continue;
        }
        const BodyState& sa = bodies_[ia].state;
        const math::Quat qa = sa.rotation;
        const math::Quat qb = world_b ? math::quat_identity() : bodies_[static_cast<std::size_t>(j.b)].state.rotation;
        const Vec3 pb = world_b ? Vec3{} : bodies_[static_cast<std::size_t>(j.b)].state.position;
        const Vec3 ra = math::rotate(qa, j.local_a);
        const Vec3 rb = math::rotate(qb, j.local_b);
        const Vec3 d = (pb + rb) - (sa.position + ra);

        bool used[8] = {};
        const auto emit = [&](int slot, const Vec3& lin, const Vec3& ang_a, const Vec3& ang_b,
                              float bias, float lo, float hi) {
            JointRow row;
            row.joint = static_cast<int>(index);
            row.slot = slot;
            row.a = j.a;
            row.b = j.b;
            row.lin = lin;
            row.ang_a = ang_a;
            row.ang_b = ang_b;
            row.bias = bias;
            row.lo = lo;
            row.hi = hi;
            row.impulse = j.impulses[slot];
            out.push_back(row);
            used[slot] = true;
        };
        const auto linear = [&](int slot, const Vec3& dir, float c, float bias_extra, float lo, float hi) {
            emit(slot, dir, math::cross(ra, dir), math::cross(rb, dir), -beta * c + bias_extra, lo, hi);
        };
        const auto angular = [&](int slot, const Vec3& axis, float c, float bias_extra, float lo, float hi) {
            emit(slot, Vec3{}, axis, axis, -beta * c + bias_extra, lo, hi);
        };
        const Vec3 ex{1, 0, 0};
        const Vec3 ey{0, 1, 0};
        const Vec3 ez{0, 0, 1};
        const auto angular_lock = [&](int first_slot) {
            const Vec3 err = orientation_error(qa, qb, j.rel0);
            angular(first_slot + 0, ex, err.x, 0.0f, -kInf, kInf);
            angular(first_slot + 1, ey, err.y, 0.0f, -kInf, kInf);
            angular(first_slot + 2, ez, err.z, 0.0f, -kInf, kInf);
        };

        switch (j.type) {
        case JointType::Ball:
        case JointType::Fixed:
            linear(0, ex, d.x, 0.0f, -kInf, kInf);
            linear(1, ey, d.y, 0.0f, -kInf, kInf);
            linear(2, ez, d.z, 0.0f, -kInf, kInf);
            if (j.type == JointType::Fixed) angular_lock(3);
            break;
        case JointType::Hinge: {
            linear(0, ex, d.x, 0.0f, -kInf, kInf);
            linear(1, ey, d.y, 0.0f, -kInf, kInf);
            linear(2, ez, d.z, 0.0f, -kInf, kInf);
            const Vec3 axis = math::rotate(qa, j.axis_a);
            const Vec3 axis_b = math::rotate(qb, j.axis_b);
            const Vec3 err = math::cross(axis, axis_b);
            const Vec3 t1 = perpendicular(axis);
            const Vec3 t2 = math::cross(axis, t1);
            angular(3, t1, math::dot(err, t1), 0.0f, -kInf, kInf);
            angular(4, t2, math::dot(err, t2), 0.0f, -kInf, kInf);
            const float angle = joint_position_unlocked(j);
            if (j.limit_enabled) {
                if (angle < j.lower) angular(5, axis, angle - j.lower, 0.0f, 0.0f, kInf);
                if (angle > j.upper) angular(6, -axis, j.upper - angle, 0.0f, 0.0f, kInf);
            }
            if (j.motor_max_force > 0.0f) {
                const float limit = j.motor_max_force * h;
                angular(7, axis, 0.0f, j.motor_speed, -limit, limit);
            }
            break;
        }
        case JointType::Slider: {
            angular_lock(0);
            const Vec3 axis = math::rotate(qa, j.axis_a);
            const Vec3 t1 = perpendicular(axis);
            const Vec3 t2 = math::cross(axis, t1);
            linear(3, t1, math::dot(d, t1), 0.0f, -kInf, kInf);
            linear(4, t2, math::dot(d, t2), 0.0f, -kInf, kInf);
            const float position = math::dot(d, axis);
            if (j.limit_enabled) {
                if (position < j.lower) linear(5, axis, position - j.lower, 0.0f, 0.0f, kInf);
                if (position > j.upper) linear(6, -axis, j.upper - position, 0.0f, 0.0f, kInf);
            }
            if (j.motor_max_force > 0.0f) {
                const float limit = j.motor_max_force * h;
                linear(7, axis, 0.0f, j.motor_speed, -limit, limit);
            }
            break;
        }
        }
        for (int slot = 0; slot < 8; ++slot) {
            if (!used[slot]) j.impulses[slot] = 0.0f;
        }
    }
}

} // namespace poko
