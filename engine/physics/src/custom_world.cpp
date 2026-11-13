#include "poko/physics/custom_world.h"

#include <cmath>
#include <cstddef>

#include "geometry.h"

namespace poko {

namespace {
bool finite3(const math::Vec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool finite_state(const BodyState& s) noexcept {
    return finite3(s.position) && finite3(s.velocity) && finite3(s.angular_velocity) &&
           std::isfinite(s.rotation.x) && std::isfinite(s.rotation.y) &&
           std::isfinite(s.rotation.z) && std::isfinite(s.rotation.w);
}

// Unit-length rotation; a zero quaternion becomes the identity.
BodyState sanitized(BodyState s) noexcept {
    s.rotation = math::normalize(s.rotation);
    return s;
}


// Diagonal of the inverse local inertia tensor for a solid shape of `mass`.
math::Vec3 inverse_inertia(const ShapeDesc& shape, float mass,
                          const physics::detail::Geometry* geometry) noexcept {
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
    case ShapeKind::ConvexHull:
        if (geometry != nullptr) {
            ix = mass * geometry->inertia_per_mass.x;
            iy = mass * geometry->inertia_per_mass.y;
            iz = mass * geometry->inertia_per_mass.z;
        }
        break;
    case ShapeKind::TriangleMesh: // static only: no inertia
        break;
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
    // 0.1 ms .. 100 ms: outside this the solver is meaningless or unstable.
    if (!std::isfinite(desc.fixed_delta_seconds) || desc.fixed_delta_seconds < 1.0e-4f ||
        desc.fixed_delta_seconds > 0.1f) {
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
    const BodyState initial{desc.position, desc.rotation, desc.velocity, desc.angular_velocity};
    if (!finite_state(initial) || !std::isfinite(desc.mass)) {
        return Result<BodyHandle>::fail(ErrorCode::InvalidArgument, "non-finite body state");
    }
    const physics::detail::Geometry* geometry = nullptr;
    if (desc.shape.kind == ShapeKind::ConvexHull || desc.shape.kind == ShapeKind::TriangleMesh) {
        const auto g = desc.shape.geometry;
        const bool live = static_cast<std::size_t>(g.index) < geometries_.size() &&
                          geometry_generations_[g.index] == g.generation && geometries_[g.index] != nullptr;
        const bool wants_hull = desc.shape.kind == ShapeKind::ConvexHull;
        if (!live || (geometries_[g.index]->kind == physics::detail::Geometry::Kind::Hull) != wants_hull) {
            return Result<BodyHandle>::fail(ErrorCode::InvalidArgument, "shape geometry is missing or of the wrong kind");
        }
        if (!wants_hull && desc.type != BodyType::Static) {
            return Result<BodyHandle>::fail(ErrorCode::InvalidArgument, "triangle meshes are static only");
        }
        geometry = geometries_[g.index].get();
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
    body.state = sanitized(initial);
    body.last_good = body.state;
    body.mass = desc.type == BodyType::Static ? 0.0f : desc.mass;
    body.geometry = geometry;
    if (geometry != nullptr) ++geometries_[desc.shape.geometry.index]->users;
    body.inv_inertia = desc.type == BodyType::Dynamic ? inverse_inertia(desc.shape, desc.mass, geometry) : math::Vec3{};
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
    for (std::size_t i = 0; i < joints_.size(); ++i) {
        Joint& joint = joints_[i];
        if (!joint.alive || (joint.a != static_cast<int>(body.index) && joint.b != static_cast<int>(body.index))) {
            continue;
        }
        for (const int other : {joint.a, joint.b}) {
            if (other >= 0 && other != static_cast<int>(body.index)) {
                bodies_[static_cast<std::size_t>(other)].sleeping = false;
                bodies_[static_cast<std::size_t>(other)].sleep_timer = 0.0f;
            }
        }
        joint.alive = false;
        ++joint_generations_[i];
        joint_free_.push_back(static_cast<HandleIndex>(i));
    }
    // Warm-start entries naming this slot would otherwise match its next occupant.
    const auto slot = static_cast<std::uint64_t>(body.index) + 1;
    for (auto it = contact_cache_.begin(); it != contact_cache_.end();) {
        if ((it->first >> 32) == slot || (it->first & 0xFFFFFFFFull) == slot) {
            it = contact_cache_.erase(it);
        } else {
            ++it;
        }
    }
    if (bodies_[body.index].geometry != nullptr) {
        const auto* geo = bodies_[body.index].geometry;
        for (auto& owned : geometries_) {
            if (owned.get() == geo) --owned->users;
        }
        bodies_[body.index].geometry = nullptr;
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
    if (!finite_state(state)) {
        return Status::fail(ErrorCode::InvalidArgument, "non-finite body state");
    }
    bodies_[body.index].state = sanitized(state);
    bodies_[body.index].last_good = bodies_[body.index].state;
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

void CustomPhysicsWorld::set_ccd_enabled(bool enabled) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    ccd_enabled_ = enabled;
}

std::uint32_t CustomPhysicsWorld::nonfinite_recoveries() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return nonfinite_recoveries_;
}

std::uint32_t CustomPhysicsWorld::ccd_hit_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return ccd_hits_;
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
