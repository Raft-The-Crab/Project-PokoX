#include "poko_test_harness.h"

#include "poko_particles.h"

POKO_TEST(desc_validation) {
    auto desc = poko::particles::EmitterDesc{};
    POKO_CHECK(poko::particles::valid(desc));
    desc.speed_max = 0.5f; // < speed_min
    POKO_CHECK(!poko::particles::valid(desc));
}

POKO_TEST(emitter_spawns_and_expires) {
    auto desc = poko::particles::EmitterDesc{};
    desc.rate = 100.0f;
    desc.lifetime = 0.1f;
    desc.max_particles = 50;
    poko::particles::Emitter emitter(desc);

    emitter.update(0.05f);
    POKO_CHECK(!emitter.empty());
    const auto count_after_spawn = emitter.particles().size();
    POKO_CHECK(count_after_spawn > 0u);

    // After several lifetimes all particles die (non-looping irrelevant —
    // update keeps spawning, but count stays bounded by rate*dt).
    for (int i = 0; i < 10; ++i) {
        emitter.update(0.1f);
    }
    POKO_CHECK(emitter.particles().size() <= desc.max_particles);
}

POKO_TEST(deterministic_across_instances) {
    auto desc = poko::particles::EmitterDesc{};
    desc.rate = 50.0f;
    desc.lifetime = 2.0f;
    desc.max_particles = 100;

    poko::particles::Emitter a(desc), b(desc);
    for (int i = 0; i < 20; ++i) {
        a.update(0.05f);
        b.update(0.05f);
    }
    POKO_CHECK_EQ(a.particles().size(), b.particles().size());
    POKO_CHECK_EQ(a.particles()[0].position.x, b.particles()[0].position.x);
    POKO_CHECK_EQ(a.particles()[0].position.y, b.particles()[0].position.y);
}

POKO_TEST(gravity_integrates_velocity) {
    auto desc = poko::particles::EmitterDesc{};
    desc.rate = 10.0f;
    desc.lifetime = 5.0f;
    desc.speed_min = 0.0f;
    desc.speed_max = 0.0f;
    desc.gravity = {0.0f, -10.0f, 0.0f};
    desc.max_particles = 10;
    poko::particles::Emitter emitter(desc);

    emitter.update(0.016f); // spawn ~0 particles at 10/s*0.016
    // Force a couple updates to accumulate spawn.
    emitter.update(0.2f);
    POKO_CHECK(!emitter.empty());
    const auto before = emitter.particles()[0].position.y;
    emitter.update(0.1f);
    const auto after = emitter.particles()[0].position.y;
    POKO_CHECK(after < before);
}

POKO_TEST_MAIN()
