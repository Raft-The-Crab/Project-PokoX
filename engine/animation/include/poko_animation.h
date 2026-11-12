#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "poko_math.h"
#include "poko_scene_contracts.h"

namespace poko::anim {

// Animation skeleton (systemplan/07): keyframe clips + deterministic
// sampling. Tracks bind to named targets (bones/nodes); skeletal skinning
// and blend trees come later.

enum class ChannelTarget : std::uint8_t {
    Translation,
    Rotation,
    Scale,
};

struct Vec3Key {
    float time = 0.0f; // seconds
    math::Vec3 value;
};

struct QuatKey {
    float time = 0.0f;
    math::Quat value;
};

// One animated channel on a named target. vec_keys serve Translation/Scale,
// rot_keys serve Rotation; the unused list stays empty.
struct AnimTrack {
    std::string target;
    ChannelTarget channel = ChannelTarget::Translation;
    std::vector<Vec3Key> vec_keys;
    std::vector<QuatKey> rot_keys;
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimTrack> tracks;
};

// One sampled pose row: target name + its local TRS at the sample time.
struct PoseChannel {
    std::string target;
    Transform transform;
};

// Validates clip structure (non-empty name optional; tracks need sorted,
// non-empty keys within [0, duration]).
bool valid(const AnimationClip& clip) noexcept;

// Samples the clip at `time` seconds. time is clamped to [0, duration].
// Linear interpolation for vec tracks, normalized-lerp for rotations.
// Returns one PoseChannel per distinct track target that has keys.
std::vector<PoseChannel> sample(const AnimationClip& clip, float time) noexcept;

} // namespace poko::anim
