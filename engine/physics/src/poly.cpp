#include "poly.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace poko::physics::detail {
namespace {

using math::Vec3;

constexpr float kContactSlop = 1.0e-3f;

std::vector<HullEdge> derive_edges(const std::vector<HullFace>& faces) {
    std::map<std::pair<std::uint16_t, std::uint16_t>, std::uint16_t> directed;
    for (std::size_t f = 0; f < faces.size(); ++f) {
        const auto& loop = faces[f].verts;
        for (std::size_t i = 0; i < loop.size(); ++i) {
            directed[{loop[i], loop[(i + 1) % loop.size()]}] = static_cast<std::uint16_t>(f);
        }
    }
    std::vector<HullEdge> edges;
    for (const auto& [edge, face] : directed) {
        if (edge.first >= edge.second) continue;
        const auto twin = directed.find({edge.second, edge.first});
        if (twin == directed.end()) continue;
        edges.push_back({edge.first, edge.second, face, twin->second});
    }
    return edges;
}

struct BoxFaceDef {
    int n[3];
    int u[3];
    int v[3];
};

// u x v == n, so the corner loop below is counter-clockwise seen from outside.
const BoxFaceDef kBoxFaces[6] = {
    {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},  {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
    {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}},  {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
    {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {0, 1, 0}, {1, 0, 0}},
};

std::uint16_t corner_index(int sx, int sy, int sz) {
    return static_cast<std::uint16_t>((sx > 0 ? 1 : 0) | (sy > 0 ? 2 : 0) | (sz > 0 ? 4 : 0));
}

const std::vector<std::vector<std::uint16_t>>& box_loops() {
    static const auto loops = [] {
        std::vector<std::vector<std::uint16_t>> out;
        for (const BoxFaceDef& f : kBoxFaces) {
            const int signs[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
            std::vector<std::uint16_t> loop;
            for (const auto& s : signs) {
                loop.push_back(corner_index(f.n[0] + s[0] * f.u[0] + s[1] * f.v[0],
                                            f.n[1] + s[0] * f.u[1] + s[1] * f.v[1],
                                            f.n[2] + s[0] * f.u[2] + s[1] * f.v[2]));
            }
            out.push_back(loop);
        }
        return out;
    }();
    return loops;
}

const std::vector<HullEdge>& box_edges() {
    static const auto edges = [] {
        std::vector<HullFace> faces;
        for (const auto& loop : box_loops()) {
            HullFace f;
            f.verts = loop;
            faces.push_back(f);
        }
        return derive_edges(faces);
    }();
    return edges;
}

Vec3 average(const std::vector<Vec3>& v) {
    Vec3 sum{};
    for (const Vec3& p : v) sum = sum + p;
    return v.empty() ? sum : sum / static_cast<float>(v.size());
}

struct FaceQuery {
    float sep = -1.0e30f;
    int face = -1;
};

// Largest separation of B from any face plane of A (positive: a separating axis).
FaceQuery face_query(const Poly& a, const Poly& b) {
    FaceQuery best;
    for (std::size_t i = 0; i < a.faces.size(); ++i) {
        const HullFace& f = a.faces[i];
        float lowest = 1.0e30f;
        for (const Vec3& p : b.verts) lowest = std::min(lowest, math::dot(f.normal, p));
        const float sep = lowest - f.offset;
        if (sep > best.sep) {
            best.sep = sep;
            best.face = static_cast<int>(i);
        }
    }
    return best;
}

// Arcs ab and cd on the unit sphere intersect: the two edges span a Minkowski face.
bool minkowski_face(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
    const Vec3 bxa = math::cross(b, a);
    const Vec3 dxc = math::cross(d, c);
    const float cba = math::dot(c, bxa);
    const float dba = math::dot(d, bxa);
    const float adc = math::dot(a, dxc);
    const float bdc = math::dot(b, dxc);
    return cba * dba < 0.0f && adc * bdc < 0.0f && cba * bdc > 0.0f;
}

struct EdgeQuery {
    float sep = -1.0e30f;
    int edge_a = -1;
    int edge_b = -1;
    Vec3 axis{};
};

EdgeQuery edge_query(const Poly& a, const Poly& b) {
    EdgeQuery best;
    for (std::size_t i = 0; i < a.edges.size(); ++i) {
        const HullEdge& ea = a.edges[i];
        const Vec3 pa = a.verts[ea.a];
        const Vec3 da = a.verts[ea.b] - pa;
        const Vec3& na1 = a.faces[ea.face_a].normal;
        const Vec3& na2 = a.faces[ea.face_b].normal;
        for (std::size_t j = 0; j < b.edges.size(); ++j) {
            const HullEdge& eb = b.edges[j];
            const Vec3& nb1 = b.faces[eb.face_a].normal;
            const Vec3& nb2 = b.faces[eb.face_b].normal;
            if (!minkowski_face(na1, na2, -nb1, -nb2)) continue;
            const Vec3 pb = b.verts[eb.a];
            const Vec3 db = b.verts[eb.b] - pb;
            Vec3 axis = math::cross(da, db);
            const float len = math::length(axis);
            if (len < 1.0e-6f * math::length(da) * math::length(db)) continue; // parallel
            axis = axis / len;
            if (math::dot(axis, pa - a.center) < 0.0f) axis = -axis;
            const float sep = math::dot(axis, pb - pa);
            if (sep > best.sep) {
                best.sep = sep;
                best.edge_a = static_cast<int>(i);
                best.edge_b = static_cast<int>(j);
                best.axis = axis;
            }
        }
    }
    return best;
}

std::vector<Vec3> clip_to_plane(const std::vector<Vec3>& in, const Vec3& normal, float offset) {
    std::vector<Vec3> out;
    for (std::size_t i = 0; i < in.size(); ++i) {
        const Vec3& cur = in[i];
        const Vec3& nxt = in[(i + 1) % in.size()];
        const float dc = math::dot(normal, cur) - offset;
        const float dn = math::dot(normal, nxt) - offset;
        if (dc <= 0.0f) out.push_back(cur);
        if ((dc <= 0.0f) != (dn <= 0.0f)) out.push_back(cur + (nxt - cur) * (dc / (dc - dn)));
    }
    return out;
}

void face_contacts(const Poly& ref, int ref_face, const Poly& inc, bool ref_is_a, Manifold& out) {
    const HullFace& rf = ref.faces[static_cast<std::size_t>(ref_face)];
    int inc_face = 0;
    float most_opposed = 1.0e30f;
    for (std::size_t i = 0; i < inc.faces.size(); ++i) {
        const float d = math::dot(inc.faces[i].normal, rf.normal);
        if (d < most_opposed) {
            most_opposed = d;
            inc_face = static_cast<int>(i);
        }
    }
    std::vector<Vec3> poly;
    for (const auto v : inc.faces[static_cast<std::size_t>(inc_face)].verts) poly.push_back(inc.verts[v]);
    // Clip the incident polygon to the reference face's side planes.
    for (std::size_t i = 0; i < rf.verts.size() && !poly.empty(); ++i) {
        const Vec3& p = ref.verts[rf.verts[i]];
        const Vec3& q = ref.verts[rf.verts[(i + 1) % rf.verts.size()]];
        const Vec3 outward = math::normalize(math::cross(q - p, rf.normal));
        poly = clip_to_plane(poly, outward, math::dot(outward, p));
    }
    const Vec3 normal = ref_is_a ? rf.normal : -rf.normal;
    for (const Vec3& p : poly) {
        const float depth = rf.offset - math::dot(rf.normal, p);
        if (depth < -kContactSlop) continue;
        out.add_deepest({p + rf.normal * (depth * 0.5f), normal, std::max(depth, 0.0f)});
    }
}

} // namespace

Poly poly_from_box(const Vec3& half, const Pose& pose) {
    Poly p;
    const Vec3 ax[3] = {math::rotate(pose.rotation, Vec3{1, 0, 0}),
                        math::rotate(pose.rotation, Vec3{0, 1, 0}),
                        math::rotate(pose.rotation, Vec3{0, 0, 1})};
    for (int i = 0; i < 8; ++i) {
        p.verts.push_back(pose.position + ax[0] * ((i & 1) ? half.x : -half.x) +
                          ax[1] * ((i & 2) ? half.y : -half.y) + ax[2] * ((i & 4) ? half.z : -half.z));
    }
    for (std::size_t f = 0; f < 6; ++f) {
        HullFace face;
        face.verts = box_loops()[f];
        const BoxFaceDef& def = kBoxFaces[f];
        face.normal = ax[0] * static_cast<float>(def.n[0]) + ax[1] * static_cast<float>(def.n[1]) +
                      ax[2] * static_cast<float>(def.n[2]);
        face.offset = math::dot(face.normal, p.verts[face.verts[0]]);
        p.faces.push_back(std::move(face));
    }
    p.edges = box_edges();
    p.center = pose.position;
    return p;
}

Poly poly_from_hull(const Geometry& hull, const Pose& pose) {
    Poly p;
    p.verts.reserve(hull.vertices.size());
    for (const Vec3& v : hull.vertices) p.verts.push_back(pose.position + math::rotate(pose.rotation, v));
    for (const HullFace& f : hull.faces) {
        HullFace face;
        face.verts = f.verts;
        face.normal = math::rotate(pose.rotation, f.normal);
        face.offset = math::dot(face.normal, p.verts[face.verts[0]]);
        p.faces.push_back(std::move(face));
    }
    p.edges = hull.edges;
    p.center = average(p.verts);
    return p;
}

Poly poly_from_triangle(const Vec3 world[3]) {
    Poly p;
    p.verts = {world[0], world[1], world[2]};
    const Vec3 n = math::normalize(math::cross(world[1] - world[0], world[2] - world[0]));
    HullFace front;
    front.verts = {0, 1, 2};
    front.normal = n;
    front.offset = math::dot(n, world[0]);
    HullFace back;
    back.verts = {2, 1, 0};
    back.normal = -n;
    back.offset = -front.offset;
    p.faces = {front, back};
    p.edges = {{0, 1, 0, 1}, {1, 2, 0, 1}, {0, 2, 0, 1}};
    p.center = average(p.verts);
    p.edge_axes = false;
    return p;
}

bool poly_poly(const Poly& a, const Poly& b, Manifold& out) {
    const FaceQuery qa = face_query(a, b);
    if (qa.sep > 0.0f) return false;
    const FaceQuery qb = face_query(b, a);
    if (qb.sep > 0.0f) return false;
    EdgeQuery qe;
    if (a.edge_axes && b.edge_axes) {
        qe = edge_query(a, b);
        if (qe.edge_a >= 0 && qe.sep > 0.0f) return false;
    }
    const float face_sep = std::max(qa.sep, qb.sep);
    if (qe.edge_a >= 0 && (-qe.sep) * 1.05f + 1.0e-3f < -face_sep) {
        const HullEdge& ea = a.edges[static_cast<std::size_t>(qe.edge_a)];
        const HullEdge& eb = b.edges[static_cast<std::size_t>(qe.edge_b)];
        const Vec3 pa = a.verts[ea.a];
        const Vec3 da = a.verts[ea.b] - pa;
        const Vec3 pb = b.verts[eb.a];
        const Vec3 db = b.verts[eb.b] - pb;
        // Closest points of the two edge lines.
        const Vec3 r = pa - pb;
        const float aa = math::dot(da, da);
        const float bb = math::dot(db, db);
        const float ab = math::dot(da, db);
        const float c = math::dot(da, r);
        const float f = math::dot(db, r);
        const float denom = aa * bb - ab * ab;
        const float s = denom > 1.0e-12f ? math::clamp((ab * f - c * bb) / denom, 0.0f, 1.0f) : 0.0f;
        const float t = math::clamp((ab * s + f) / bb, 0.0f, 1.0f);
        const Vec3 qa_pt = pa + da * s;
        const Vec3 qb_pt = pb + db * t;
        out.add({(qa_pt + qb_pt) * 0.5f, qe.axis, -qe.sep});
        return true;
    }
    if (qa.sep + 5.0e-4f >= qb.sep) {
        face_contacts(a, qa.face, b, true, out);
    } else {
        face_contacts(b, qb.face, a, false, out);
    }
    if (out.count == 0) { // fully clipped away: fall back to one contact on the axis
        const bool a_ref = qa.sep + 5.0e-4f >= qb.sep;
        const Poly& ref = a_ref ? a : b;
        const HullFace& f = ref.faces[static_cast<std::size_t>(a_ref ? qa.face : qb.face)];
        out.add({(a.center + b.center) * 0.5f, a_ref ? f.normal : -f.normal, -face_sep});
    }
    return true;
}

namespace {

ConvexCore world_core(const Poly& p) {
    ConvexCore c;
    c.kind = ConvexCore::Kind::Hull;
    c.verts = p.verts.data();
    c.vert_count = static_cast<int>(p.verts.size());
    c.axes[0] = {1, 0, 0};
    c.axes[1] = {0, 1, 0};
    c.axes[2] = {0, 0, 1};
    return c;
}

ConvexCore point_core(const Vec3& pt) {
    ConvexCore c;
    c.kind = ConvexCore::Kind::Point;
    c.center = c.p0 = c.p1 = pt;
    return c;
}

} // namespace

bool sphere_poly(const Vec3& c, float r, const Poly& poly, ContactPoint& out) {
    float max_sd = -1.0e30f;
    int face = 0;
    for (std::size_t i = 0; i < poly.faces.size(); ++i) {
        const float sd = math::dot(poly.faces[i].normal, c) - poly.faces[i].offset;
        if (sd > max_sd) {
            max_sd = sd;
            face = static_cast<int>(i);
        }
    }
    if (max_sd > r) return false;
    const HullFace& f = poly.faces[static_cast<std::size_t>(face)];
    const auto inside = [&] {
        out.normal = -f.normal;
        out.depth = r - max_sd;
        out.point = c - f.normal * max_sd;
        return true;
    };
    if (max_sd <= 0.0f) return inside();
    const CoreDistance d = core_distance(point_core(c), world_core(poly));
    if (d.intersecting) return inside();
    if (d.distance >= r) return false;
    out.normal = math::normalize(d.point_b - d.point_a);
    out.depth = r - d.distance;
    out.point = d.point_b;
    return true;
}

bool capsule_poly(const Vec3& p0, const Vec3& p1, float r, const Poly& poly, Manifold& out) {
    ConvexCore seg;
    seg.kind = ConvexCore::Kind::Segment;
    seg.center = (p0 + p1) * 0.5f;
    seg.p0 = p0;
    seg.p1 = p1;
    const CoreDistance d = core_distance(seg, world_core(poly));
    const Vec3 closest = d.intersecting ? seg.center : d.point_a;
    // Endpoints keep a capsule lying on a face stable; the closest point covers
    // a capsule balanced on an edge or vertex.
    const Vec3 candidates[3] = {p0, p1, closest};
    bool hit = false;
    for (const Vec3& c : candidates) {
        ContactPoint cp;
        if (sphere_poly(c, r, poly, cp)) {
            out.add_deepest(cp);
            hit = true;
        }
    }
    return hit;
}

} // namespace poko::physics::detail
