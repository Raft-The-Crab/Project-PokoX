#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "poko/core/api_seams.h"

namespace poko {

// MOCK — test-only fake audio device (systemplan/07 skeleton). It records
// sounds and voice state transitions so tests can verify the seam contract.
// It produces NO audio output and does NO decoding — the production backend
// (planned: miniaudio) owns that.
class NullAudioDevice final : public api::IAudioDevice {
public:
    NullAudioDevice() = default;

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

private:
    struct Sound {
        bool alive = false;
        SoundDesc desc;
        bool playing = false;
        bool paused = false;
        float volume = 1.0f;
    };

    Sound* find(SoundHandle handle) noexcept;
    const Sound* find(SoundHandle handle) const noexcept;
    bool handle_live(SoundHandle handle) const noexcept; // caller holds mutex_

    mutable std::mutex mutex_;
    bool initialized_ = false;
    ListenerState listener_{};
    std::vector<Sound> sounds_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    std::uint64_t voices_played_ = 0;
};

} // namespace poko
