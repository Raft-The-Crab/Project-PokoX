#include "poko_test_harness.h"

#include "poko_character.h"

namespace {

// Flat ground at y=0 probe.
bool flat_ground(const poko::math::Vec3& origin, float distance, poko::math::Vec3& hit_point,
                 poko::math::Vec3& normal, void*) {
    if (origin.y - distance <= 0.0f) {
        hit_point = {origin.x, 0.0f, origin.z};
        normal = {0.0f, 1.0f, 0.0f};
        return true;
    }
    return false;
}

} // namespace

POKO_TEST(desc_validation) {
    auto desc = poko::character::CharacterDesc{};
    POKO_CHECK(poko::character::valid(desc));
    desc.walk_speed = 0.0f;
    POKO_CHECK(!poko::character::valid(desc));
}

POKO_TEST(motor_moves_and_stays_grounded) {
    poko::character::CharacterMotor motor;
    motor.configure(poko::character::CharacterDesc{});
    motor.teleport({0.0f, 0.05f, 0.0f});

    poko::character::MoveInput input;
    input.wish_direction = {0.0f, 0.0f, -1.0f}; // forward -Z
    for (int i = 0; i < 120; ++i) {
        motor.update(1.0f / 60.0f, input, flat_ground, nullptr);
    }
    const auto& s = motor.state();
    POKO_CHECK(s.grounded);
    POKO_CHECK(s.position.z < -1.0f); // moved forward
    POKO_CHECK(s.position.y <= 0.2f); // stayed near ground
}

POKO_TEST(jumping_leaves_ground) {
    poko::character::CharacterMotor motor;
    motor.configure(poko::character::CharacterDesc{});
    motor.teleport({0.0f, 0.05f, 0.0f});
    // settle on ground
    for (int i = 0; i < 30; ++i) {
        motor.update(1.0f / 60.0f, {}, flat_ground, nullptr);
    }
    POKO_CHECK(motor.state().grounded);

    poko::character::MoveInput jump;
    jump.jump = true;
    motor.update(1.0f / 60.0f, jump, flat_ground, nullptr);
    POKO_CHECK(!motor.state().grounded);
    POKO_CHECK(motor.state().velocity.y > 0.0f);
}

POKO_TEST(no_probe_falls_forever) {
    poko::character::CharacterMotor motor;
    motor.configure(poko::character::CharacterDesc{});
    motor.teleport({0.0f, 10.0f, 0.0f});
    motor.update(1.0f, {}, nullptr, nullptr);
    POKO_CHECK(!motor.state().grounded);
    POKO_CHECK(motor.state().velocity.y < 0.0f);
    POKO_CHECK(motor.state().position.y < 10.0f);
}

POKO_TEST_MAIN()
