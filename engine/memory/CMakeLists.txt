# Memory allocators skeleton (Phase 1): arena + fixed pools.
add_library(poko_memory STATIC
    src/allocators.cpp)

target_include_directories(poko_memory PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_memory PUBLIC poko_core)
poko_apply_common(poko_memory)
poko_set_impl_label(poko_memory PRODUCTION)

poko_add_unit_tests(poko_memory TEST_SOURCES memory_allocators_test.cpp)
