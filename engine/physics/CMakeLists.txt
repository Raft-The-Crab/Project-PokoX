# Physics world seam (systemplan/06). Only the MOCK backend exists so far;
# the custom physics module is planned (open decision in systemplan/28).
add_library(poko_physics_null STATIC
    src/world.cpp)

target_include_directories(poko_physics_null PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_physics_null PUBLIC poko_core)
poko_apply_common(poko_physics_null)
poko_set_impl_label(poko_physics_null MOCK)

poko_add_unit_tests(poko_physics_null
    TEST_SOURCES
        physics_contracts_test.cpp
        physics_null_world_test.cpp)
