#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "poko_result.h"
#include "poko_scene_contracts.h"

namespace poko {

// Scene graph skeleton (systemplan/08): hierarchy + TRS transforms only.
// Component attachment, serialization and systems integration come later.
// Deterministic and allocation-light; a mutex guards the public API anyway.
class SceneGraph {
public:
    SceneGraph() = default;

    // Creates a node; returns its handle. desc.parent must be live or invalid.
    Result<NodeHandle> create_node(const NodeDesc& desc);
    Status destroy_node(NodeHandle node); // destroys descendants too
    Status set_parent(NodeHandle node, NodeHandle parent);
    Status set_local_transform(NodeHandle node, const Transform& transform);
    Result<Transform> local_transform(NodeHandle node) const;

    // World-space matrix (row-major, matches math::Mat4) for rendering.
    // Global TRS is ambiguous under non-uniform parent scale, so the matrix
    // is the canonical global transform.
    Result<math::Mat4> global_matrix(NodeHandle node) const;

    std::uint32_t node_count() const noexcept;
    bool contains(NodeHandle node) const;

private:
    struct Node {
        bool alive = false;
        std::string name;
        NodeHandle parent;
        Transform local{};
        std::vector<NodeHandle> children;
    };

    Node* find(NodeHandle handle) noexcept;
    const Node* find(NodeHandle handle) const noexcept;
    bool handle_live(NodeHandle handle) const noexcept;
    math::Mat4 compute_global(NodeHandle handle) const; // caller holds mutex_

    mutable std::mutex mutex_;
    std::vector<Node> nodes_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
};

} // namespace poko
