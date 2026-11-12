#include "poko/character/character.h"

namespace poko::character {

void CharacterMotor::configure(const CharacterDesc& desc) noexcept {
    desc_ = valid(desc) ? desc : CharacterDesc{};
}

void CharacterMotor::teleport(const math::Vec3& feet_position) noexcept {
    state_.position = feet_position;
    state_.velocity = {};
    state_.grounded = false;
    state_.ground_normal = {0.0f, 1.0f, 0.0f};
}

} // namespace poko::character
