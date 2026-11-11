#include "poko_test_harness.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "poko_frame_stats.h"
#include "poko_render_device.h"
#include "poko_renderer.h"

POKO_TEST(frame_stats_count_draws_and_triangles) {
    poko::FrameStats stats;
    stats.begin_frame();
    stats.record_draw(2);
    stats.record_draw(4);
    stats.set_gpu_time_ms(1.5);
    stats.end_frame();

    POKO_CHECK_EQ(stats.frame_index(), std::uint64_t{1});
    POKO_CHECK_EQ(stats.draw_calls(), std::uint32_t{2});
    POKO_CHECK_EQ(stats.triangles(), std::uint64_t{6});
    POKO_CHECK(stats.cpu_frame_ms() >= 0.0);
    POKO_CHECK_EQ(stats.gpu_frame_ms(), 1.5);

    stats.reset();
    POKO_CHECK_EQ(stats.frame_index(), std::uint64_t{0});
    POKO_CHECK_EQ(stats.draw_calls(), std::uint32_t{0});
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

POKO_TEST(renderer_renders_a_batched_frame) {
    auto device = std::make_unique<poko::NullRenderDevice>();
    poko::Renderer renderer(std::move(device));

    poko::ShaderDesc shader;
    shader.source = "void main() {}";
    const auto vs = renderer.device().create_shader(shader);
    const auto fs = renderer.device().create_shader(shader);
    POKO_CHECK(vs.ok() && fs.ok());
    const auto pipe = renderer.device().create_pipeline(make_pipeline(vs.value(), fs.value()));
    POKO_CHECK(pipe.ok());

    poko::DrawCall call;
    call.pipeline = pipe.value();
    call.index_count = 6;
    renderer.submission().submit(call);
    renderer.submission().submit(call);
    renderer.submission().batch();

    POKO_CHECK(renderer.render_frame(poko::ClearValue{}).ok());
    POKO_CHECK_EQ(renderer.stats().draw_calls(), std::uint32_t{1});
    POKO_CHECK_EQ(renderer.stats().triangles(), std::uint64_t{4});
    POKO_CHECK(renderer.device().capabilities().backend == poko::GraphicsBackend::Null);
}

POKO_TEST(renderer_reports_an_unavailable_device) {
    auto device = std::make_unique<poko::NullRenderDevice>();
    device->simulate_device_loss();
    poko::Renderer renderer(std::move(device));
    const auto status = renderer.render_frame(poko::ClearValue{});
    POKO_CHECK(!status.ok());
    POKO_CHECK(status.code() == poko::ErrorCode::FailedPrecondition);
}

POKO_TEST_MAIN()
