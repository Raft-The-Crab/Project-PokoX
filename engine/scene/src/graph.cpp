#include "poko/scene/graph.h"

#include <algorithm>
#include <utility>

namespace poko {
namespace {

// Composes TRS into a row-major matrix: M = T * R * S.
math::Mat4 compose_trs(const Transform& t) {
    math::Mat4 rot = math::Mat4::identity();
    // Quaternion -> rotation matrix (normalized q).
    const math::Quat q = math::normalize(t.rotation);
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    rot.at(0, 0) = 1.0f - 2.0f * (y * y + z * z);
    rot.at(0, 1) = 2.0f * (x * y - z * w);
    rot.at(0, 2) = 2.0f * (x * z + y * w);
    rot.at(1, 0) = 2.0f * (x * y + z * w);
    rot.at(1, 1) = 1.0f - 2.0f * (x * x + z * z);
    rot.at(1, 2) = 2.0f * (y * z - x * w);
    rot.at(2, 0) = 2.0f * (x * z - y * w);
    rot.at(2, 1) = 2.0f * (y * z + x * w);
    rot.at(2, 2) = 1.0f - 2.0f * (x * x + y * y);

    const math::Mat4 scale = math::Mat4::scaling(t.scale);
    return math::multiply(math::multiply(math::Mat4::translation(t.position), rot), scale);
}

} // namespace

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

Status SceneGraph::set_local_transform(NodeHandle node, const Transform& transform) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(node)) {
        return Status::fail(ErrorCode::NotFound, "node handle is not live");
    }
    if (!valid(transform)) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid transform");
    }
    nodes_[node.index].local = transform;
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

Result<Transform> SceneGraph::local_transform(NodeHandle node) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const auto* n = handle_live(node) ? find(node) : nullptr;
    if (n == nullptr) {
        return Result<Transform>::fail(ErrorCode::NotFound, "node handle is not live");
    }
    return Result<Transform>::success(n->local);
}

math::Mat4 SceneGraph::compute_global(NodeHandle handle) const {
    const auto* n = find(handle);
    if (n == nullptr || !n->alive) {
        return math::Mat4::identity();
    }
    const math::Mat4 local = compose_trs(n->local);
    if (!is_valid(n->parent)) {
        return local;
    }
    return math::multiply(compute_global(n->parent), local);
}

Result<math::Mat4> SceneGraph::global_matrix(NodeHandle node) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(node)) {
        return Result<math::Mat4>::fail(ErrorCode::NotFound, "node handle is not live");
    }
    return Result<math::Mat4>::success(compute_global(node));
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
