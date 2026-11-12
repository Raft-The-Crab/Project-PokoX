#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "poko/core/math.h"
#include "poko/core/result.h"

namespace poko::nav {

// Navigation skeleton (Phase 1, systemplan/06): waypoint graph + A* path
// queries. A navmesh (recast-style) replaces this later; the query API stays.

using NodeId = std::uint32_t;
inline constexpr NodeId kInvalidNode = std::numeric_limits<NodeId>::max();

struct NavNode {
    math::Vec3 position;
    std::vector<NodeId> neighbors; // undirected edges listed once per node
};

// Deterministic waypoint graph. Edge cost = Euclidean distance; optional
// per-node penalty via set_cost().
class NavGraph {
public:
    NavGraph() = default;

    NodeId add_node(const math::Vec3& position);
    Status connect(NodeId a, NodeId b); // symmetric edge
    Status set_cost(NodeId node, float cost_multiplier);

    // A* shortest path; returns node ids start..goal inclusive, or empty if
    // unreachable. Deterministic tie-breaking by node id.
    std::vector<NodeId> find_path(NodeId start, NodeId goal) const;

    std::uint32_t node_count() const noexcept {
        return static_cast<std::uint32_t>(nodes_.size());
    }
    math::Vec3 position(NodeId node) const;

private:
    struct Node {
        math::Vec3 position;
        std::vector<NodeId> neighbors;
        float cost_multiplier = 1.0f;
    };

    std::vector<Node> nodes_;
};

// Snap a world position to the nearest node within max_distance; returns
// kInvalidNode when nothing is close enough.
NodeId nearest_node(const NavGraph& graph, const math::Vec3& position, float max_distance) noexcept;

} // namespace poko::nav
