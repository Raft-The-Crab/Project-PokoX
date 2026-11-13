#include "poko_test_harness.h"

#include <vector>

#include "poko/ecs/registry.h"

namespace {

struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

struct Velocity {
    float dx = 0.0f;
    float dy = 0.0f;
};

} // namespace

POKO_TEST(entity_lifecycle) {
    poko::ecs::Registry registry;
    const auto a = registry.create_entity();
    const auto b = registry.create_entity();
    POKO_CHECK(registry.alive(a));
    POKO_CHECK(registry.alive(b));
    POKO_CHECK_EQ(registry.entity_count(), 2u);

    registry.destroy_entity(a);
    POKO_CHECK(!registry.alive(a));
    POKO_CHECK(registry.alive(b));

    // Slot reuse bumps the generation, so the stale handle stays dead.
    const auto c = registry.create_entity();
    POKO_CHECK(registry.alive(c));
    POKO_CHECK(c.index == a.index);
    POKO_CHECK(c.generation != a.generation);
    POKO_CHECK(!registry.alive(a));
}

POKO_TEST(components_add_get_remove) {
    poko::ecs::Registry registry;
    const auto e = registry.create_entity();

    registry.add_component(e, Position{1.0f, 2.0f});
    registry.add_component(e, Velocity{3.0f, 4.0f});
    POKO_CHECK(registry.has_component<Position>(e));
    POKO_CHECK(registry.has_component<Velocity>(e));
    POKO_CHECK(!registry.has_component<int>(e));

    auto* pos = registry.get_component<Position>(e);
    POKO_CHECK(pos != nullptr);
    POKO_CHECK_EQ(pos->x, 1.0f);
    pos->y = 9.0f; // mutable view
    POKO_CHECK_EQ(registry.get_component<Position>(e)->y, 9.0f);

    POKO_CHECK(registry.remove_component<Velocity>(e));
    POKO_CHECK(!registry.has_component<Velocity>(e));
    POKO_CHECK(!registry.remove_component<Velocity>(e));
}

POKO_TEST(destroy_entity_removes_components) {
    poko::ecs::Registry registry;
    const auto e = registry.create_entity();
    registry.add_component(e, Position{});
    registry.destroy_entity(e);
    POKO_CHECK(!registry.has_component<Position>(e));
}

POKO_TEST(stale_generation_gets_nothing) {
    poko::ecs::Registry registry;
    const auto e = registry.create_entity();
    registry.add_component(e, Position{});
    registry.destroy_entity(e);
    POKO_CHECK(registry.get_component<Position>(e) == nullptr);
}

POKO_TEST(dead_entities_cannot_gain_components) {
    poko::ecs::Registry registry;
    const auto e = registry.create_entity();
    registry.destroy_entity(e);
    POKO_CHECK(!registry.add_component(e, Position{}));
    const auto reused = registry.create_entity(); // same slot, new generation
    POKO_CHECK(!registry.has_component<Position>(reused));
    POKO_CHECK(registry.get_component<Position>(e) == nullptr);
}

POKO_TEST(each_visits_only_entities_with_every_component) {
    poko::ecs::Registry registry;
    std::vector<poko::ecs::Entity> all;
    for (int i = 0; i < 6; ++i) {
        const auto e = registry.create_entity();
        registry.add_component(e, Position{static_cast<float>(i), 0.0f});
        if (i % 2 == 0) registry.add_component(e, Velocity{1.0f, 0.0f});
        all.push_back(e);
    }
    int visited = 0;
    registry.each<Position, Velocity>([&](poko::ecs::Entity, Position& p, Velocity& v) {
        p.x += v.dx * 10.0f;
        ++visited;
    });
    POKO_CHECK_EQ(visited, 3);
    POKO_CHECK_EQ(registry.get_component<Position>(all[0])->x, 10.0f);
    POKO_CHECK_EQ(registry.get_component<Position>(all[1])->x, 1.0f);
    POKO_CHECK_EQ(registry.count<Velocity>(), 3u);
    POKO_CHECK_EQ((registry.entities_with<Position, Velocity>().size()), 3u);
}

POKO_TEST(each_tolerates_structural_changes_in_callbacks) {
    poko::ecs::Registry registry;
    std::vector<poko::ecs::Entity> all;
    for (int i = 0; i < 4; ++i) {
        const auto e = registry.create_entity();
        registry.add_component(e, Position{});
        all.push_back(e);
    }
    int visited = 0;
    registry.each<Position>([&](poko::ecs::Entity e, Position&) {
        ++visited;
        if (e.index == all[0].index) {
            registry.destroy_entity(all[1]);         // later entity dies: skipped
            registry.remove_component<Position>(all[2]); // later entity stops matching
            const auto fresh = registry.create_entity();
            registry.add_component(fresh, Position{}); // added during the pass: not visited
        }
    });
    POKO_CHECK_EQ(visited, 2);
    POKO_CHECK_EQ(registry.count<Position>(), 3u);
}

POKO_TEST(swap_remove_keeps_remaining_components_intact) {
    poko::ecs::Registry registry;
    std::vector<poko::ecs::Entity> all;
    for (int i = 0; i < 5; ++i) {
        const auto e = registry.create_entity();
        registry.add_component(e, Position{static_cast<float>(i), 0.0f});
        all.push_back(e);
    }
    POKO_CHECK(registry.remove_component<Position>(all[1]));
    POKO_CHECK(registry.remove_component<Position>(all[4]));
    for (const int i : {0, 2, 3}) {
        const auto* p = registry.get_component<Position>(all[static_cast<std::size_t>(i)]);
        POKO_CHECK(p != nullptr && p->x == static_cast<float>(i));
    }
    POKO_CHECK_EQ(registry.count<Position>(), 3u);
}

POKO_TEST_MAIN()
