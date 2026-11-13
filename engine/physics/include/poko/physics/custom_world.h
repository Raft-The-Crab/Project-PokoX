#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

#include "poko/core/api_seams.h"

namespace poko {

namespace physics::detail {
struct JointRow;
}

using JointHandle = TypedHandle;

enum class JointType : std::uint8_t {
    Ball,   // shared anchor point, free rotation
    Fixed,  // weld: anchor and relative orientation locked
    Hinge,  // shared anchor, rotation only about `axis`
    Slider, // orientation locked, translation only along `axis`
};

// Anchor and axis are world-space at creation time; the joint stores them in
// each body's local frame. An invalid body_b anchors body_a to the world.
// Hinge limits are radians in [-pi, pi], slider limits are metres, both
// measured from the creation pose.
struct JointDesc {
    JointType type = JointType::Ball;
    BodyHandle body_a{};
    BodyHandle body_b{};
    math::Vec3 anchor{};
    math::Vec3 axis{0.0f, 1.0f, 0.0f};
    bool limit_enabled = false;
    float lower = 0.0f;
    float upper = 0.0f;
    bool collide_connected = false;
};

// PRODUCTION custom rigid-body world (systemplan/06, systemplan/29 "custom
// physics"). Deterministic fixed-substep simulation: semi-implicit Euler with
// angular integration, a static ground plane, impulse-based collision response
// with restitution/friction, layer/mask filtering, and analytic raycasts.
//
// Exact narrow phase for sphere, capsule and oriented box (SAT + face clipping
// for boxes), sweep-and-prune broadphase, sequential-impulse solver with
// friction, and island sleeping. Raycasts are exact against oriented shapes.
//
// Thread-safety contract: synchronised; any thread may call any method.
class CustomPhysicsWorld final : public api::IPhysicsWorld {
public:
    CustomPhysicsWorld() = default;

    bool initialize(const PhysicsWorldDesc& desc) noexcept override;
    Result<BodyHandle> create_body(const BodyDesc& desc) override;
    Status destroy_body(BodyHandle body) override;
    Status set_body_state(BodyHandle body, const BodyState& state) override;
    Result<BodyState> get_body_state(BodyHandle body) const override;
    bool raycast(const Ray& ray, float max_distance, std::uint32_t layer_mask,
                 RayHit& out) const override;
    void step(float delta_seconds) override;
    std::uint32_t body_count() const noexcept override;

    // Backend-specific controls and introspection, outside the seam.
    void set_ground(bool enabled, float height = 0.0f) noexcept;
    bool ground_enabled() const noexcept;
    std::uint32_t contact_count() const noexcept;
    std::uint32_t substep_count() const noexcept;

    // Joints. Bodies connected by a joint do not collide unless
    // `collide_connected` is set. Destroying a body destroys its joints.
    Result<JointHandle> create_joint(const JointDesc& desc);
    Status destroy_joint(JointHandle joint);
    // Drives a hinge (rad/s) or slider (m/s) toward `target_speed` with at most
    // `max_force` (N or N*m); max_force = 0 disables the motor.
    Status set_joint_motor(JointHandle joint, float target_speed, float max_force);
    // Hinge angle (rad) or slider translation (m) from the creation pose; 0 for
    // ball and fixed joints.
    Result<float> joint_position(JointHandle joint) const;
    std::uint32_t joint_count() const noexcept;

    // Sleeping: an island of dynamic bodies at rest for `sleep_time` seconds
    // stops simulating until touched by a moving body or modified.
    void set_sleep_enabled(bool enabled) noexcept;
    bool is_sleeping(BodyHandle body) const noexcept;
    Status wake(BodyHandle body);
    std::uint32_t sleeping_body_count() const noexcept;

private:
    struct Body {
        bool alive = false;
        BodyType type = BodyType::Dynamic;
        ShapeDesc shape{};
        BodyState state{};
        float mass = 1.0f;
        math::Vec3 inv_inertia{}; // diagonal of the inverse local inertia tensor
        float friction = 0.5f;
        float restitution = 0.0f;
        std::uint32_t collision_layer = 1;
        std::uint32_t collision_mask = 0xFFFFFFFFu;
        bool sleeping = false;
        float sleep_timer = 0.0f;
    };

    struct Joint {
        bool alive = false;
        JointType type = JointType::Ball;
        int a = -1;
        int b = -1; // -1: anchored to the world
        math::Vec3 local_a{};
        math::Vec3 local_b{};
        math::Vec3 axis_a{};
        math::Vec3 axis_b{};
        math::Vec3 ref_a{}; // reference perpendicular to the axis, for hinge angle
        math::Vec3 ref_b{};
        math::Quat rel0{};  // creation-time orientation of b relative to a
        bool limit_enabled = false;
        float lower = 0.0f;
        float upper = 0.0f;
        bool collide_connected = false;
        float motor_speed = 0.0f;
        float motor_max_force = 0.0f;
        float impulses[8] = {};
    };
    struct CachedContact {
        math::Vec3 point{};
        float normal_impulse = 0.0f;
        math::Vec3 friction_impulse{};
    };

    bool joint_live(JointHandle handle) const noexcept; // caller holds mutex_
    void build_joint_rows(float h, const std::vector<float>& inv_m,
                          std::vector<physics::detail::JointRow>& out);
    float joint_position_unlocked(const Joint& joint) const noexcept;
    bool handle_live(BodyHandle handle) const noexcept; // caller holds mutex_
    Body* find(BodyHandle handle) noexcept;             // caller holds mutex_
    const Body* find(BodyHandle handle) const noexcept; // caller holds mutex_
    void substep(float h);                              // caller holds mutex_

    mutable std::mutex mutex_;
    bool initialized_ = false;
    PhysicsWorldDesc desc_{};
    bool ground_enabled_ = true;
    float ground_height_ = 0.0f;
    bool sleep_enabled_ = true;
    float accumulator_ = 0.0f;
    std::uint32_t contacts_ = 0;
    std::uint32_t substeps_ = 0;
    std::vector<Body> bodies_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    std::vector<Joint> joints_;
    std::vector<Generation> joint_generations_;
    std::vector<HandleIndex> joint_free_;
    std::map<std::uint64_t, std::vector<CachedContact>> contact_cache_;
};

} // namespace poko
