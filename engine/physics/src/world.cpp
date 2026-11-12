#include "poko/physics/world.h"

#include <cmath>
#include <limits>

namespace poko {
namespace {

using math::Vec3;

// Ray vs sphere (center = body position, radius = shape.radius).
bool ray_sphere(const Ray& ray, const Vec3& center, float radius, float max_distance, float& t_out,
                Vec3& normal_out) {
    const Vec3 oc = ray.origin - center;
    const float b = math::dot(oc, ray.direction);
    const float c = math::dot(oc, oc) - radius * radius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f) {
        return false;
    }
    const float root = std::sqrt(discriminant);
    float t = -b - root;
    if (t < 0.0f) {
        t = -b + root; // origin inside the sphere
    }
    if (t < 0.0f || t > max_distance) {
        return false;
    }
    t_out = t;
    normal_out = math::normalize(ray.origin + ray.direction * t - center);
    return true;
}

// Ray vs axis-aligned box centered at origin with given half extents, in the
// ray's local frame (MOCK ignores body rotation — see header comment).
bool ray_box(const Ray& ray, const Vec3& half, float max_distance, float& t_out, Vec3& normal_out) {
    float t_min = 0.0f;
    float t_max = max_distance;
    int axis = -1;
    float sign = 0.0f;
    const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float dir[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float he[3] = {half.x, half.y, half.z};
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(dir[i]) < 1e-9f) {
            if (origin[i] < -he[i] || origin[i] > he[i]) {
                return false;
            }
            continue;
        }
        float t1 = (-he[i] - origin[i]) / dir[i];
        float t2 = (he[i] - origin[i]) / dir[i];
        float s = -1.0f;
        if (t1 > t2) {
            const float tmp = t1;
            t1 = t2;
            t2 = tmp;
            s = 1.0f;
        }
        if (t1 > t_min) {
            t_min = t1;
            axis = i;
            sign = s;
        }
        t_max = t_max < t2 ? t_max : t2;
        if (t_min > t_max) {
            return false;
        }
    }
    if (axis < 0 || t_min > max_distance) {
        return false;
    }
    t_out = t_min;
    normal_out = Vec3{};
    if (axis == 0) {
        normal_out.x = sign;
    } else if (axis == 1) {
        normal_out.y = sign;
    } else {
        normal_out.z = sign;
    }
    return true;
}

} // namespace

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

bool NullPhysicsWorld::raycast(const Ray& ray, float max_distance, std::uint32_t layer_mask,
                               RayHit& out) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_ || !valid(ray) || max_distance <= 0.0f) {
        return false;
    }
    const Vec3 dir = math::normalize(ray.direction);
    const Ray unit{ray.origin, dir};
    bool hit = false;
    float best_t = max_distance;
    RayHit best{};
    for (const auto& body : bodies_) {
        if (!body.alive || (body.collision_layer & layer_mask) == 0) {
            continue;
        }
        float t = 0.0f;
        Vec3 normal{};
        bool shape_hit = false;
        if (body.shape.kind == ShapeKind::Sphere) {
            shape_hit = ray_sphere(unit, body.state.position, body.shape.radius, best_t, t, normal);
        } else if (body.shape.kind == ShapeKind::Box) {
            // MOCK: box test ignores rotation; position is the box center.
            const Ray local{unit.origin - body.state.position, unit.direction};
            shape_hit = ray_box(local, body.shape.half_extents, best_t, t, normal);
        }
        // Capsule unsupported in MOCK raycast (documented limitation).
        if (shape_hit && t < best_t) {
            best_t = t;
            best.body = make_handle(static_cast<HandleIndex>(&body - bodies_.data()),
                                    generations_[static_cast<HandleIndex>(&body - bodies_.data())]);
            best.distance = t;
            best.point = unit.origin + unit.direction * t;
            best.normal = normal;
            hit = true;
        }
    }
    if (hit) {
        out = best;
    }
    return hit;
}

void NullPhysicsWorld::step(float delta_seconds) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_ || delta_seconds <= 0.0f) {
        return;
    }
    // MOCK integrator: semi-implicit Euler on dynamic bodies only. No
    // collisions, constraints or sleeping — a real backend replaces this.
    for (auto& body : bodies_) {
        if (!body.alive || body.type != BodyType::Dynamic) {
            continue;
        }
        body.state.velocity = body.state.velocity + desc_.gravity * delta_seconds;
        body.state.position = body.state.position + body.state.velocity * delta_seconds;
    }
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
