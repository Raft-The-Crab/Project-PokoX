#include "poko/physics/custom_world.h"

#include <cmath>
#include <cstddef>

namespace poko {
namespace {

// Diagonal of the inverse local inertia tensor. Capsules use their bounding box.
math::Vec3 inverse_inertia(const ShapeDesc& shape, float mass) noexcept {
    float ix = 0.0f;
    float iy = 0.0f;
    float iz = 0.0f;
    auto box = [&](float hx, float hy, float hz) {
        ix = mass / 3.0f * (hy * hy + hz * hz);
        iy = mass / 3.0f * (hx * hx + hz * hz);
        iz = mass / 3.0f * (hx * hx + hy * hy);
    };
    switch (shape.kind) {
    case ShapeKind::Sphere:
        ix = iy = iz = 0.4f * mass * shape.radius * shape.radius;
        break;
    case ShapeKind::Box:
        box(shape.half_extents.x, shape.half_extents.y, shape.half_extents.z);
        break;
    case ShapeKind::Capsule:
        box(shape.radius, shape.half_height + shape.radius, shape.radius);
        break;
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
    // Anything resting on the removed body may have lost its support.
    for (auto& other : bodies_) {
        wake(other);
    }
    ++generations_[body.index];
    free_.push_back(body.index);
    return Status::success();
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
}

std::uint32_t CustomPhysicsWorld::substep_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return substeps_;
}

} // namespace poko
