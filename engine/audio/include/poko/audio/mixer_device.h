#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

#include "poko/core/api_seams.h"

namespace poko {

// PRODUCTION software audio mixer (systemplan/07). Implements the whole
// IAudioDevice seam with real DSP: deterministic PCM synthesis at load time,
// voice state (play/stop/pause/volume/loop), linear resampling, positional
// distance attenuation against the listener, and mono mixdown into a caller
// supplied buffer. Standard library only.
//
// OS audio output (miniaudio) is deliberately deferred: it is NOT VENDORED in
// third_party/DEPENDENCIES.md and may not be pinned speculatively (see the
// deferral recorded in systemplan/28). render() is the output boundary a real
// device callback will drive once that dependency is pinned.
//
// Thread-safety contract: synchronised; any thread may call any method.
class MixerAudioDevice final : public api::IAudioDevice {
public:
    MixerAudioDevice() = default;

    bool initialize() noexcept override;
    Result<SoundHandle> load_sound(const SoundDesc& desc) override;
    Status unload_sound(SoundHandle sound) override;
    Status play(SoundHandle sound) override;
    Status stop(SoundHandle sound) override;
    Status set_volume(SoundHandle sound, float volume) override;
    Status set_paused(SoundHandle sound, bool paused) override;
    bool is_playing(SoundHandle sound) const override;
    Status set_listener(const ListenerState& listener) override;
    AudioStats stats() const override;

    // Mixes `out.size()` mono float samples at sample_rate, returning the
    // number of active voices mixed. Output is clamped to [-1, 1].
    std::uint32_t render(std::span<float> out, std::uint32_t sample_rate);
    // Backend-specific introspection, outside the seam.
    std::uint32_t sample_count(SoundHandle sound) const;
    float peak_level() const noexcept;

private:
    struct Sound {
        bool alive = false;
        SoundDesc desc{};
        std::vector<float> pcm;
        double cursor = 0.0;
        bool playing = false;
        bool paused = false;
        float volume = 1.0f;
    };

    bool handle_live(SoundHandle handle) const noexcept; // caller holds mutex_
    Sound* find(SoundHandle handle) noexcept;            // caller holds mutex_
    const Sound* find(SoundHandle handle) const noexcept;

    mutable std::mutex mutex_;
    bool initialized_ = false;
    ListenerState listener_{};
    std::vector<Sound> sounds_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    std::uint64_t voices_played_ = 0;
    float peak_ = 0.0f;
};

} // namespace poko
