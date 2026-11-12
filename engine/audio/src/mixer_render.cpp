#include "poko/audio/mixer_device.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "mixer_detail.h"

namespace poko {

std::uint32_t MixerAudioDevice::render(std::span<float> out, std::uint32_t sample_rate) {
    std::lock_guard<std::mutex> guard(mutex_);
    std::fill(out.begin(), out.end(), 0.0f);
    if (sample_rate == 0 || out.empty()) {
        peak_ = 0.0f;
        return 0;
    }

    const double step =
        static_cast<double>(audio::detail::kInternalRate) / static_cast<double>(sample_rate);
    const std::size_t frames = out.size();
    std::uint32_t voices = 0;

    for (auto& sound : sounds_) {
        if (!sound.alive || !sound.playing || sound.paused || sound.pcm.empty()) {
            continue;
        }
        ++voices;
        const float gain = sound.volume * audio::detail::attenuation(sound.desc, listener_);
        const double length = static_cast<double>(sound.pcm.size());
        const double start = sound.cursor;
        bool ended = false;

        for (std::size_t i = 0; i < frames; ++i) {
            double position = start + static_cast<double>(i) * step;
            if (sound.desc.loop) {
                position = std::fmod(position, length);
            } else if (position >= length - 1.0) {
                ended = true;
                break; // remaining frames stay silent
            }
            if (gain > 0.0f) {
                out[i] += audio::detail::sample_linear(sound.pcm, position) * gain;
            }
        }

        const double next = start + static_cast<double>(frames) * step;
        if (sound.desc.loop) {
            sound.cursor = std::fmod(next, length);
        } else if (ended || next >= length - 1.0) {
            sound.playing = false;
            sound.cursor = length - 1.0;
        } else {
            sound.cursor = next;
        }
    }

    float peak = 0.0f;
    for (float& sample : out) {
        sample = std::clamp(sample, -1.0f, 1.0f);
        peak = std::max(peak, std::abs(sample));
    }
    peak_ = peak;
    return voices;
}

} // namespace poko
