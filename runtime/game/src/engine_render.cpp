#include "poko/runtime/engine.h"

#include <memory>

#include "engine_impl.h"

namespace poko::runtime {

Status Engine::Impl::upload_resources() {
    meshes = std::make_unique<graphics::MeshLibrary>(renderer->device());
    materials = std::make_unique<graphics::MaterialLibrary>(renderer->device());
    draw_builder = std::make_unique<graphics::DrawCallBuilder>(*meshes, *materials);

    // A failed upload is recorded but does not abort initialization; the render
    // step skips the affected draw items and the frame loop keeps running.
    const auto mesh = meshes->create(detail::make_demo_mesh());
    if (mesh.ok()) {
        mesh_handles.push_back(mesh.value());
    } else {
        ++resource_failures;
    }

    const auto material =
        materials->create(detail::make_demo_material(desc.inject_resource_failure));
    if (material.ok()) {
        material_handles.push_back(material.value());
    } else {
        ++resource_failures;
    }

    renderer->camera().set_perspective(math::kHalfPi, 16.0f / 9.0f, 0.1f, 100.0f);
    return Status::success();
}

Status Engine::Impl::render_frame(const FrameContext& ctx) {
    api::IRenderDevice& gpu = renderer->device();

    // Automatic device-loss recovery (systemplan/04 step 10). If recovery
    // fails we degrade by skipping this frame rather than killing simulation.
    if (gpu.device_lost()) {
        const Status recovered = gpu.recover_from_device_loss();
        if (!recovered.ok()) {
            ++resource_failures;
            return Status::success();
        }
        ++device_recoveries;
    }

    renderer->camera().set_position(ctx.camera_position);
    renderer->camera().set_target(ctx.camera_target);
    renderer->camera().set_up(ctx.camera_up);

    renderer->submission().clear();
    for (const auto& item : ctx.render_items) {
        if (item.mesh_id >= mesh_handles.size() ||
            item.material_id >= material_handles.size()) {
            ++skipped_draw_items;
            continue;
        }
        graphics::DrawOptions options;
        options.instance_count = item.instance_count;
        options.sort_key = item.sort_key;
        const auto call = draw_builder->build(mesh_handles[item.mesh_id],
                                              material_handles[item.material_id], options);
        if (!call.ok()) {
            ++resource_failures;
            ++skipped_draw_items;
            continue;
        }
        renderer->submission().submit(call.value());
    }
    renderer->submission().batch();

    // A failed GPU frame is counted but non-fatal: the next frame retries, and
    // simulation state is never rolled back by a presentation failure.
    if (!renderer->render_frame(ClearValue{0.08f, 0.09f, 0.12f, 1.0f}).ok()) {
        ++resource_failures;
    }
    return Status::success();
}

} // namespace poko::runtime
