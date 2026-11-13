#pragma once

#include "poko/core/math.h"

namespace poko::physics::detail {

// One scalar velocity constraint between bodies a and b (b < 0: the world).
// Row velocity: dot(lin, vb - va) + dot(ang_b, wb) - dot(ang_a, wa). The solver
// drives it toward `bias`, accumulating an impulse clamped to [lo, hi].
struct JointRow {
    int joint = 0;
    int slot = 0;
    int a = -1;
    int b = -1;
    math::Vec3 lin{};
    math::Vec3 ang_a{};
    math::Vec3 ang_b{};
    float bias = 0.0f;
    float lo = -1.0e30f;
    float hi = 1.0e30f;
    float mass = 0.0f;
    float impulse = 0.0f;
};

} // namespace poko::physics::detail
