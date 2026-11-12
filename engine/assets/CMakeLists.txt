# Asset provider seam (systemplan/05). MOCK backend kept for deterministic
# tests; the PRODUCTION backend reads real files through IPlatformFileSystem.
add_library(poko_assets_null STATIC
    src/assets.cpp
    src/requests.cpp
    src/payload.cpp)

target_include_directories(poko_assets_null PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_assets_null PUBLIC poko_core)
poko_apply_common(poko_assets_null)
poko_set_impl_label(poko_assets_null MOCK)

poko_add_unit_tests(poko_assets_null
    TEST_SOURCES
        asset_contracts_test.cpp
        assets_null_provider_test.cpp)

# PRODUCTION filesystem-backed provider: real reads, PKX1 decoding and a
# payload cache. Standard library only; the filesystem is injected at the seam.
add_library(poko_assets_fs STATIC
    src/fs_provider.cpp
    src/fs_requests.cpp)

target_include_directories(poko_assets_fs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_assets_fs PUBLIC poko_core)
poko_apply_common(poko_assets_fs)
poko_set_impl_label(poko_assets_fs PRODUCTION)

poko_add_unit_tests(poko_assets_fs
    TEST_SOURCES
        assets_fs_provider_test.cpp
    LINK_LIBS poko_platform)
