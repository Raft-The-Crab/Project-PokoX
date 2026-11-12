# Platform-agnostic input skeleton (Phase 1): events + per-frame state.
add_library(poko_input STATIC
    src/input.cpp)

target_include_directories(poko_input PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_input PUBLIC poko_core)
poko_apply_common(poko_input)
poko_set_impl_label(poko_input PRODUCTION)

poko_add_unit_tests(poko_input TEST_SOURCES input_system_test.cpp)
