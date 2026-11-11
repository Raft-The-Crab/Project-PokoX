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

# ---------------------------------------------------------------------------
# OpenGL backend (DEC-004). Only built when POKO_RENDER_OPENGL=ON, which
# requires POKO_FETCH_DEPENDENCIES=ON for the pinned SDL3 tarball. The GL
# loader is the vendored, pre-generated gl3w sources — no build-time network.
# ---------------------------------------------------------------------------
if(POKO_RENDER_OPENGL)
    find_package(OpenGL REQUIRED)

    # Vendored third-party loader: compiled without the PokoX warning
    # baseline and without an implementation label (it is not our code).
    add_library(poko_gl3w STATIC
        "${PROJECT_SOURCE_DIR}/third_party/vendored/gl3w/src/gl3w.c")
    target_include_directories(poko_gl3w PUBLIC
        "${PROJECT_SOURCE_DIR}/third_party/vendored/gl3w/include")
    target_link_libraries(poko_gl3w PUBLIC OpenGL::GL)
    set_target_properties(poko_gl3w PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

    add_library(poko_render_gl STATIC src/poko_gl_device.cpp)
    target_include_directories(poko_render_gl PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_link_libraries(poko_render_gl PUBLIC poko_core poko_gl3w OpenGL::GL)
    poko_apply_common(poko_render_gl)
    poko_set_impl_label(poko_render_gl PRODUCTION)
endif()
