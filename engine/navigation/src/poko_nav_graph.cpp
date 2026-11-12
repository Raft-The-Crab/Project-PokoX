#include "poko_nav_graph.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace poko::nav {
namespace {

struct OpenEntry {
    float f = 0.0f;
    float g = 0.0f;
    NodeId node = kInvalidNode;
    bool operator>(const OpenEntry& other) const {
        // Min-heap; ties break on node id for determinism.
        if (f != other.f) {
            return f > other.f;
        }
        return node > other.node;
    }
};

float heuristic(const math::Vec3& a, const math::Vec3& b) noexcept {
    return math::distance(a, b);
}

} // namespace

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

std::vector<NodeId> NavGraph::find_path(NodeId start, NodeId goal) const {
    if (start >= nodes_.size() || goal >= nodes_.size()) {
        return {};
    }
    if (start == goal) {
        return {start};
    }

    const auto& goal_pos = nodes_[goal].position;
    std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<OpenEntry>> open;
    std::unordered_map<NodeId, float> best_g;
    std::unordered_map<NodeId, NodeId> came_from;

    open.push(OpenEntry{heuristic(nodes_[start].position, goal_pos), 0.0f, start});
    best_g[start] = 0.0f;

    while (!open.empty()) {
        const auto current = open.top();
        open.pop();
        if (current.node == goal) {
            // Reconstruct.
            std::vector<NodeId> path{goal};
            NodeId cursor = goal;
            while (cursor != start) {
                cursor = came_from[cursor];
                path.push_back(cursor);
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        const float known_g = best_g.count(current.node) ? best_g[current.node] : 0.0f;
        if (current.g > known_g + 1e-6f) {
            continue; // stale entry
        }
        for (const NodeId next : nodes_[current.node].neighbors) {
            const float step =
                math::distance(nodes_[current.node].position, nodes_[next].position) *
                nodes_[next].cost_multiplier;
            const float tentative = current.g + step;
            auto it = best_g.find(next);
            if (it != best_g.end() && tentative >= it->second - 1e-6f) {
                continue;
            }
            best_g[next] = tentative;
            came_from[next] = current.node;
            open.push(
                OpenEntry{tentative + heuristic(nodes_[next].position, goal_pos), tentative, next});
        }
    }
    return {};
}

NodeId nearest_node(const NavGraph& graph, const math::Vec3& position,
                    float max_distance) noexcept {
    NodeId best = kInvalidNode;
    float best_d = max_distance;
    for (NodeId i = 0; i < graph.node_count(); ++i) {
        const float d = math::distance(graph.position(i), position);
        if (d <= best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

} // namespace poko::nav
