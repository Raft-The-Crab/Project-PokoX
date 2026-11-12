#include "poko/character/character.h"

#include <algorithm>
#include <cmath>

namespace poko::character {
namespace {

math::Vec3 flatten(const math::Vec3& v) {
    return {v.x, 0.0f, v.z};
}

} // namespace

void CharacterMotor::configure(const CharacterDesc& desc) noexcept {
    desc_ = valid(desc) ? desc : CharacterDesc{};
}

void CharacterMotor::teleport(const math::Vec3& feet_position) noexcept {
    state_.position = feet_position;
    state_.velocity = {};
    state_.grounded = false;
    state_.ground_normal = {0.0f, 1.0f, 0.0f};
}

void CharacterMotor::update(float delta_seconds, const MoveInput& input, GroundProbe probe,
                            void* probe_user) {
    if (delta_seconds <= 0.0f) {
        return;
    }

    // Horizontal velocity toward wish direction.
    const math::Vec3 wish = flatten(input.wish_direction);
    const float wish_len = math::length(wish);
    const math::Vec3 wish_dir = wish_len > 1e-5f ? wish / wish_len : math::Vec3{};
    const float speed = input.sprint ? desc_.sprint_speed : desc_.walk_speed;
    const math::Vec3 target_h = wish_dir * speed;
    // Smooth acceleration (snappy but not instant).
    const float accel = state_.grounded ? 40.0f : 12.0f;
    const math::Vec3 h_vel = state_.velocity;
    const math::Vec3 h_delta = target_h - flatten(h_vel);
    const float max_dv = accel * delta_seconds;
    const math::Vec3 h_step =
        math::length(h_delta) > max_dv ? h_delta * (max_dv / math::length(h_delta)) : h_delta;
    state_.velocity.x += h_step.x;
    state_.velocity.z += h_step.z;

    // Vertical: gravity + jump.
    state_.velocity.y += desc_.gravity * delta_seconds;
    if (input.jump && state_.grounded) {
        state_.velocity.y = desc_.jump_velocity;
        state_.grounded = false;
    }

    // Integrate.
    state_.position = state_.position + state_.velocity * delta_seconds;

    // Ground probe from feet downward.
    state_.grounded = false;
    if (probe != nullptr) {
        math::Vec3 hit_point, normal;
        const math::Vec3 probe_origin =
            state_.position + math::Vec3{0.0f, desc_.ground_probe, 0.0f};
        if (probe(probe_origin, desc_.ground_probe * 2.0f, hit_point, normal, probe_user)) {
            // Walkable if slope within limit and not moving up fast (rising
            // through the probe range must not count as grounded).
            if (normal.y >= std::cos(desc_.max_slope_radians) && state_.velocity.y <= 0.5f) {
                state_.grounded = true;
                state_.ground_normal = normal;
                // Snap to ground when falling onto it.
                if (state_.velocity.y <= 0.0f) {
                    state_.position.y = hit_point.y;
                    state_.velocity.y = 0.0f;
                }
            }
        }
    }
}

} // namespace poko::character
