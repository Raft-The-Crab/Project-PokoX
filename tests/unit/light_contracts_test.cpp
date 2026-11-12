#include "poko_test_harness.h"

#include "poko_light_contracts.h"

POKO_TEST(light_descriptors_validate) {
    POKO_CHECK(!poko::valid(poko::LightDesc{}));

    poko::LightDesc dir;
    dir.kind = poko::LightKind::Directional;
    dir.directional.direction = {0.0f, -1.0f, 0.0f};
    POKO_CHECK(poko::valid(dir));
    dir.directional.direction = {0.0f, 0.0f, 0.0f};
    POKO_CHECK(!poko::valid(dir));

    dir.kind = poko::LightKind::Spot;
    dir.spot.direction = {0.0f, 0.0f, -1.0f};
    dir.spot.inner_angle = 0.9f;
    dir.spot.outer_angle = 0.5f; // outer <= inner: invalid
    POKO_CHECK(!poko::valid(dir));
}

POKO_TEST_MAIN()
