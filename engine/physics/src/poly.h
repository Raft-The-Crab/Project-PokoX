#pragma once

#include <vector>

#include "gjk.h"
#include "narrowphase.h"

namespace poko::physics::detail {

// A convex polyhedron in world space: vertices, polygon faces (CCW from outside)
// and edges with their two adjacent faces. Boxes, hulls and single mesh
// triangles (two back-to-back faces) all reduce to this for SAT queries.
struct Poly {
    std::vector<math::Vec3> verts;
    std::vector<HullFace> faces;
    std::vector<HullEdge> edges;
    math::Vec3 center{};  // inside the polyhedron (vertex average)
    bool edge_axes = true; // false for mesh triangles: skip edge-edge axes
};

Poly poly_from_box(const math::Vec3& half, const Pose& pose);
Poly poly_from_hull(const Geometry& hull, const Pose& pose);
Poly poly_from_triangle(const math::Vec3 world[3]);

// SAT with reference-face clipping. Normals point from A toward B.
bool poly_poly(const Poly& a, const Poly& b, Manifold& out);

// Sphere centre `c` radius `r` against a polyhedron; normal points sphere -> poly.
bool sphere_poly(const math::Vec3& c, float r, const Poly& poly, ContactPoint& out);

// Capsule (segment p0-p1, radius r) against a polyhedron; normals point capsule -> poly.
bool capsule_poly(const math::Vec3& p0, const math::Vec3& p1, float r, const Poly& poly,
                  Manifold& out);

} // namespace poko::physics::detail
