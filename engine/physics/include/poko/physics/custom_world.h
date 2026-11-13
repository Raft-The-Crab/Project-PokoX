#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <set>
#include <vector>

#include "poko/core/api_seams.h"

namespace poko {

namespace physics::detail {
struct ContactConstraint;
struct Collider;
} // namespace physics::detail

// Result of CustomPhysicsWorld::shape_cast.
struct SweepHit {
    BodyHandle body; // invalid when the hit is the ground plane
    bool ground = false;
    float distance = 0.0f; // travelled before first contact (0 = already touching)
    math::Vec3 point;      // world-space contact point
    math::Vec3 normal;     // surface normal facing the cast shape
};

// Reported by CustomPhysicsWorld::drain_trigger_events. Handles of destroyed
// bodies are stale (generation mismatch) but still identify the pair.
struct TriggerEvent {
    BodyHandle trigger;
    BodyHandle other;
    bool entered = true; // false = left
};

// PRODUCTION custom rigid-body world (systemplan/06, systemplan/29 "custom
// physics"). Deterministic fixed-substep simulation:
//   * shapes: sphere, oriented box, capsule (local Y axis), with rotation;
//   * broadphase: sweep-and-prune on the X axis over world AABBs;
//   * narrow phase: SAT box-box manifolds, segment-based capsules, plus a
//     static ground plane; contacts are speculative within a 4 cm margin;
//   * solver: warm-started sequential impulses (normal + two friction axes,
//     accumulated and clamped) with a per-shape inertia tensor, joints solved
//     in the same loop, then a short positional projection pass;
//   * continuous collision: fast dynamic bodies are swept (conservative
//     advancement) against the world so they cannot tunnel;
//   * joints: ball, distance, hinge and fixed, between two bodies or a body
//     and the world;
//   * triggers: non-colliding overlap volumes reported as enter/leave events;
//   * queries: raycast, shape_cast (sweep) and overlap;
//   * sleeping: bodies at rest for 0.5 s stop simulating until touched by a
//     moving body, moved with set_body_state, or a body is destroyed. Bodies
//     attached to a joint never sleep;
//   * Kinematic bodies integrate their velocity, are never pushed, and push
//     dynamic bodies.
// Sweeps and CCD treat motion as linear (rotation during the sweep is ignored).
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
    std::uint32_t sleeping_count() const noexcept;
    bool is_sleeping(BodyHandle body) const noexcept;

    // Joints. Destroying either body destroys the joint.
    Result<JointHandle> create_joint(const JointDesc& desc);
    Status destroy_joint(JointHandle joint);
    std::uint32_t joint_count() const noexcept;

    // Sweeps `shape` (placed at position/rotation) along `direction` for up to
    // max_distance and reports the first body (or the ground plane) it touches.
    // Triggers are ignored. An already-overlapping shape reports distance 0.
    bool shape_cast(const ShapeDesc& shape, const math::Vec3& position, const math::Quat& rotation,
                    const math::Vec3& direction, float max_distance, std::uint32_t layer_mask,
                    SweepHit& out) const;

    // Writes up to `capacity` overlapping bodies (triggers and ground ignored)
    // and returns the total number overlapping, which may exceed capacity.
    std::uint32_t overlap(const ShapeDesc& shape, const math::Vec3& position,
                          const math::Quat& rotation, std::uint32_t layer_mask, BodyHandle* out,
                          std::uint32_t capacity) const;

    // Moves pending trigger enter/leave events into `out` (appending).
    void drain_trigger_events(std::vector<TriggerEvent>& out);

private:
    struct Body {
        bool alive = false;
        BodyType type = BodyType::Dynamic;
        ShapeDesc shape{};
        BodyState state{};
        float mass = 1.0f;
        float friction = 0.5f;
        float restitution = 0.0f;
        std::uint32_t collision_layer = 1;
        std::uint32_t collision_mask = 0xFFFFFFFFu;
        float inv_mass = 0.0f;
        math::Vec3 inv_inertia{}; // diagonal of the inverse local inertia tensor
        bool trigger = false;
        bool jointed = false;
        bool sleeping = false;
        float sleep_time = 0.0f;
        bool in_motion = false; // above the sleep thresholds after the previous substep
    };

    struct Joint {
        bool alive = false;
        JointDesc desc{};
        math::Quat reference{}; // Fixed: rotation of b relative to a at creation
        float rest_length = 0.0f;
    };

    // Impulses remembered from the previous substep to warm-start the solver.
    struct CachedImpulse {
        int a = -1;
        int b = -1;
        math::Vec3 point;
        float jn = 0.0f;
        float jt1 = 0.0f;
        float jt2 = 0.0f;
    };

    bool handle_live(BodyHandle handle) const noexcept; // caller holds mutex_
    Body* find(BodyHandle handle) noexcept;             // caller holds mutex_
    const Body* find(BodyHandle handle) const noexcept; // caller holds mutex_
    void wake(Body& body) noexcept;                     // caller holds mutex_
    bool moving(const Body& body) const noexcept;       // caller holds mutex_
    bool joint_live(JointHandle handle) const noexcept; // caller holds mutex_
    void forget_cached(int body_index);                 // caller holds mutex_
    void substep(float h);                              // caller holds mutex_
    void build_contacts(std::vector<physics::detail::ContactConstraint>& out, float h,
                        bool warm_start);
    void solve_contacts(std::vector<physics::detail::ContactConstraint>& contacts, float h);
    void solve_joints(float h);
    void project_positions(float h);
    void update_sleep(float h);
    void update_triggers();
    bool cast_locked(const physics::detail::Collider& caster, std::uint32_t caster_layer,
                     std::uint32_t caster_mask, bool use_caster_mask, std::uint32_t layer_mask,
                     const math::Vec3& direction, float max_distance, int skip_body,
                     SweepHit& out) const;

    mutable std::mutex mutex_;
    bool initialized_ = false;
    PhysicsWorldDesc desc_{};
    bool ground_enabled_ = true;
    float ground_height_ = 0.0f;
    float accumulator_ = 0.0f;
    std::uint32_t contacts_ = 0;
    std::uint32_t substeps_ = 0;
    std::vector<Body> bodies_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    std::vector<Joint> joints_;
    std::vector<Generation> joint_generations_;
    std::vector<HandleIndex> joint_free_;
    std::vector<CachedImpulse> cache_;
    std::set<std::array<std::uint32_t, 4>> overlaps_; // trigger idx/gen, other idx/gen
    std::vector<TriggerEvent> trigger_events_;
};

} // namespace poko
