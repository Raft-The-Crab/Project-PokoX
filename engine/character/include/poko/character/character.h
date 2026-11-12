#pragma once

#include <cstdint>

#include "poko/core/math.h"
#include "poko/core/result.h"

namespace poko::character {

// Character controller skeleton (Phase 1, systemplan/06/07): kinematic motor
// over a physics world. The motor owns desired velocity + grounded state;
// collision resolution delegates to IPhysicsWorld raycasts/shapecasts (real
// backend) — the skeleton ships a probe-based floor/step model.

struct CharacterDesc {
    float radius = 0.4f;     // capsule radius, metres
    float height = 1.8f;     // capsule total height incl. radius caps
    float walk_speed = 4.0f; // m/s
    float sprint_speed = 7.0f;
    float jump_velocity = 5.0f;     // m/s upward impulse
    float gravity = -9.81f;         // m/s^2 (world gravity applied here)
    float ground_probe = 0.1f;      // metres below feet for grounded check
    float max_slope_radians = 0.9f; // ~51 deg: steeper = slide/blocked
};

// Per-frame input intent, in world space.
struct MoveInput {
    math::Vec3 wish_direction; // unit XZ direction; y ignored
    bool sprint = false;
    bool jump = false;
};

struct CharacterState {
    math::Vec3 position; // feet position (capsule bottom center)
    math::Vec3 velocity; // m/s
    bool grounded = false;
    math::Vec3 ground_normal{0.0f, 1.0f, 0.0f};
};

// Ground probe callback: returns true + fills hit when a downward ray from
// `feet + up*probe` hits within probe distance. Supplied by the physics layer.
using GroundProbe = bool (*)(const math::Vec3& origin, float distance, math::Vec3& hit_point,
                             math::Vec3& normal, void* user);

class CharacterMotor {
public:
    CharacterMotor() = default;

    void configure(const CharacterDesc& desc) noexcept;
    const CharacterDesc& desc() const noexcept {
        return desc_;
    }

    void teleport(const math::Vec3& feet_position) noexcept;
    const CharacterState& state() const noexcept {
        return state_;
    }

    // Advances the motor by dt. `probe` may be nullptr (then grounded stays
    // false and gravity applies unbounded — useful for tests/fly).
    void update(float delta_seconds, const MoveInput& input, GroundProbe probe, void* probe_user);

private:
    CharacterDesc desc_{};
    CharacterState state_{};
};

inline bool valid(const CharacterDesc& desc) noexcept {
    return desc.radius > 0.0f && desc.height > 2.0f * desc.radius && desc.walk_speed > 0.0f &&
           desc.sprint_speed >= desc.walk_speed && desc.jump_velocity >= 0.0f &&
           desc.gravity < 0.0f && desc.ground_probe > 0.0f;
}

} // namespace poko::character
