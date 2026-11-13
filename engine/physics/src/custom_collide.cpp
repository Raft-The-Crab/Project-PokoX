#include "custom_collide.h"

#include <algorithm>
#include <cmath>

namespace poko::physics::detail {
namespace {

using math::Vec3;

constexpr float kEps = 1.0e-6f;

Vec3 local_axis(const Collider& c, int i) noexcept {
    static const Vec3 axes[3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    return math::rotate(c.rotation, axes[i]);
}

float comp(const Vec3& v, int i) noexcept {
    return i == 0 ? v.x : (i == 1 ? v.y : v.z);
}

struct Segment {
    Vec3 p0;
    Vec3 p1;
};

Segment segment_of(const Collider& c) noexcept {
    const Vec3 axis = local_axis(c, 1) * c.half_height;
    return {c.position - axis, c.position + axis};
}

// Closest points between segments p1-q1 and p2-q2 (Ericson, Real-Time
// Collision Detection 5.1.9).
void closest_segments(const Vec3& p1, const Vec3& q1, const Vec3& p2, const Vec3& q2, Vec3& c1,
                      Vec3& c2) noexcept {
    const Vec3 d1 = q1 - p1;
    const Vec3 d2 = q2 - p2;
    const Vec3 r = p1 - p2;
    const float a = math::dot(d1, d1);
    const float e = math::dot(d2, d2);
    const float f = math::dot(d2, r);
    float s = 0.0f;
    float t = 0.0f;
    if (a <= kEps && e <= kEps) {
        s = 0.0f;
        t = 0.0f;
    } else if (a <= kEps) {
        t = math::clamp(f / e, 0.0f, 1.0f);
    } else {
        const float c = math::dot(d1, r);
        if (e <= kEps) {
            s = math::clamp(-c / a, 0.0f, 1.0f);
        } else {
            const float b = math::dot(d1, d2);
            const float denom = a * e - b * b;
            s = denom > kEps ? math::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = math::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = math::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
}

int capsule_capsule(const Collider& a, const Collider& b, Contact* out) noexcept {
    const Segment sa = segment_of(a);
    const Segment sb = segment_of(b);
    Vec3 ca;
    Vec3 cb;
    closest_segments(sa.p0, sa.p1, sb.p0, sb.p1, ca, cb);
    const Vec3 delta = cb - ca;
    const float dist = math::length(delta);
    const float sum = a.radius + b.radius;
    if (dist >= sum) {
        return 0;
    }
    const Vec3 normal = dist > 1.0e-5f ? delta / dist : Vec3{0.0f, 1.0f, 0.0f};
    const float depth = sum - dist;
    out[0] = {ca + normal * (a.radius - depth * 0.5f), normal, depth};
    return 1;
}

// Sphere against an oriented box. Normal points box -> sphere.
bool sphere_box(const Vec3& center, float radius, const Collider& box, Vec3& normal, float& depth,
                Vec3& point) noexcept {
    const math::Quat inverse = math::conjugate(box.rotation);
    const Vec3 local = math::rotate(inverse, center - box.position);
    const Vec3 clamped{math::clamp(local.x, -box.half.x, box.half.x),
                       math::clamp(local.y, -box.half.y, box.half.y),
                       math::clamp(local.z, -box.half.z, box.half.z)};
    const Vec3 offset = local - clamped;
    const float dist = math::length(offset);
    Vec3 local_normal;
    Vec3 local_point = clamped;
    if (dist > kEps) {
        if (dist >= radius) {
            return false;
        }
        local_normal = offset / dist;
        depth = radius - dist;
    } else {
        // Center inside the box: leave through the nearest face.
        int axis = 0;
        float best = comp(box.half, 0) - std::abs(comp(local, 0));
        for (int i = 1; i < 3; ++i) {
            const float gap = comp(box.half, i) - std::abs(comp(local, i));
            if (gap < best) {
                best = gap;
                axis = i;
            }
        }
        const float sign = comp(local, axis) >= 0.0f ? 1.0f : -1.0f;
        float n[3] = {0.0f, 0.0f, 0.0f};
        n[axis] = sign;
        local_normal = {n[0], n[1], n[2]};
        float p[3] = {local.x, local.y, local.z};
        p[axis] = sign * comp(box.half, axis);
        local_point = {p[0], p[1], p[2]};
        depth = radius + best;
    }
    normal = math::rotate(box.rotation, local_normal);
    point = box.position + math::rotate(box.rotation, local_point);
    return true;
}

float box_distance(const Vec3& world_point, const Collider& box) noexcept {
    const Vec3 local = math::rotate(math::conjugate(box.rotation), world_point - box.position);
    const Vec3 clamped{math::clamp(local.x, -box.half.x, box.half.x),
                       math::clamp(local.y, -box.half.y, box.half.y),
                       math::clamp(local.z, -box.half.z, box.half.z)};
    return math::length(local - clamped);
}

int capsule_box(const Collider& capsule, const Collider& box, bool capsule_first,
                Contact* out) noexcept {
    const Segment seg = segment_of(capsule);
    float params[3] = {0.0f, 1.0f, 0.0f};
    int candidates = 1;
    if (capsule.half_height > 0.0f) {
        float lo = 0.0f;
        float hi = 1.0f;
        for (int i = 0; i < 24; ++i) {
            const float m1 = lo + (hi - lo) / 3.0f;
            const float m2 = hi - (hi - lo) / 3.0f;
            if (box_distance(math::lerp(seg.p0, seg.p1, m1), box) <
                box_distance(math::lerp(seg.p0, seg.p1, m2), box)) {
                hi = m2;
            } else {
                lo = m1;
            }
        }
        params[2] = 0.5f * (lo + hi);
        candidates = 2;
        if (params[2] > 0.02f && params[2] < 0.98f) {
            candidates = 3;
        }
    }
    int count = 0;
    for (int i = 0; i < candidates; ++i) {
        Vec3 normal;
        Vec3 point;
        float depth = 0.0f;
        if (!sphere_box(math::lerp(seg.p0, seg.p1, params[i]), capsule.radius, box, normal, depth,
                        point)) {
            continue;
        }
        // sphere_box reports box -> capsule; flip when the capsule is the first collider.
        out[count++] = {point, capsule_first ? -normal : normal, depth};
    }
    return count;
}

int box_box(const Collider& a, const Collider& b, Contact* out) noexcept {
    Vec3 axes_a[3];
    Vec3 axes_b[3];
    for (int i = 0; i < 3; ++i) {
        axes_a[i] = local_axis(a, i);
        axes_b[i] = local_axis(b, i);
    }
    const float ha[3] = {a.half.x, a.half.y, a.half.z};
    const float hb[3] = {b.half.x, b.half.y, b.half.z};
    const Vec3 d = b.position - a.position;

    float best = 1.0e30f;
    Vec3 best_axis;
    int kind = -1; // 0 = face of A, 1 = face of B, 2 = edge pair
    int ia = 0;
    int ib = 0;

    bool separated = false;
    auto test = [&](Vec3 axis, int axis_kind, int i, int j) {
        const float len = math::length(axis);
        if (len < 1.0e-4f) {
            return;
        }
        axis = axis / len;
        float ra = 0.0f;
        float rb = 0.0f;
        for (int k = 0; k < 3; ++k) {
            ra += ha[k] * std::abs(math::dot(axes_a[k], axis));
            rb += hb[k] * std::abs(math::dot(axes_b[k], axis));
        }
        const float dist = math::dot(d, axis);
        const float penetration = ra + rb - std::abs(dist);
        if (penetration <= 0.0f) {
            separated = true;
            return;
        }
        // Edge axes must win clearly so face contacts stay stable.
        const float tolerance = axis_kind == 2 ? 0.01f : 0.0f;
        if (penetration + tolerance < best) {
            best = penetration;
            best_axis = dist < 0.0f ? -axis : axis;
            kind = axis_kind;
            ia = i;
            ib = j;
        }
    };

    for (int i = 0; i < 3; ++i) {
        test(axes_a[i], 0, i, 0);
    }
    for (int j = 0; j < 3; ++j) {
        test(axes_b[j], 1, 0, j);
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            test(math::cross(axes_a[i], axes_b[j]), 2, i, j);
        }
    }
    if (separated || kind < 0) {
        return 0;
    }

    int count = 0;
    constexpr float kBoundsSlack = 0.01f;
    if (kind == 0 || kind == 1) {
        const bool reference_is_a = kind == 0;
        const Collider& ref = reference_is_a ? a : b;
        const Collider& inc = reference_is_a ? b : a;
        const Vec3* ref_axes = reference_is_a ? axes_a : axes_b;
        const Vec3* inc_axes = reference_is_a ? axes_b : axes_a;
        const float* ref_half = reference_is_a ? ha : hb;
        const float* inc_half = reference_is_a ? hb : ha;
        const int face = reference_is_a ? ia : ib;
        for (int corner = 0; corner < 8 && count < kMaxContacts; ++corner) {
            Vec3 v = inc.position;
            for (int k = 0; k < 3; ++k) {
                const float sign = (corner >> k) & 1 ? 1.0f : -1.0f;
                v = v + inc_axes[k] * (sign * inc_half[k]);
            }
            const Vec3 offset = v - ref.position;
            // Reference face toward the other box sits at +/- half along best_axis.
            const float along = math::dot(offset, best_axis);
            const float depth = reference_is_a ? ref_half[face] - along : along + ref_half[face];
            if (depth <= 0.0f) {
                continue;
            }
            bool inside = true;
            for (int k = 0; k < 3; ++k) {
                if (k != face &&
                    std::abs(math::dot(offset, ref_axes[k])) > ref_half[k] + kBoundsSlack) {
                    inside = false;
                }
            }
            if (inside) {
                out[count++] = {v, best_axis, depth};
            }
        }
    } else {
        Vec3 pa = a.position;
        Vec3 pb = b.position;
        for (int k = 0; k < 3; ++k) {
            if (k != ia) {
                pa = pa +
                     axes_a[k] * ((math::dot(axes_a[k], best_axis) > 0.0f ? 1.0f : -1.0f) * ha[k]);
            }
            if (k != ib) {
                pb = pb +
                     axes_b[k] * ((math::dot(axes_b[k], best_axis) > 0.0f ? -1.0f : 1.0f) * hb[k]);
            }
        }
        const Vec3 u = axes_a[ia];
        const Vec3 v = axes_b[ib];
        const Vec3 r = pa - pb;
        const float b_dot = math::dot(u, v);
        const float c = math::dot(u, r);
        const float f = math::dot(v, r);
        const float denom = 1.0f - b_dot * b_dot;
        const float s = denom > kEps ? (b_dot * f - c) / denom : 0.0f;
        const float t = b_dot * s + f;
        out[count++] = {((pa + u * s) + (pb + v * t)) * 0.5f, best_axis, best};
    }
    if (count == 0) {
        out[count++] = {a.position + d * 0.5f, best_axis, best};
    }
    return count;
}

} // namespace

Collider make_collider(const ShapeDesc& shape, const Vec3& position,
                       const math::Quat& rotation) noexcept {
    Collider c;
    c.position = position;
    c.rotation = rotation;
    switch (shape.kind) {
    case ShapeKind::Sphere:
        c.radius = shape.radius;
        break;
    case ShapeKind::Box:
        c.is_box = true;
        c.half = shape.half_extents;
        break;
    case ShapeKind::Capsule:
        c.radius = shape.radius;
        c.half_height = shape.half_height;
        break;
    }
    return c;
}

Aabb bounds(const Collider& c) noexcept {
    Vec3 extent;
    if (c.is_box) {
        const Vec3 x = local_axis(c, 0);
        const Vec3 y = local_axis(c, 1);
        const Vec3 z = local_axis(c, 2);
        extent = {std::abs(x.x) * c.half.x + std::abs(y.x) * c.half.y + std::abs(z.x) * c.half.z,
                  std::abs(x.y) * c.half.x + std::abs(y.y) * c.half.y + std::abs(z.y) * c.half.z,
                  std::abs(x.z) * c.half.x + std::abs(y.z) * c.half.y + std::abs(z.z) * c.half.z};
    } else {
        const Vec3 axis = local_axis(c, 1) * c.half_height;
        extent = {std::abs(axis.x) + c.radius, std::abs(axis.y) + c.radius,
                  std::abs(axis.z) + c.radius};
    }
    return {c.position - extent, c.position + extent};
}

int collide(const Collider& a, const Collider& b, Contact* out) noexcept {
    if (a.is_box && b.is_box) {
        return box_box(a, b, out);
    }
    if (!a.is_box && !b.is_box) {
        return capsule_capsule(a, b, out);
    }
    return a.is_box ? capsule_box(b, a, false, out) : capsule_box(a, b, true, out);
}

int collide_plane(const Collider& c, float height, Contact* out) noexcept {
    const Vec3 up{0.0f, 1.0f, 0.0f};
    int count = 0;
    if (c.is_box) {
        for (int corner = 0; corner < 8; ++corner) {
            Vec3 v = c.position;
            for (int k = 0; k < 3; ++k) {
                const float sign = (corner >> k) & 1 ? 1.0f : -1.0f;
                v = v + local_axis(c, k) * (sign * comp(c.half, k));
            }
            const float depth = height - v.y;
            if (depth > 0.0f) {
                out[count++] = {v, up, depth};
            }
        }
        return count;
    }
    const Segment seg = segment_of(c);
    const Vec3 ends[2] = {seg.p0, seg.p1};
    const int n = c.half_height > 0.0f ? 2 : 1;
    for (int i = 0; i < n; ++i) {
        const float low = ends[i].y - c.radius;
        const float depth = height - low;
        if (depth > 0.0f) {
            out[count++] = {Vec3{ends[i].x, low, ends[i].z}, up, depth};
        }
    }
    return count;
}

} // namespace poko::physics::detail
