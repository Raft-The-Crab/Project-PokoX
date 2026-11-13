// Software-rendered physics showcase: a falling box stack, spheres and capsules
// on a ground plane, simulated by CustomPhysicsWorld and drawn by SceneRenderer
// on the SoftwareRenderDevice. Writes PPM snapshots; no GPU or window needed.
//
//   poko_physics_showcase [output_dir]
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "poko/graphics/primitives.h"
#include "poko/graphics/scene_renderer.h"
#include "poko/physics/custom_world.h"
#include "poko/renderer/camera.h"
#include "poko/renderer/software_device.h"

namespace {
using namespace poko;

struct Visual {
    BodyHandle body;
    std::uint32_t mesh;
    math::Vec3 color;
};
} // namespace

int main(int argc, char** argv) {
    const std::string out_dir = argc > 1 ? argv[1] : ".";

    SoftwareRenderDevice device(640, 360);
    graphics::SceneRenderer renderer(device, 256);
    if (!renderer.initialize().ok()) {
        std::cerr << "physics showcase: renderer init failed\n";
        return 1;
    }
    const auto cube = renderer.add_mesh(graphics::make_cube_mesh(0.5f)).value();
    const auto ground = renderer.add_mesh(graphics::make_plane_mesh(20.0f)).value();
    const auto sphere = renderer.add_mesh(graphics::make_sphere_mesh(0.5f)).value();
    const auto capsule = renderer.add_mesh(graphics::make_capsule_mesh(0.3f, 0.5f)).value();

    CustomPhysicsWorld world;
    if (!world.initialize(PhysicsWorldDesc{})) return 1;

    std::vector<Visual> visuals;
    const auto add = [&](const BodyDesc& desc, std::uint32_t mesh, math::Vec3 color) {
        visuals.push_back({world.create_body(desc).value(), mesh, color});
    };
    for (int i = 0; i < 5; ++i) {
        BodyDesc box;
        box.shape.kind = ShapeKind::Box;
        box.position = {-2.0f, 0.5f + static_cast<float>(i) * 1.02f, 0.0f};
        box.friction = 0.6f;
        add(box, cube, {0.85f, 0.35f, 0.25f});
    }
    BodyDesc tilted;
    tilted.shape.kind = ShapeKind::Box;
    tilted.shape.half_extents = {0.7f, 0.4f, 0.5f};
    tilted.position = {1.5f, 3.0f, 0.2f};
    tilted.rotation = math::quat_from_axis_angle({0.3f, 0.2f, 1.0f}, 0.7f);
    add(tilted, cube, {0.3f, 0.6f, 0.9f}); // drawn unit-sized, scaled below
    for (int i = 0; i < 4; ++i) {
        BodyDesc ball;
        ball.shape.kind = ShapeKind::Sphere;
        ball.position = {0.3f * static_cast<float>(i), 4.0f + 1.3f * static_cast<float>(i), 0.1f * static_cast<float>(i)};
        ball.restitution = 0.5f;
        add(ball, sphere, {0.95f, 0.8f, 0.2f});
    }
    BodyDesc pill;
    pill.shape.kind = ShapeKind::Capsule;
    pill.shape.radius = 0.3f;
    pill.shape.half_height = 0.5f;
    pill.position = {3.2f, 2.5f, -0.5f};
    pill.rotation = math::quat_from_axis_angle({0, 0, 1}, 1.2f);
    add(pill, capsule, {0.4f, 0.85f, 0.5f});

    Camera camera;
    camera.set_position({1.0f, 4.5f, 11.0f});
    camera.set_target({0.5f, 1.8f, 0.0f});
    camera.set_perspective(0.8f, 640.0f / 360.0f, 0.1f, 100.0f);

    const auto render = [&](const std::string& name) {
        renderer.begin(camera.view_projection(), {0.5f, 1.0f, 0.6f}, 0.25f);
        device.begin_frame();
        device.clear(ClearValue{0.55f, 0.7f, 0.9f, 1.0f});
        renderer.draw(ground, graphics::make_model_matrix({0, 0, 0}, math::quat_identity()), {0.35f, 0.5f, 0.35f});
        for (const auto& v : visuals) {
            const auto state = world.get_body_state(v.body).value();
            math::Vec3 scale{1, 1, 1};
            if (v.mesh == cube && &v == &visuals[5]) scale = {1.4f, 0.8f, 1.0f};
            renderer.draw(v.mesh, graphics::make_model_matrix(state.position, state.rotation, scale), v.color);
        }
        device.end_frame();
        const auto status = device.write_ppm(out_dir + "/" + name);
        if (!status.ok()) std::cerr << "physics showcase: " << status.error().message << '\n';
    };

    render("physics_t0.ppm");
    for (int i = 0; i < 90; ++i) world.step(1.0f / 60.0f);
    render("physics_t1.ppm");
    for (int i = 0; i < 300; ++i) world.step(1.0f / 60.0f);
    render("physics_t2.ppm");

    std::cout << "physics showcase: frames=" << device.frame_count()
              << " triangles=" << device.triangles_drawn()
              << " pixels=" << device.pixels_shaded()
              << " sleeping=" << world.sleeping_body_count() << "/" << world.body_count() << '\n';
    return 0;
}
