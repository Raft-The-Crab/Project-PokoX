#pragma once

// Audio contracts (systemplan/07). POD-only sound descriptors and opaque
// handles so the IAudioDevice seam stays backend-free. miniaudio is the
// planned production backend (systemplan/07); a MOCK NullAudioDevice
// implements the seam for offline tests.
//
// Coordinates/units: sample_rate in Hz, amplitudes normalized to [-1, 1],
// distances in metres (same as poko_math.h).

#include <cstdint>
#include <string_view>

#include "poko/core/types.h"

namespace poko {

using SoundHandle = TypedHandle;

enum class SoundKind : std::uint8_t {
    Unknown = 0,
    Sfx,     // short one-shot effect
    Music,   // looping stream
    Ambient, // positional loop
    Voice,   // dialogue/voice
};

struct SoundDesc {
    SoundKind kind = SoundKind::Sfx;
    std::string_view logical_path; // package-relative, never a raw OS path
    float volume = 1.0f;           // 0..1 master gain
    float pitch = 1.0f;            // playback rate multiplier
    bool loop = false;
    // Positional audio (skeleton): world position; ignored when !positional.
    bool positional = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float min_distance = 1.0f;  // metres, full volume inside
    float max_distance = 50.0f; // metres, silent beyond
};

struct ListenerState {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    // Forward direction (unit vector); default looks down -Z.
    float fx = 0.0f;
    float fy = 0.0f;
    float fz = -1.0f;
};

struct AudioStats {
    std::uint32_t active_voices = 0;
    std::uint32_t total_voices_played = 0;
};

// Input validation at the module boundary.
inline bool valid(const SoundDesc& desc) noexcept {
    if (desc.kind == SoundKind::Unknown || desc.logical_path.empty()) {
        return false;
    }
    if (desc.volume < 0.0f || desc.volume > 1.0f) {
        return false;
    }
    if (desc.pitch <= 0.0f) {
        return false;
    }
    if (desc.positional && !(desc.min_distance > 0.0f && desc.max_distance > desc.min_distance)) {
        return false;
    }
    return true;
}

} // namespace poko
