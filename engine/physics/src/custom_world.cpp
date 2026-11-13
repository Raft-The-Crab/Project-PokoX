#include "poko/physics/custom_world.h"

#include <cmath>
#include <cstddef>

namespace poko {

namespace {

// Diagonal of the inverse local inertia tensor for a solid shape of `mass`.
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
        const float height = 2.0f * shape.half_height;
        const float cylinder = r * r * height;
        const float sphere = 4.0f / 3.0f * r * r * r;
        const float mc = mass * cylinder / (cylinder + sphere);
        const float ms = mass - mc;
        iy = mc * r * r * 0.5f + ms * 0.4f * r * r;
        ix = iz = mc * (r * r / 4.0f + height * height / 12.0f) +
                  ms * (0.4f * r * r + height * height / 4.0f + 0.375f * height * r);
        break;
    }
    }
    const auto inv = [](float v) { return v > 1.0e-9f ? 1.0f / v : 0.0f; };
    return {inv(ix), inv(iy), inv(iz)};
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
    body.inv_inertia = desc.type == BodyType::Dynamic ? inverse_inertia(desc.shape, desc.mass) : math::Vec3{};
    body.friction = desc.friction;
    body.restitution = desc.restitution;
    body.collision_layer = desc.collision_layer;
    body.collision_mask = desc.collision_mask;
    return Result<BodyHandle>::success(make_handle(index, generations_[index]));
}

Status CustomPhysicsWorld::destroy_body(BodyHandle body) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(body)) {
        return Status::fail(ErrorCode::NotFound, "body handle is not live");
    }
    bodies_[body.index].alive = false;
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
    bodies_[body.index].sleeping = false;
    bodies_[body.index].sleep_timer = 0.0f;
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

std::uint32_t CustomPhysicsWorld::substep_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return substeps_;
}

void CustomPhysicsWorld::set_sleep_enabled(bool enabled) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    sleep_enabled_ = enabled;
    if (!enabled) {
        for (auto& body : bodies_) {
            body.sleeping = false;
            body.sleep_timer = 0.0f;
        }
    }
}

bool CustomPhysicsWorld::is_sleeping(BodyHandle body) const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return handle_live(body) && bodies_[body.index].sleeping;
}

Status CustomPhysicsWorld::wake(BodyHandle body) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(body)) {
        return Status::fail(ErrorCode::NotFound, "body handle is not live");
    }
    bodies_[body.index].sleeping = false;
    bodies_[body.index].sleep_timer = 0.0f;
    return Status::success();
}

std::uint32_t CustomPhysicsWorld::sleeping_body_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t count = 0;
    for (const auto& body : bodies_) {
        if (body.alive && body.sleeping) {
            ++count;
        }
    }
    return count;
}

} // namespace poko
