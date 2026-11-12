#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "poko_api_seams.h"

namespace poko {

// MOCK — test-only fake physics world (systemplan/06 skeleton). It stores
// body state, validates descriptors, applies trivial gravity integration to
// dynamic bodies in step(), and answers raycasts analytically against
// spheres and axis-aligned boxes. It performs NO collision detection,
// response, broadphase, or sleeping — those belong to the real backend.
// Raycasts ignore body rotation (documented MOCK limitation).
class NullPhysicsWorld final : public api::IPhysicsWorld {
public:
    NullPhysicsWorld() = default;

    bool initialize(const PhysicsWorldDesc& desc) noexcept override;

    Result<BodyHandle> create_body(const BodyDesc& desc) override;
    Status destroy_body(BodyHandle body) override;
    Status set_body_state(BodyHandle body, const BodyState& state) override;
    Result<BodyState> get_body_state(BodyHandle body) const override;
    bool raycast(const Ray& ray, float max_distance, std::uint32_t layer_mask,
                 RayHit& out) const override;
    void step(float delta_seconds) override;
    std::uint32_t body_count() const noexcept override;

private:
    struct Body {
        bool alive = false;
        BodyType type = BodyType::Dynamic;
        ShapeDesc shape;
        std::uint32_t collision_layer = 1;
        std::uint32_t collision_mask = 0xFFFFFFFFu;
        BodyState state;
    };

    bool handle_live(BodyHandle handle) const noexcept; // caller holds mutex_
    Body* find(BodyHandle handle) noexcept;
    const Body* find(BodyHandle handle) const noexcept;

    mutable std::mutex mutex_;
    bool initialized_ = false;
    PhysicsWorldDesc desc_{};
    std::vector<Body> bodies_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
};

} // namespace poko
