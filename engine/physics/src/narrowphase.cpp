#include "narrowphase.h"

#include <algorithm>
#include <cmath>

namespace poko::physics::detail {
namespace {

using math::Vec3;

constexpr float kEps = 1.0e-6f;

struct Obb {
    Vec3 c;
    Vec3 ax[3];
    float h[3];
};

Obb make_obb(const ShapeDesc& shape, const Pose& pose) noexcept {
    Obb o;
    o.c = pose.position;
    o.ax[0] = math::rotate(pose.rotation, Vec3{1, 0, 0});
    o.ax[1] = math::rotate(pose.rotation, Vec3{0, 1, 0});
    o.ax[2] = math::rotate(pose.rotation, Vec3{0, 0, 1});
    o.h[0] = shape.half_extents.x;
    o.h[1] = shape.half_extents.y;
    o.h[2] = shape.half_extents.z;
    return o;
}

struct Segment {
    Vec3 a, b;
};

Segment capsule_segment(const ShapeDesc& shape, const Pose& pose) noexcept {
    const Vec3 up = math::rotate(pose.rotation, Vec3{0, 1, 0}) * shape.half_height;
    return {pose.position - up, pose.position + up};
}

Vec3 closest_on_segment(const Segment& s, const Vec3& p) noexcept {
    const Vec3 ab = s.b - s.a;
    const float len2 = math::dot(ab, ab);
    if (len2 < kEps) {
        return s.a;
    }
    const float t = math::clamp(math::dot(p - s.a, ab) / len2, 0.0f, 1.0f);
    return s.a + ab * t;
}

// Ericson, Real-Time Collision Detection 5.1.9.
void closest_segments(const Segment& s1, const Segment& s2, Vec3& c1, Vec3& c2) noexcept {
    const Vec3 d1 = s1.b - s1.a;
    const Vec3 d2 = s2.b - s2.a;
    const Vec3 r = s1.a - s2.a;
    const float a = math::dot(d1, d1);
    const float e = math::dot(d2, d2);
    const float f = math::dot(d2, r);
    float s = 0.0f;
    float t = 0.0f;
    if (a <= kEps && e <= kEps) {
        c1 = s1.a;
        c2 = s2.a;
        return;
    }
    if (a <= kEps) {
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
    c1 = s1.a + d1 * s;
    c2 = s2.a + d2 * t;
}

bool sphere_sphere_contact(const Vec3& ca, float ra, const Vec3& cb, float rb,
                           ContactPoint& out) noexcept {
    const Vec3 delta = cb - ca;
    const float d = math::length(delta);
    const float sum = ra + rb;
    if (d >= sum) {
        return false;
    }
    out.normal = d > 1.0e-5f ? delta / d : Vec3{0.0f, 1.0f, 0.0f};
    out.depth = sum - d;
    out.point = ca + out.normal * (ra - out.depth * 0.5f);
    return true;
}

// Normal points from the sphere toward the box.
bool sphere_obb_contact(const Vec3& center, float r, const Obb& box, ContactPoint& out) noexcept {
    const Vec3 rel = center - box.c;
    float local[3];
    float clamped[3];
    bool inside = true;
    for (int i = 0; i < 3; ++i) {
        local[i] = math::dot(rel, box.ax[i]);
        clamped[i] = math::clamp(local[i], -box.h[i], box.h[i]);
        inside = inside && clamped[i] == local[i];
    }
    if (!inside) {
        const Vec3 closest = box.c + box.ax[0] * clamped[0] + box.ax[1] * clamped[1] +
                             box.ax[2] * clamped[2];
        const Vec3 delta = closest - center;
        const float d = math::length(delta);
        if (d >= r) {
            return false;
        }
        out.normal = d > 1.0e-6f ? delta / d : Vec3{0.0f, 1.0f, 0.0f};
        out.depth = r - d;
        out.point = closest;
        return true;
    }
    int axis = 0;
    float best = box.h[0] - std::abs(local[0]);
    for (int i = 1; i < 3; ++i) {
        const float gap = box.h[i] - std::abs(local[i]);
        if (gap < best) {
            best = gap;
            axis = i;
        }
    }
    const float sign = local[axis] >= 0.0f ? 1.0f : -1.0f;
    out.normal = box.ax[axis] * (-sign);
    out.depth = r + best;
    out.point = center + box.ax[axis] * (sign * best);
    return true;
}

bool near_point(const Manifold& m, const Vec3& p) noexcept {
    for (int i = 0; i < m.count; ++i) {
        if (math::length_squared(m.points[i].point - p) < 1.0e-6f) {
            return true;
        }
    }
    return false;
}

bool capsule_capsule(const Segment& s1, float r1, const Segment& s2, float r2,
                     Manifold& out) noexcept {
    Vec3 c1, c2;
    closest_segments(s1, s2, c1, c2);
    ContactPoint cp;
    if (!sphere_sphere_contact(c1, r1, c2, r2, cp)) {
        return false;
    }
    out.add(cp);
    // Endpoint contacts keep parallel, side-by-side capsules stable.
    for (const Vec3& p : {s1.a, s1.b}) {
        ContactPoint extra;
        if (sphere_sphere_contact(p, r1, closest_on_segment(s2, p), r2, extra) &&
            !near_point(out, extra.point)) {
            out.add(extra);
        }
    }
    return true;
}

bool capsule_obb(const Segment& seg, float r, const Obb& box, Manifold& out) noexcept {
    Vec3 s = box.c;
    for (int i = 0; i < 3; ++i) {
        const Vec3 on_seg = closest_on_segment(seg, s);
        const Vec3 rel = on_seg - box.c;
        Vec3 clamped = box.c;
        for (int k = 0; k < 3; ++k) {
            clamped = clamped + box.ax[k] * math::clamp(math::dot(rel, box.ax[k]), -box.h[k], box.h[k]);
        }
        s = clamped;
    }
    const Vec3 candidates[3] = {seg.a, seg.b, closest_on_segment(seg, s)};
    bool hit = false;
    for (const Vec3& p : candidates) {
        ContactPoint cp;
        if (sphere_obb_contact(p, r, box, cp) && !near_point(out, cp.point)) {
            out.add(cp);
            hit = true;
        }
    }
    return hit;
}

int clip_polygon(const Vec3* in, int n, const Vec3& plane_n, float plane_d, Vec3* out) noexcept {
    // Keep points with dot(p, plane_n) <= plane_d.
    int count = 0;
    for (int i = 0; i < n; ++i) {
        const Vec3& cur = in[i];
        const Vec3& nxt = in[(i + 1) % n];
        const float dc = math::dot(cur, plane_n) - plane_d;
        const float dn = math::dot(nxt, plane_n) - plane_d;
        if (dc <= 0.0f && count < 16) out[count++] = cur;
        if (((dc <= 0.0f) != (dn <= 0.0f)) && count < 16) {
            out[count++] = cur + (nxt - cur) * (dc / (dc - dn));
        }
    }
    return count;
}

bool box_box(const Obb& A, const Obb& B, Manifold& out) noexcept {
    const Vec3 t = B.c - A.c;
    float best = 1.0e30f;
    int best_axis = -1;
    Vec3 best_n{};

    const auto test = [&](const Vec3& axis, int index) -> bool {
        const float len = math::length(axis);
        if (len < 1.0e-4f) {
            return true;
        }
        Vec3 n = axis / len;
        float ra = 0.0f;
        float rb = 0.0f;
        for (int i = 0; i < 3; ++i) {
            ra += A.h[i] * std::abs(math::dot(n, A.ax[i]));
            rb += B.h[i] * std::abs(math::dot(n, B.ax[i]));
        }
        float dist = math::dot(t, n);
        const float overlap = ra + rb - std::abs(dist);
        if (overlap < 0.0f) {
            return false;
        }
        const bool edge = index >= 6;
        const bool better = edge ? (overlap * 1.05f + 1.0e-3f < best) : (overlap < best);
        if (better) {
            best = overlap;
            best_axis = index;
            best_n = dist < 0.0f ? -n : n;
        }
        return true;
    };

    for (int i = 0; i < 3; ++i) {
        if (!test(A.ax[i], i)) return false;
    }
    for (int i = 0; i < 3; ++i) {
        if (!test(B.ax[i], 3 + i)) return false;
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (!test(math::cross(A.ax[i], B.ax[j]), 6 + 3 * i + j)) return false;
        }
    }
    if (best_axis < 0) {
        return false;
    }
    const Vec3& n = best_n; // A -> B

    if (best_axis >= 6) {
        const int i = (best_axis - 6) / 3;
        const int j = (best_axis - 6) % 3;
        Vec3 pa = A.c;
        Vec3 pb = B.c;
        for (int k = 0; k < 3; ++k) {
            if (k != i) pa = pa + A.ax[k] * ((math::dot(n, A.ax[k]) > 0.0f ? 1.0f : -1.0f) * A.h[k]);
            if (k != j) pb = pb + B.ax[k] * ((math::dot(n, B.ax[k]) > 0.0f ? -1.0f : 1.0f) * B.h[k]);
        }
        const Vec3 da = A.ax[i];
        const Vec3 db = B.ax[j];
        const Vec3 r = pa - pb;
        const float b = math::dot(da, db);
        const float c = math::dot(da, r);
        const float f = math::dot(db, r);
        const float denom = 1.0f - b * b;
        const float s = denom > kEps ? (b * f - c) / denom : 0.0f;
        const float tt = b * s + f;
        const Vec3 qa = pa + da * s;
        const Vec3 qb = pb + db * tt;
        out.add({(qa + qb) * 0.5f, n, best});
        return true;
    }

    const bool ref_is_a = best_axis < 3;
    const Obb& ref = ref_is_a ? A : B;
    const Obb& inc = ref_is_a ? B : A;
    const int ref_axis = ref_is_a ? best_axis : best_axis - 3;
    const Vec3 nr = ref_is_a ? n : -n; // outward from the reference box toward the incident box

    int inc_axis = 0;
    float most = -1.0f;
    for (int k = 0; k < 3; ++k) {
        const float d = std::abs(math::dot(nr, inc.ax[k]));
        if (d > most) {
            most = d;
            inc_axis = k;
        }
    }
    const float face_sign = math::dot(nr, inc.ax[inc_axis]) > 0.0f ? -1.0f : 1.0f;
    const int u = (inc_axis + 1) % 3;
    const int v = (inc_axis + 2) % 3;
    const Vec3 fc = inc.c + inc.ax[inc_axis] * (face_sign * inc.h[inc_axis]);
    Vec3 poly[16] = {
        fc + inc.ax[u] * inc.h[u] + inc.ax[v] * inc.h[v],
        fc - inc.ax[u] * inc.h[u] + inc.ax[v] * inc.h[v],
        fc - inc.ax[u] * inc.h[u] - inc.ax[v] * inc.h[v],
        fc + inc.ax[u] * inc.h[u] - inc.ax[v] * inc.h[v],
    };
    int count = 4;
    Vec3 tmp[16];
    for (int side = 1; side <= 2; ++side) {
        const int k = (ref_axis + side) % 3;
        const float base = math::dot(ref.c, ref.ax[k]);
        count = clip_polygon(poly, count, ref.ax[k], base + ref.h[k], tmp);
        for (int i = 0; i < count; ++i) poly[i] = tmp[i];
        if (count == 0) break;
        count = clip_polygon(poly, count, -ref.ax[k], -base + ref.h[k], tmp);
        for (int i = 0; i < count; ++i) poly[i] = tmp[i];
        if (count == 0) break;
    }

    const float plane = math::dot(ref.c, nr) + ref.h[ref_axis];
    for (int i = 0; i < count; ++i) {
        const float depth = plane - math::dot(poly[i], nr);
        if (depth >= 0.0f) {
            out.add({poly[i] + nr * (depth * 0.5f), n, depth});
        }
    }
    if (out.count == 0) {
        out.add({(A.c + B.c) * 0.5f, n, best});
    }
    return true;
}

int rank(ShapeKind kind) noexcept {
    switch (kind) {
    case ShapeKind::Sphere:
        return 0;
    case ShapeKind::Capsule:
        return 1;
    case ShapeKind::Box:
        return 2;
    }
    return 0;
}

} // namespace

bool collide(const ShapeDesc& a, const Pose& pa, const ShapeDesc& b, const Pose& pb,
             Manifold& out) noexcept {
    if (rank(a.kind) > rank(b.kind)) {
        Manifold flipped;
        const bool hit = collide(b, pb, a, pa, flipped);
        for (int i = 0; i < flipped.count; ++i) {
            ContactPoint p = flipped.points[i];
            p.normal = -p.normal;
            out.add(p);
        }
        return hit;
    }
    const int before = out.count;
    ContactPoint cp;
    switch (a.kind) {
    case ShapeKind::Sphere:
        switch (b.kind) {
        case ShapeKind::Sphere:
            if (sphere_sphere_contact(pa.position, a.radius, pb.position, b.radius, cp)) out.add(cp);
            break;
        case ShapeKind::Capsule:
            if (sphere_sphere_contact(pa.position, a.radius,
                                      closest_on_segment(capsule_segment(b, pb), pa.position),
                                      b.radius, cp)) {
                out.add(cp);
            }
            break;
        case ShapeKind::Box:
            if (sphere_obb_contact(pa.position, a.radius, make_obb(b, pb), cp)) out.add(cp);
            break;
        }
        break;
    case ShapeKind::Capsule:
        if (b.kind == ShapeKind::Capsule) {
            capsule_capsule(capsule_segment(a, pa), a.radius, capsule_segment(b, pb), b.radius, out);
        } else {
            capsule_obb(capsule_segment(a, pa), a.radius, make_obb(b, pb), out);
        }
        break;
    case ShapeKind::Box:
        box_box(make_obb(a, pa), make_obb(b, pb), out);
        break;
    }
    return out.count > before;
}

bool collide_ground(const ShapeDesc& shape, const Pose& pose, float height,
                    Manifold& out) noexcept {
    const int before = out.count;
    const Vec3 down{0.0f, -1.0f, 0.0f};
    const auto add_sphere = [&](const Vec3& c, float r) {
        const float depth = height - (c.y - r);
        if (depth > 0.0f) {
            out.add({Vec3{c.x, height, c.z}, down, depth});
        }
    };
    switch (shape.kind) {
    case ShapeKind::Sphere:
        add_sphere(pose.position, shape.radius);
        break;
    case ShapeKind::Capsule: {
        const Segment s = capsule_segment(shape, pose);
        add_sphere(s.a, shape.radius);
        add_sphere(s.b, shape.radius);
        break;
    }
    case ShapeKind::Box: {
        const Obb o = make_obb(shape, pose);
        for (int i = 0; i < 8; ++i) {
            const Vec3 corner = o.c + o.ax[0] * ((i & 1) ? o.h[0] : -o.h[0]) +
                                o.ax[1] * ((i & 2) ? o.h[1] : -o.h[1]) +
                                o.ax[2] * ((i & 4) ? o.h[2] : -o.h[2]);
            const float depth = height - corner.y;
            if (depth > 0.0f) {
                out.add({Vec3{corner.x, height, corner.z}, down, depth});
            }
        }
        break;
    }
    }
    return out.count > before;
}

Vec3 aabb_half_extents(const ShapeDesc& shape, const math::Quat& rotation) noexcept {
    switch (shape.kind) {
    case ShapeKind::Sphere:
        return {shape.radius, shape.radius, shape.radius};
    case ShapeKind::Capsule: {
        const Vec3 up = math::rotate(rotation, Vec3{0, 1, 0});
        return {std::abs(up.x) * shape.half_height + shape.radius,
                std::abs(up.y) * shape.half_height + shape.radius,
                std::abs(up.z) * shape.half_height + shape.radius};
    }
    case ShapeKind::Box: {
        const Obb o = make_obb(shape, Pose{Vec3{}, rotation});
        Vec3 h{};
        for (int i = 0; i < 3; ++i) {
            h.x += std::abs(o.ax[i].x) * o.h[i];
            h.y += std::abs(o.ax[i].y) * o.h[i];
            h.z += std::abs(o.ax[i].z) * o.h[i];
        }
        return h;
    }
    }
    return {};
}

namespace {

bool ray_sphere_t(const Vec3& o, const Vec3& d, const Vec3& c, float r, float max_t,
                  float& t_out) noexcept {
    const Vec3 oc = o - c;
    const float b = math::dot(oc, d);
    const float cc = math::dot(oc, oc) - r * r;
    const float disc = b * b - cc;
    if (disc < 0.0f) {
        return false;
    }
    const float root = std::sqrt(disc);
    float t = -b - root;
    if (t < 0.0f) {
        t = -b + root;
    }
    if (t < 0.0f || t > max_t) {
        return false;
    }
    t_out = t;
    return true;
}

} // namespace

bool ray_shape(const Vec3& origin, const Vec3& direction, const ShapeDesc& shape, const Pose& pose,
               float max_distance, float& out_t, Vec3& out_normal) noexcept {
    switch (shape.kind) {
    case ShapeKind::Sphere: {
        if (!ray_sphere_t(origin, direction, pose.position, shape.radius, max_distance, out_t)) {
            return false;
        }
        out_normal = math::normalize(origin + direction * out_t - pose.position);
        return true;
    }
    case ShapeKind::Box: {
        const math::Quat inv = math::conjugate(pose.rotation);
        const Vec3 o = math::rotate(inv, origin - pose.position);
        const Vec3 d = math::rotate(inv, direction);
        const float oa[3] = {o.x, o.y, o.z};
        const float da[3] = {d.x, d.y, d.z};
        const float ha[3] = {shape.half_extents.x, shape.half_extents.y, shape.half_extents.z};
        float t_min = 0.0f;
        float t_max = max_distance;
        int axis = -1;
        float sign = 1.0f;
        for (int i = 0; i < 3; ++i) {
            if (std::abs(da[i]) < 1.0e-8f) {
                if (oa[i] < -ha[i] || oa[i] > ha[i]) return false;
                continue;
            }
            float t1 = (-ha[i] - oa[i]) / da[i];
            float t2 = (ha[i] - oa[i]) / da[i];
            float s = -1.0f;
            if (t1 > t2) {
                std::swap(t1, t2);
                s = 1.0f;
            }
            if (t1 > t_min) {
                t_min = t1;
                axis = i;
                sign = s;
            }
            t_max = std::min(t_max, t2);
            if (t_min > t_max) return false;
        }
        Vec3 local_n{};
        if (axis >= 0) {
            (axis == 0 ? local_n.x : axis == 1 ? local_n.y : local_n.z) = sign;
        } else {
            local_n = -d; // origin inside the box
        }
        out_t = t_min;
        out_normal = math::rotate(pose.rotation, local_n);
        return true;
    }
    case ShapeKind::Capsule: {
        const math::Quat inv = math::conjugate(pose.rotation);
        const Vec3 o = math::rotate(inv, origin - pose.position);
        const Vec3 d = math::rotate(inv, direction);
        const float hh = shape.half_height;
        const float r = shape.radius;
        float best = max_distance;
        bool hit = false;
        Vec3 local_n{};
        // Cylinder side.
        const float a = d.x * d.x + d.z * d.z;
        if (a > 1.0e-10f) {
            const float b = o.x * d.x + o.z * d.z;
            const float c = o.x * o.x + o.z * o.z - r * r;
            const float disc = b * b - a * c;
            if (disc >= 0.0f) {
                const float t = (-b - std::sqrt(disc)) / a;
                const float y = o.y + d.y * t;
                if (t >= 0.0f && t <= best && std::abs(y) <= hh) {
                    best = t;
                    hit = true;
                    local_n = Vec3{o.x + d.x * t, 0.0f, o.z + d.z * t} / r;
                }
            }
        }
        for (const float cap : {hh, -hh}) {
            float t = 0.0f;
            const Vec3 center{0.0f, cap, 0.0f};
            if (ray_sphere_t(o, d, center, r, best, t)) {
                const Vec3 p = o + d * t;
                if ((cap > 0.0f && p.y >= hh) || (cap < 0.0f && p.y <= -hh)) {
                    best = t;
                    hit = true;
                    local_n = math::normalize(p - center);
                }
            }
        }
        if (!hit) return false;
        out_t = best;
        out_normal = math::rotate(pose.rotation, local_n);
        return true;
    }
    }
    return false;
}

} // namespace poko::physics::detail
