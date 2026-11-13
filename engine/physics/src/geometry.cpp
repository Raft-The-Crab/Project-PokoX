#include "geometry.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace poko::physics::detail {
namespace {

using math::Vec3;

struct Tri {
    int a, b, c;
    Vec3 n;
    float d;
    bool alive = true;
};

Vec3 pick(const std::vector<Vec3>& p, int i) {
    return p[static_cast<std::size_t>(i)];
}

bool finite(const Vec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

Vec3 abs_max(const std::vector<Vec3>& v) {
    Vec3 m{};
    for (const Vec3& p : v) {
        m.x = std::max(m.x, std::abs(p.x));
        m.y = std::max(m.y, std::abs(p.y));
        m.z = std::max(m.z, std::abs(p.z));
    }
    return m;
}

float max_radius(const std::vector<Vec3>& v) {
    float r = 0.0f;
    for (const Vec3& p : v) r = std::max(r, math::length(p));
    return r;
}

} // namespace

Result<std::shared_ptr<Geometry>> build_hull(std::span<const Vec3> input) {
    using R = Result<std::shared_ptr<Geometry>>;
    if (input.size() < 4 || input.size() > kMaxHullPoints) {
        return R::fail(ErrorCode::InvalidArgument, "a hull needs 4 to 256 points");
    }
    float scale = 0.0f;
    for (const Vec3& p : input) {
        if (!finite(p)) return R::fail(ErrorCode::InvalidArgument, "non-finite hull point");
        scale = std::max({scale, std::abs(p.x), std::abs(p.y), std::abs(p.z)});
    }
    if (scale <= 0.0f) return R::fail(ErrorCode::InvalidArgument, "degenerate hull");
    const float eps = 1.0e-5f * scale;

    std::vector<Vec3> pts;
    for (const Vec3& p : input) {
        bool dup = false;
        for (const Vec3& q : pts) {
            if (math::length_squared(p - q) < (1.0e-6f * scale) * (1.0e-6f * scale)) {
                dup = true;
                break;
            }
        }
        if (!dup) pts.push_back(p);
    }
    if (pts.size() < 4) return R::fail(ErrorCode::InvalidArgument, "degenerate hull");
    const int n = static_cast<int>(pts.size());

    // Initial tetrahedron from extreme points.
    int i0 = 0;
    for (int i = 1; i < n; ++i) {
        if (pick(pts, i).x < pick(pts, i0).x) i0 = i;
    }
    int i1 = i0;
    float best = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float d = math::length_squared(pick(pts, i) - pick(pts, i0));
        if (d > best) { best = d; i1 = i; }
    }
    const Vec3 axis = math::normalize(pick(pts, i1) - pick(pts, i0));
    int i2 = i0;
    best = 0.0f;
    for (int i = 0; i < n; ++i) {
        const Vec3 rel = pick(pts, i) - pick(pts, i0);
        const float d = math::length_squared(rel - axis * math::dot(rel, axis));
        if (d > best) { best = d; i2 = i; }
    }
    if (best < (10.0f * eps) * (10.0f * eps)) return R::fail(ErrorCode::InvalidArgument, "hull points are collinear");
    const Vec3 plane_n = math::normalize(math::cross(pick(pts, i1) - pick(pts, i0), pick(pts, i2) - pick(pts, i0)));
    int i3 = i0;
    best = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float d = std::abs(math::dot(pick(pts, i) - pick(pts, i0), plane_n));
        if (d > best) { best = d; i3 = i; }
    }
    if (best < 10.0f * eps) return R::fail(ErrorCode::InvalidArgument, "hull points are coplanar");

    std::vector<Tri> tris;
    const auto add_tri = [&](int a, int b, int c) {
        Tri t{a, b, c, {}, 0.0f, true};
        const Vec3 cr = math::cross(pick(pts, b) - pick(pts, a), pick(pts, c) - pick(pts, a));
        const float len = math::length(cr);
        if (len < 1.0e-12f * scale * scale) return false; // sliver: skipped, validated below
        t.n = cr / len;
        t.d = math::dot(t.n, pick(pts, a));
        tris.push_back(t);
        return true;
    };
    const Vec3 centroid = (pick(pts, i0) + pick(pts, i1) + pick(pts, i2) + pick(pts, i3)) * 0.25f;
    const auto add_oriented = [&](int a, int b, int c) {
        const Vec3 cr = math::cross(pick(pts, b) - pick(pts, a), pick(pts, c) - pick(pts, a));
        if (math::dot(cr, centroid - pick(pts, a)) > 0.0f) std::swap(b, c);
        add_tri(a, b, c);
    };
    add_oriented(i0, i1, i2);
    add_oriented(i0, i1, i3);
    add_oriented(i0, i2, i3);
    add_oriented(i1, i2, i3);

    for (int p = 0; p < n; ++p) {
        if (p == i0 || p == i1 || p == i2 || p == i3) continue;
        const Vec3 point = pick(pts, p);
        std::vector<std::size_t> visible;
        for (std::size_t t = 0; t < tris.size(); ++t) {
            if (tris[t].alive && math::dot(tris[t].n, point) - tris[t].d > eps) visible.push_back(t);
        }
        if (visible.empty()) continue;
        std::map<std::pair<int, int>, int> directed;
        for (const std::size_t t : visible) {
            const Tri& tri = tris[t];
            directed[{tri.a, tri.b}] = 1;
            directed[{tri.b, tri.c}] = 1;
            directed[{tri.c, tri.a}] = 1;
        }
        std::vector<std::pair<int, int>> horizon;
        for (const auto& [edge, unused] : directed) {
            (void)unused;
            if (!directed.contains({edge.second, edge.first})) horizon.push_back(edge);
        }
        for (const std::size_t t : visible) tris[t].alive = false;
        for (const auto& [u, v] : horizon) add_tri(u, v, p);
    }

    // The surface must be closed: every directed edge needs its reverse.
    std::map<std::pair<int, int>, std::size_t> edge_tri;
    for (std::size_t t = 0; t < tris.size(); ++t) {
        if (!tris[t].alive) continue;
        edge_tri[{tris[t].a, tris[t].b}] = t;
        edge_tri[{tris[t].b, tris[t].c}] = t;
        edge_tri[{tris[t].c, tris[t].a}] = t;
    }
    for (const auto& [edge, unused] : edge_tri) {
        (void)unused;
        if (!edge_tri.contains({edge.second, edge.first})) {
            return R::fail(ErrorCode::InvalidArgument, "hull is numerically degenerate");
        }
    }

    // Merge coplanar triangles into polygon faces.
    std::vector<std::size_t> group(tris.size());
    std::iota(group.begin(), group.end(), std::size_t{0});
    const auto root = [&](std::size_t x) {
        while (group[x] != x) { group[x] = group[group[x]]; x = group[x]; }
        return x;
    };
    for (const auto& [edge, t] : edge_tri) {
        const std::size_t o = edge_tri[{edge.second, edge.first}];
        if (math::dot(tris[t].n, tris[o].n) > 1.0f - 1.0e-5f &&
            std::abs(tris[t].d - tris[o].d) < 1.0e-4f * scale) {
            group[root(t)] = root(o);
        }
    }
    std::map<std::size_t, std::vector<std::size_t>> by_group;
    for (std::size_t t = 0; t < tris.size(); ++t) {
        if (tris[t].alive) by_group[root(t)].push_back(t);
    }

    auto geo = std::make_shared<Geometry>();
    geo->kind = Geometry::Kind::Hull;
    std::map<int, std::uint16_t> remap;
    const auto vertex_index = [&](int point) {
        const auto it = remap.find(point);
        if (it != remap.end()) return it->second;
        const auto idx = static_cast<std::uint16_t>(geo->vertices.size());
        remap[point] = idx;
        geo->vertices.push_back(pick(pts, point));
        return idx;
    };
    std::map<std::pair<std::uint16_t, std::uint16_t>, std::uint16_t> directed_face;
    for (const auto& [unused_root, members] : by_group) {
        (void)unused_root;
        std::map<int, int> next;
        std::size_t boundary = 0;
        for (const std::size_t t : members) {
            const int e[3][2] = {{tris[t].a, tris[t].b}, {tris[t].b, tris[t].c}, {tris[t].c, tris[t].a}};
            for (const auto& uv : e) {
                const std::size_t other = edge_tri[{uv[1], uv[0]}];
                if (root(other) != root(t)) {
                    next[uv[0]] = uv[1];
                    ++boundary;
                }
            }
        }
        if (boundary < 3 || next.size() != boundary) {
            return R::fail(ErrorCode::InvalidArgument, "hull face is not a simple polygon");
        }
        HullFace face;
        const int start = next.begin()->first;
        int cur = start;
        do {
            face.verts.push_back(vertex_index(cur));
            const auto it = next.find(cur);
            if (it == next.end()) return R::fail(ErrorCode::InvalidArgument, "hull face loop is open");
            cur = it->second;
        } while (cur != start && face.verts.size() <= boundary);
        if (face.verts.size() != boundary) {
            return R::fail(ErrorCode::InvalidArgument, "hull face is not a simple polygon");
        }
        // Newell normal.
        Vec3 normal{};
        for (std::size_t i = 0; i < face.verts.size(); ++i) {
            const Vec3& p = geo->vertices[face.verts[i]];
            const Vec3& q = geo->vertices[face.verts[(i + 1) % face.verts.size()]];
            normal = normal + math::cross(p, q);
        }
        face.normal = math::normalize(normal);
        float offset = 0.0f;
        for (const auto v : face.verts) offset += math::dot(face.normal, geo->vertices[v]);
        face.offset = offset / static_cast<float>(face.verts.size());
        const auto face_index = static_cast<std::uint16_t>(geo->faces.size());
        for (std::size_t i = 0; i < face.verts.size(); ++i) {
            directed_face[{face.verts[i], face.verts[(i + 1) % face.verts.size()]}] = face_index;
        }
        geo->faces.push_back(std::move(face));
    }
    for (const auto& [edge, fa] : directed_face) {
        if (edge.first >= edge.second) continue;
        const auto twin = directed_face.find({edge.second, edge.first});
        if (twin == directed_face.end()) return R::fail(ErrorCode::InvalidArgument, "hull is not closed");
        geo->edges.push_back({edge.first, edge.second, fa, twin->second});
    }

    // Mass properties about the origin by tetrahedron decomposition.
    float volume = 0.0f;
    float sxx = 0.0f;
    float syy = 0.0f;
    float szz = 0.0f;
    for (const HullFace& f : geo->faces) {
        const Vec3& a = geo->vertices[f.verts[0]];
        for (std::size_t i = 1; i + 1 < f.verts.size(); ++i) {
            const Vec3& b = geo->vertices[f.verts[i]];
            const Vec3& c = geo->vertices[f.verts[i + 1]];
            const float v = math::dot(a, math::cross(b, c)) / 6.0f;
            volume += v;
            const auto moment = [&](float xa, float xb, float xc) {
                return v / 20.0f * (xa * xa + xb * xb + xc * xc + (xa + xb + xc) * (xa + xb + xc));
            };
            sxx += moment(a.x, b.x, c.x);
            syy += moment(a.y, b.y, c.y);
            szz += moment(a.z, b.z, c.z);
        }
    }
    if (!(volume > 1.0e-12f * scale * scale * scale)) {
        return R::fail(ErrorCode::InvalidArgument, "hull has no volume");
    }
    geo->inertia_per_mass = {(syy + szz) / volume, (sxx + szz) / volume, (sxx + syy) / volume};
    geo->half = abs_max(geo->vertices);
    geo->radius = max_radius(geo->vertices);
    float nearest = 1.0e30f;
    for (const HullFace& f : geo->faces) nearest = std::min(nearest, f.offset);
    geo->inner = std::max(nearest, 0.02f * geo->radius);
    return R::success(std::move(geo));
}

namespace {

struct Bounds {
    Vec3 lo{1.0e30f, 1.0e30f, 1.0e30f};
    Vec3 hi{-1.0e30f, -1.0e30f, -1.0e30f};
    void grow(const Vec3& p) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
};

std::int32_t build_node(Geometry& g, std::vector<Bounds>& tri_bounds, std::vector<Vec3>& centres,
                        std::uint32_t first, std::uint32_t count) {
    const auto index = static_cast<std::int32_t>(g.nodes.size());
    g.nodes.push_back({});
    Bounds b;
    Bounds c;
    for (std::uint32_t i = first; i < first + count; ++i) {
        const auto t = g.tri_order[i];
        b.grow(tri_bounds[t].lo);
        b.grow(tri_bounds[t].hi);
        c.grow(centres[t]);
    }
    BvhNode node;
    node.lo = b.lo;
    node.hi = b.hi;
    if (count <= 4) {
        node.first = first;
        node.count = count;
        g.nodes[static_cast<std::size_t>(index)] = node;
        return index;
    }
    const Vec3 extent = c.hi - c.lo;
    const int axis = extent.x >= extent.y && extent.x >= extent.z ? 0 : (extent.y >= extent.z ? 1 : 2);
    const auto key = [&](std::uint32_t t) {
        return axis == 0 ? centres[t].x : (axis == 1 ? centres[t].y : centres[t].z);
    };
    std::sort(g.tri_order.begin() + first, g.tri_order.begin() + first + count,
              [&](std::uint32_t l, std::uint32_t r) {
                  return key(l) != key(r) ? key(l) < key(r) : l < r;
              });
    const std::uint32_t half = count / 2;
    const auto left = build_node(g, tri_bounds, centres, first, half);
    const auto right = build_node(g, tri_bounds, centres, first + half, count - half);
    node.left = left;
    node.right = right;
    g.nodes[static_cast<std::size_t>(index)] = node;
    return index;
}

} // namespace

Result<std::shared_ptr<Geometry>> build_mesh(std::span<const Vec3> vertices,
                                             std::span<const std::uint32_t> indices) {
    using R = Result<std::shared_ptr<Geometry>>;
    if (vertices.size() < 3 || indices.size() < 3 || indices.size() % 3 != 0) {
        return R::fail(ErrorCode::InvalidArgument, "a mesh needs vertices and index triples");
    }
    if (indices.size() / 3 > kMaxMeshTriangles || vertices.size() > 4 * kMaxMeshTriangles) {
        return R::fail(ErrorCode::InvalidArgument, "mesh is too large");
    }
    for (const Vec3& v : vertices) {
        if (!finite(v)) return R::fail(ErrorCode::InvalidArgument, "non-finite mesh vertex");
    }
    auto geo = std::make_shared<Geometry>();
    geo->kind = Geometry::Kind::Mesh;
    geo->vertices.assign(vertices.begin(), vertices.end());
    for (std::size_t i = 0; i < indices.size(); i += 3) {
        const auto a = indices[i];
        const auto b = indices[i + 1];
        const auto c = indices[i + 2];
        if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) {
            return R::fail(ErrorCode::OutOfRange, "mesh index out of range");
        }
        const Vec3 cr = math::cross(vertices[b] - vertices[a], vertices[c] - vertices[a]);
        if (math::length_squared(cr) < 1.0e-14f) continue; // degenerate triangle
        geo->indices.insert(geo->indices.end(), {a, b, c});
    }
    const auto count = static_cast<std::uint32_t>(geo->indices.size() / 3);
    if (count == 0) return R::fail(ErrorCode::InvalidArgument, "mesh has no valid triangles");

    std::vector<Bounds> tri_bounds(count);
    std::vector<Vec3> centres(count);
    for (std::uint32_t t = 0; t < count; ++t) {
        Vec3 tri[3];
        mesh_triangle(*geo, t, tri);
        for (const Vec3& p : tri) tri_bounds[t].grow(p);
        centres[t] = (tri[0] + tri[1] + tri[2]) / 3.0f;
    }
    geo->tri_order.resize(count);
    std::iota(geo->tri_order.begin(), geo->tri_order.end(), 0u);
    build_node(*geo, tri_bounds, centres, 0, count);
    geo->half = abs_max(geo->vertices);
    geo->radius = max_radius(geo->vertices);
    geo->inner = 0.0f;
    return R::success(std::move(geo));
}

void mesh_query(const Geometry& mesh, const Vec3& lo, const Vec3& hi, std::vector<std::uint32_t>& out) {
    if (mesh.nodes.empty()) return;
    std::int32_t stack[64];
    int top = 0;
    stack[top++] = 0;
    while (top > 0) {
        const BvhNode& node = mesh.nodes[static_cast<std::size_t>(stack[--top])];
        if (node.lo.x > hi.x || node.hi.x < lo.x || node.lo.y > hi.y || node.hi.y < lo.y ||
            node.lo.z > hi.z || node.hi.z < lo.z) {
            continue;
        }
        if (node.left < 0) {
            for (std::uint32_t i = node.first; i < node.first + node.count; ++i) {
                const auto t = mesh.tri_order[i];
                Vec3 tri[3];
                mesh_triangle(mesh, t, tri);
                Vec3 tlo = tri[0];
                Vec3 thi = tri[0];
                for (const Vec3& p : tri) {
                    tlo = {std::min(tlo.x, p.x), std::min(tlo.y, p.y), std::min(tlo.z, p.z)};
                    thi = {std::max(thi.x, p.x), std::max(thi.y, p.y), std::max(thi.z, p.z)};
                }
                if (tlo.x <= hi.x && thi.x >= lo.x && tlo.y <= hi.y && thi.y >= lo.y &&
                    tlo.z <= hi.z && thi.z >= lo.z) {
                    out.push_back(t);
                }
            }
        } else if (top + 2 <= 64) {
            stack[top++] = node.left;
            stack[top++] = node.right;
        }
    }
}

} // namespace poko::physics::detail
