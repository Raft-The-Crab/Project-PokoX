#include "poko/physics/world.h"

#include <cmath>
#include <limits>

namespace poko {

using math::Vec3;

bool NullPhysicsWorld::initialize(const PhysicsWorldDesc& desc) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    if (initialized_) {
        return true;
    }
    if (math::length_squared(desc.gravity) < 0.0f || desc.fixed_delta_seconds <= 0.0f) {
        return false;
    }
    desc_ = desc;
    initialized_ = true;
    return true;
}

bool NullPhysicsWorld::handle_live(BodyHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < bodies_.size() &&
           generations_[handle.index] == handle.generation && bodies_[handle.index].alive;
}

NullPhysicsWorld::Body* NullPhysicsWorld::find(BodyHandle handle) noexcept {
    if (static_cast<std::size_t>(handle.index) >= bodies_.size()) {
        return nullptr;
    }
    return &bodies_[handle.index];
}

const NullPhysicsWorld::Body* NullPhysicsWorld::find(BodyHandle handle) const noexcept {
    if (static_cast<std::size_t>(handle.index) >= bodies_.size()) {
        return nullptr;
    }
    return &bodies_[handle.index];
}

Result<BodyHandle> NullPhysicsWorld::create_body(const BodyDesc& desc) {
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
    auto& body = bodies_[index];
    body.alive = true;
    body.type = desc.type;
    body.shape = desc.shape;
    body.collision_layer = desc.collision_layer;
    body.collision_mask = desc.collision_mask;
    body.state = BodyState{desc.position, desc.rotation, desc.velocity, desc.angular_velocity};
    return Result<BodyHandle>::success(make_handle(index, generations_[index]));
}

Status NullPhysicsWorld::destroy_body(BodyHandle body) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(body)) {
        return Status::fail(ErrorCode::NotFound, "body handle is not live");
    }
    bodies_[body.index].alive = false;
    ++generations_[body.index];
    free_.push_back(body.index);
    return Status::success();
}

Status NullPhysicsWorld::set_body_state(BodyHandle body, const BodyState& state) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "physics world is not initialized");
    }
    if (!handle_live(body)) {
        return Status::fail(ErrorCode::NotFound, "body handle is not live");
    }
    bodies_[body.index].state = state;
    return Status::success();
}

Result<BodyState> NullPhysicsWorld::get_body_state(BodyHandle body) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(body)) {
        return Result<BodyState>::fail(ErrorCode::NotFound, "body handle is not live");
    }
    return Result<BodyState>::success(bodies_[body.index].state);
}

std::uint32_t NullPhysicsWorld::body_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t count = 0;
    for (const auto& body : bodies_) {
        if (body.alive) {
            ++count;
        }
    }
    return count;
}

} // namespace poko
