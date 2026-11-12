#include "poko_test_harness.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "poko/audio/mixer_device.h"
#include "poko/core/audio_contracts.h"

namespace {
using poko::ListenerState;
using poko::MixerAudioDevice;
using poko::SoundDesc;
using poko::SoundKind;

SoundDesc make_sound(const char* path = "sfx/test.pcm") {
    SoundDesc desc;
    desc.kind = SoundKind::Sfx;
    desc.logical_path = path;
    desc.volume = 1.0f;
    return desc;
}
} // namespace

POKO_TEST(initialize_validation_and_generation_checked_handles) {
    MixerAudioDevice device;
    POKO_CHECK(!device.load_sound(make_sound()).ok()); // not initialized
    POKO_CHECK(device.initialize());
    POKO_CHECK(!device.load_sound(SoundDesc{}).ok()); // invalid descriptor

    auto first = device.load_sound(make_sound());
    POKO_CHECK(first.ok());
    POKO_CHECK(device.sample_count(first.value()) > 0u);
    POKO_CHECK(!device.set_volume(first.value(), 1.5f).ok());
    POKO_CHECK(device.set_volume(first.value(), 0.5f).ok());

    POKO_CHECK(device.unload_sound(first.value()).ok());
    POKO_CHECK(!device.unload_sound(first.value()).ok());
    POKO_CHECK_EQ(device.sample_count(first.value()), 0u);
    auto second = device.load_sound(make_sound());
    POKO_CHECK(second.ok());
    POKO_CHECK(!poko::same_handle(first.value(), second.value()));
    POKO_CHECK_EQ(device.sample_count(second.value()), device.sample_count(second.value()));
}

POKO_TEST(voice_lifecycle_and_stats) {
    MixerAudioDevice device;
    POKO_CHECK(device.initialize());
    auto a = device.load_sound(make_sound("sfx/a.pcm"));
    auto b = device.load_sound(make_sound("sfx/b.pcm"));
    POKO_CHECK(a.ok() && b.ok());

    POKO_CHECK(device.play(a.value()).ok());
    POKO_CHECK(device.play(a.value()).ok()); // idempotent, one voice
    POKO_CHECK(device.is_playing(a.value()));
    POKO_CHECK_EQ(device.stats().active_voices, 1u);
    POKO_CHECK_EQ(device.stats().total_voices_played, 1u);

    POKO_CHECK(device.set_paused(a.value(), true).ok());
    POKO_CHECK(!device.is_playing(a.value()));
    POKO_CHECK_EQ(device.stats().active_voices, 0u);
    POKO_CHECK(device.set_paused(a.value(), false).ok());
    POKO_CHECK(device.is_playing(a.value()));

    POKO_CHECK(device.stop(a.value()).ok());
    POKO_CHECK(!device.is_playing(a.value()));
    POKO_CHECK(device.play(b.value()).ok());
    POKO_CHECK_EQ(device.stats().total_voices_played, 2u);
    POKO_CHECK_EQ(device.stats().active_voices, 1u);
}

POKO_TEST(render_mixes_signal_and_respects_volume) {
    MixerAudioDevice device;
    POKO_CHECK(device.initialize());
    auto sound = device.load_sound(make_sound());
    POKO_CHECK(sound.ok());
    std::vector<float> buffer(256, 0.0f);

    POKO_CHECK(device.play(sound.value()).ok());
    POKO_CHECK_EQ(device.render(buffer, 48000), 1u);
    const float loud_peak = device.peak_level();
    POKO_CHECK(loud_peak > 0.0f);

    POKO_CHECK(device.stop(sound.value()).ok());
    POKO_CHECK_EQ(device.render(buffer, 48000), 0u);
    POKO_CHECK_EQ(device.peak_level(), 0.0f);
    for (const float sample : buffer) {
        POKO_CHECK_EQ(sample, 0.0f);
    }

    POKO_CHECK(device.set_volume(sound.value(), 0.5f).ok());
    POKO_CHECK(device.play(sound.value()).ok());
    POKO_CHECK_EQ(device.render(buffer, 48000), 1u);
    const float half_peak = device.peak_level();
    POKO_CHECK(half_peak > 0.4f * loud_peak && half_peak < 0.6f * loud_peak);
}

POKO_TEST(non_looping_voice_ends_and_looping_voice_wraps) {
    MixerAudioDevice device;
    POKO_CHECK(device.initialize());

    auto one_shot = device.load_sound(make_sound("sfx/one.pcm"));
    POKO_CHECK(one_shot.ok());
    POKO_CHECK(device.play(one_shot.value()).ok());
    std::vector<float> long_buffer(13000, 0.0f); // > 0.25s at 48kHz
    POKO_CHECK_EQ(device.render(long_buffer, 48000), 1u);
    POKO_CHECK(!device.is_playing(one_shot.value()));

    SoundDesc looping = make_sound("music/loop.pcm");
    looping.kind = SoundKind::Music;
    looping.loop = true;
    auto music = device.load_sound(looping);
    POKO_CHECK(music.ok());
    POKO_CHECK(device.play(music.value()).ok());
    std::vector<float> huge_buffer(100000, 0.0f); // > 2s at 48kHz
    POKO_CHECK_EQ(device.render(huge_buffer, 48000), 1u);
    POKO_CHECK(device.is_playing(music.value())); // wrapped, still playing
}

POKO_TEST(positional_attenuation_and_listener_validation) {
    MixerAudioDevice device;
    POKO_CHECK(device.initialize());

    SoundDesc positional = make_sound("sfx/pos.pcm");
    positional.positional = true;
    positional.x = 0.0f;
    positional.y = 0.0f;
    positional.z = 0.0f;
    positional.min_distance = 1.0f;
    positional.max_distance = 20.0f;
    auto sound = device.load_sound(positional);
    POKO_CHECK(sound.ok());

    ListenerState near;
    near.x = 0.0f;
    near.y = 0.0f;
    near.z = 0.0f;
    POKO_CHECK(device.set_listener(near).ok());
    std::vector<float> buffer(256, 0.0f);
    POKO_CHECK(device.play(sound.value()).ok());
    POKO_CHECK_EQ(device.render(buffer, 48000), 1u);
    const float near_peak = device.peak_level();
    POKO_CHECK(near_peak > 0.0f);

    ListenerState far;
    far.x = 100.0f; // beyond max_distance -> silent
    POKO_CHECK(device.set_listener(far).ok());
    POKO_CHECK(device.stop(sound.value()).ok());
    POKO_CHECK(device.play(sound.value()).ok());
    POKO_CHECK_EQ(device.render(buffer, 48000), 1u);
    POKO_CHECK_EQ(device.peak_level(), 0.0f);

    ListenerState invalid;
    invalid.x = std::numeric_limits<float>::quiet_NaN();
    POKO_CHECK(!device.set_listener(invalid).ok());
}

POKO_TEST_MAIN()
