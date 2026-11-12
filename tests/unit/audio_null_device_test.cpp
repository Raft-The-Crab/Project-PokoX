#include "poko_test_harness.h"

#include "poko_audio_device.h"

POKO_TEST(device_requires_initialize) {
    poko::NullAudioDevice device;
    poko::SoundDesc desc;
    desc.logical_path = "sfx/hit.wav";
    POKO_CHECK(!device.load_sound(desc).ok());
    POKO_CHECK(device.initialize());
    POKO_CHECK(device.load_sound(desc).ok());
}

POKO_TEST(sound_lifecycle_and_playback) {
    poko::NullAudioDevice device;
    POKO_CHECK(device.initialize());
    poko::SoundDesc desc;
    desc.logical_path = "music/theme.ogg";
    desc.kind = poko::SoundKind::Music;
    desc.loop = true;
    const auto sound = device.load_sound(desc);
    POKO_CHECK(sound.ok());

    POKO_CHECK(!device.is_playing(sound.value()));
    POKO_CHECK(device.play(sound.value()).ok());
    POKO_CHECK(device.is_playing(sound.value()));
    POKO_CHECK_EQ(device.stats().active_voices, 1u);

    POKO_CHECK(device.set_paused(sound.value(), true).ok());
    POKO_CHECK(!device.is_playing(sound.value()));
    POKO_CHECK_EQ(device.stats().active_voices, 0u);

    POKO_CHECK(device.stop(sound.value()).ok());
    POKO_CHECK(!device.is_playing(sound.value()));

    POKO_CHECK(device.set_volume(sound.value(), 0.5f).ok());
    POKO_CHECK(!device.set_volume(sound.value(), 2.0f).ok());

    POKO_CHECK(device.unload_sound(sound.value()).ok());
    POKO_CHECK(!device.play(sound.value()).ok());
}

POKO_TEST(listener_state_accepted) {
    poko::NullAudioDevice device;
    POKO_CHECK(device.initialize());
    poko::ListenerState listener;
    listener.x = 1.0f;
    POKO_CHECK(device.set_listener(listener).ok());
}

POKO_TEST_MAIN()
