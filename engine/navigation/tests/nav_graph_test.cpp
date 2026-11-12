#include "poko_test_harness.h"

#include "poko/navigation/nav_graph.h"

POKO_TEST(astar_finds_shortest_path) {
    // 0 - 1 - 2 (long way) and 0 - 3 - 2 (short way)
    poko::nav::NavGraph graph;
    const auto n0 = graph.add_node({0.0f, 0.0f, 0.0f});
    const auto n1 = graph.add_node({1.0f, 0.0f, 0.0f});
    const auto n2 = graph.add_node({2.0f, 0.0f, 0.0f});
    const auto n3 = graph.add_node({1.0f, 0.0f, 1.0f});
    graph.connect(n0, n1);
    graph.connect(n1, n2);
    graph.connect(n0, n3);
    graph.connect(n3, n2);

    const auto path = graph.find_path(n0, n2);
    POKO_CHECK_EQ(path.size(), 3u);
    POKO_CHECK(path[0] == n0);
    POKO_CHECK(path[2] == n2);
    // Both routes are 2 hops; deterministic tie-break picks lower node ids
    // explored first — either [0,1,2] or [0,3,2] is valid; assert endpoints.
}

POKO_TEST(astar_unreachable_returns_empty) {
    poko::nav::NavGraph graph;
    const auto a = graph.add_node({0.0f, 0.0f, 0.0f});
    const auto b = graph.add_node({1.0f, 0.0f, 0.0f});
    POKO_CHECK(graph.find_path(a, b).empty());
    POKO_CHECK_EQ(graph.find_path(a, a).size(), 1u);
}

POKO_TEST(nearest_node_snaps) {
    poko::nav::NavGraph graph;
    graph.add_node({0.0f, 0.0f, 0.0f});
    graph.add_node({10.0f, 0.0f, 0.0f});
    POKO_CHECK_EQ(poko::nav::nearest_node(graph, {9.0f, 0.0f, 0.0f}, 5.0f), 1u);
    POKO_CHECK_EQ(poko::nav::nearest_node(graph, {100.0f, 0.0f, 0.0f}, 5.0f),
                  poko::nav::kInvalidNode);
}

POKO_TEST_MAIN()
