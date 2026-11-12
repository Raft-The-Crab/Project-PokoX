#include "poko/scene/graph.h"

#include <algorithm>
#include <utility>

namespace poko {

SceneGraph::Node* SceneGraph::find(NodeHandle handle) noexcept {
    if (static_cast<std::size_t>(handle.index) >= nodes_.size()) {
        return nullptr;
    }
    return &nodes_[handle.index];
}

const SceneGraph::Node* SceneGraph::find(NodeHandle handle) const noexcept {
    if (static_cast<std::size_t>(handle.index) >= nodes_.size()) {
        return nullptr;
    }
    return &nodes_[handle.index];
}

bool SceneGraph::handle_live(NodeHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < nodes_.size() &&
           generations_[handle.index] == handle.generation && nodes_[handle.index].alive;
}

bool SceneGraph::contains(NodeHandle handle) const {
    std::lock_guard<std::mutex> guard(mutex_);
    return handle_live(handle);
}

Result<NodeHandle> SceneGraph::create_node(const NodeDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid(desc)) {
        return Result<NodeHandle>::fail(ErrorCode::InvalidArgument, "invalid node descriptor");
    }
    if (is_valid(desc.parent) && !handle_live(desc.parent)) {
        return Result<NodeHandle>::fail(ErrorCode::NotFound, "parent node is not live");
    }
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        nodes_[index] = Node{};
    } else {
        index = static_cast<HandleIndex>(nodes_.size());
        nodes_.push_back(Node{});
        generations_.push_back(0);
    }
    auto& node = nodes_[index];
    node.alive = true;
    node.name = desc.name;
    node.parent = desc.parent;
    node.local = desc.local;
    if (is_valid(desc.parent)) {
        nodes_[desc.parent.index].children.push_back(make_handle(index, generations_[index]));
    }
    return Result<NodeHandle>::success(make_handle(index, generations_[index]));
}

Status SceneGraph::set_parent(NodeHandle node, NodeHandle parent) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(node)) {
        return Status::fail(ErrorCode::NotFound, "node handle is not live");
    }
    if (is_valid(parent)) {
        if (!handle_live(parent)) {
            return Status::fail(ErrorCode::NotFound, "parent node is not live");
        }
        // Reject cycles: parent must not be node itself or a descendant.
        for (NodeHandle cursor = parent; is_valid(cursor);) {
            if (cursor.index == node.index) {
                return Status::fail(ErrorCode::InvalidArgument, "parent would create a cycle");
            }
            const auto* n = find(cursor);
            if (n == nullptr) {
                break;
            }
            cursor = n->parent;
        }
    }
    auto& n = nodes_[node.index];
    if (is_valid(n.parent)) {
        auto& siblings = nodes_[n.parent.index].children;
        siblings.erase(std::remove_if(siblings.begin(), siblings.end(),
                                      [&](const NodeHandle& s) { return same_handle(s, node); }),
                       siblings.end());
    }
    n.parent = parent;
    if (is_valid(parent)) {
        nodes_[parent.index].children.push_back(node);
    }
    return Status::success();
}

Status SceneGraph::destroy_node(NodeHandle node) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(node)) {
        return Status::fail(ErrorCode::NotFound, "node handle is not live");
    }
    // Iterative traversal; descendants destroyed before ancestors.
    std::vector<NodeHandle> order;
    std::vector<NodeHandle> stack{node};
    while (!stack.empty()) {
        const NodeHandle current = stack.back();
        stack.pop_back();
        order.push_back(current);
        const auto* n = find(current);
        if (n != nullptr) {
            for (const auto& child : n->children) {
                stack.push_back(child);
            }
        }
    }
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        auto& n = nodes_[it->index];
        if (is_valid(n.parent)) {
            auto& siblings = nodes_[n.parent.index].children;
            siblings.erase(std::remove_if(siblings.begin(), siblings.end(),
                                          [&](const NodeHandle& s) { return same_handle(s, *it); }),
                           siblings.end());
        }
        n.alive = false;
        n.children.clear();
        ++generations_[it->index];
        free_.push_back(it->index);
    }
    return Status::success();
}

std::uint32_t SceneGraph::node_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t count = 0;
    for (const auto& node : nodes_) {
        if (node.alive) {
            ++count;
        }
    }
    return count;
}

} // namespace poko
