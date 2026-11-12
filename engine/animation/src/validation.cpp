#include "poko/animation/animation.h"

#include <algorithm>

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

} // namespace poko::anim
