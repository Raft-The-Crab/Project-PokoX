# Scene graph skeleton (systemplan/08): hierarchy + transforms, pure logic.
add_library(poko_scene STATIC
    src/graph.cpp)

target_include_directories(poko_scene PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_scene PUBLIC poko_core)
poko_apply_common(poko_scene)
poko_set_impl_label(poko_scene PRODUCTION)

poko_add_unit_tests(poko_scene TEST_SOURCES scene_graph_test.cpp)
