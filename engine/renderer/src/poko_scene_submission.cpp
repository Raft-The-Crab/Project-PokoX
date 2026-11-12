#include "poko_scene_submission.h"

#include <algorithm>
#include <utility>

namespace poko {

void SceneSubmission::submit(const DrawCall& call) {
    if (valid(call)) {
        draws_.push_back(call);
    }
}

void SceneSubmission::clear() noexcept {
    draws_.clear();
    batches_ = 0;
}

bool SceneSubmission::same_batch(const DrawCall& a, const DrawCall& b) noexcept {
    if (a.pipeline.index != b.pipeline.index || a.pipeline.generation != b.pipeline.generation ||
        a.vertex_buffer.index != b.vertex_buffer.index ||
        a.vertex_buffer.generation != b.vertex_buffer.generation ||
        a.index_buffer.index != b.index_buffer.index ||
        a.index_buffer.generation != b.index_buffer.generation ||
        a.uniform_buffer.index != b.uniform_buffer.index ||
        a.uniform_buffer.generation != b.uniform_buffer.generation ||
        a.uniform_offset != b.uniform_offset || a.index_type != b.index_type ||
        a.texture_count != b.texture_count || a.vertex_count != b.vertex_count ||
        a.index_count != b.index_count || a.sort_key != b.sort_key) {
        return false;
    }
    for (std::uint32_t i = 0; i < a.texture_count; ++i) {
        if (a.textures[i].index != b.textures[i].index ||
            a.textures[i].generation != b.textures[i].generation ||
            a.samplers[i].index != b.samplers[i].index ||
            a.samplers[i].generation != b.samplers[i].generation) {
            return false;
        }
    }
    return true;
}

void SceneSubmission::batch() {
    std::stable_sort(draws_.begin(), draws_.end(), [](const DrawCall& a, const DrawCall& b) {
        if (a.pipeline.index != b.pipeline.index) {
            return a.pipeline.index < b.pipeline.index;
        }
        if (a.pipeline.generation != b.pipeline.generation) {
            return a.pipeline.generation < b.pipeline.generation;
        }
        return a.sort_key < b.sort_key;
    });

    std::vector<DrawCall> merged;
    merged.reserve(draws_.size());
    for (const auto& call : draws_) {
        if (!merged.empty() && same_batch(merged.back(), call)) {
            merged.back().instance_count += call.instance_count;
        } else {
            merged.push_back(call);
        }
    }
    draws_ = std::move(merged);

    batches_ = 0;
    for (std::size_t i = 0; i < draws_.size(); ++i) {
        if (i == 0 || draws_[i].pipeline.index != draws_[i - 1].pipeline.index ||
            draws_[i].pipeline.generation != draws_[i - 1].pipeline.generation) {
            ++batches_;
        }
    }
}

std::uint64_t SceneSubmission::total_triangles() const {
    std::uint64_t total = 0;
    for (const auto& call : draws_) {
        const std::uint32_t indices = call.index_count > 0 ? call.index_count : call.vertex_count;
        total += static_cast<std::uint64_t>(indices / 3U) * call.instance_count;
    }
    return total;
}

} // namespace poko
