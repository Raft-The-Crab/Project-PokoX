#include "poko/physics/custom_world.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace poko {
namespace {

// Diagonal of the inverse local inertia tensor. A capsule is a cylinder plus
// two hemispheres sharing the mass by volume (axis = local Y).
math::Vec3 inverse_inertia(const ShapeDesc& shape, float mass) noexcept {
    float ix = 0.0f;
    float iy = 0.0f;
    float iz = 0.0f;
    switch (shape.kind) {
    case ShapeKind::Sphere:
        ix = iy = iz = 0.4f * mass * shape.radius * shape.radius;
        break;
    case ShapeKind::Box: {
        const math::Vec3& h = shape.half_extents;
        ix = mass / 3.0f * (h.y * h.y + h.z * h.z);
        iy = mass / 3.0f * (h.x * h.x + h.z * h.z);
        iz = mass / 3.0f * (h.x * h.x + h.y * h.y);
        break;
    }
    case ShapeKind::Capsule: {
        const float r = shape.radius;
        const float h = 2.0f * shape.half_height; // cylinder height
        const float cylinder = r * r * h;         // volume / pi
        const float spheres = 4.0f / 3.0f * r * r * r;
        const float total = cylinder + spheres;
        const float m_cyl = mass * cylinder / total;
        const float m_sph = mass * spheres / total;
        iy = m_cyl * r * r * 0.5f + m_sph * 0.4f * r * r;
        ix = iz = m_cyl * (r * r / 4.0f + h * h / 12.0f) +
                  m_sph * (0.4f * r * r + h * h / 2.0f + 3.0f * h * r / 8.0f);
        break;
    }
    }
    return {1.0f / ix, 1.0f / iy, 1.0f / iz};
}

} // namespace

bool CustomPhysicsWorld::initialize(const PhysicsWorldDesc& desc) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    if (initialized_) {
        return true;
    }
    if (!std::isfinite(desc.fixed_delta_seconds) || desc.fixed_delta_seconds <= 0.0f) {
        return false;
    }
    if (!std::isfinite(desc.gravity.x) || !std::isfinite(desc.gravity.y) ||
        !std::isfinite(desc.gravity.z)) {
        return false;
    }
    desc_ = desc;
    initialized_ = true;
    return true;
}

bool CustomPhysicsWorld::handle_live(BodyHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < bodies_.size() &&
           generations_[handle.index] == handle.generation && bodies_[handle.index].alive;
}

CustomPhysicsWorld::Body* CustomPhysicsWorld::find(BodyHandle handle) noexcept {
    if (!is_valid(handle) || static_cast<std::size_t>(handle.index) >= bodies_.size()) {
        return nullptr;
    }
    return &bodies_[handle.index];
}

const CustomPhysicsWorld::Body* CustomPhysicsWorld::find(BodyHandle handle) const noexcept {
    if (!is_valid(handle) || static_cast<std::size_t>(handle.index) >= bodies_.size()) {
        return nullptr;
    }
    return &bodies_[handle.index];
}

Result<BodyHandle> CustomPhysicsWorld::create_body(const BodyDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<BodyHandle>::fail(ErrorCode::FailedPrecondition,
                                        "physics world is not initialized");
    }
    if (!valid(desc)) {
        return Result<BodyHandle>::fail(ErrorCode::InvalidArgument, "invalid body descriptor");
    }
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        bodies_[index] = Body{};
    } else {
        index = static_cast<HandleIndex>(bodies_.size());
        bodies_.push_back(Body{});
        generations_.push_back(0);
    }
    Body& body = bodies_[index];
    body.alive = true;
    body.type = desc.type;
    body.shape = desc.shape;
    body.state = BodyState{desc.position, desc.rotation, desc.velocity, desc.angular_velocity};
    body.mass = desc.type == BodyType::Static ? 0.0f : desc.mass;
    body.friction = desc.friction;
    body.restitution = desc.restitution;
    body.collision_layer = desc.collision_layer;
    body.collision_mask = desc.collision_mask;
    body.trigger = desc.is_trigger;
    body.in_motion = desc.type != BodyType::Static;
    if (desc.type == BodyType::Dynamic) {
        body.inv_mass = 1.0f / desc.mass;
        body.inv_inertia = inverse_inertia(desc.shape, desc.mass);
    }
    return Result<BodyHandle>::success(make_handle(index, generations_[index]));
}

Status CustomPhysicsWorld::destroy_body(BodyHandle body) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(body)) {
        return Status::fail(ErrorCode::NotFound, "body handle is not live");
    }
    bodies_[body.index].alive = false;
    forget_cached(static_cast<int>(body.index));
    // Anything resting on the removed body may have lost its support.
    for (auto& other : bodies_) {
        wake(other);
    }
    ++generations_[body.index];
    free_.push_back(body.index);
    for (std::size_t i = 0; i < joints_.size(); ++i) {
        Joint& joint = joints_[i];
        if (joint.alive && (!handle_live(joint.desc.body_a) ||
                            (is_valid(joint.desc.body_b) && !handle_live(joint.desc.body_b)))) {
            joint.alive = false;
            ++joint_generations_[i];
            joint_free_.push_back(static_cast<HandleIndex>(i));
        }
    }
    return Status::success();
}

void CustomPhysicsWorld::forget_cached(int body_index) {
    cache_.erase(std::remove_if(cache_.begin(), cache_.end(),
                                [body_index](const CachedImpulse& c) {
                                    return c.a == body_index || c.b == body_index;
                                }),
                 cache_.end());
}

bool CustomPhysicsWorld::joint_live(JointHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < joints_.size() &&
           joint_generations_[handle.index] == handle.generation && joints_[handle.index].alive;
}

Result<JointHandle> CustomPhysicsWorld::create_joint(const JointDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<JointHandle>::fail(ErrorCode::FailedPrecondition,
                                         "physics world is not initialized");
    }
    if (!valid(desc)) {
        return Result<JointHandle>::fail(ErrorCode::InvalidArgument, "invalid joint descriptor");
    }
    if (!handle_live(desc.body_a) || (is_valid(desc.body_b) && !handle_live(desc.body_b))) {
        return Result<JointHandle>::fail(ErrorCode::NotFound, "joint body handle is not live");
    }
    Body& a = bodies_[desc.body_a.index];
    Body* b = is_valid(desc.body_b) ? &bodies_[desc.body_b.index] : nullptr;
    if (a.type != BodyType::Dynamic && (b == nullptr || b->type != BodyType::Dynamic)) {
        return Result<JointHandle>::fail(ErrorCode::InvalidArgument,
                                         "joint needs at least one dynamic body");
    }

    const math::Quat qb = b != nullptr ? b->state.rotation : math::Quat{};
    const math::Vec3 pa = a.state.position + math::rotate(a.state.rotation, desc.anchor_a);
    const math::Vec3 pb =
        b != nullptr ? b->state.position + math::rotate(qb, desc.anchor_b) : desc.anchor_b;

    HandleIndex index;
    if (!joint_free_.empty()) {
        index = joint_free_.back();
        joint_free_.pop_back();
        joints_[index] = Joint{};
    } else {
        index = static_cast<HandleIndex>(joints_.size());
        joints_.push_back(Joint{});
        joint_generations_.push_back(0);
    }
    Joint& joint = joints_[index];
    joint.alive = true;
    joint.desc = desc;
    joint.reference = math::multiply(math::conjugate(a.state.rotation), qb);
    joint.rest_length = desc.distance >= 0.0f ? desc.distance : math::distance(pa, pb);
    wake(a);
    if (b != nullptr) {
        wake(*b);
    }
    return Result<JointHandle>::success(make_handle(index, joint_generations_[index]));
}

Status CustomPhysicsWorld::destroy_joint(JointHandle handle) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!joint_live(handle)) {
        return Status::fail(ErrorCode::NotFound, "joint handle is not live");
    }
    joints_[handle.index].alive = false;
    ++joint_generations_[handle.index];
    joint_free_.push_back(handle.index);
    for (auto& body : bodies_) {
        wake(body);
    }
    return Status::success();
}

std::uint32_t CustomPhysicsWorld::joint_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t count = 0;
    for (const auto& joint : joints_) {
        if (joint.alive) {
            ++count;
        }
    }
    return count;
}

Status CustomPhysicsWorld::set_body_state(BodyHandle body, const BodyState& state) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "physics world is not initialized");
    }
    if (!handle_live(body)) {
        return Status::fail(ErrorCode::NotFound, "body handle is not live");
    }
    bodies_[body.index].state = state;
    forget_cached(static_cast<int>(body.index));
    wake(bodies_[body.index]);
    return Status::success();
}

Result<BodyState> CustomPhysicsWorld::get_body_state(BodyHandle body) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const Body* found = find(body);
    if (found == nullptr || !handle_live(body)) {
        return Result<BodyState>::fail(ErrorCode::NotFound, "body handle is not live");
    }
    return Result<BodyState>::success(found->state);
}

std::uint32_t CustomPhysicsWorld::body_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t count = 0;
    for (const auto& body : bodies_) {
        if (body.alive) {
            ++count;
        }
    }
    return count;
}

void CustomPhysicsWorld::set_ground(bool enabled, float height) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    ground_enabled_ = enabled;
    ground_height_ = height;
}

bool CustomPhysicsWorld::ground_enabled() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return ground_enabled_;
}

std::uint32_t CustomPhysicsWorld::contact_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return contacts_;
}

std::uint32_t CustomPhysicsWorld::sleeping_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t count = 0;
    for (const auto& body : bodies_) {
        if (body.alive && body.sleeping) {
            ++count;
        }
    }
    return count;
}

bool CustomPhysicsWorld::is_sleeping(BodyHandle body) const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return handle_live(body) && bodies_[body.index].sleeping;
}

void CustomPhysicsWorld::wake(Body& body) noexcept {
    body.sleeping = false;
    body.sleep_time = 0.0f;
    body.in_motion = true; // recomputed from real velocities at the end of the next substep
}

std::uint32_t CustomPhysicsWorld::substep_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return substeps_;
}

} // namespace poko
