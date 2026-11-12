#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace poko::math {

// ---------------------------------------------------------------------------
// Coordinate system and units (systemplan/06 requires these to be documented):
//   * Right-handed, +Y up, -Z forward.
//   * Distances in metres, angles in radians, time in seconds.
//   * Matrices are row-major; vectors are column vectors (M * v).
//   * IEEE-754 single precision; no fast-math. Comparisons use an explicit
//     epsilon. Fixed-timestep simulation lives in poko_clock.h.
// GLM is intentionally NOT used yet; it is unpinned in
// third_party/DEPENDENCIES.md. This is a small deterministic foundation.
// ---------------------------------------------------------------------------

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kHalfPi = kPi / 2.0f;
inline constexpr float kTau = kPi * 2.0f;
inline constexpr float kEpsilon = 1.0e-5f;
inline constexpr float kDegToRad = kPi / 180.0f;
inline constexpr float kRadToDeg = 180.0f / kPi;

constexpr float to_radians(float degrees) noexcept {
    return degrees * kDegToRad;
}
constexpr float to_degrees(float radians) noexcept {
    return radians * kRadToDeg;
}

constexpr bool nearly_equal(float a, float b, float epsilon = kEpsilon) noexcept {
    const float difference = a > b ? a - b : b - a;
    return difference <= epsilon;
}

constexpr float clamp(float value, float low, float high) noexcept {
    return value < low ? low : (value > high ? high : value);
}

constexpr float lerp(float a, float b, float t) noexcept {
    return a + (b - a) * t;
}

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_value, float y_value) : x(x_value), y(y_value) {}
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float x_value, float y_value, float z_value)
        : x(x_value), y(y_value), z(z_value) {}
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    constexpr Vec4() = default;
    constexpr Vec4(float x_value, float y_value, float z_value, float w_value)
        : x(x_value), y(y_value), z(z_value), w(w_value) {}
};

constexpr Vec3 operator+(const Vec3& a, const Vec3& b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
constexpr Vec3 operator-(const Vec3& a, const Vec3& b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
constexpr Vec3 operator*(const Vec3& v, float scalar) noexcept {
    return {v.x * scalar, v.y * scalar, v.z * scalar};
}
constexpr Vec3 operator*(float scalar, const Vec3& v) noexcept {
    return v * scalar;
}
constexpr Vec3 operator/(const Vec3& v, float scalar) noexcept {
    return {v.x / scalar, v.y / scalar, v.z / scalar};
}
constexpr Vec3 operator-(const Vec3& v) noexcept {
    return {-v.x, -v.y, -v.z};
}

constexpr float dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
constexpr Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& v) noexcept {
    return std::sqrt(dot(v, v));
}
constexpr float length_squared(const Vec3& v) noexcept {
    return dot(v, v);
}
inline float distance(const Vec3& a, const Vec3& b) noexcept {
    return length(a - b);
}
inline Vec3 normalize(const Vec3& v) noexcept {
    const float len = length(v);
    return len > 0.0f ? v / len : Vec3{};
}
constexpr Vec3 lerp(const Vec3& a, const Vec3& b, float t) noexcept {
    return {lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t)};
}

struct Mat4 {
    std::array<float, 16> m{};

    constexpr float& at(std::size_t row, std::size_t column) noexcept {
        return m[row * 4 + column];
    }
    constexpr float at(std::size_t row, std::size_t column) const noexcept {
        return m[row * 4 + column];
    }

    static Mat4 identity() noexcept {
        Mat4 out;
        out.at(0, 0) = 1.0f;
        out.at(1, 1) = 1.0f;
        out.at(2, 2) = 1.0f;
        out.at(3, 3) = 1.0f;
        return out;
    }

    static Mat4 translation(const Vec3& v) noexcept {
        Mat4 out = identity();
        out.at(0, 3) = v.x;
        out.at(1, 3) = v.y;
        out.at(2, 3) = v.z;
        return out;
    }

    static Mat4 scaling(const Vec3& v) noexcept {
        Mat4 out = identity();
        out.at(0, 0) = v.x;
        out.at(1, 1) = v.y;
        out.at(2, 2) = v.z;
        return out;
    }

    static Mat4 rotation_x(float radians) noexcept {
        Mat4 out = identity();
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        out.at(1, 1) = c;
        out.at(1, 2) = -s;
        out.at(2, 1) = s;
        out.at(2, 2) = c;
        return out;
    }

    static Mat4 rotation_y(float radians) noexcept {
        Mat4 out = identity();
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        out.at(0, 0) = c;
        out.at(0, 2) = s;
        out.at(2, 0) = -s;
        out.at(2, 2) = c;
        return out;
    }

    static Mat4 rotation_z(float radians) noexcept {
        Mat4 out = identity();
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        out.at(0, 0) = c;
        out.at(0, 1) = -s;
        out.at(1, 0) = s;
        out.at(1, 1) = c;
        return out;
    }
};

inline Mat4 multiply(const Mat4& a, const Mat4& b) noexcept {
    Mat4 out;
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            float sum = 0.0f;
            for (std::size_t k = 0; k < 4; ++k) {
                sum += a.at(row, k) * b.at(k, column);
            }
            out.at(row, column) = sum;
        }
    }
    return out;
}

// w = 1 for points (translation applies), w = 0 for directions.
inline Vec3 transform(const Mat4& matrix, const Vec3& v, float w) noexcept {
    return {
        matrix.at(0, 0) * v.x + matrix.at(0, 1) * v.y + matrix.at(0, 2) * v.z + matrix.at(0, 3) * w,
        matrix.at(1, 0) * v.x + matrix.at(1, 1) * v.y + matrix.at(1, 2) * v.z + matrix.at(1, 3) * w,
        matrix.at(2, 0) * v.x + matrix.at(2, 1) * v.y + matrix.at(2, 2) * v.z + matrix.at(2, 3) * w,
    };
}
inline Vec3 transform_point(const Mat4& matrix, const Vec3& v) noexcept {
    return transform(matrix, v, 1.0f);
}
inline Vec3 transform_direction(const Mat4& matrix, const Vec3& v) noexcept {
    return transform(matrix, v, 0.0f);
}

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

inline Quat quat_identity() noexcept {
    return Quat{};
}

inline Quat normalize(const Quat& q) noexcept {
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return len > 0.0f ? Quat{q.x / len, q.y / len, q.z / len, q.w / len} : quat_identity();
}

inline Quat conjugate(const Quat& q) noexcept {
    return {-q.x, -q.y, -q.z, q.w};
}

inline Quat quat_from_axis_angle(const Vec3& axis, float radians) noexcept {
    const Vec3 unit = normalize(axis);
    const float half = radians * 0.5f;
    const float s = std::sin(half);
    return normalize(Quat{unit.x * s, unit.y * s, unit.z * s, std::cos(half)});
}

inline Quat multiply(const Quat& a, const Quat& b) noexcept {
    return normalize(Quat{
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    });
}

inline Vec3 rotate(const Quat& rotation, const Vec3& v) noexcept {
    // q*v*q^-1 = (s^2 - |u|^2)v + 2(u.v)u + 2s(u x v); valid for unit q.
    const Quat q = normalize(rotation);
    const Vec3 u{q.x, q.y, q.z};
    const float s = q.w;
    return v * (2.0f * s * s - 1.0f) + u * (2.0f * dot(u, v)) + cross(u, v) * (2.0f * s);
}

} // namespace poko::math
