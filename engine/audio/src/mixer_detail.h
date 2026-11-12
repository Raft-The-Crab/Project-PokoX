#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "poko/core/audio_contracts.h"

namespace poko::audio::detail {

inline constexpr std::uint32_t kInternalRate = 48000;
inline constexpr float kTwoPi = 6.28318530718f;

inline std::uint32_t hash_path(std::string_view text) noexcept {
    std::uint32_t hash = 2166136261u;
    for (const char character : text) {
        hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(character));
        hash *= 16777619u;
    }
    return hash;
}

inline float duration_for(SoundKind kind) noexcept {
    switch (kind) {
    case SoundKind::Sfx:
        return 0.25f;
    case SoundKind::Music:
        return 2.0f;
    case SoundKind::Ambient:
        return 1.0f;
    case SoundKind::Voice:
        return 0.5f;
    case SoundKind::Unknown:
        break;
    }
    return 0.25f;
}

// Deterministic mono waveform: fundamental + a soft second harmonic with a
// short attack and (for non-looping sounds) a release envelope. No RNG state,
// so the same descriptor always yields identical samples.
inline std::vector<float> synthesize(const SoundDesc& desc) {
    const float duration = duration_for(desc.kind);
    const std::size_t frames = static_cast<std::size_t>(duration * kInternalRate);
    const float base = 180.0f + static_cast<float>(hash_path(desc.logical_path) % 240u);
    const float frequency = base * desc.pitch;
    std::vector<float> out(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kInternalRate);
        const float phase = kTwoPi * frequency * t;
        float envelope = std::fmin(1.0f, t / 0.01f);
        if (!desc.loop) {
            envelope *= std::fmin(1.0f, (duration - t) / 0.02f);
        }
        out[i] = 0.25f * (std::sin(phase) + 0.25f * std::sin(2.0f * phase)) * envelope;
    }
    return out;
}

// Linear distance attenuation in [min_distance, max_distance]; non-positional
// sounds are always full gain.
inline float attenuation(const SoundDesc& desc, const ListenerState& listener) noexcept {
    if (!desc.positional) {
        return 1.0f;
    }
    const float dx = desc.x - listener.x;
    const float dy = desc.y - listener.y;
    const float dz = desc.z - listener.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance <= desc.min_distance) {
        return 1.0f;
    }
    if (distance >= desc.max_distance) {
        return 0.0f;
    }
    return 1.0f - (distance - desc.min_distance) / (desc.max_distance - desc.min_distance);
}

inline float sample_linear(const std::vector<float>& pcm, double position) noexcept {
    if (pcm.empty()) {
        return 0.0f;
    }
    const double last = static_cast<double>(pcm.size() - 1);
    const double clamped = position < 0.0 ? 0.0 : (position > last ? last : position);
    const std::size_t i = static_cast<std::size_t>(clamped);
    const std::size_t j = i + 1 < pcm.size() ? i + 1 : i;
    const float fraction = static_cast<float>(clamped - static_cast<double>(i));
    return pcm[i] + (pcm[j] - pcm[i]) * fraction;
}

} // namespace poko::audio::detail
