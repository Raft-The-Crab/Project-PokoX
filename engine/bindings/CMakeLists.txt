# Script binding registry skeleton (Phase 1): name -> callable map for MUTE.
add_library(poko_bindings STATIC
    src/registry.cpp)

target_include_directories(poko_bindings PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_bindings PUBLIC poko_core)
poko_apply_common(poko_bindings)
poko_set_impl_label(poko_bindings PRODUCTION)

poko_add_unit_tests(poko_bindings TEST_SOURCES bindings_registry_test.cpp)
