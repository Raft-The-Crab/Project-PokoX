#include "poko_test_harness.h"

#include <cstddef>
#include <cstdint>

#include "graphics_fixtures.h"
#include "poko/graphics/draw_call.h"
#include "poko/graphics/material.h"
#include "poko/graphics/mesh.h"
#include "poko/renderer/render_device.h"

namespace {
using poko::DrawCall;
using poko::NullRenderDevice;
using poko::graphics::DrawCallBuilder;
using poko::graphics::DrawOptions;
using poko::graphics::MaterialHandle;
using poko::graphics::MaterialLibrary;
using poko::graphics::MeshHandle;
using poko::graphics::MeshLibrary;
using poko::graphics::test::make_material;
using poko::graphics::test::make_mesh;
} // namespace

POKO_TEST(build_produces_a_submittable_draw_call) {
    NullRenderDevice device;
    MeshLibrary meshes(device);
    MaterialLibrary materials(device);
    DrawCallBuilder builder(meshes, materials);

    auto mesh = meshes.create(make_mesh());
    auto material = materials.create(make_material(2));
    POKO_CHECK(mesh.ok() && material.ok());

    DrawOptions options;
    options.instance_count = 3;
    options.sort_key = 42;
    auto result = builder.build(mesh.value(), material.value(), options);
    POKO_CHECK(result.ok());

    const DrawCall& call = result.value();
    POKO_CHECK(::poko::valid(call));
    POKO_CHECK_EQ(call.instance_count, 3u);
    POKO_CHECK_EQ(call.sort_key, 42u);
    POKO_CHECK_EQ(call.vertex_count, 3u);
    POKO_CHECK_EQ(call.index_count, 3u);
    POKO_CHECK_EQ(call.texture_count, 2u);

    POKO_CHECK(device.begin_frame().ok());
    POKO_CHECK(device.draw(call).ok());
    POKO_CHECK_EQ(device.recorded_draws().size(), std::size_t{1});
    POKO_CHECK(device.end_frame().ok());
}

POKO_TEST(build_rejects_unknown_handles) {
    NullRenderDevice device;
    MeshLibrary meshes(device);
    MaterialLibrary materials(device);
    DrawCallBuilder builder(meshes, materials);

    auto material = materials.create(make_material(0));
    POKO_CHECK(material.ok());
    POKO_CHECK(!builder.build(MeshHandle{}, material.value()).ok());
    POKO_CHECK(!builder.build(poko::make_handle(0), MaterialHandle{}).ok());
}

POKO_TEST(build_rejects_layout_mismatch) {
    NullRenderDevice device;
    MeshLibrary meshes(device);
    MaterialLibrary materials(device);
    DrawCallBuilder builder(meshes, materials);

    auto mesh = meshes.create(make_mesh());
    auto desc = make_material(0);
    desc.layout.topology = poko::PrimitiveTopology::TriangleStrip; // differs from mesh
    auto material = materials.create(desc);
    POKO_CHECK(mesh.ok() && material.ok());
    POKO_CHECK(!builder.build(mesh.value(), material.value()).ok());
    POKO_CHECK_EQ(materials.count(), 1u);
}

POKO_TEST(build_rejects_zero_instances) {
    NullRenderDevice device;
    MeshLibrary meshes(device);
    MaterialLibrary materials(device);
    DrawCallBuilder builder(meshes, materials);

    auto mesh = meshes.create(make_mesh());
    auto material = materials.create(make_material(0));
    POKO_CHECK(mesh.ok() && material.ok());
    DrawOptions options;
    options.instance_count = 0;
    POKO_CHECK(!builder.build(mesh.value(), material.value(), options).ok());
}

POKO_TEST_MAIN()
