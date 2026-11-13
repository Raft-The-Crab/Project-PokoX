#include "poko_test_harness.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

#include "poko/renderer/camera.h"
#include "poko/renderer/software_device.h"

namespace {
using namespace poko;

std::span<const std::byte> bytes_of(const void* data, std::size_t size) {
    return {static_cast<const std::byte*>(data), size};
}

BufferDesc buf_desc(std::uint64_t size, BufferUsage usage) {
    BufferDesc d;
    d.size_bytes = size;
    d.usage = usage;
    return d;
}

ShaderDesc shader_desc(ShaderStage stage, std::string_view source) {
    ShaderDesc d;
    d.stage = stage;
    d.source = source;
    return d;
}

TextureDesc tex_desc(std::uint32_t w, std::uint32_t h) {
    TextureDesc d;
    d.width = w;
    d.height = h;
    return d;
}

std::uint32_t red(std::uint32_t p) { return p & 0xFF; }
std::uint32_t green(std::uint32_t p) { return (p >> 8) & 0xFF; }
std::uint32_t blue(std::uint32_t p) { return (p >> 16) & 0xFF; }

struct Scene {
    SoftwareRenderDevice device{64, 64};
    RenderHandle vs{}, fs{};

    RenderHandle pipeline(const char* fragment_tag, std::uint32_t attr1_components = 0,
                          CullMode cull = CullMode::Back, bool depth = true,
                          PrimitiveTopology topology = PrimitiveTopology::Triangles) {
        vs = device.create_shader(shader_desc(ShaderStage::Vertex, "poko:vs")).value();
        fs = device.create_shader(shader_desc(ShaderStage::Fragment, fragment_tag)).value();
        PipelineDesc desc;
        desc.vertex_shader = vs;
        desc.fragment_shader = fs;
        desc.cull_mode = cull;
        desc.depth_test = depth;
        const std::uint32_t floats = 3 + attr1_components;
        desc.layout.attributes[0] = {0, 3, floats * 4, 0};
        desc.layout.attribute_count = 1;
        if (attr1_components > 0) {
            desc.layout.attributes[1] = {1, attr1_components, floats * 4, 12};
            desc.layout.attribute_count = 2;
        }
        desc.layout.topology = topology;
        return device.create_pipeline(desc).value();
    }

    RenderHandle buffer(const std::vector<float>& data, BufferUsage usage = BufferUsage::Vertex) {
        const auto h = device.create_buffer(buf_desc(data.size() * 4, usage)).value();
        POKO_CHECK(device.update_buffer(h, bytes_of(data.data(), data.size() * 4)).ok());
        return h;
    }

    RenderHandle uniforms(const std::array<float, 24>& block) {
        const auto h = device.create_buffer(buf_desc(sizeof(block), BufferUsage::Uniform)).value();
        POKO_CHECK(device.update_buffer(h, bytes_of(block.data(), sizeof(block))).ok());
        return h;
    }

    Status draw(RenderHandle pipe, RenderHandle vb, std::uint32_t count,
                RenderHandle ub = RenderHandle{}) {
        DrawCall call;
        call.pipeline = pipe;
        call.vertex_buffer = vb;
        call.vertex_count = count;
        call.uniform_buffer = ub;
        return device.draw(call);
    }
};

std::array<float, 24> block(float r, float g, float b) {
    std::array<float, 24> u{};
    u[0] = u[5] = u[10] = u[15] = 1.0f;
    u[16] = r; u[17] = g; u[18] = b; u[19] = 1.0f;
    u[20] = 0; u[21] = 0; u[22] = 1; u[23] = 0.2f;
    return u;
}

// Counter-clockwise in NDC (front-facing), covering the middle of the screen.
const std::vector<float> kTriangle = {-0.8f, -0.8f, 0.0f, 0.8f, -0.8f, 0.0f, 0.0f, 0.8f, 0.0f};
} // namespace

POKO_TEST(triangle_is_filled_and_background_untouched) {
    Scene s;
    const auto pipe = s.pipeline("poko:unlit");
    const auto ub = s.uniforms(block(1, 0, 0));
    POKO_CHECK(s.device.begin_frame().ok());
    POKO_CHECK(s.device.clear(ClearValue{0, 0, 1, 1}).ok());
    POKO_CHECK(s.draw(pipe, s.buffer(kTriangle), 3, ub).ok());
    POKO_CHECK(s.device.end_frame().ok());
    const auto centre = s.device.pixel(32, 40);
    POKO_CHECK(red(centre) == 255 && green(centre) == 0 && blue(centre) == 0);
    const auto corner = s.device.pixel(1, 1);
    POKO_CHECK(blue(corner) == 255 && red(corner) == 0);
    POKO_CHECK_EQ(s.device.triangles_drawn(), 1u);
    POKO_CHECK(s.device.pixels_shaded() > 500);
    POKO_CHECK(s.device.pixels_shaded() < 64 * 64);
}

POKO_TEST(back_faces_are_culled_unless_disabled) {
    const std::vector<float> reversed = {-0.8f, -0.8f, 0.0f, 0.0f, 0.8f, 0.0f, 0.8f, -0.8f, 0.0f};
    for (const auto cull : {CullMode::Back, CullMode::Front, CullMode::None}) {
        Scene s;
        const auto pipe = s.pipeline("poko:unlit", 0, cull);
        POKO_CHECK(s.device.begin_frame().ok());
        POKO_CHECK(s.device.clear(ClearValue{}).ok());
        POKO_CHECK(s.draw(pipe, s.buffer(reversed), 3).ok());
        const bool drawn = s.device.triangles_drawn() == 1;
        POKO_CHECK(drawn == (cull != CullMode::Back));
    }
}

POKO_TEST(depth_test_resolves_overlap_regardless_of_draw_order) {
    const auto quad = [](float z) {
        return std::vector<float>{-0.9f, -0.9f, z, 0.9f, -0.9f, z, 0.9f, 0.9f, z,
                                  -0.9f, -0.9f, z, 0.9f, 0.9f, z, -0.9f, 0.9f, z};
    };
    for (const bool near_first : {true, false}) {
        Scene s;
        const auto pipe = s.pipeline("poko:unlit");
        const auto near_ub = s.uniforms(block(0, 1, 0));
        const auto far_ub = s.uniforms(block(1, 0, 0));
        POKO_CHECK(s.device.begin_frame().ok());
        POKO_CHECK(s.device.clear(ClearValue{}).ok());
        const auto vb_near = s.buffer(quad(-0.5f));
        const auto vb_far = s.buffer(quad(0.5f));
        if (near_first) {
            POKO_CHECK(s.draw(pipe, vb_near, 6, near_ub).ok());
            POKO_CHECK(s.draw(pipe, vb_far, 6, far_ub).ok());
        } else {
            POKO_CHECK(s.draw(pipe, vb_far, 6, far_ub).ok());
            POKO_CHECK(s.draw(pipe, vb_near, 6, near_ub).ok());
        }
        const auto p = s.device.pixel(32, 32);
        POKO_CHECK(green(p) == 255 && red(p) == 0);
        POKO_CHECK(std::abs(s.device.depth_at(32, 32) - 0.25f) < 1e-3f);
    }
}

POKO_TEST(vertex_colors_interpolate) {
    Scene s;
    const auto pipe = s.pipeline("poko:vertex_color", 3);
    const std::vector<float> tri = {-0.9f, -0.9f, 0, 1, 0, 0, 0.9f, -0.9f, 0, 0, 1, 0,
                                    0.0f,  0.9f,  0, 0, 0, 1};
    POKO_CHECK(s.device.begin_frame().ok());
    POKO_CHECK(s.device.clear(ClearValue{}).ok());
    POKO_CHECK(s.draw(pipe, s.buffer(tri), 3, s.uniforms(block(1, 1, 1))).ok());
    // Centroid of the triangle in NDC is (0, -0.3).
    const auto p = s.device.pixel(32, 41);
    POKO_CHECK(std::abs(static_cast<int>(red(p)) - 85) < 20);
    POKO_CHECK(std::abs(static_cast<int>(green(p)) - 85) < 20);
    POKO_CHECK(std::abs(static_cast<int>(blue(p)) - 85) < 20);
}

POKO_TEST(lambert_shades_by_light_direction) {
    const auto render = [](float lx, float lz) {
        Scene s;
        const auto pipe = s.pipeline("poko:lambert", 3);
        std::vector<float> tri = {-0.8f, -0.8f, 0, 0, 0, 1, 0.8f, -0.8f, 0, 0, 0, 1,
                                  0.0f,  0.8f,  0, 0, 0, 1};
        auto u = block(1, 1, 1);
        u[20] = lx; u[21] = 0; u[22] = lz; u[23] = 0.1f;
        POKO_CHECK(s.device.begin_frame().ok());
        POKO_CHECK(s.device.clear(ClearValue{}).ok());
        POKO_CHECK(s.draw(pipe, s.buffer(tri), 3, s.uniforms(u)).ok());
        return red(s.device.pixel(32, 40));
    };
    const auto facing = render(0, 1);
    const auto away = render(0, -1);
    POKO_CHECK(facing > 250);
    POKO_CHECK(away > 15 && away < 35); // ambient only
    POKO_CHECK(render(1, 1) > away && render(1, 1) < facing);
}

POKO_TEST(textured_quad_samples_the_texture) {
    Scene s;
    const auto pipe = s.pipeline("poko:textured", 2);
    const auto tex = s.device.create_texture(tex_desc(2, 2)).value();
    const std::uint8_t texels[16] = {255, 0, 0, 255,   0, 255, 0, 255,
                                     0, 0, 255, 255,   255, 255, 0, 255};
    POKO_CHECK(s.device.update_texture(tex, 0, bytes_of(texels, sizeof(texels))).ok());
    SamplerDesc smp;
    smp.mag_filter = FilterMode::Nearest;
    smp.min_filter = FilterMode::Nearest;
    const auto sampler = s.device.create_sampler(smp).value();
    // Quad, uv (0,0) at the top-left.
    const std::vector<float> quad = {-1, -1, 0, 0, 1,  1, -1, 0, 1, 1,  1, 1, 0, 1, 0,
                                     -1, -1, 0, 0, 1,  1, 1, 0, 1, 0,  -1, 1, 0, 0, 0};
    DrawCall call;
    call.pipeline = pipe;
    call.vertex_buffer = s.buffer(quad);
    call.vertex_count = 6;
    call.textures[0] = tex;
    call.samplers[0] = sampler;
    call.texture_count = 1;
    POKO_CHECK(s.device.begin_frame().ok());
    POKO_CHECK(s.device.clear(ClearValue{}).ok());
    POKO_CHECK(s.device.draw(call).ok());
    const auto tl = s.device.pixel(8, 8);
    const auto tr = s.device.pixel(56, 8);
    const auto bl = s.device.pixel(8, 56);
    const auto br = s.device.pixel(56, 56);
    POKO_CHECK(red(tl) == 255 && green(tl) == 0);
    POKO_CHECK(green(tr) == 255 && red(tr) == 0);
    POKO_CHECK(blue(bl) == 255 && red(bl) == 0);
    POKO_CHECK(red(br) == 255 && green(br) == 255 && blue(br) == 0);

    DrawCall no_texture = call;
    no_texture.texture_count = 0;
    POKO_CHECK(!s.device.draw(no_texture).ok());
}

POKO_TEST(perspective_camera_projects_and_near_plane_clips) {
    Camera camera;
    camera.set_position({0, 0, 3});
    camera.set_target({0, 0, 0});
    camera.set_perspective(1.0f, 1.0f, 0.1f, 50.0f);
    const auto vp = camera.view_projection();

    Scene s;
    const auto pipe = s.pipeline("poko:unlit", 0, CullMode::None);
    std::array<float, 24> u = block(1, 1, 1);
    std::memcpy(u.data(), vp.m.data(), 64);
    const auto ub = s.uniforms(u);

    // A triangle one unit in front of the origin plus one vertex behind the camera.
    const std::vector<float> crossing = {-1, -1, 0, 1, -1, 0, 0, 0, 10};
    POKO_CHECK(s.device.begin_frame().ok());
    POKO_CHECK(s.device.clear(ClearValue{}).ok());
    POKO_CHECK(s.draw(pipe, s.buffer(crossing), 3, ub).ok());
    POKO_CHECK(s.device.pixels_shaded() > 0); // clipped, not discarded

    // Same quad nearer to the camera looks bigger.
    const auto area = [&](float z) {
        Scene t;
        const auto p = t.pipeline("poko:unlit", 0, CullMode::None);
        const auto b = t.uniforms(u);
        const std::vector<float> tri = {-0.5f, -0.5f, z, 0.5f, -0.5f, z, 0.0f, 0.5f, z};
        POKO_CHECK(t.device.begin_frame().ok());
        POKO_CHECK(t.device.clear(ClearValue{}).ok());
        POKO_CHECK(t.draw(p, t.buffer(tri), 3, b).ok());
        return t.device.pixels_shaded();
    };
    POKO_CHECK(area(1.0f) > area(-2.0f) * 2);
}

POKO_TEST(lines_points_and_strips_draw) {
    {
        Scene s;
        const auto pipe = s.pipeline("poko:unlit", 0, CullMode::None, true, PrimitiveTopology::Lines);
        POKO_CHECK(s.device.begin_frame().ok());
        POKO_CHECK(s.device.clear(ClearValue{}).ok());
        POKO_CHECK(s.draw(pipe, s.buffer({-0.9f, 0, 0, 0.9f, 0, 0}), 2).ok());
        POKO_CHECK(s.device.pixels_shaded() >= 50);
    }
    {
        Scene s;
        const auto pipe = s.pipeline("poko:unlit", 0, CullMode::None, true, PrimitiveTopology::Points);
        POKO_CHECK(s.device.begin_frame().ok());
        POKO_CHECK(s.device.clear(ClearValue{}).ok());
        POKO_CHECK(s.draw(pipe, s.buffer({0, 0, 0, 0.5f, 0.5f, 0}), 2).ok());
        POKO_CHECK_EQ(s.device.pixels_shaded(), 2u);
    }
    {
        Scene s;
        const auto pipe = s.pipeline("poko:unlit", 0, CullMode::None, true, PrimitiveTopology::TriangleStrip);
        POKO_CHECK(s.device.begin_frame().ok());
        POKO_CHECK(s.device.clear(ClearValue{}).ok());
        POKO_CHECK(s.draw(pipe, s.buffer({-0.9f, -0.9f, 0, 0.9f, -0.9f, 0, -0.9f, 0.9f, 0, 0.9f, 0.9f, 0}), 4).ok());
        POKO_CHECK_EQ(s.device.triangles_drawn(), 2u);
        POKO_CHECK(s.device.pixels_shaded() > 64 * 64 * 6 / 10);
    }
}

POKO_TEST(indexed_draws_and_validation) {
    Scene s;
    const auto pipe = s.pipeline("poko:unlit");
    const auto vb = s.buffer(kTriangle);
    const std::uint16_t good[3] = {0, 1, 2};
    const std::uint16_t bad[3] = {0, 1, 9};
    const auto ib = s.device.create_buffer(buf_desc(6, BufferUsage::Index)).value();
    POKO_CHECK(s.device.update_buffer(ib, bytes_of(good, 6)).ok());
    DrawCall call;
    call.pipeline = pipe;
    call.vertex_buffer = vb;
    call.index_buffer = ib;
    call.index_count = 3;
    call.index_type = IndexType::UInt16;

    POKO_CHECK(!s.device.draw(call).ok()); // outside a frame
    POKO_CHECK(s.device.begin_frame().ok());
    POKO_CHECK(!s.device.begin_frame().ok());
    POKO_CHECK(s.device.draw(call).ok());
    POKO_CHECK_EQ(s.device.triangles_drawn(), 1u);

    POKO_CHECK(s.device.update_buffer(ib, bytes_of(bad, 6)).ok());
    POKO_CHECK(!s.device.draw(call).ok()); // vertex fetch out of range
    POKO_CHECK(!s.device.update_buffer(ib, bytes_of(bad, 6), 2).ok());
    POKO_CHECK(!s.device.create_buffer(buf_desc(0, BufferUsage::Vertex)).ok());

    POKO_CHECK(s.device.destroy(vb).ok());
    POKO_CHECK(!s.device.draw(call).ok());
    POKO_CHECK(!s.device.destroy(vb).ok());
}

POKO_TEST(device_loss_blocks_work_until_recovery_and_ppm_is_written) {
    Scene s;
    const auto pipe = s.pipeline("poko:unlit");
    s.device.simulate_device_loss();
    POKO_CHECK(s.device.device_lost());
    POKO_CHECK(!s.device.begin_frame().ok());
    POKO_CHECK(!s.device.create_buffer(buf_desc(16, BufferUsage::Vertex)).ok());
    POKO_CHECK(s.device.recover_from_device_loss().ok());
    POKO_CHECK(s.device.begin_frame().ok());
    POKO_CHECK(s.device.clear(ClearValue{1, 0, 0, 1}).ok());
    POKO_CHECK(s.draw(pipe, s.buffer(kTriangle), 3).ok());
    POKO_CHECK(s.device.end_frame().ok());
    POKO_CHECK_EQ(s.device.frame_count(), 1u);

    const char* path = "/tmp/poko_software_test.ppm";
    POKO_CHECK(s.device.write_ppm(path).ok());
    std::FILE* f = std::fopen(path, "rb");
    POKO_CHECK(f != nullptr);
    char header[16] = {};
    if (f != nullptr) {
        POKO_CHECK(std::fread(header, 1, 11, f) == 11);
        std::fclose(f);
    }
    POKO_CHECK(std::strncmp(header, "P6\n64 64\n255", 11) == 0);
    POKO_CHECK(!s.device.write_ppm("/nonexistent_dir/x.ppm").ok());
}

POKO_TEST_MAIN()
