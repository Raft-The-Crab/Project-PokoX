# Post-process chain skeleton (systemplan/04): ordered screen-space effects.
add_library(poko_postprocess STATIC
    src/chain.cpp)

target_include_directories(poko_postprocess PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_postprocess PUBLIC poko_core)
poko_apply_common(poko_postprocess)
poko_set_impl_label(poko_postprocess PRODUCTION)

poko_add_unit_tests(poko_postprocess
    TEST_SOURCES
        postprocess_contracts_test.cpp
        postprocess_chain_test.cpp)
