#include "poko_test_harness.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "graphics_fixtures.h"
#include "poko/graphics/material.h"
#include "poko/renderer/render_device.h"

namespace {
using poko::NullRenderDevice;
using poko::graphics::MaterialDesc;
using poko::graphics::MaterialLibrary;
using poko::graphics::test::make_material;
} // namespace

POKO_TEST(material_creates_pipeline_and_texture_bindings) {
    NullRenderDevice device;
    MaterialLibrary library(device);

    auto handle = library.create(make_material(2));
    POKO_CHECK(handle.ok());
    POKO_CHECK_EQ(library.count(), 1u);

    auto gpu = library.get(handle.value());
    POKO_CHECK(gpu.ok());
    POKO_CHECK(poko::is_valid(gpu.value().pipeline));
    POKO_CHECK_EQ(gpu.value().texture_count, 2u);
    POKO_CHECK(poko::is_valid(gpu.value().textures[0]));
    POKO_CHECK(poko::is_valid(gpu.value().samplers[1]));
    POKO_CHECK(!poko::is_valid(gpu.value().uniform_buffer));
    // vertex + fragment shaders, pipeline, 2 textures, 2 samplers.
    POKO_CHECK_EQ(device.resource_count(), std::size_t{7});
}

POKO_TEST(material_uploads_uniform_data) {
    NullRenderDevice device;
    MaterialLibrary library(device);

    MaterialDesc desc = make_material(0);
    desc.uniform_data = std::vector<std::byte>(16, std::byte{0});
    auto handle = library.create(desc);
    POKO_CHECK(handle.ok());
    auto gpu = library.get(handle.value());
    POKO_CHECK(poko::is_valid(gpu.value().uniform_buffer));
    POKO_CHECK_EQ(device.buffer_update_count(), std::size_t{1});
}

POKO_TEST(invalid_material_is_rejected_without_allocating) {
    NullRenderDevice device;
    MaterialLibrary library(device);

    POKO_CHECK(!library.create(MaterialDesc{}).ok());
    MaterialDesc no_fs = make_material(0);
    no_fs.fragment_shader_source.clear();
    POKO_CHECK(!library.create(no_fs).ok());
    MaterialDesc too_many = make_material(5); // > DrawCall::kMaxDrawTextures
    POKO_CHECK(!library.create(too_many).ok());
    POKO_CHECK_EQ(library.count(), 0u);
    POKO_CHECK_EQ(device.resource_count(), std::size_t{0});
}

POKO_TEST(material_destroy_releases_all_handles) {
    NullRenderDevice device;
    MaterialLibrary library(device);

    auto handle = library.create(make_material(2));
    POKO_CHECK(handle.ok());
    POKO_CHECK(library.destroy(handle.value()).ok());
    POKO_CHECK(!library.destroy(handle.value()).ok());
    POKO_CHECK(!library.get(handle.value()).ok());
    POKO_CHECK_EQ(library.count(), 0u);
    POKO_CHECK_EQ(device.resource_count(), std::size_t{0});
}

POKO_TEST_MAIN()
