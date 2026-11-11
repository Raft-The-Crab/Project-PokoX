#include "poko_test_harness.h"

#include <cstddef>
#include <cstring>
#include <span>

#include "poko_render_contracts.h"
#include "poko_render_device.h"

POKO_TEST(backend_chain_is_vulkan_then_opengl_then_null) {
    POKO_CHECK(poko::preferred_backend() == poko::GraphicsBackend::Vulkan);
    POKO_CHECK(poko::fallback_backend() == poko::GraphicsBackend::OpenGL);
    POKO_CHECK(poko::last_resort_backend() == poko::GraphicsBackend::Null);
    POKO_CHECK(poko::null_device_capabilities().backend == poko::GraphicsBackend::Null);
}

POKO_TEST(null_device_metadata) {
    poko::NullRenderDevice device;
    POKO_CHECK(std::strcmp(device.name(), "null-mock") == 0);
    POKO_CHECK(device.backend() == poko::GraphicsBackend::Null);
    POKO_CHECK(device.available());
    POKO_CHECK(device.capabilities().backend == poko::GraphicsBackend::Null);
    POKO_CHECK(device.capabilities().max_texture_size > 0);
    POKO_CHECK(device.capabilities().max_buffer_bytes > 0);
}

POKO_TEST(resource_creation_and_generation_checked_destruction) {
    poko::NullRenderDevice device;
    poko::BufferDesc desc;
    desc.size_bytes = 64;
    const auto handle = device.create_buffer(desc);
    POKO_CHECK(handle.ok());
    POKO_CHECK(poko::is_valid(handle.value()));
    POKO_CHECK(device.is_handle_live(handle.value()));
    POKO_CHECK_EQ(device.resource_count(), std::size_t{1});

    POKO_CHECK(device.destroy(handle.value()).ok());
    POKO_CHECK(!device.destroy(handle.value()).ok());
    POKO_CHECK(!device.is_handle_live(handle.value()));
    POKO_CHECK_EQ(device.resource_count(), std::size_t{0});

    // A freed slot is reused with a bumped generation.
    const auto reused = device.create_buffer(desc);
    POKO_CHECK(reused.ok());
    POKO_CHECK(!poko::same_handle(handle.value(), reused.value()));
    POKO_CHECK(!device.is_handle_live(handle.value()));
}

POKO_TEST(invalid_descriptors_are_rejected) {
    poko::NullRenderDevice device;
    const auto bad_buffer = device.create_buffer(poko::BufferDesc{});
    POKO_CHECK(!bad_buffer.ok());
    POKO_CHECK(bad_buffer.error().code == poko::ErrorCode::InvalidArgument);
    POKO_CHECK(!device.create_texture(poko::TextureDesc{}).ok());
    POKO_CHECK(!device.create_shader(poko::ShaderDesc{}).ok());
    POKO_CHECK(!device.destroy(poko::RenderHandle{}).ok());
}

namespace {
poko::PipelineDesc make_pipeline(poko::RenderHandle vs, poko::RenderHandle fs) {
    poko::PipelineDesc pipeline;
    pipeline.vertex_shader = vs;
    pipeline.fragment_shader = fs;
    pipeline.layout.attribute_count = 1;
    pipeline.layout.attributes[0] = {0, 3, 12, 0};
    return pipeline;
}
} // namespace

POKO_TEST(pipeline_requires_live_shader_handles) {
    poko::NullRenderDevice device;
    poko::ShaderDesc shader;
    shader.source = "void main() {}";
    const auto vs = device.create_shader(shader);
    const auto fs = device.create_shader(shader);
    POKO_CHECK(vs.ok() && fs.ok());

    const auto pipeline = make_pipeline(vs.value(), fs.value());
    POKO_CHECK(device.create_pipeline(pipeline).ok());

    POKO_CHECK(device.destroy(vs.value()).ok());
    const auto stale = device.create_pipeline(pipeline);
    POKO_CHECK(!stale.ok());
    POKO_CHECK(stale.error().code == poko::ErrorCode::NotFound);

    poko::PipelineDesc bad_layout = pipeline;
    bad_layout.layout.attribute_count = 0;
    const auto invalid = device.create_pipeline(bad_layout);
    POKO_CHECK(!invalid.ok());
    POKO_CHECK(invalid.error().code == poko::ErrorCode::InvalidArgument);
}

POKO_TEST(frame_lifecycle_and_command_recording) {
    poko::NullRenderDevice device;
    poko::ShaderDesc shader;
    shader.source = "void main() {}";
    const auto vs = device.create_shader(shader);
    const auto fs = device.create_shader(shader);
    const auto pipe = device.create_pipeline(make_pipeline(vs.value(), fs.value()));
    POKO_CHECK(pipe.ok());

    POKO_CHECK(!device.clear(poko::ClearValue{}).ok());
    POKO_CHECK(device.begin_frame().ok());
    POKO_CHECK(!device.begin_frame().ok());

    poko::ClearValue clear;
    clear.r = 0.25f;
    POKO_CHECK(device.clear(clear).ok());

    poko::DrawCall call;
    call.pipeline = pipe.value();
    call.index_count = 6;
    POKO_CHECK(device.draw(call).ok());

    poko::DrawCall bad = call;
    bad.instance_count = 0;
    POKO_CHECK(!device.draw(bad).ok());
    POKO_CHECK(!device.draw(poko::DrawCall{}).ok());

    POKO_CHECK(device.end_frame().ok());
    POKO_CHECK(!device.end_frame().ok());

    POKO_CHECK_EQ(device.frame_count(), std::size_t{1});
    POKO_CHECK_EQ(device.recorded_clears().size(), std::size_t{1});
    POKO_CHECK_EQ(device.recorded_draws().size(), std::size_t{1});
    POKO_CHECK_EQ(device.recorded_clears()[0].r, 0.25f);
    POKO_CHECK_EQ(device.recorded_draws()[0].index_count, 6u);

    device.reset_records();
    POKO_CHECK_EQ(device.frame_count(), std::size_t{0});
    POKO_CHECK_EQ(device.recorded_draws().size(), std::size_t{0});
}

POKO_TEST(update_buffer_validates_handle_and_bounds) {
    poko::NullRenderDevice device;
    poko::BufferDesc desc;
    desc.size_bytes = 64;
    const auto buffer = device.create_buffer(desc);
    POKO_CHECK(buffer.ok());

    const std::byte payload[16] = {};
    const std::span<const std::byte> bytes{payload, sizeof(payload)};

    // Uploads do not require an open frame (they are resource operations).
    POKO_CHECK(device.update_buffer(buffer.value(), bytes).ok());
    POKO_CHECK_EQ(device.buffer_update_count(), std::size_t{1});

    // Past the end of the declared buffer is rejected.
    POKO_CHECK(!device.update_buffer(buffer.value(), bytes, 56).ok());
    // An empty write is rejected.
    POKO_CHECK(!device.update_buffer(buffer.value(), {}).ok());
    // A non-buffer handle is rejected.
    poko::ShaderDesc shader;
    shader.source = "void main() {}";
    const auto shader_handle = device.create_shader(shader);
    POKO_CHECK(shader_handle.ok());
    POKO_CHECK(!device.update_buffer(shader_handle.value(), bytes).ok());
    // A stale handle is rejected.
    POKO_CHECK(device.destroy(buffer.value()).ok());
    POKO_CHECK(!device.update_buffer(buffer.value(), bytes).ok());
    POKO_CHECK_EQ(device.buffer_update_count(), std::size_t{1});
}

POKO_TEST(device_loss_and_recovery) {
    poko::NullRenderDevice device;
    POKO_CHECK(!device.recover_from_device_loss().ok());

    POKO_CHECK(device.begin_frame().ok());
    POKO_CHECK(device.clear(poko::ClearValue{}).ok());

    device.simulate_device_loss();
    POKO_CHECK(device.device_lost());
    POKO_CHECK(!device.available());
    POKO_CHECK(!device.begin_frame().ok());
    poko::BufferDesc desc;
    desc.size_bytes = 8;
    POKO_CHECK(!device.create_buffer(desc).ok());

    POKO_CHECK(device.recover_from_device_loss().ok());
    POKO_CHECK(!device.device_lost());
    POKO_CHECK(device.available());
    POKO_CHECK_EQ(device.recorded_clears().size(), std::size_t{0});
    POKO_CHECK(device.begin_frame().ok());
    POKO_CHECK(device.end_frame().ok());
}

POKO_TEST_MAIN()
