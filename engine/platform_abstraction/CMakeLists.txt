add_library(poko_platform STATIC
    src/fs.cpp)

target_include_directories(poko_platform PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_platform PUBLIC poko_core)

poko_apply_common(poko_platform)
poko_set_impl_label(poko_platform LOCAL_DEV)

poko_add_unit_tests(poko_platform
    TEST_SOURCES core_platform_fs_test.cpp)
