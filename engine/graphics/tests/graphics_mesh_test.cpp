#include "poko_test_harness.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "graphics_fixtures.h"
#include "poko/graphics/mesh.h"
#include "poko/renderer/render_device.h"

namespace {
using poko::NullRenderDevice;
using poko::graphics::MeshDesc;
using poko::graphics::MeshLibrary;
using poko::graphics::test::make_mesh;
} // namespace

POKO_TEST(mesh_upload_creates_buffers_and_counts) {
    NullRenderDevice device;
    MeshLibrary library(device);

    auto handle = library.create(make_mesh());
    POKO_CHECK(handle.ok());
    POKO_CHECK_EQ(library.count(), 1u);

    auto gpu = library.get(handle.value());
    POKO_CHECK(gpu.ok());
    POKO_CHECK(poko::is_valid(gpu.value().vertex_buffer));
    POKO_CHECK(poko::is_valid(gpu.value().index_buffer));
    POKO_CHECK_EQ(gpu.value().vertex_count, 3u);
    POKO_CHECK_EQ(gpu.value().index_count, 3u);
    POKO_CHECK(gpu.value().index_type == poko::IndexType::UInt32);
    POKO_CHECK_EQ(device.resource_count(), std::size_t{2});
    POKO_CHECK_EQ(device.buffer_update_count(), std::size_t{2});
}

POKO_TEST(nonindexed_mesh_has_no_index_buffer) {
    NullRenderDevice device;
    MeshLibrary library(device);

    auto handle = library.create(make_mesh(false));
    POKO_CHECK(handle.ok());
    auto gpu = library.get(handle.value());
    POKO_CHECK(!poko::is_valid(gpu.value().index_buffer));
    POKO_CHECK_EQ(gpu.value().index_count, 0u);
    POKO_CHECK_EQ(device.resource_count(), std::size_t{1});
    POKO_CHECK_EQ(device.buffer_update_count(), std::size_t{1});
}

POKO_TEST(invalid_mesh_is_rejected_without_allocating) {
    NullRenderDevice device;
    MeshLibrary library(device);

    POKO_CHECK(!library.create(MeshDesc{}).ok());
    MeshDesc no_stride = make_mesh();
    no_stride.layout.attributes[0].stride_bytes = 0;
    POKO_CHECK(!library.create(no_stride).ok());
    MeshDesc misaligned = make_mesh();
    misaligned.indices = std::vector<std::byte>(13, std::byte{0}); // not a multiple of 4
    POKO_CHECK(!library.create(misaligned).ok());
    POKO_CHECK_EQ(library.count(), 0u);
    POKO_CHECK_EQ(device.resource_count(), std::size_t{0});
}

POKO_TEST(mesh_destroy_releases_and_reuses_slot) {
    NullRenderDevice device;
    MeshLibrary library(device);

    auto first = library.create(make_mesh());
    POKO_CHECK(first.ok());
    POKO_CHECK(library.destroy(first.value()).ok());
    POKO_CHECK(!library.destroy(first.value()).ok()); // stale handle
    POKO_CHECK(!library.get(first.value()).ok());
    POKO_CHECK_EQ(library.count(), 0u);
    POKO_CHECK_EQ(device.resource_count(), std::size_t{0});

    auto second = library.create(make_mesh());
    POKO_CHECK(second.ok());
    POKO_CHECK(!poko::same_handle(first.value(), second.value()));
}

POKO_TEST_MAIN()
