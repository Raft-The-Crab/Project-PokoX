#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "poko/core/api_seams.h"
#include "poko/core/render_contracts.h"
#include "poko/core/result.h"
#include "poko/core/types.h"

namespace poko::graphics {

using MeshHandle = TypedHandle;

// Byte size of one index for a given index type.
constexpr std::uint32_t index_size_bytes(IndexType type) noexcept {
    return type == IndexType::UInt16 ? 2u : 4u;
}

// CPU-side mesh geometry authored by game code / the asset importer. Byte
// blobs keep this layer layout-agnostic; VertexLayout describes how the GPU
// should read them. The first attribute's stride_bytes is the vertex stride.
struct MeshDesc {
    std::vector<std::byte> vertices;
    std::vector<std::byte> indices; // empty => non-indexed draw
    VertexLayout layout{};
    IndexType index_type = IndexType::UInt32;
};

// Device-resident mesh: handles and counts consumed when building a DrawCall.
struct GpuMesh {
    RenderHandle vertex_buffer;
    RenderHandle index_buffer; // invalid for a non-indexed mesh
    VertexLayout layout{};
    IndexType index_type = IndexType::UInt32;
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
};

inline bool valid(const MeshDesc& desc) noexcept {
    if (!::poko::valid(desc.layout) || desc.vertices.empty()) {
        return false;
    }
    const std::uint32_t stride = desc.layout.attributes[0].stride_bytes;
    if (stride == 0 || desc.vertices.size() % stride != 0) {
        return false;
    }
    if (!desc.indices.empty() && desc.indices.size() % index_size_bytes(desc.index_type) != 0) {
        return false;
    }
    return true;
}

// Owns GPU mesh resources created through an injected IRenderDevice. Meshes
// are referenced by generational handles; the device is never owned here.
// Thread-safety contract: synchronised; may be used from any thread.
class MeshLibrary {
public:
    explicit MeshLibrary(api::IRenderDevice& device) noexcept : device_(device) {}

    Result<MeshHandle> create(const MeshDesc& desc);
    Result<GpuMesh> get(MeshHandle handle) const;
    Status destroy(MeshHandle handle);
    std::uint32_t count() const noexcept;

private:
    struct Slot {
        bool alive = false;
        GpuMesh mesh{};
        std::vector<RenderHandle> owned;
    };

    bool live(MeshHandle handle) const noexcept; // caller holds mutex_

    api::IRenderDevice& device_;
    mutable std::mutex mutex_;
    std::vector<Slot> slots_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
};

} // namespace poko::graphics
