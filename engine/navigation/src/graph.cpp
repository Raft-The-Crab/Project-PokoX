#include "poko/navigation/nav_graph.h"

#include <algorithm>

namespace poko::nav {

NodeId NavGraph::add_node(const math::Vec3& position) {
    nodes_.push_back(Node{position, {}, 1.0f});
    return static_cast<NodeId>(nodes_.size() - 1);
}

Status NavGraph::connect(NodeId a, NodeId b) {
    if (a >= nodes_.size() || b >= nodes_.size() || a == b) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid nav node ids");
    }
    auto& na = nodes_[a];
    auto& nb = nodes_[b];
    if (std::find(na.neighbors.begin(), na.neighbors.end(), b) == na.neighbors.end()) {
        na.neighbors.push_back(b);
    }
    if (std::find(nb.neighbors.begin(), nb.neighbors.end(), a) == nb.neighbors.end()) {
        nb.neighbors.push_back(a);
    }
    return Status::success();
}

Status NavGraph::set_cost(NodeId node, float cost_multiplier) {
    if (node >= nodes_.size() || cost_multiplier <= 0.0f) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid nav node or cost");
    }
    nodes_[node].cost_multiplier = cost_multiplier;
    return Status::success();
}

math::Vec3 NavGraph::position(NodeId node) const {
    return node < nodes_.size() ? nodes_[node].position : math::Vec3{};
}

} // namespace poko::nav
