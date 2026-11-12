# Minimal ECS registry skeleton (Phase 1): entities + typed component pools.
add_library(poko_ecs STATIC
    src/poko_ecs.cpp)

target_include_directories(poko_ecs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_ecs PUBLIC poko_core)
poko_apply_common(poko_ecs)
poko_set_impl_label(poko_ecs PRODUCTION)
