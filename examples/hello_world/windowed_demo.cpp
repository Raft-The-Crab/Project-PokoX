// Windowed renderer demo (systemplan/04 steps 1-4): opens an SDL3 window,
// creates an OpenGL 3.3 core context, and renders a textured triangle through
// the real api::IRenderDevice seam (uniform buffer + sampler + texture).
//
// Only built when POKO_RENDER_OPENGL=ON (requires POKO_FETCH_DEPENDENCIES=ON).
// Usage: poko_windowed_demo [--frames N]
//   --frames N  render N frames and exit (used by automated checks)
//   (default)   run until the window is closed

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <span>
#include <string>

#include "poko_gl_device.h"
#include "poko_render_device.h"
#include "poko_renderer.h"
#include "poko_result.h"
#include "poko_version.h"

#ifndef POKO_IMPL_LABEL
#define POKO_IMPL_LABEL "LOCAL_DEV"
#endif

namespace {

constexpr const char* kVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(std140) uniform Camera {
    mat4 mvp;
} camera;
out vec2 vUv;
void main() {
    gl_Position = camera.mvp * vec4(aPos, 1.0);
    vUv = aPos.xy * 0.5 + 0.5;
}
)";

constexpr const char* kFragmentShader = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uTex;
out vec4 outColor;
void main() {
    outColor = texture(uTex, vUv);
}
)";

// Three vec3 positions in clip space: one triangle, 36 bytes.
constexpr float kTriangle[] = {
    -0.6f, -0.4f, 0.0f, //
    0.6f,  -0.4f, 0.0f, //
    0.0f,  0.6f,  0.0f, //
};

// Column-major identity MVP (std140 mat4), 64 bytes.
constexpr float kIdentityMvp[16] = {
    1.0f, 0.0f, 0.0f, 0.0f, //
    0.0f, 1.0f, 0.0f, 0.0f, //
    0.0f, 0.0f, 1.0f, 0.0f, //
    0.0f, 0.0f, 0.0f, 1.0f, //
};

// 4x4 RGBA8 checkerboard, generated at startup (64 bytes).
constexpr std::uint32_t kCheckerSize = 4;

std::uint8_t checker_texel(std::uint32_t x, std::uint32_t y) {
    return ((x ^ y) & 1) != 0 ? 235 : 45;
}

int run(int argc, char** argv) {
    int frame_limit = -1; // -1 = run until the window closes
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frame_limit = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::cout << "usage: " << argv[0] << " [--frames N]\n";
            return 0;
        } else {
            std::cerr << "unknown argument: " << argv[i] << '\n';
            return 2;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow("PokoX renderer demo", 1280, 720, SDL_WINDOW_OPENGL);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, context);

    int exit_code = 0;
    {
        auto device = std::make_unique<poko::GlRenderDevice>();
        poko::GlRenderDevice* gl_device = device.get();
        if (!device->initialize()) {
            std::cerr << "OpenGL device initialization failed (need GL 3.3 core)\n";
            exit_code = 1;
        } else {
            int width = 0;
            int height = 0;
            SDL_GetWindowSizeInPixels(window, &width, &height);
            gl_device->set_viewport(static_cast<std::uint32_t>(width),
                                    static_cast<std::uint32_t>(height));

            poko::Renderer renderer(std::move(device));
            const auto caps = renderer.device().capabilities();
            std::cout << "poko_windowed_demo " << poko::version_string() << " [" << POKO_IMPL_LABEL
                      << "]\n"
                      << "backend: " << poko::to_string(caps.backend)
                      << " device: " << caps.device_name
                      << " max_texture: " << caps.max_texture_size << '\n';

            const poko::ShaderDesc vertex{poko::ShaderStage::Vertex, kVertexShader, "main",
                                          "tri.vert"};
            const poko::ShaderDesc fragment{poko::ShaderStage::Fragment, kFragmentShader, "main",
                                            "tri.frag"};
            const auto vs = renderer.device().create_shader(vertex);
            const auto fs = renderer.device().create_shader(fragment);
            if (!vs.ok() || !fs.ok()) {
                std::cerr << "shader creation failed\n";
                exit_code = 1;
            } else {
                poko::PipelineDesc pipeline;
                pipeline.vertex_shader = vs.value();
                pipeline.fragment_shader = fs.value();
                pipeline.layout.attribute_count = 1;
                pipeline.layout.attributes[0] = {0, 3, 12, 0};
                pipeline.cull_mode = poko::CullMode::None;
                pipeline.depth_test = false;
                pipeline.debug_name = "tri.pipeline";

                const auto pipe = renderer.device().create_pipeline(pipeline);
                const auto vertices = renderer.device().create_buffer(
                    poko::BufferDesc{sizeof(kTriangle), poko::BufferUsage::Vertex, true, "tri.vb"});
                if (!pipe.ok() || !vertices.ok()) {
                    std::cerr << "pipeline/buffer creation failed\n";
                    exit_code = 1;
                } else {
                    const std::span<const std::byte> bytes{
                        reinterpret_cast<const std::byte*>(kTriangle), sizeof(kTriangle)};
                    const auto uploaded = renderer.device().update_buffer(vertices.value(), bytes);
                    if (!uploaded.ok()) {
                        std::cerr << "vertex upload failed: " << uploaded.error().message << '\n';
                        exit_code = 1;
                    } else {
                        const auto uniforms = renderer.device().create_buffer(poko::BufferDesc{
                            sizeof(kIdentityMvp), poko::BufferUsage::Uniform, true, "camera.ubo"});
                        const std::span<const std::byte> mvp_bytes{
                            reinterpret_cast<const std::byte*>(kIdentityMvp), sizeof(kIdentityMvp)};
                        if (!uniforms.ok() ||
                            !renderer.device().update_buffer(uniforms.value(), mvp_bytes).ok()) {
                            std::cerr << "uniform buffer creation/upload failed\n";
                            exit_code = 1;
                        } else {
                            std::array<std::uint8_t, kCheckerSize * kCheckerSize * 4> checker{};
                            for (std::uint32_t y = 0; y < kCheckerSize; ++y) {
                                for (std::uint32_t x = 0; x < kCheckerSize; ++x) {
                                    const std::uint8_t v = checker_texel(x, y);
                                    const std::size_t px = (y * kCheckerSize + x) * 4;
                                    checker[px + 0] = v;
                                    checker[px + 1] = v;
                                    checker[px + 2] = v;
                                    checker[px + 3] = 255;
                                }
                            }
                            const auto texture = renderer.device().create_texture(
                                poko::TextureDesc{kCheckerSize, kCheckerSize, 1, 1, 1,
                                                  poko::PixelFormat::RGBA8, "checker.tex"});
                            const auto sampler =
                                renderer.device().create_sampler(poko::SamplerDesc{});
                            const std::span<const std::byte> tex_bytes{
                                reinterpret_cast<const std::byte*>(checker.data()), checker.size()};
                            const bool tex_ok = texture.ok() && sampler.ok() &&
                                                renderer.device()
                                                    .update_texture(texture.value(), 0, tex_bytes)
                                                    .ok();
                            if (!tex_ok) {
                                std::cerr << "texture creation/upload failed\n";
                                exit_code = 1;
                            }
                            if (tex_ok) {
                                poko::DrawCall call;
                                call.pipeline = pipe.value();
                                call.vertex_buffer = vertices.value();
                                call.uniform_buffer = uniforms.value();
                                call.textures[0] = texture.value();
                                call.samplers[0] = sampler.value();
                                call.texture_count = 1;
                                call.vertex_count = 3;
                                renderer.submission().submit(call);

                                bool running = true;
                                int frames = 0;
                                while (running) {
                                    SDL_Event event;
                                    while (SDL_PollEvent(&event)) {
                                        if (event.type == SDL_EVENT_QUIT ||
                                            event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                                            running = false;
                                        } else if (event.type ==
                                                   SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                                            int w = 0;
                                            int h = 0;
                                            SDL_GetWindowSizeInPixels(window, &w, &h);
                                            gl_device->set_viewport(static_cast<std::uint32_t>(w),
                                                                    static_cast<std::uint32_t>(h));
                                        }
                                    }
                                    if (!running || (frame_limit >= 0 && frames >= frame_limit)) {
                                        break;
                                    }
                                    const poko::Status status = renderer.render_frame(
                                        poko::ClearValue{0.08f, 0.09f, 0.12f, 1.0f});
                                    if (!status.ok()) {
                                        std::cerr
                                            << "render_frame failed: " << status.error().message
                                            << '\n';
                                        exit_code = 1;
                                        break;
                                    }
                                    SDL_GL_SwapWindow(window);
                                    ++frames;
                                }
                                std::cout << "frames rendered: " << frames << '\n';
                            }
                        }
                    }
                }
            }
        }
    } // renderer + device destroyed while the GL context is still current

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit_code;
}

} // namespace

int main(int argc, char** argv) {
    return run(argc, argv);
}
