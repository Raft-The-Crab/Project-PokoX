# Waypoint nav graph + A* skeleton (Phase 1): deterministic path queries.
add_library(poko_navigation STATIC
    src/poko_nav_graph.cpp)

target_include_directories(poko_navigation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_navigation PUBLIC poko_core)
poko_apply_common(poko_navigation)
poko_set_impl_label(poko_navigation PRODUCTION)
