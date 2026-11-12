# Physics world seam (systemplan/06). MOCK backend kept for deterministic
# tests; the PRODUCTION custom rigid-body world is confirmed by systemplan/29.
add_library(poko_physics_null STATIC
    src/bodies.cpp
    src/raycast.cpp
    src/step.cpp)

target_include_directories(poko_physics_null PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_physics_null PUBLIC poko_core)
poko_apply_common(poko_physics_null)
poko_set_impl_label(poko_physics_null MOCK)

poko_add_unit_tests(poko_physics_null
    TEST_SOURCES
        physics_contracts_test.cpp
        physics_null_world_test.cpp)

# PRODUCTION custom rigid-body world: fixed substeps, impulse collision
# response, ground plane and analytic raycasts. Standard library only.
add_library(poko_physics_custom STATIC
    src/custom_world.cpp
    src/custom_step.cpp
    src/custom_raycast.cpp)

target_include_directories(poko_physics_custom PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_physics_custom PUBLIC poko_core)
poko_apply_common(poko_physics_custom)
poko_set_impl_label(poko_physics_custom PRODUCTION)

poko_add_unit_tests(poko_physics_custom
    TEST_SOURCES
        physics_custom_world_test.cpp)
