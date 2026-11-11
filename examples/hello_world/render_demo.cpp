#include <iostream>
#include <memory>

#include "poko_render_device.h"
#include "poko_renderer.h"
#include "poko_result.h"

// Console "playable test scene" stand-in for the renderer foundation
// (systemplan/04, systemplan/27 Phase 1). It exercises resource creation,
// shader caching, camera setup, submission batching and a full frame using the
// labelled MOCK device. A real window needs SDL2/Vulkan to be pinned.
int main() {
    auto device = std::make_unique<poko::NullRenderDevice>();
    poko::Renderer renderer(std::move(device));

    const poko::ShaderDesc vertex_shader{
        poko::ShaderStage::Vertex,
        "void main() { gl_Position = vec4(0.0); }",
        "main",
        "hello.vert",
    };
    const poko::ShaderDesc fragment_shader{
        poko::ShaderStage::Fragment,
        "void main() { outColor = vec4(1.0); }",
        "main",
        "hello.frag",
    };

    const auto vs = renderer.device().create_shader(vertex_shader);
    const auto fs = renderer.device().create_shader(fragment_shader);
    if (!vs.ok() || !fs.ok()) {
        std::cerr << "shader creation failed\n";
        return 1;
    }

    poko::PipelineDesc pipeline;
    pipeline.vertex_shader = vs.value();
    pipeline.fragment_shader = fs.value();
    pipeline.layout.attribute_count = 1;
    pipeline.layout.attributes[0] = {0, 3, 12, 0};
    pipeline.debug_name = "hello.pipeline";

    const auto pipe = renderer.device().create_pipeline(pipeline);
    const auto vertex_buffer = renderer.device().create_buffer(
        poko::BufferDesc{36, poko::BufferUsage::Vertex, true, "hello.vb"});
    const auto index_buffer = renderer.device().create_buffer(
        poko::BufferDesc{12, poko::BufferUsage::Index, true, "hello.ib"});
    if (!pipe.ok() || !vertex_buffer.ok() || !index_buffer.ok()) {
        std::cerr << "resource creation failed\n";
        return 1;
    }

    renderer.shaders().get_or_compile(vertex_shader);
    renderer.shaders().get_or_compile(fragment_shader);
    renderer.camera().set_perspective(poko::math::kHalfPi, 16.0f / 9.0f, 0.1f,
                                      100.0f);
    renderer.camera().set_position(poko::math::Vec3{0.0f, 0.0f, 3.0f});

    poko::DrawCall call;
    call.pipeline = pipe.value();
    call.vertex_buffer = vertex_buffer.value();
    call.index_buffer = index_buffer.value();
    call.index_count = 6;
    call.sort_key = 1;
    renderer.submission().submit(call);
    renderer.submission().submit(call);  // same batch -> one instanced draw
    renderer.submission().batch();

    const poko::Status status = renderer.render_frame(poko::ClearValue{});
    if (!status.ok()) {
        std::cerr << "render_frame failed: " << status.error().message << '\n';
        return 1;
    }

    std::cout << "renderer demo [" << renderer.device().name() << "] OK\n"
              << "backend chain: " << poko::to_string(poko::preferred_backend())
              << " -> " << poko::to_string(poko::fallback_backend()) << " -> "
              << poko::to_string(poko::last_resort_backend()) << '\n'
              << "shaders cached: " << renderer.shaders().size()
              << " draws: " << renderer.stats().draw_calls()
              << " triangles: " << renderer.stats().triangles() << '\n';
    return 0;
}
