#include "poko/graphics/mesh.h"

#include <cstddef>
#include <utility>

#include "gfx_detail.h"

namespace poko::graphics {

bool MeshLibrary::live(MeshHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < slots_.size() &&
           generations_[handle.index] == handle.generation && slots_[handle.index].alive;
}

Result<MeshHandle> MeshLibrary::create(const MeshDesc& desc) {
    if (!valid(desc)) {
        return Result<MeshHandle>::fail(ErrorCode::InvalidArgument, "invalid mesh descriptor");
    }

    std::vector<RenderHandle> owned;
    owned.reserve(2);

    BufferDesc vb;
    vb.size_bytes = desc.vertices.size();
    vb.usage = BufferUsage::Vertex;
    vb.host_visible = true;
    auto vb_result = device_.create_buffer(vb);
    if (!vb_result.ok()) {
        return Result<MeshHandle>::fail(vb_result.error().code, vb_result.error().message);
    }
    owned.push_back(vb_result.value());
    Status upload = device_.update_buffer(vb_result.value(), detail::byte_span(desc.vertices));
    if (!upload.ok()) {
        detail::destroy_all(device_, owned);
        return Result<MeshHandle>::fail(upload.code(), upload.error().message);
    }

    RenderHandle index_buffer{};
    if (!desc.indices.empty()) {
        BufferDesc ib;
        ib.size_bytes = desc.indices.size();
        ib.usage = BufferUsage::Index;
        ib.host_visible = true;
        auto ib_result = device_.create_buffer(ib);
        if (!ib_result.ok()) {
            detail::destroy_all(device_, owned);
            return Result<MeshHandle>::fail(ib_result.error().code, ib_result.error().message);
        }
        index_buffer = ib_result.value();
        owned.push_back(index_buffer);
        upload = device_.update_buffer(index_buffer, detail::byte_span(desc.indices));
        if (!upload.ok()) {
            detail::destroy_all(device_, owned);
            return Result<MeshHandle>::fail(upload.code(), upload.error().message);
        }
    }

    GpuMesh mesh;
    mesh.vertex_buffer = vb_result.value();
    mesh.index_buffer = index_buffer;
    mesh.layout = desc.layout;
    mesh.index_type = desc.index_type;
    mesh.vertex_count = static_cast<std::uint32_t>(desc.vertices.size() /
                                                   desc.layout.attributes[0].stride_bytes);
    mesh.index_count = static_cast<std::uint32_t>(desc.indices.size() /
                                                  index_size_bytes(desc.index_type));

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
    slot.mesh = mesh;
    slot.owned = std::move(owned);
    return Result<MeshHandle>::success(make_handle(index, generations_[index]));
}

Result<GpuMesh> MeshLibrary::get(MeshHandle handle) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(handle)) {
        return Result<GpuMesh>::fail(ErrorCode::NotFound, "mesh handle is not live");
    }
    return Result<GpuMesh>::success(slots_[handle.index].mesh);
}

Status MeshLibrary::destroy(MeshHandle handle) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(handle)) {
        return Status::fail(ErrorCode::NotFound, "mesh handle is not live");
    }
    auto& slot = slots_[handle.index];
    detail::destroy_all(device_, slot.owned);
    slot.alive = false;
    ++generations_[handle.index];
    free_.push_back(handle.index);
    return Status::success();
}

std::uint32_t MeshLibrary::count() const noexcept {
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
