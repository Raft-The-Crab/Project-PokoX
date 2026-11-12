# Compression codec registry skeleton (systemplan/13): store codec only.
add_library(poko_compression STATIC
    src/compression.cpp)

target_include_directories(poko_compression PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_compression PUBLIC poko_core)
poko_apply_common(poko_compression)
poko_set_impl_label(poko_compression PRODUCTION)

poko_add_unit_tests(poko_compression TEST_SOURCES compression_codec_test.cpp)
