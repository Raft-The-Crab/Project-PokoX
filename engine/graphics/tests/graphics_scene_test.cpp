#include "poko_test_harness.h"

#include <cmath>
#include <cstring>

#include "poko/graphics/primitives.h"
#include "poko/graphics/scene_renderer.h"
#include "poko/renderer/camera.h"
#include "poko/renderer/render_device.h"
#include "poko/renderer/software_device.h"

namespace {
using namespace poko;
using namespace poko::graphics;

struct Tri {
    math::Vec3 a, b, c;
};

std::vector<Tri> triangles(const MeshDesc& mesh) {
    std::vector<Tri> out;
    const auto* v = reinterpret_cast<const float*>(mesh.vertices.data());
    const auto* idx = reinterpret_cast<const std::uint32_t*>(mesh.indices.data());
    const auto count = mesh.indices.size() / 4;
    const auto vec = [&](std::uint32_t i) { return math::Vec3{v[i * 6], v[i * 6 + 1], v[i * 6 + 2]}; };
    for (std::size_t i = 0; i + 2 < count; i += 3) out.push_back({vec(idx[i]), vec(idx[i + 1]), vec(idx[i + 2])});
    return out;
}

// Every non-degenerate triangle must wind counter-clockwise seen from outside.
bool winds_outward(const MeshDesc& mesh) {
    for (const auto& t : triangles(mesh)) {
        const auto n = math::cross(t.b - t.a, t.c - t.a);
        if (math::length(n) < 1e-5f) continue;
        const auto centroid = (t.a + t.b + t.c) / 3.0f;
        if (math::dot(n, centroid) <= 0.0f) return false;
    }
    return true;
}

std::uint32_t red(std::uint32_t p) { return p & 0xFF; }

Camera look_at(const math::Vec3& eye, const math::Vec3& target) {
    Camera camera;
    camera.set_position(eye);
    camera.set_target(target);
    camera.set_perspective(1.0f, 1.0f, 0.1f, 100.0f);
    return camera;
}
} // namespace

POKO_TEST(primitive_meshes_are_valid_and_wind_outward) {
    const MeshDesc cube = make_cube_mesh(1.0f);
    POKO_CHECK(valid(cube));
    POKO_CHECK_EQ(cube.vertices.size(), 24u * 24u);
    POKO_CHECK_EQ(cube.indices.size(), 36u * 4u);
    POKO_CHECK(winds_outward(cube));

    const MeshDesc sphere = make_sphere_mesh(2.0f);
    POKO_CHECK(valid(sphere));
    POKO_CHECK(winds_outward(sphere));
    const MeshDesc capsule = make_capsule_mesh(0.5f, 1.0f, 16, 6);
    POKO_CHECK(valid(capsule));
    POKO_CHECK(winds_outward(capsule));

    // Capsule tips reach radius + half_height.
    float max_y = 0.0f;
    const auto* v = reinterpret_cast<const float*>(capsule.vertices.data());
    for (std::size_t i = 0; i < capsule.vertices.size() / 24; ++i) max_y = std::max(max_y, v[i * 6 + 1]);
    POKO_CHECK(std::abs(max_y - 1.5f) < 1e-4f);

    const MeshDesc plane = make_plane_mesh(3.0f);
    POKO_CHECK(valid(plane));
    for (const auto& t : triangles(plane)) {
        POKO_CHECK(math::cross(t.b - t.a, t.c - t.a).y > 0.0f);
    }
}

POKO_TEST(model_matrix_applies_scale_rotation_translation) {
    const auto q = math::quat_from_axis_angle({0, 1, 0}, math::kHalfPi);
    const auto m = make_model_matrix({1, 2, 3}, q, {2, 1, 1});
    const auto p = math::transform_point(m, {1, 0, 0}); // x scaled to 2, rotated to -z
    POKO_CHECK(std::abs(p.x - 1.0f) < 1e-5f);
    POKO_CHECK(std::abs(p.y - 2.0f) < 1e-5f);
    POKO_CHECK(std::abs(p.z - 1.0f) < 1e-5f);
}

POKO_TEST(scene_renderer_draws_lit_cube_on_software_device) {
    SoftwareRenderDevice device(96, 96);
    SceneRenderer renderer(device, 8);
    POKO_CHECK(!renderer.draw(0, math::Mat4::identity(), {1, 1, 1}).ok()); // not initialized
    POKO_CHECK(renderer.initialize().ok());
    const auto cube = renderer.add_mesh(make_cube_mesh(0.5f));
    POKO_CHECK(cube.ok());
    POKO_CHECK(!renderer.draw(99, math::Mat4::identity(), {1, 1, 1}).ok());

    const auto camera = look_at({0, 0, 3}, {0, 0, 0});
    // Light from the camera side and slightly above: the front face is bright.
    renderer.begin(camera.view_projection(), {0.0f, 0.3f, 1.0f}, 0.1f);
    POKO_CHECK(device.begin_frame().ok());
    POKO_CHECK(device.clear(ClearValue{0, 0, 0, 1}).ok());
    POKO_CHECK(renderer.draw(cube.value(), math::Mat4::identity(), {1, 0, 0}).ok());
    POKO_CHECK(device.end_frame().ok());

    POKO_CHECK_EQ(renderer.draws_issued(), 1u);
    POKO_CHECK_EQ(device.triangles_drawn(), 2u); // only the front face survives culling
    const auto centre = red(device.pixel(48, 48));
    POKO_CHECK(centre > 200);
    POKO_CHECK(device.pixel(2, 2) == 0xFF000000u);
}

POKO_TEST(scene_renderer_light_follows_object_rotation_and_budget_is_enforced) {
    SoftwareRenderDevice device(64, 64);
    SceneRenderer renderer(device, 2);
    POKO_CHECK(renderer.initialize().ok());
    const auto plane = renderer.add_mesh(make_plane_mesh(1.0f)).value();
    const auto camera = look_at({0, 0, 3}, {0, 0, 0});

    const auto shade = [&](const math::Quat& rotation) {
        renderer.begin(camera.view_projection(), {0, 0, 1}, 0.0f);
        POKO_CHECK(device.begin_frame().ok());
        POKO_CHECK(device.clear(ClearValue{0, 0, 0, 1}).ok());
        POKO_CHECK(renderer.draw(plane, make_model_matrix({0, 0, 0}, rotation), {1, 1, 1}).ok());
        POKO_CHECK(device.end_frame().ok());
        return red(device.pixel(32, 32));
    };
    const auto face_camera = math::quat_from_axis_angle({1, 0, 0}, math::kHalfPi);
    const auto flat = shade(face_camera);
    // Turn the plane 60 degrees away: object-space light dims by cos(60) = 0.5.
    const auto tilted = shade(math::multiply(math::quat_from_axis_angle({0, 1, 0}, math::kPi / 3.0f), face_camera));
    POKO_CHECK(flat > 250);
    POKO_CHECK(tilted > 110 && tilted < 145);

    renderer.begin(camera.view_projection(), {0, 1, 0});
    POKO_CHECK(device.begin_frame().ok());
    POKO_CHECK(renderer.draw(plane, math::Mat4::identity(), {1, 1, 1}).ok());
    POKO_CHECK(renderer.draw(plane, math::Mat4::identity(), {1, 1, 1}).ok());
    POKO_CHECK(!renderer.draw(plane, math::Mat4::identity(), {1, 1, 1}).ok());
}

POKO_TEST(scene_renderer_works_on_the_mock_device_too) {
    NullRenderDevice device;
    SceneRenderer renderer(device, 4);
    POKO_CHECK(renderer.initialize().ok());
    const auto cube = renderer.add_mesh(make_cube_mesh()).value();
    renderer.begin(math::Mat4::identity(), {0, 1, 0});
    POKO_CHECK(device.begin_frame().ok());
    POKO_CHECK(renderer.draw(cube, math::Mat4::identity(), {1, 1, 1}).ok());
    POKO_CHECK_EQ(device.recorded_draws().size(), 1u);
    POKO_CHECK(device.recorded_draws()[0].uniform_offset == 0);
}

POKO_TEST_MAIN()
