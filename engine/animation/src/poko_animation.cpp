#include "poko_animation.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace poko::anim {
namespace {

bool keys_sorted(const std::vector<Vec3Key>& keys) noexcept {
    for (std::size_t i = 1; i < keys.size(); ++i) {
        if (keys[i].time < keys[i - 1].time) {
            return false;
        }
    }
    return true;
}

bool keys_sorted(const std::vector<QuatKey>& keys) noexcept {
    for (std::size_t i = 1; i < keys.size(); ++i) {
        if (keys[i].time < keys[i - 1].time) {
            return false;
        }
    }
    return true;
}

template <typename Key, typename Interp>
auto sample_track(const std::vector<Key>& keys, float time, Interp interpolate)
    -> std::invoke_result_t<Interp, const Key&, const Key&, float> {
    using Sample = std::invoke_result_t<Interp, const Key&, const Key&, float>;
    if (keys.empty()) {
        return Sample{};
    }
    if (keys.size() == 1 || time <= keys.front().time) {
        return keys.front().value;
    }
    if (time >= keys.back().time) {
        return keys.back().value;
    }
    auto it = std::upper_bound(keys.begin(), keys.end(), time,
                               [](float t, const Key& k) { return t < k.time; });
    const Key& hi = *it;
    const Key& lo = *(it - 1);
    const float span = hi.time - lo.time;
    const float t = span > 0.0f ? (time - lo.time) / span : 0.0f;
    return interpolate(lo, hi, t);
}

math::Vec3 lerp_vec(const Vec3Key& a, const Vec3Key& b, float t) {
    return math::lerp(a.value, b.value, t);
}

math::Quat lerp_quat(const QuatKey& a, const QuatKey& b, float t) {
    // Nlerp with hemisphere fix; adequate for skeleton poses.
    math::Quat qa = math::normalize(a.value);
    math::Quat qb = math::normalize(b.value);
    const float dot = qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w;
    if (dot < 0.0f) {
        qb = math::Quat{-qb.x, -qb.y, -qb.z, -qb.w};
    }
    return math::normalize(math::Quat{qa.x + (qb.x - qa.x) * t, qa.y + (qb.y - qa.y) * t,
                                      qa.z + (qb.z - qa.z) * t, qa.w + (qb.w - qa.w) * t});
}

} // namespace

bool valid(const AnimationClip& clip) noexcept {
    if (clip.duration < 0.0f) {
        return false;
    }
    for (const auto& track : clip.tracks) {
        if (track.target.empty()) {
            return false;
        }
        switch (track.channel) {
        case ChannelTarget::Translation:
        case ChannelTarget::Scale:
            if (track.vec_keys.empty() || !keys_sorted(track.vec_keys)) {
                return false;
            }
            break;
        case ChannelTarget::Rotation:
            if (track.rot_keys.empty() || !keys_sorted(track.rot_keys)) {
                return false;
            }
            break;
        }
    }
    return true;
}

std::vector<PoseChannel> sample(const AnimationClip& clip, float time) noexcept {
    const float t = std::clamp(time, 0.0f, clip.duration);
    // Merge tracks per target: translation/rotation/scale compose one TRS.
    std::unordered_map<std::string, Transform> poses;
    std::vector<std::string> order;
    for (const auto& track : clip.tracks) {
        auto [it, inserted] = poses.try_emplace(track.target);
        if (inserted) {
            order.push_back(track.target);
            it->second.scale = {1.0f, 1.0f, 1.0f};
        }
        Transform& pose = it->second;
        switch (track.channel) {
        case ChannelTarget::Translation:
            pose.position =
                sample_track(track.vec_keys, t, [](const Vec3Key& a, const Vec3Key& b, float f) {
                    return lerp_vec(a, b, f);
                });
            break;
        case ChannelTarget::Scale:
            pose.scale =
                sample_track(track.vec_keys, t, [](const Vec3Key& a, const Vec3Key& b, float f) {
                    return lerp_vec(a, b, f);
                });
            break;
        case ChannelTarget::Rotation:
            pose.rotation =
                sample_track(track.rot_keys, t, [](const QuatKey& a, const QuatKey& b, float f) {
                    return lerp_quat(a, b, f);
                });
            break;
        }
    }
    std::vector<PoseChannel> out;
    out.reserve(order.size());
    for (const auto& target : order) {
        out.push_back(PoseChannel{target, poses[target]});
    }
    return out;
}

} // namespace poko::anim
