#include "poko_test_harness.h"

#include "poko_scene_graph.h"

POKO_TEST(hierarchy_and_transforms) {
    poko::SceneGraph graph;
    poko::NodeDesc root;
    root.name = "root";
    const auto root_handle = graph.create_node(root);
    POKO_CHECK(root_handle.ok());
    POKO_CHECK_EQ(graph.node_count(), 1u);

    poko::NodeDesc child;
    child.name = "child";
    child.parent = root_handle.value();
    child.local.position = {0.0f, 2.0f, 0.0f};
    child.local.scale = {2.0f, 2.0f, 2.0f};
    const auto child_handle = graph.create_node(child);
    POKO_CHECK(child_handle.ok());

    const auto m = graph.global_matrix(child_handle.value());
    POKO_CHECK(m.ok());
    POKO_CHECK_EQ(m.value().at(1, 3), 2.0f); // world y = parent(0) + local(2)
    POKO_CHECK_EQ(m.value().at(0, 0), 2.0f); // scale applied
}

POKO_TEST(destroy_cascades_to_descendants) {
    poko::SceneGraph graph;
    const auto a = graph.create_node(poko::NodeDesc{});
    poko::NodeDesc b;
    b.parent = a.value();
    const auto b_handle = graph.create_node(b);
    POKO_CHECK(b_handle.ok());

    POKO_CHECK(graph.destroy_node(a.value()).ok());
    POKO_CHECK_EQ(graph.node_count(), 0u);
    POKO_CHECK(!graph.contains(b_handle.value()));
}

POKO_TEST(reparent_rejects_cycles) {
    poko::SceneGraph graph;
    const auto a = graph.create_node(poko::NodeDesc{});
    poko::NodeDesc b;
    b.parent = a.value();
    const auto b_handle = graph.create_node(b);
    POKO_CHECK(b_handle.ok());

    // Making a a child of b would cycle.
    POKO_CHECK(!graph.set_parent(a.value(), b_handle.value()).ok());
    POKO_CHECK(graph.set_parent(b_handle.value(), {}).ok()); // to root ok
}

POKO_TEST(invalid_handles_rejected) {
    poko::SceneGraph graph;
    POKO_CHECK(!graph.set_local_transform({}, {}).ok());
    POKO_CHECK(!graph.global_matrix({}).ok());
    poko::NodeDesc bad;
    bad.local.scale = {0.0f, 1.0f, 1.0f};
    POKO_CHECK(!graph.create_node(bad).ok());
}

POKO_TEST_MAIN()
