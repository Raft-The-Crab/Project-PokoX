#pragma once

#include <cmath>

#include "poko/core/math.h"

namespace poko::physics::detail {

// Applies a diagonal local inverse inertia tensor rotated into world space.
inline math::Vec3 apply_inertia(const math::Quat& q, const math::Vec3& inv_diag,
                                const math::Vec3& v) noexcept {
    const math::Vec3 local = math::rotate(math::conjugate(q), v);
    return math::rotate(q, {local.x * inv_diag.x, local.y * inv_diag.y, local.z * inv_diag.z});
}

// Row-major 3x3 matrix for the joint solver.
struct Mat3 {
    float m[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
};

inline Mat3 identity3() noexcept {
    Mat3 r;
    r.m[0] = r.m[4] = r.m[8] = 1.0f;
    return r;
}

inline Mat3 operator+(const Mat3& a, const Mat3& b) noexcept {
    Mat3 r;
    for (int i = 0; i < 9; ++i) {
        r.m[i] = a.m[i] + b.m[i];
    }
    return r;
}

inline Mat3 operator-(const Mat3& a, const Mat3& b) noexcept {
    Mat3 r;
    for (int i = 0; i < 9; ++i) {
        r.m[i] = a.m[i] - b.m[i];
    }
    return r;
}

inline Mat3 operator*(const Mat3& a, float s) noexcept {
    Mat3 r;
    for (int i = 0; i < 9; ++i) {
        r.m[i] = a.m[i] * s;
    }
    return r;
}

inline Mat3 operator*(const Mat3& a, const Mat3& b) noexcept {
    Mat3 r;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 3; ++k) {
                sum += a.m[i * 3 + k] * b.m[k * 3 + j];
            }
            r.m[i * 3 + j] = sum;
        }
    }
    return r;
}

inline math::Vec3 operator*(const Mat3& a, const math::Vec3& v) noexcept {
    return {a.m[0] * v.x + a.m[1] * v.y + a.m[2] * v.z, a.m[3] * v.x + a.m[4] * v.y + a.m[5] * v.z,
            a.m[6] * v.x + a.m[7] * v.y + a.m[8] * v.z};
}

// Cross-product matrix: skew(a) * b == cross(a, b).
inline Mat3 skew(const math::Vec3& a) noexcept {
    Mat3 r;
    r.m[1] = -a.z;
    r.m[2] = a.y;
    r.m[3] = a.z;
    r.m[5] = -a.x;
    r.m[6] = -a.y;
    r.m[7] = a.x;
    return r;
}

// World-space inverse inertia matrix: R * diag(inv_diag) * R^T.
inline Mat3 world_inverse_inertia(const math::Quat& q, const math::Vec3& inv_diag) noexcept {
    const math::Vec3 cx = math::rotate(q, {1.0f, 0.0f, 0.0f});
    const math::Vec3 cy = math::rotate(q, {0.0f, 1.0f, 0.0f});
    const math::Vec3 cz = math::rotate(q, {0.0f, 0.0f, 1.0f});
    const float r[3][3] = {{cx.x, cy.x, cz.x}, {cx.y, cy.y, cz.y}, {cx.z, cy.z, cz.z}};
    const float d[3] = {inv_diag.x, inv_diag.y, inv_diag.z};
    Mat3 out;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out.m[i * 3 + j] =
                r[i][0] * d[0] * r[j][0] + r[i][1] * d[1] * r[j][1] + r[i][2] * d[2] * r[j][2];
        }
    }
    return out;
}

// Inverse via the adjugate; returns zero matrix when singular.
inline Mat3 inverse(const Mat3& a) noexcept {
    const float* m = a.m;
    const float c00 = m[4] * m[8] - m[5] * m[7];
    const float c01 = m[5] * m[6] - m[3] * m[8];
    const float c02 = m[3] * m[7] - m[4] * m[6];
    const float det = m[0] * c00 + m[1] * c01 + m[2] * c02;
    Mat3 out;
    if (std::abs(det) < 1.0e-12f) {
        return out;
    }
    const float inv = 1.0f / det;
    out.m[0] = c00 * inv;
    out.m[1] = (m[2] * m[7] - m[1] * m[8]) * inv;
    out.m[2] = (m[1] * m[5] - m[2] * m[4]) * inv;
    out.m[3] = c01 * inv;
    out.m[4] = (m[0] * m[8] - m[2] * m[6]) * inv;
    out.m[5] = (m[2] * m[3] - m[0] * m[5]) * inv;
    out.m[6] = c02 * inv;
    out.m[7] = (m[1] * m[6] - m[0] * m[7]) * inv;
    out.m[8] = (m[0] * m[4] - m[1] * m[3]) * inv;
    return out;
}

} // namespace poko::physics::detail
