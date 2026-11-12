#include "poko_test_harness.h"

#include "poko/core/audio_contracts.h"

POKO_TEST(sound_descriptors_validate) {
    POKO_CHECK(!poko::valid(poko::SoundDesc{}));
    poko::SoundDesc desc;
    desc.logical_path = "sfx/hit.wav";
    POKO_CHECK(poko::valid(desc));
    desc.volume = 1.5f;
    POKO_CHECK(!poko::valid(desc));
    desc.volume = 1.0f;
    desc.pitch = 0.0f;
    POKO_CHECK(!poko::valid(desc));
    desc.pitch = 1.0f;
    desc.positional = true;
    desc.min_distance = 5.0f;
    desc.max_distance = 2.0f;
    POKO_CHECK(!poko::valid(desc)); // max <= min
}

POKO_TEST_MAIN()
