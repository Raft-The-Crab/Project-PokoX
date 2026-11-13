#include <algorithm>
#include <cmath>
#include <cstring>

#include "sw_detail.h"

namespace poko {
namespace {

struct Vtx {
    float p[4] = {0, 0, 0, 1}; // clip-space position
    float v[4] = {1, 1, 1, 1}; // attribute 1, passed to the fragment stage
};

struct Uniforms {
    float mvp[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float color[4] = {1, 1, 1, 1};
    float light[4] = {0, 0, 1, 0.2f};
};

struct Target {
    std::uint32_t* color = nullptr;
    float* depth = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    SwProgram program = SwProgram::Unlit;
    bool depth_test = true;
    bool depth_write = true;
    CompareOp compare = CompareOp::LessOrEqual;
    CullMode cull = CullMode::Back;
    const Uniforms* uniforms = nullptr;
    const SoftwareRenderDevice* none = nullptr;
    const std::vector<std::byte>* texture = nullptr;
    TextureDesc texture_desc{};
    SamplerDesc sampler{};
    std::uint64_t triangles = 0;
    std::uint64_t shaded = 0;
};

bool passes(CompareOp op, float incoming, float stored) noexcept {
    switch (op) {
    case CompareOp::Never: return false;
    case CompareOp::Less: return incoming < stored;
    case CompareOp::Equal: return incoming == stored;
    case CompareOp::LessOrEqual: return incoming <= stored;
    case CompareOp::Greater: return incoming > stored;
    case CompareOp::NotEqual: return incoming != stored;
    case CompareOp::GreaterOrEqual: return incoming >= stored;
    case CompareOp::Always: return true;
    }
    return false;
}

float wrap_coord(float t, WrapMode mode) noexcept {
    switch (mode) {
    case WrapMode::ClampToEdge:
        return std::clamp(t, 0.0f, 1.0f);
    case WrapMode::MirroredRepeat: {
        const float m = std::fmod(t, 2.0f);
        const float a = m < 0.0f ? m + 2.0f : m;
        return a > 1.0f ? 2.0f - a : a;
    }
    case WrapMode::Repeat:
        break;
    }
    return t - std::floor(t);
}

void texel(const Target& t, std::int32_t x, std::int32_t y, float out[4]) noexcept {
    const auto w = static_cast<std::int32_t>(t.texture_desc.width);
    const auto h = static_cast<std::int32_t>(t.texture_desc.height);
    x = std::clamp(x, 0, w - 1);
    y = std::clamp(y, 0, h - 1);
    const auto bpp = swdetail::bytes_per_pixel(t.texture_desc.format);
    const auto offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                         static_cast<std::size_t>(x)) * bpp;
    const auto* d = reinterpret_cast<const unsigned char*>(t.texture->data()) + offset;
    const float k = 1.0f / 255.0f;
    switch (t.texture_desc.format) {
    case PixelFormat::RGBA8:
        out[0] = d[0] * k; out[1] = d[1] * k; out[2] = d[2] * k; out[3] = d[3] * k;
        return;
    case PixelFormat::BGRA8:
        out[0] = d[2] * k; out[1] = d[1] * k; out[2] = d[0] * k; out[3] = d[3] * k;
        return;
    case PixelFormat::RGB8:
        out[0] = d[0] * k; out[1] = d[1] * k; out[2] = d[2] * k; out[3] = 1.0f;
        return;
    case PixelFormat::R8:
        out[0] = out[1] = out[2] = d[0] * k; out[3] = 1.0f;
        return;
    default: // not sampleable on the CPU path: loud magenta
        out[0] = 1.0f; out[1] = 0.0f; out[2] = 1.0f; out[3] = 1.0f;
        return;
    }
}

void sample(const Target& t, float u, float v, float out[4]) noexcept {
    const float uu = wrap_coord(u, t.sampler.wrap_u);
    const float vv = wrap_coord(v, t.sampler.wrap_v);
    const float fx = uu * static_cast<float>(t.texture_desc.width);
    const float fy = vv * static_cast<float>(t.texture_desc.height);
    if (t.sampler.mag_filter == FilterMode::Nearest) {
        texel(t, static_cast<std::int32_t>(fx), static_cast<std::int32_t>(fy), out);
        return;
    }
    const float gx = fx - 0.5f;
    const float gy = fy - 0.5f;
    const auto x0 = static_cast<std::int32_t>(std::floor(gx));
    const auto y0 = static_cast<std::int32_t>(std::floor(gy));
    const float ax = gx - static_cast<float>(x0);
    const float ay = gy - static_cast<float>(y0);
    float c[4][4];
    texel(t, x0, y0, c[0]);
    texel(t, x0 + 1, y0, c[1]);
    texel(t, x0, y0 + 1, c[2]);
    texel(t, x0 + 1, y0 + 1, c[3]);
    for (int i = 0; i < 4; ++i) {
        const float top = c[0][i] + (c[1][i] - c[0][i]) * ax;
        const float bottom = c[2][i] + (c[3][i] - c[2][i]) * ax;
        out[i] = top + (bottom - top) * ay;
    }
}

void shade(const Target& t, const float varying[4], float out[4]) noexcept {
    const float* base = t.uniforms->color;
    switch (t.program) {
    case SwProgram::Unlit:
        std::memcpy(out, base, sizeof(float) * 4);
        return;
    case SwProgram::VertexColor:
        for (int i = 0; i < 4; ++i) out[i] = varying[i] * base[i];
        return;
    case SwProgram::Lambert: {
        const float* l = t.uniforms->light;
        const float nl = std::sqrt(varying[0] * varying[0] + varying[1] * varying[1] +
                                   varying[2] * varying[2]);
        const float ll = std::sqrt(l[0] * l[0] + l[1] * l[1] + l[2] * l[2]);
        float diffuse = 0.0f;
        if (nl > 1e-6f && ll > 1e-6f) {
            diffuse = std::max(0.0f, (varying[0] * l[0] + varying[1] * l[1] + varying[2] * l[2]) /
                                         (nl * ll));
        }
        const float lit = l[3] + (1.0f - l[3]) * diffuse;
        out[0] = base[0] * lit;
        out[1] = base[1] * lit;
        out[2] = base[2] * lit;
        out[3] = base[3];
        return;
    }
    case SwProgram::Textured: {
        float s[4];
        sample(t, varying[0], varying[1], s);
        for (int i = 0; i < 4; ++i) out[i] = s[i] * base[i];
        return;
    }
    }
}

void write_pixel(Target& t, std::int32_t x, std::int32_t y, float z01, const float varying[4]) {
    const auto idx = static_cast<std::size_t>(y) * t.width + static_cast<std::size_t>(x);
    if (t.depth_test && !passes(t.compare, z01, t.depth[idx])) return;
    if (z01 < 0.0f || z01 > 1.0f) return;
    float rgba[4];
    shade(t, varying, rgba);
    const auto to8 = [](float v) {
        return static_cast<std::uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    t.color[idx] = to8(rgba[0]) | (to8(rgba[1]) << 8) | (to8(rgba[2]) << 16) | (to8(rgba[3]) << 24);
    if (t.depth_write && t.depth_test) t.depth[idx] = z01;
    ++t.shaded;
}

struct Screen {
    float x, y, z, inv_w;
    float v[4];
};

Screen to_screen(const Target& t, const Vtx& p) noexcept {
    Screen s;
    s.inv_w = 1.0f / p.p[3];
    const float nx = p.p[0] * s.inv_w;
    const float ny = p.p[1] * s.inv_w;
    s.z = p.p[2] * s.inv_w * 0.5f + 0.5f;
    s.x = (nx * 0.5f + 0.5f) * static_cast<float>(t.width);
    s.y = (1.0f - (ny * 0.5f + 0.5f)) * static_cast<float>(t.height);
    for (int i = 0; i < 4; ++i) s.v[i] = p.v[i];
    return s;
}

void raster_triangle(Target& t, const Vtx& a, const Vtx& b, const Vtx& c) {
    const Screen s0 = to_screen(t, a);
    const Screen s1 = to_screen(t, b);
    const Screen s2 = to_screen(t, c);
    // Screen y points down, so counter-clockwise in NDC is a negative area here.
    const float area = (s1.x - s0.x) * (s2.y - s0.y) - (s2.x - s0.x) * (s1.y - s0.y);
    if (area == 0.0f || !std::isfinite(area)) return;
    const bool front = area < 0.0f;
    if ((t.cull == CullMode::Back && !front) || (t.cull == CullMode::Front && front)) return;
    ++t.triangles;

    const float min_x = std::min({s0.x, s1.x, s2.x});
    const float max_x = std::max({s0.x, s1.x, s2.x});
    const float min_y = std::min({s0.y, s1.y, s2.y});
    const float max_y = std::max({s0.y, s1.y, s2.y});
    const auto x0 = std::max(0, static_cast<std::int32_t>(std::floor(min_x)));
    const auto x1 = std::min(static_cast<std::int32_t>(t.width) - 1,
                             static_cast<std::int32_t>(std::ceil(max_x)));
    const auto y0 = std::max(0, static_cast<std::int32_t>(std::floor(min_y)));
    const auto y1 = std::min(static_cast<std::int32_t>(t.height) - 1,
                             static_cast<std::int32_t>(std::ceil(max_y)));
    const float inv_area = 1.0f / area;
    for (std::int32_t y = y0; y <= y1; ++y) {
        for (std::int32_t x = x0; x <= x1; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float e0 = ((s2.x - s1.x) * (py - s1.y) - (s2.y - s1.y) * (px - s1.x)) * inv_area;
            const float e1 = ((s0.x - s2.x) * (py - s2.y) - (s0.y - s2.y) * (px - s2.x)) * inv_area;
            const float e2 = 1.0f - e0 - e1;
            if (e0 < 0.0f || e1 < 0.0f || e2 < 0.0f) continue;
            const float z = e0 * s0.z + e1 * s1.z + e2 * s2.z;
            const float iw = e0 * s0.inv_w + e1 * s1.inv_w + e2 * s2.inv_w;
            float varying[4];
            for (int i = 0; i < 4; ++i) {
                varying[i] = (e0 * s0.v[i] * s0.inv_w + e1 * s1.v[i] * s1.inv_w +
                              e2 * s2.v[i] * s2.inv_w) / iw;
            }
            write_pixel(t, x, y, z, varying);
        }
    }
}

Vtx lerp_vtx(const Vtx& a, const Vtx& b, float f) noexcept {
    Vtx r;
    for (int i = 0; i < 4; ++i) {
        r.p[i] = a.p[i] + (b.p[i] - a.p[i]) * f;
        r.v[i] = a.v[i] + (b.v[i] - a.v[i]) * f;
    }
    return r;
}

float near_dist(const Vtx& v) noexcept {
    return v.p[2] + v.p[3]; // inside when >= 0
}

bool trivially_outside(const Vtx& a, const Vtx& b, const Vtx& c) noexcept {
    for (int axis = 0; axis < 3; ++axis) {
        if (a.p[axis] > a.p[3] && b.p[axis] > b.p[3] && c.p[axis] > c.p[3]) return true;
        if (a.p[axis] < -a.p[3] && b.p[axis] < -b.p[3] && c.p[axis] < -c.p[3]) return true;
    }
    return false;
}

void draw_triangle(Target& t, const Vtx& a, const Vtx& b, const Vtx& c) {
    if (trivially_outside(a, b, c)) return;
    Vtx in[3] = {a, b, c};
    Vtx out[4];
    int n = 0;
    for (int i = 0; i < 3; ++i) {
        const Vtx& cur = in[i];
        const Vtx& nxt = in[(i + 1) % 3];
        const float dc = near_dist(cur);
        const float dn = near_dist(nxt);
        if (dc >= 0.0f) out[n++] = cur;
        if ((dc >= 0.0f) != (dn >= 0.0f)) out[n++] = lerp_vtx(cur, nxt, dc / (dc - dn));
    }
    for (int i = 0; i < n; ++i) {
        if (!(out[i].p[3] > 1e-6f)) return;
    }
    for (int i = 1; i + 1 < n; ++i) raster_triangle(t, out[0], out[i], out[i + 1]);
}

void draw_line(Target& t, Vtx a, Vtx b) {
    const float da = near_dist(a);
    const float db = near_dist(b);
    if (da < 0.0f && db < 0.0f) return;
    if (da < 0.0f) a = lerp_vtx(a, b, da / (da - db));
    if (db < 0.0f) b = lerp_vtx(a, b, da / (da - db));
    if (!(a.p[3] > 1e-6f) || !(b.p[3] > 1e-6f)) return;
    const Screen s0 = to_screen(t, a);
    const Screen s1 = to_screen(t, b);
    const float steps = std::max(std::abs(s1.x - s0.x), std::abs(s1.y - s0.y));
    if (!std::isfinite(steps) || steps > 16384.0f) return;
    const int count = std::max(1, static_cast<int>(std::ceil(steps)));
    for (int i = 0; i <= count; ++i) {
        const float f = static_cast<float>(i) / static_cast<float>(count);
        const auto x = static_cast<std::int32_t>(std::floor(s0.x + (s1.x - s0.x) * f));
        const auto y = static_cast<std::int32_t>(std::floor(s0.y + (s1.y - s0.y) * f));
        if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(t.width) ||
            y >= static_cast<std::int32_t>(t.height)) {
            continue;
        }
        float varying[4];
        for (int k = 0; k < 4; ++k) varying[k] = s0.v[k] + (s1.v[k] - s0.v[k]) * f;
        write_pixel(t, x, y, s0.z + (s1.z - s0.z) * f, varying);
    }
}

void draw_point(Target& t, const Vtx& a) {
    if (near_dist(a) < 0.0f || !(a.p[3] > 1e-6f)) return;
    const Screen s = to_screen(t, a);
    const auto x = static_cast<std::int32_t>(std::floor(s.x));
    const auto y = static_cast<std::int32_t>(std::floor(s.y));
    if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(t.width) ||
        y >= static_cast<std::int32_t>(t.height)) {
        return;
    }
    write_pixel(t, x, y, s.z, s.v);
}

bool read_floats(const std::vector<std::byte>& buffer, std::size_t offset, std::uint32_t n,
                 float* out) noexcept {
    if (offset > buffer.size() || buffer.size() - offset < static_cast<std::size_t>(n) * 4) {
        return false;
    }
    std::memcpy(out, buffer.data() + offset, static_cast<std::size_t>(n) * 4);
    for (std::uint32_t i = 0; i < n; ++i) {
        if (!std::isfinite(out[i])) return false;
    }
    return true;
}

const VertexAttribute* find_attribute(const VertexLayout& layout, std::uint32_t location) {
    for (std::uint32_t i = 0; i < layout.attribute_count; ++i) {
        if (layout.attributes[i].location == location) return &layout.attributes[i];
    }
    return nullptr;
}

bool fetch(const std::vector<std::byte>& buffer, const VertexLayout& layout, std::uint32_t index,
           const Uniforms& u, Vtx& out) noexcept {
    const VertexAttribute* pos = find_attribute(layout, 0);
    if (pos == nullptr) return false;
    const auto stride = [&](const VertexAttribute& a) {
        return a.stride_bytes != 0 ? a.stride_bytes : a.components * 4u;
    };
    float p[4] = {0, 0, 0, 1};
    if (!read_floats(buffer, static_cast<std::size_t>(index) * stride(*pos) + pos->offset_bytes,
                     pos->components, p)) {
        return false;
    }
    for (int r = 0; r < 4; ++r) {
        out.p[r] = u.mvp[r * 4 + 0] * p[0] + u.mvp[r * 4 + 1] * p[1] + u.mvp[r * 4 + 2] * p[2] +
                   u.mvp[r * 4 + 3] * p[3];
    }
    if (const VertexAttribute* a1 = find_attribute(layout, 1)) {
        float v[4] = {0, 0, 0, 1};
        if (!read_floats(buffer, static_cast<std::size_t>(index) * stride(*a1) + a1->offset_bytes,
                         a1->components, v)) {
            return false;
        }
        std::memcpy(out.v, v, sizeof(v));
    }
    return true;
}

} // namespace

Status SoftwareRenderDevice::draw(const DrawCall& call) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lost_) return Status::fail(ErrorCode::FailedPrecondition, "device lost");
    if (!in_frame_) return Status::fail(ErrorCode::FailedPrecondition, "no frame in progress");
    if (!valid(call)) return Status::fail(ErrorCode::InvalidArgument, "invalid draw call");

    const Slot* pipeline = get(call.pipeline);
    if (pipeline == nullptr || pipeline->kind != SwKind::Pipeline) {
        return Status::fail(ErrorCode::NotFound, "draw references a dead pipeline");
    }
    const Slot* vertices = get(call.vertex_buffer);
    if (vertices == nullptr || vertices->kind != SwKind::Buffer) {
        return Status::fail(ErrorCode::NotFound, "draw references a dead vertex buffer");
    }
    const Slot* indices = nullptr;
    if (call.index_count > 0) {
        indices = get(call.index_buffer);
        if (indices == nullptr || indices->kind != SwKind::Buffer) {
            return Status::fail(ErrorCode::NotFound, "draw references a dead index buffer");
        }
    }
    Uniforms uniforms;
    if (is_valid(call.uniform_buffer)) {
        const Slot* ub = get(call.uniform_buffer);
        if (ub == nullptr || ub->kind != SwKind::Buffer) {
            return Status::fail(ErrorCode::NotFound, "draw references a dead uniform buffer");
        }
        const auto offset = static_cast<std::size_t>(call.uniform_offset);
        if (offset > ub->bytes.size()) {
            return Status::fail(ErrorCode::OutOfRange, "uniform offset past the buffer");
        }
        const auto avail = ub->bytes.size() - offset;
        const auto* src = ub->bytes.data() + offset;
        // Fields beyond a short block keep their defaults.
        if (avail >= 64) std::memcpy(uniforms.mvp, src, 64);
        if (avail >= 80) std::memcpy(uniforms.color, src + 64, 16);
        if (avail >= 96) std::memcpy(uniforms.light, src + 80, 16);
    }

    Target target;
    target.color = color_.data();
    target.depth = depth_.data();
    target.width = width_;
    target.height = height_;
    target.program = pipeline->program;
    target.depth_test = pipeline->pipeline.depth_test;
    target.depth_write = pipeline->pipeline.depth_write;
    target.compare = pipeline->pipeline.depth_compare;
    target.cull = pipeline->pipeline.cull_mode;
    target.uniforms = &uniforms;
    if (pipeline->program == SwProgram::Textured) {
        const Slot* tex = call.texture_count > 0 ? get(call.textures[0]) : nullptr;
        if (tex == nullptr || tex->kind != SwKind::Texture) {
            return Status::fail(ErrorCode::NotFound, "textured draw needs a live texture in slot 0");
        }
        target.texture = &tex->bytes;
        target.texture_desc = tex->texture;
        if (const Slot* smp = get(call.samplers[0]); smp != nullptr && smp->kind == SwKind::Sampler) {
            target.sampler = smp->sampler;
        }
    }

    const VertexLayout& layout = pipeline->pipeline.layout;
    const std::uint32_t count = call.index_count > 0 ? call.index_count : call.vertex_count;
    std::vector<Vtx> fetched;
    fetched.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t index = i;
        if (indices != nullptr) {
            const std::size_t size = call.index_type == IndexType::UInt16 ? 2 : 4;
            const std::size_t offset = static_cast<std::size_t>(i) * size;
            if (offset + size > indices->bytes.size()) {
                return Status::fail(ErrorCode::OutOfRange, "index read past the index buffer");
            }
            if (size == 2) {
                std::uint16_t v;
                std::memcpy(&v, indices->bytes.data() + offset, 2);
                index = v;
            } else {
                std::memcpy(&index, indices->bytes.data() + offset, 4);
            }
        }
        Vtx v;
        if (!fetch(vertices->bytes, layout, index, uniforms, v)) {
            return Status::fail(ErrorCode::OutOfRange, "vertex fetch outside the vertex buffer");
        }
        fetched.push_back(v);
    }

    switch (layout.topology) {
    case PrimitiveTopology::Triangles:
        for (std::size_t i = 0; i + 2 < fetched.size(); i += 3) {
            draw_triangle(target, fetched[i], fetched[i + 1], fetched[i + 2]);
        }
        break;
    case PrimitiveTopology::TriangleStrip:
        for (std::size_t i = 0; i + 2 < fetched.size(); ++i) {
            if (i % 2 == 0) {
                draw_triangle(target, fetched[i], fetched[i + 1], fetched[i + 2]);
            } else {
                draw_triangle(target, fetched[i + 1], fetched[i], fetched[i + 2]);
            }
        }
        break;
    case PrimitiveTopology::Lines:
        for (std::size_t i = 0; i + 1 < fetched.size(); i += 2) {
            draw_line(target, fetched[i], fetched[i + 1]);
        }
        break;
    case PrimitiveTopology::Points:
        for (const Vtx& v : fetched) draw_point(target, v);
        break;
    }
    triangles_ += target.triangles;
    shaded_ += target.shaded;
    return Status::success();
}

} // namespace poko
