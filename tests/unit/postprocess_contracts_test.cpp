#include "poko_test_harness.h"

#include "poko_postprocess_contracts.h"

POKO_TEST(effect_descriptors_validate) {
    POKO_CHECK(!poko::valid(poko::PostProcessDesc{}));

    poko::PostProcessDesc tone;
    tone.kind = poko::PostProcessKind::ToneMap;
    POKO_CHECK(poko::valid(tone));
    tone.tone_map.exposure = -1.0f;
    POKO_CHECK(!poko::valid(tone));

    tone.kind = poko::PostProcessKind::Vignette;
    tone.vignette.intensity = 1.5f;
    POKO_CHECK(!poko::valid(tone));

    tone.kind = poko::PostProcessKind::FXAA;
    POKO_CHECK(poko::valid(tone));
}

POKO_TEST_MAIN()
