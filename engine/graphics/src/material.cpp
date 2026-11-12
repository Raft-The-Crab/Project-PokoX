#include "poko/graphics/material.h"

#include <cstddef>
#include <utility>

#include "gfx_detail.h"

namespace poko::graphics {

bool MaterialLibrary::live(MaterialHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < slots_.size() &&
           generations_[handle.index] == handle.generation && slots_[handle.index].alive;
}

Result<MaterialHandle> MaterialLibrary::create(const MaterialDesc& desc) {
    if (!valid(desc)) {
        return Result<MaterialHandle>::fail(ErrorCode::InvalidArgument,
                                            "invalid material descriptor");
    }

    std::vector<RenderHandle> owned;
    owned.reserve(3u + desc.textures.size() * 2u + 1u);

    ShaderDesc vs;
    vs.stage = ShaderStage::Vertex;
    vs.source = desc.vertex_shader_source;
    auto vs_result = device_.create_shader(vs);
    if (!vs_result.ok()) {
        return Result<MaterialHandle>::fail(vs_result.error().code, vs_result.error().message);
    }
    owned.push_back(vs_result.value());

    ShaderDesc fs;
    fs.stage = ShaderStage::Fragment;
    fs.source = desc.fragment_shader_source;
    auto fs_result = device_.create_shader(fs);
    if (!fs_result.ok()) {
        detail::destroy_all(device_, owned);
        return Result<MaterialHandle>::fail(fs_result.error().code, fs_result.error().message);
    }
    owned.push_back(fs_result.value());

    PipelineDesc pipeline;
    pipeline.vertex_shader = vs_result.value();
    pipeline.fragment_shader = fs_result.value();
    pipeline.layout = desc.layout;
    pipeline.cull_mode = desc.cull_mode;
    pipeline.depth_compare = desc.depth_compare;
    pipeline.depth_test = desc.depth_test;
    pipeline.depth_write = desc.depth_write;
    auto pipeline_result = device_.create_pipeline(pipeline);
    if (!pipeline_result.ok()) {
        detail::destroy_all(device_, owned);
        return Result<MaterialHandle>::fail(pipeline_result.error().code,
                                            pipeline_result.error().message);
    }
    owned.push_back(pipeline_result.value());

    GpuMaterial material;
    material.pipeline = pipeline_result.value();
    material.layout = desc.layout;
    material.texture_count = static_cast<std::uint32_t>(desc.textures.size());
    for (std::size_t i = 0; i < desc.textures.size(); ++i) {
        auto texture = device_.create_texture(desc.textures[i].texture);
        if (!texture.ok()) {
            detail::destroy_all(device_, owned);
            return Result<MaterialHandle>::fail(texture.error().code, texture.error().message);
        }
        owned.push_back(texture.value());

        auto sampler = device_.create_sampler(desc.textures[i].sampler);
        if (!sampler.ok()) {
            detail::destroy_all(device_, owned);
            return Result<MaterialHandle>::fail(sampler.error().code, sampler.error().message);
        }
        owned.push_back(sampler.value());

        material.textures[i] = texture.value();
        material.samplers[i] = sampler.value();
    }

    if (!desc.uniform_data.empty()) {
        BufferDesc ub;
        ub.size_bytes = desc.uniform_data.size();
        ub.usage = BufferUsage::Uniform;
        ub.host_visible = true;
        auto uniform = device_.create_buffer(ub);
        if (!uniform.ok()) {
            detail::destroy_all(device_, owned);
            return Result<MaterialHandle>::fail(uniform.error().code, uniform.error().message);
        }
        owned.push_back(uniform.value());
        Status upload =
            device_.update_buffer(uniform.value(), detail::byte_span(desc.uniform_data));
        if (!upload.ok()) {
            detail::destroy_all(device_, owned);
            return Result<MaterialHandle>::fail(upload.code(), upload.error().message);
        }
        material.uniform_buffer = uniform.value();
    }

    std::lock_guard<std::mutex> guard(mutex_);
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        slots_[index] = Slot{};
    } else {
        index = static_cast<HandleIndex>(slots_.size());
        slots_.push_back(Slot{});
        generations_.push_back(0);
    }
    auto& slot = slots_[index];
    slot.alive = true;
    slot.material = material;
    slot.owned = std::move(owned);
    return Result<MaterialHandle>::success(make_handle(index, generations_[index]));
}

Result<GpuMaterial> MaterialLibrary::get(MaterialHandle handle) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(handle)) {
        return Result<GpuMaterial>::fail(ErrorCode::NotFound, "material handle is not live");
    }
    return Result<GpuMaterial>::success(slots_[handle.index].material);
}

Status MaterialLibrary::destroy(MaterialHandle handle) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(handle)) {
        return Status::fail(ErrorCode::NotFound, "material handle is not live");
    }
    auto& slot = slots_[handle.index];
    detail::destroy_all(device_, slot.owned);
    slot.alive = false;
    ++generations_[handle.index];
    free_.push_back(handle.index);
    return Status::success();
}

std::uint32_t MaterialLibrary::count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t n = 0;
    for (const auto& slot : slots_) {
        if (slot.alive) {
            ++n;
        }
    }
    return n;
}

} // namespace poko::graphics
