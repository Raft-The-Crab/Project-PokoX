#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "poko/core/api_seams.h"

namespace poko {

namespace physics::detail {
struct ContactConstraint;
}

// PRODUCTION custom rigid-body world (systemplan/06, systemplan/29 "custom
// physics"). Deterministic fixed-substep simulation:
//   * shapes: sphere, oriented box, capsule (local Y axis), with rotation;
//   * broadphase: sweep-and-prune on the X axis over world AABBs;
//   * narrow phase: SAT box-box manifolds, segment-based capsules, plus a
//     static ground plane;
//   * solver: sequential impulses (normal + two friction axes, accumulated
//     and clamped) using a per-shape inertia tensor, then a short positional
//     projection pass against residual penetration;
//   * sleeping: bodies at rest for 0.5 s stop simulating until touched by a
//     moving body, moved with set_body_state, or a body is destroyed;
//   * Kinematic bodies integrate their velocity, are never pushed, and push
//     dynamic bodies.
// Capsule inertia is approximated by the capsule's bounding box. Raycasts are
// exact against rotated boxes, capsules and spheres.
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
        bool sleeping = false;
        float sleep_time = 0.0f;
    };

    bool handle_live(BodyHandle handle) const noexcept; // caller holds mutex_
    Body* find(BodyHandle handle) noexcept;             // caller holds mutex_
    const Body* find(BodyHandle handle) const noexcept; // caller holds mutex_
    void wake(Body& body) noexcept;                     // caller holds mutex_
    bool moving(const Body& body) const noexcept;       // caller holds mutex_
    void substep(float h);                              // caller holds mutex_
    void build_contacts(std::vector<physics::detail::ContactConstraint>& out);
    void solve_contacts(std::vector<physics::detail::ContactConstraint>& contacts);
    void project_positions();
    void update_sleep(float h);

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
};

} // namespace poko
