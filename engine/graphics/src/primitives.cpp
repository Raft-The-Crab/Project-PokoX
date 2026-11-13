#include "poko/graphics/primitives.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace poko::graphics {
namespace {

using math::Vec3;

struct Builder {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;

    void vertex(const Vec3& p, const Vec3& n) {
        vertices.insert(vertices.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
    }
    std::uint32_t count() const { return static_cast<std::uint32_t>(vertices.size() / 6); }

    MeshDesc finish() const {
        MeshDesc desc;
        desc.vertices.resize(vertices.size() * sizeof(float));
        std::memcpy(desc.vertices.data(), vertices.data(), desc.vertices.size());
        desc.indices.resize(indices.size() * sizeof(std::uint32_t));
        std::memcpy(desc.indices.data(), indices.data(), desc.indices.size());
        desc.index_type = IndexType::UInt32;
        desc.layout.attribute_count = 2;
        desc.layout.attributes[0] = {0, 3, 24, 0};
        desc.layout.attributes[1] = {1, 3, 24, 12};
        return desc;
    }
};

// One outward-facing square: u x v == n so the winding is counter-clockwise.
void add_face(Builder& b, const Vec3& n, const Vec3& u, const Vec3& v, float h,
              float lift = 1.0f) {
    const auto base = b.count();
    const Vec3 c = n * (h * lift); // lift 0 centres the square on the origin
    b.vertex(c - u * h - v * h, n);
    b.vertex(c + u * h - v * h, n);
    b.vertex(c + u * h + v * h, n);
    b.vertex(c - u * h + v * h, n);
    b.indices.insert(b.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

} // namespace

MeshDesc make_cube_mesh(float h) {
    Builder b;
    add_face(b, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, h);
    add_face(b, {-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, h);
    add_face(b, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}, h);
    add_face(b, {0, -1, 0}, {1, 0, 0}, {0, 0, 1}, h);
    add_face(b, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, h);
    add_face(b, {0, 0, -1}, {0, 1, 0}, {1, 0, 0}, h);
    return b.finish();
}

MeshDesc make_plane_mesh(float h) {
    Builder b;
    add_face(b, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}, h, 0.0f);
    return b.finish();
}

MeshDesc make_capsule_mesh(float radius, float half_height, std::uint32_t segments,
                           std::uint32_t rings) {
    segments = std::max(segments, 3u);
    rings = std::max(rings, 2u);
    Builder b;
    // Top hemisphere rows, then bottom hemisphere rows; the two middle rows
    // form the cylinder section (zero height for a sphere).
    const std::uint32_t rows_per_half = rings / 2 + 1;
    const std::uint32_t rows = rows_per_half * 2;
    for (std::uint32_t row = 0; row < rows; ++row) {
        const bool top = row < rows_per_half;
        const std::uint32_t k = top ? row : row - rows_per_half;
        const float quarter = static_cast<float>(rows_per_half - 1);
        const float theta = (top ? 0.0f : math::kHalfPi) +
                            (quarter > 0.0f ? static_cast<float>(k) / quarter : 0.0f) * math::kHalfPi;
        for (std::uint32_t s = 0; s <= segments; ++s) {
            const float phi = static_cast<float>(s) / static_cast<float>(segments) * math::kTau;
            const Vec3 n{std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
            b.vertex(n * radius + Vec3{0.0f, top ? half_height : -half_height, 0.0f}, n);
        }
    }
    const std::uint32_t stride = segments + 1;
    for (std::uint32_t row = 0; row + 1 < rows; ++row) {
        for (std::uint32_t s = 0; s < segments; ++s) {
            const std::uint32_t a = row * stride + s;
            const std::uint32_t c = a + stride;
            b.indices.insert(b.indices.end(), {a, a + 1, c, a + 1, c + 1, c});
        }
    }
    return b.finish();
}

MeshDesc make_sphere_mesh(float radius, std::uint32_t segments, std::uint32_t rings) {
    return make_capsule_mesh(radius, 0.0f, segments, rings);
}

math::Mat4 make_model_matrix(const Vec3& position, const math::Quat& rotation,
                             const Vec3& scale) noexcept {
    const Vec3 x = math::rotate(rotation, Vec3{1, 0, 0}) * scale.x;
    const Vec3 y = math::rotate(rotation, Vec3{0, 1, 0}) * scale.y;
    const Vec3 z = math::rotate(rotation, Vec3{0, 0, 1}) * scale.z;
    math::Mat4 m = math::Mat4::identity();
    m.at(0, 0) = x.x; m.at(1, 0) = x.y; m.at(2, 0) = x.z;
    m.at(0, 1) = y.x; m.at(1, 1) = y.y; m.at(2, 1) = y.z;
    m.at(0, 2) = z.x; m.at(1, 2) = z.y; m.at(2, 2) = z.z;
    m.at(0, 3) = position.x; m.at(1, 3) = position.y; m.at(2, 3) = position.z;
    return m;
}

} // namespace poko::graphics
