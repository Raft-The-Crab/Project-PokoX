#include "gjk.h"

#include <algorithm>
#include <cmath>

namespace poko::physics::detail {
namespace {

using math::Vec3;

struct Simplex {
    Vec3 w[4];  // Minkowski-difference points (a - b)
    Vec3 a[4];  // witness on shape A
    Vec3 b[4];  // witness on shape B
    float lam[4] = {};
    int n = 0;

    void push(const Vec3& wi, const Vec3& ai, const Vec3& bi) {
        w[n] = wi;
        a[n] = ai;
        b[n] = bi;
        ++n;
    }
    void keep(const int* idx, int count, const float* lambdas) {
        Vec3 nw[4], na[4], nb[4];
        for (int i = 0; i < count; ++i) {
            nw[i] = w[idx[i]];
            na[i] = a[idx[i]];
            nb[i] = b[idx[i]];
        }
        for (int i = 0; i < count; ++i) {
            w[i] = nw[i];
            a[i] = na[i];
            b[i] = nb[i];
            lam[i] = lambdas[i];
        }
        n = count;
    }
};

// Closest point to the origin on triangle (a, b, c); barycentric out, plus the
// vertex subset that supports it. Ericson, Real-Time Collision Detection 5.1.5.
float closest_on_triangle(const Vec3& a, const Vec3& b, const Vec3& c, float out_lam[3],
                          int out_idx[3], int& out_count, Vec3& out_point) noexcept {
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const Vec3 ap = -a;
    const float d1 = math::dot(ab, ap);
    const float d2 = math::dot(ac, ap);
    const auto vertex = [&](int i, const Vec3& p) {
        out_lam[0] = 1.0f;
        out_idx[0] = i;
        out_count = 1;
        out_point = p;
        return math::length_squared(p);
    };
    const auto edge = [&](int i, int j, float t, const Vec3& p) {
        out_lam[0] = 1.0f - t;
        out_lam[1] = t;
        out_idx[0] = i;
        out_idx[1] = j;
        out_count = 2;
        out_point = p;
        return math::length_squared(p);
    };
    if (d1 <= 0.0f && d2 <= 0.0f) return vertex(0, a);
    const Vec3 bp = -b;
    const float d3 = math::dot(ab, bp);
    const float d4 = math::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return vertex(1, b);
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float v = d1 / (d1 - d3);
        return edge(0, 1, v, a + ab * v);
    }
    const Vec3 cp = -c;
    const float d5 = math::dot(ab, cp);
    const float d6 = math::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return vertex(2, c);
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float w = d2 / (d2 - d6);
        return edge(0, 2, w, a + ac * w);
    }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return edge(1, 2, w, b + (c - b) * w);
    }
    const float denom = 1.0f / (va + vb + vc);
    const float v = vb * denom;
    const float w = vc * denom;
    out_lam[0] = 1.0f - v - w;
    out_lam[1] = v;
    out_lam[2] = w;
    out_idx[0] = 0;
    out_idx[1] = 1;
    out_idx[2] = 2;
    out_count = 3;
    out_point = a + ab * v + ac * w;
    return math::length_squared(out_point);
}

// Reduces the simplex to the feature closest to the origin and returns that
// closest point. Returns true if the origin lies inside a full tetrahedron.
bool reduce(Simplex& s, Vec3& v) noexcept {
    if (s.n == 1) {
        s.lam[0] = 1.0f;
        v = s.w[0];
        return false;
    }
    if (s.n == 2) {
        const Vec3 ab = s.w[1] - s.w[0];
        const float len2 = math::dot(ab, ab);
        float t = len2 > 1.0e-20f ? -math::dot(s.w[0], ab) / len2 : 0.0f;
        t = math::clamp(t, 0.0f, 1.0f);
        v = s.w[0] + ab * t;
        if (t <= 0.0f) {
            const int idx[1] = {0};
            const float l[1] = {1.0f};
            s.keep(idx, 1, l);
        } else if (t >= 1.0f) {
            const int idx[1] = {1};
            const float l[1] = {1.0f};
            s.keep(idx, 1, l);
        } else {
            s.lam[0] = 1.0f - t;
            s.lam[1] = t;
        }
        return false;
    }
    if (s.n == 3) {
        float lam[3];
        int idx[3];
        int count = 0;
        closest_on_triangle(s.w[0], s.w[1], s.w[2], lam, idx, count, v);
        s.keep(idx, count, lam);
        return false;
    }
    // Tetrahedron: origin inside if it is on the inner side of all four faces.
    static const int faces[4][4] = {{0, 1, 2, 3}, {0, 3, 1, 2}, {0, 2, 3, 1}, {1, 3, 2, 0}};
    // A flat tetrahedron cannot contain the origin; treat every face as a candidate.
    const float volume = math::dot(math::cross(s.w[1] - s.w[0], s.w[2] - s.w[0]), s.w[3] - s.w[0]);
    const bool flat = std::abs(volume) < 1.0e-9f;
    bool inside = true;
    float best = 1.0e30f;
    float best_lam[3] = {};
    int best_idx[3] = {};
    int best_count = 0;
    Vec3 best_point{};
    for (const auto& f : faces) {
        const Vec3& p = s.w[f[0]];
        const Vec3 normal = math::cross(s.w[f[1]] - p, s.w[f[2]] - p);
        const float side_origin = math::dot(normal, -p);
        const float side_opposite = math::dot(normal, s.w[f[3]] - p);
        if (flat || side_origin * side_opposite <= 0.0f) {
            inside = false;
            float lam[3];
            int idx[3];
            int count = 0;
            Vec3 point{};
            const float d2 = closest_on_triangle(s.w[f[0]], s.w[f[1]], s.w[f[2]], lam, idx, count, point);
            if (d2 < best) {
                best = d2;
                best_count = count;
                best_point = point;
                for (int i = 0; i < count; ++i) {
                    best_lam[i] = lam[i];
                    best_idx[i] = f[idx[i]];
                }
            }
        }
    }
    if (inside) return true;
    v = best_point;
    s.keep(best_idx, best_count, best_lam);
    return false;
}

} // namespace

ConvexCore make_core(const ConvexSource& source, const Vec3& position, const math::Quat& rotation) noexcept {
    ConvexCore c;
    c.center = position;
    c.axes[0] = math::rotate(rotation, Vec3{1, 0, 0});
    c.axes[1] = math::rotate(rotation, Vec3{0, 1, 0});
    c.axes[2] = math::rotate(rotation, Vec3{0, 0, 1});
    if (source.triangle != nullptr) {
        c.kind = ConvexCore::Kind::Hull;
        c.verts = source.triangle;
        c.vert_count = 3;
        return c;
    }
    const ShapeDesc& shape = *source.desc;
    switch (shape.kind) {
    case ShapeKind::Sphere:
        c.kind = ConvexCore::Kind::Point;
        c.p0 = c.p1 = position;
        c.margin = shape.radius;
        break;
    case ShapeKind::Capsule: {
        c.kind = ConvexCore::Kind::Segment;
        const Vec3 up = math::rotate(rotation, Vec3{0, 1, 0}) * shape.half_height;
        c.p0 = position - up;
        c.p1 = position + up;
        c.margin = shape.radius;
        break;
    }
    case ShapeKind::Box:
        c.kind = ConvexCore::Kind::Box;
        c.half = shape.half_extents;
        break;
    case ShapeKind::ConvexHull:
    case ShapeKind::TriangleMesh: // a whole mesh is never one convex core; callers go triangle by triangle
        c.kind = ConvexCore::Kind::Hull;
        if (source.geo != nullptr) {
            c.verts = source.geo->vertices.data();
            c.vert_count = static_cast<int>(source.geo->vertices.size());
        }
        break;
    }
    return c;
}

Vec3 support(const ConvexCore& core, const Vec3& d) noexcept {
    switch (core.kind) {
    case ConvexCore::Kind::Point:
        return core.p0;
    case ConvexCore::Kind::Segment:
        return math::dot(d, core.p1 - core.p0) > 0.0f ? core.p1 : core.p0;
    case ConvexCore::Kind::Box: {
        Vec3 p = core.center;
        const float h[3] = {core.half.x, core.half.y, core.half.z};
        for (int i = 0; i < 3; ++i) {
            p = p + core.axes[i] * (math::dot(d, core.axes[i]) >= 0.0f ? h[i] : -h[i]);
        }
        return p;
    }
    case ConvexCore::Kind::Hull: {
        // Search in local space so the vertex array never needs transforming.
        const Vec3 local{math::dot(d, core.axes[0]), math::dot(d, core.axes[1]),
                         math::dot(d, core.axes[2])};
        float best = -1.0e30f;
        const Vec3* pick = core.verts;
        for (int i = 0; i < core.vert_count; ++i) {
            const float s = math::dot(local, core.verts[i]);
            if (s > best) {
                best = s;
                pick = core.verts + i;
            }
        }
        if (pick == nullptr) return core.center;
        return core.center + core.axes[0] * pick->x + core.axes[1] * pick->y + core.axes[2] * pick->z;
    }
    }
    return core.center;
}

float bounding_radius(const ConvexSource& source) noexcept {
    if (source.triangle != nullptr) return source.triangle_radius;
    const ShapeDesc& shape = *source.desc;
    switch (shape.kind) {
    case ShapeKind::Sphere:
        return shape.radius;
    case ShapeKind::Capsule:
        return shape.radius + shape.half_height;
    case ShapeKind::Box:
        return math::length(shape.half_extents);
    case ShapeKind::ConvexHull:
    case ShapeKind::TriangleMesh:
        return source.geo != nullptr ? source.geo->radius : 0.0f;
    }
    return shape.radius;
}

CoreDistance core_distance(const ConvexCore& a, const ConvexCore& b) noexcept {
    CoreDistance out;
    Simplex s;
    Vec3 dir = a.center - b.center;
    if (math::length_squared(dir) < 1.0e-12f) dir = {1, 0, 0};
    const auto add = [&](const Vec3& d) {
        const Vec3 pa = support(a, d);
        const Vec3 pb = support(b, -d);
        s.push(pa - pb, pa, pb);
        return s.w[s.n - 1];
    };
    add(dir);
    Vec3 v{};
    for (int iter = 0; iter < 64; ++iter) {
        if (reduce(s, v)) {
            out.intersecting = true;
            return out;
        }
        const float v2 = math::length_squared(v);
        const auto finish = [&] {
            Vec3 pa{};
            Vec3 pb{};
            for (int i = 0; i < s.n; ++i) {
                pa = pa + s.a[i] * s.lam[i];
                pb = pb + s.b[i] * s.lam[i];
            }
            out.distance = std::sqrt(v2);
            out.point_a = pa;
            out.point_b = pb;
        };
        if (v2 < 1.0e-10f) {
            out.intersecting = true;
            return out;
        }
        const Vec3 pa = support(a, -v);
        const Vec3 pb = support(b, v);
        const Vec3 w = pa - pb;
        bool duplicate = false;
        for (int i = 0; i < s.n; ++i) {
            if (math::length_squared(s.w[i] - w) < 1.0e-12f) duplicate = true;
        }
        if (duplicate || v2 - math::dot(v, w) <= 1.0e-6f * v2) {
            finish();
            return out;
        }
        s.push(w, pa, pb);
    }
    // Iteration cap: report the best feature found.
    Vec3 pa{};
    Vec3 pb{};
    for (int i = 0; i < s.n; ++i) {
        pa = pa + s.a[i] * s.lam[i];
        pb = pb + s.b[i] * s.lam[i];
    }
    out.distance = math::length(v);
    out.point_a = pa;
    out.point_b = pb;
    return out;
}

math::Quat advance_rotation(const math::Quat& rotation, const Vec3& spin, float t) noexcept {
    const float speed = math::length(spin);
    if (speed < 1.0e-6f) return rotation;
    return math::multiply(math::quat_from_axis_angle(spin / speed, speed * t), rotation);
}

bool time_of_impact(const ConvexSource& shape_a, const Motion& a, const ConvexSource& shape_b,
                    const Motion& b, float duration, float tolerance, ImpactResult& out) noexcept {
    const float bound = math::length(a.velocity - b.velocity) +
                        math::length(a.spin) * bounding_radius(shape_a) +
                        math::length(b.spin) * bounding_radius(shape_b);
    if (bound < 1.0e-6f) return false;
    float t = 0.0f;
    for (int iter = 0; iter < 48; ++iter) {
        const ConvexCore ca = make_core(shape_a, a.position + a.velocity * t,
                                        advance_rotation(a.rotation, a.spin, t));
        const ConvexCore cb = make_core(shape_b, b.position + b.velocity * t,
                                        advance_rotation(b.rotation, b.spin, t));
        const CoreDistance d = core_distance(ca, cb);
        const float gap = d.distance - ca.margin - cb.margin;
        if (d.intersecting || gap <= 0.0f) {
            if (t == 0.0f) return false; // already overlapping: solver's job
            out.time = t;
            // Cores touching: the separating direction is lost, so use the approach direction.
            const Vec3 approach = a.velocity - b.velocity;
            out.normal = d.intersecting && math::length_squared(approach) > 1.0e-12f
                             ? math::normalize(approach)
                             : math::normalize(d.point_b - d.point_a);
            out.closing = d.intersecting ? bound : math::dot(a.velocity - b.velocity, out.normal);
            return true;
        }
        if (gap < tolerance) {
            const Vec3 n = math::normalize(d.point_b - d.point_a);
            // Approach speed of the two contact points, spin included.
            const Vec3 pa_vel = a.velocity + math::cross(a.spin, d.point_a - (a.position + a.velocity * t));
            const Vec3 pb_vel = b.velocity + math::cross(b.spin, d.point_b - (b.position + b.velocity * t));
            const float closing = math::dot(pa_vel - pb_vel, n);
            if (t == 0.0f && closing <= 0.0f) return false; // touching but separating
            out.time = t;
            out.normal = n;
            out.closing = closing;
            return true;
        }
        t += std::max(gap - 0.5f * tolerance, 0.25f * tolerance) / bound; // stop short of touching
        if (t >= duration) return false;
    }
    return false;
}

} // namespace poko::physics::detail
