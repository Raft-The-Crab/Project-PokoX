#include "poko_test_harness.h"

#include "poko/lighting/light_list.h"

POKO_TEST(light_list_lifecycle) {
    poko::lighting::LightList lights;
    poko::LightDesc desc;
    desc.kind = poko::LightKind::Point;
    desc.point.position = {1.0f, 2.0f, 3.0f};
    desc.point.radius = 8.0f;

    const auto handle = lights.add(desc);
    POKO_CHECK(handle.ok());
    POKO_CHECK_EQ(lights.count(), 1u);

    const auto got = lights.get(handle.value());
    POKO_CHECK(got.ok());
    POKO_CHECK_EQ(got.value().point.radius, 8.0f);

    const auto frame = lights.frame_lights();
    POKO_CHECK_EQ(frame.size(), 1u);
    POKO_CHECK(frame[0].kind == poko::LightKind::Point);

    POKO_CHECK(lights.remove(handle.value()).ok());
    POKO_CHECK_EQ(lights.count(), 0u);
    POKO_CHECK(!lights.get(handle.value()).ok());
}

POKO_TEST(invalid_and_stale_rejected) {
    poko::lighting::LightList lights;
    POKO_CHECK(!lights.add(poko::LightDesc{}).ok());
    POKO_CHECK(!lights.remove({}).ok());
    POKO_CHECK(!lights.get({}).ok());
}

POKO_TEST(ambient_roundtrip) {
    poko::lighting::LightList lights;
    poko::AmbientLight ambient;
    ambient.intensity = 0.5f;
    lights.set_ambient(ambient);
    POKO_CHECK_EQ(lights.ambient().intensity, 0.5f);
}

POKO_TEST_MAIN()
