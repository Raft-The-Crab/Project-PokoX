# Asset provider seam (systemplan/05). Only the MOCK backend exists so far;
# the real pipeline (import/validate/convert/cache) is planned.
add_library(poko_assets_null STATIC
    src/provider.cpp)

target_include_directories(poko_assets_null PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_assets_null PUBLIC poko_core)
poko_apply_common(poko_assets_null)
poko_set_impl_label(poko_assets_null MOCK)

poko_add_unit_tests(poko_assets_null
    TEST_SOURCES
        asset_contracts_test.cpp
        assets_null_provider_test.cpp)
