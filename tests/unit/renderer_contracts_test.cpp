#include "poko_test_harness.h"

#include <cstring>

#include "poko_render_contracts.h"

POKO_TEST(backend_names_are_stable) {
    POKO_CHECK(std::strcmp(poko::to_string(poko::GraphicsBackend::Vulkan),
                           "vulkan") == 0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::GraphicsBackend::OpenGL),
                           "opengl") == 0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::GraphicsBackend::Null),
                           "null") == 0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::GraphicsBackend::Unknown),
                           "unknown") == 0);
}

POKO_TEST(descriptors_validate_at_the_boundary) {
    POKO_CHECK(!poko::valid(poko::BufferDesc{}));
    poko::BufferDesc buffer;
    buffer.size_bytes = 64;
    POKO_CHECK(poko::valid(buffer));

    POKO_CHECK(!poko::valid(poko::TextureDesc{}));
    poko::TextureDesc texture;
    texture.width = 4;
    texture.height = 4;
    POKO_CHECK(poko::valid(texture));
    texture.format = poko::PixelFormat::Unknown;
    POKO_CHECK(!poko::valid(texture));

    POKO_CHECK(!poko::valid(poko::ShaderDesc{}));
    poko::ShaderDesc shader;
    shader.source = "void main() {}";
    POKO_CHECK(poko::valid(shader));

    POKO_CHECK(!poko::valid(poko::VertexLayout{}));
    poko::VertexLayout layout;
    layout.attribute_count = 1;
    layout.attributes[0] = {0, 3, 12, 0};
    POKO_CHECK(poko::valid(layout));
    layout.attributes[0].components = 5;
    POKO_CHECK(!poko::valid(layout));
    layout.attributes[0].components = 3;
    layout.attribute_count = 99;
    POKO_CHECK(!poko::valid(layout));
}

POKO_TEST(draw_calls_validate_at_the_boundary) {
    poko::DrawCall call;
    POKO_CHECK(!poko::valid(call));
    call.pipeline = poko::make_handle(0, 0);
    call.index_count = 6;
    POKO_CHECK(poko::valid(call));
    call.instance_count = 0;
    POKO_CHECK(!poko::valid(call));
    call.instance_count = 1;
    call.index_count = 0;
    call.vertex_count = 3;
    POKO_CHECK(poko::valid(call));
}

POKO_TEST(default_handles_are_invalid) {
    POKO_CHECK(!poko::is_valid(poko::RenderHandle{}));
    POKO_CHECK(poko::is_valid(poko::make_handle(0, 0)));
}

POKO_TEST_MAIN()
