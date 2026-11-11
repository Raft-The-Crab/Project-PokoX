# Backend-neutral renderer logic (systemplan/04). No graphics vendor is linked.
add_library(poko_renderer STATIC
    src/poko_shader_cache.cpp
    src/poko_camera.cpp
    src/poko_scene_submission.cpp
    src/poko_frame_stats.cpp
    src/poko_renderer.cpp)

target_include_directories(poko_renderer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_renderer PUBLIC poko_core)
poko_apply_common(poko_renderer)
poko_set_impl_label(poko_renderer PRODUCTION)

# MOCK backend: test-only fake, deliberately a separate library so it can never
# be confused with a production target (systemplan/30 rule 12).
add_library(poko_render_null STATIC
    src/poko_render_device.cpp)

target_include_directories(poko_render_null PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_render_null PUBLIC poko_core)
poko_apply_common(poko_render_null)
poko_set_impl_label(poko_render_null MOCK)
