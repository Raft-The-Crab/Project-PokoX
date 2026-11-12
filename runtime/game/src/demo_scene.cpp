#include "poko/runtime/demo_scene.h"

#include <array>
#include <cstdint>

#include "poko/core/asset_contracts.h"
#include "poko/core/light_contracts.h"
#include "poko/core/physics_contracts.h"
#include "poko/core/postprocess_contracts.h"
#include "poko/core/scene_contracts.h"

namespace poko::runtime {
namespace {

// A small POD component attached to each demo entity (ECS storage exercise).
struct DemoComponent {
    std::uint32_t node_index = 0;
    float spin = 0.0f;
};

Status fail(const char* message) {
    return Status::fail(ErrorCode::Internal, message);
}

} // namespace

Status build_demo_scene(GameRuntime& runtime) {
    // Camera: orbit around the origin at a fixed pose.
    runtime.camera_rig().set_mode(camera::CameraMode::Orbit);
    runtime.camera_rig().set_target(math::Vec3{0.0f, 0.0f, 0.0f});
    runtime.camera_rig().set_distance(6.0f);
    runtime.camera_rig().orbit(0.6f, 0.15f);

    // Lighting: ambient fill + a sun + one point light.
    AmbientLight ambient;
    ambient.sky = LightColor{0.25f, 0.28f, 0.35f};
    ambient.ground = LightColor{0.10f, 0.10f, 0.12f};
    ambient.intensity = 1.0f;
    runtime.lights().set_ambient(ambient);

    LightDesc sun;
    sun.kind = LightKind::Directional;
    sun.directional.direction = math::Vec3{0.4f, 0.8f, 0.3f};
    sun.directional.color = LightColor{1.0f, 0.96f, 0.88f};
    sun.directional.intensity = 1.2f;
    if (!runtime.lights().add(sun).ok()) {
        return fail("failed to add sun light");
    }

    LightDesc fill;
    fill.kind = LightKind::Point;
    fill.point.position = math::Vec3{2.0f, 2.5f, 2.0f};
    fill.point.color = LightColor{0.4f, 0.6f, 1.0f};
    fill.point.intensity = 0.8f;
    fill.point.radius = 12.0f;
    if (!runtime.lights().add(fill).ok()) {
        return fail("failed to add point light");
    }

    // Post-process: ACES tonemap then bloom, in chain order.
    PostProcessDesc tonemap;
    tonemap.kind = PostProcessKind::ToneMap;
    tonemap.tone_map.operator_ = ToneMap::ACES;
    tonemap.tone_map.exposure = 1.1f;
    if (!runtime.postprocess().add(tonemap).ok()) {
        return fail("failed to add tonemap pass");
    }

    PostProcessDesc bloom;
    bloom.kind = PostProcessKind::Bloom;
    bloom.bloom.threshold = 1.0f;
    bloom.bloom.intensity = 0.4f;
    bloom.bloom.radius = 4.0f;
    if (!runtime.postprocess().add(bloom).ok()) {
        return fail("failed to add bloom pass");
    }

    // Scene hierarchy: one root and three drawable children.
    NodeDesc root;
    root.name = "demo.root";
    auto root_handle = runtime.scene().create_node(root);
    if (!root_handle.ok()) {
        return fail("failed to create scene root");
    }

    const std::array<math::Vec3, 3> offsets = {
        math::Vec3{-2.0f, 0.0f, 0.0f},
        math::Vec3{0.0f, 0.0f, 0.0f},
        math::Vec3{2.0f, 0.0f, 0.0f},
    };
    for (std::uint32_t i = 0; i < 3; ++i) {
        NodeDesc child;
        child.name = "demo.object";
        child.parent = root_handle.value();
        child.local.position = offsets[i];
        if (!runtime.scene().create_node(child).ok()) {
            return fail("failed to create scene child");
        }

        const auto entity = runtime.entities().create_entity();
        runtime.entities().add_component(entity,
                                         DemoComponent{i, 0.2f * static_cast<float>(i + 1)});
    }

    // Physics: one dynamic body that falls onto the flat ground.
    BodyDesc body;
    body.type = BodyType::Dynamic;
    body.shape.kind = ShapeKind::Sphere;
    body.shape.radius = 0.5f;
    body.position = math::Vec3{0.0f, 3.0f, 0.0f};
    body.mass = 1.0f;
    body.debug_name = "demo.body";
    if (!runtime.physics().create_body(body).ok()) {
        return fail("failed to create physics body");
    }

    // Assets: one queued load, pumped by the frame loop.
    AssetDesc asset;
    asset.kind = AssetKind::Mesh;
    asset.logical_path = "models/demo.mesh";
    asset.debug_name = "demo.mesh";
    if (!runtime.assets().request_load(asset).ok()) {
        return fail("failed to queue asset load");
    }

    // Input: hold W so the character walks forward deterministically.
    input::InputEvent forward;
    forward.type = input::EventType::KeyDown;
    forward.key = input::KeyCode::W;
    if (!runtime.input().feed_event(forward).ok()) {
        return fail("failed to feed input event");
    }

    // Render list: the first two items share a sort key so SceneSubmission
    // merges them into one instanced draw; the third stays separate.
    runtime.set_render_items({
        RenderItem{0, 0, 1, 1},
        RenderItem{0, 0, 2, 1},
        RenderItem{0, 0, 1, 2},
    });

    return Status::success();
}

} // namespace poko::runtime
