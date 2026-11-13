#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "poko/core/api_seams.h"

namespace poko {

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
};

} // namespace poko
