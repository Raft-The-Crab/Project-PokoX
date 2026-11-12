#include "poko/scene/graph.h"

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

} // namespace poko
