// Windowed Vulkan renderer demo (systemplan/04): SDL3 window + VkSurfaceKHR,
// then the real api::IRenderDevice Vulkan backend renders a triangle.
// Only built when POKO_RENDER_VULKAN=ON. Usage: --frames N or until closed.

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <span>

#include <vulkan/vulkan.h>

#include "poko_render_device.h"
#include "poko_renderer.h"
#include "poko_result.h"
#include "poko_version.h"
#include "poko_vulkan_device.h"

#ifndef POKO_IMPL_LABEL
#define POKO_IMPL_LABEL "LOCAL_DEV"
#endif

namespace {

constexpr const char* kVertexShader = R"(#version 450
layout(location = 0) in vec3 aPos;
layout(set = 0, binding = 0, std140) uniform Camera {
    mat4 mvp;
} camera;
layout(location = 0) out vec2 vUv;
void main() {
    gl_Position = camera.mvp * vec4(aPos, 1.0);
    vUv = aPos.xy * 0.5 + 0.5;
}
)";

constexpr const char* kFragmentShader = R"(#version 450
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 1) uniform sampler2D uTex;
void main() {
    outColor = texture(uTex, vUv);
}
)";

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
    int frame_limit = -1;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frame_limit = std::atoi(argv[++i]);
        }
    }

    if (!SDL_Init(SDL_InitSubSystem(SDL_INIT_VIDEO))) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("PokoX vulkan demo", 1280, 720, SDL_WINDOW_VULKAN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    // Vulkan instance with the extensions SDL requires.
    Uint32 ext_count = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&ext_count);
    if (exts == nullptr || ext_count == 0) {
        std::cerr << "SDL_Vulkan_GetInstanceExtensions failed\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        std::cerr << "SDL_Vulkan_LoadLibrary failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "poko_windowed_demo";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = ext_count;
    ici.ppEnabledExtensionNames = exts;
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "vkCreateInstance failed\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        std::cerr << "SDL_Vulkan_CreateSurface failed\n";
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Pick the first physical device with a graphics queue.
    Uint32 phys_count = 0;
    vkEnumeratePhysicalDevices(instance, &phys_count, nullptr);
    std::vector<VkPhysicalDevice> phys_devices(phys_count);
    vkEnumeratePhysicalDevices(instance, &phys_count, phys_devices.data());
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    for (auto candidate : phys_devices) {
        Uint32 fc = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &fc, nullptr);
        std::vector<VkQueueFamilyProperties> fams(fc);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &fc, fams.data());
        for (const auto& f : fams) {
            if (f.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                phys = candidate;
                break;
            }
        }
        if (phys != VK_NULL_HANDLE) {
            break;
        }
    }
    if (phys == VK_NULL_HANDLE) {
        std::cerr << "no Vulkan physical device\n";
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int exit_code = 0;
    {
        auto device = std::make_unique<poko::VulkanRenderDevice>(instance, phys, surface);
        poko::VulkanRenderDevice* vk_device = device.get();
        if (!device->initialize()) {
            std::cerr << "Vulkan device initialization failed\n";
            exit_code = 1;
        } else {
            int w = 0, h = 0;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            const auto caps = device->capabilities();
            std::cout << "poko_windowed_demo " << poko::version_string() << " [" << POKO_IMPL_LABEL
                      << "]\n"
                      << "backend: " << poko::to_string(caps.backend)
                      << " device: " << caps.device_name << std::endl;
            if (const auto sc = device->recreate_swapchain(static_cast<std::uint32_t>(w),
                                                           static_cast<std::uint32_t>(h));
                !sc.ok()) {
                std::cerr << "swapchain failed: " << sc.error().message << '\n';
                exit_code = 1;
            } else {
                poko::Renderer renderer(std::move(device));
                const poko::ShaderDesc vs{poko::ShaderStage::Vertex, kVertexShader, "main",
                                          "tri.vert"};
                const poko::ShaderDesc fs{poko::ShaderStage::Fragment, kFragmentShader, "main",
                                          "tri.frag"};
                const auto vsh = renderer.device().create_shader(vs);
                const auto fsh = renderer.device().create_shader(fs);
                if (!vsh.ok() || !fsh.ok()) {
                    std::cerr << "shader creation failed\n";
                    exit_code = 1;
                } else {
                    poko::PipelineDesc pipe;
                    pipe.vertex_shader = vsh.value();
                    pipe.fragment_shader = fsh.value();
                    pipe.layout.attribute_count = 1;
                    pipe.layout.attributes[0] = {0, 3, 12, 0};
                    pipe.cull_mode = poko::CullMode::None;
                    pipe.depth_test = false;
                    pipe.debug_name = "tri.pipeline";
                    const auto pipeline = renderer.device().create_pipeline(pipe);
                    const auto vb = renderer.device().create_buffer(poko::BufferDesc{
                        sizeof(kTriangle), poko::BufferUsage::Vertex, true, "tri.vb"});
                    if (!pipeline.ok() || !vb.ok()) {
                        std::cerr << "pipeline/buffer creation failed\n";
                        exit_code = 1;
                    } else {
                        const std::span<const std::byte> bytes{
                            reinterpret_cast<const std::byte*>(kTriangle), sizeof(kTriangle)};
                        if (!renderer.device().update_buffer(vb.value(), bytes).ok()) {
                            std::cerr << "vertex upload failed\n";
                            exit_code = 1;
                        } else {
                            const auto ubo = renderer.device().create_buffer(
                                poko::BufferDesc{sizeof(kIdentityMvp), poko::BufferUsage::Uniform,
                                                 true, "camera.ubo"});
                            const std::span<const std::byte> mvp_bytes{
                                reinterpret_cast<const std::byte*>(kIdentityMvp),
                                sizeof(kIdentityMvp)};
                            if (!ubo.ok() ||
                                !renderer.device().update_buffer(ubo.value(), mvp_bytes).ok()) {
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
                                    reinterpret_cast<const std::byte*>(checker.data()),
                                    checker.size()};
                                const bool tex_ok =
                                    texture.ok() && sampler.ok() &&
                                    renderer.device()
                                        .update_texture(texture.value(), 0, tex_bytes)
                                        .ok();
                                if (!tex_ok) {
                                    std::cerr << "texture creation/upload failed\n";
                                    exit_code = 1;
                                }
                                if (tex_ok) {
                                    poko::DrawCall call;
                                    call.pipeline = pipeline.value();
                                    call.vertex_buffer = vb.value();
                                    call.uniform_buffer = ubo.value();
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
                                                int new_w = 0;
                                                int new_h = 0;
                                                SDL_GetWindowSizeInPixels(window, &new_w, &new_h);
                                                const auto resize = vk_device->recreate_swapchain(
                                                    static_cast<std::uint32_t>(new_w),
                                                    static_cast<std::uint32_t>(new_h));
                                                if (!resize.ok()) {
                                                    std::cerr << "swapchain resize failed: "
                                                              << resize.error().message << '\n';
                                                    exit_code = 1;
                                                    running = false;
                                                }
                                            }
                                        }
                                        if (!running ||
                                            (frame_limit >= 0 && frames >= frame_limit)) {
                                            break;
                                        }
                                        const auto st = renderer.render_frame(
                                            poko::ClearValue{0.08f, 0.09f, 0.12f, 1.0f});
                                        if (!st.ok()) {
                                            std::cerr
                                                << "render_frame failed: " << st.error().message
                                                << '\n';
                                            exit_code = 1;
                                            break;
                                        }
                                        ++frames;
                                    }
                                    std::cout << "frames rendered: " << frames << std::endl;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit_code;
}

} // namespace

int main(int argc, char** argv) {
    return run(argc, argv);
}
