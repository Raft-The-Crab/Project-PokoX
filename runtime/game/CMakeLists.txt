# Integrated game runtime (Phase 1 exit, systemplan/27). The frame loop is
# backend-neutral; the client demo adds the renderer + graphics layer and is
# excluded from the headless Server configuration.

add_executable(poko_headless src/headless_main.cpp)
target_link_libraries(poko_headless PRIVATE poko_core)
poko_apply_common(poko_headless)
poko_set_impl_label(poko_headless LOCAL_DEV)

add_test(NAME runtime.headless COMMAND poko_headless)

# Backend-neutral simulation frame loop: builds in every configuration.
add_library(poko_runtime STATIC
    src/game_runtime.cpp
    src/demo_scene.cpp)

target_include_directories(poko_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_runtime PUBLIC
    poko_core
    poko_ecs
    poko_scene
    poko_input
    poko_lighting
    poko_camera
    poko_postprocess
    poko_particles
    poko_character)
poko_apply_common(poko_runtime)
poko_set_impl_label(poko_runtime PRODUCTION)

poko_add_unit_tests(poko_runtime
    TEST_SOURCES runtime_frame_loop_test.cpp
    LINK_LIBS poko_physics_null poko_assets_null)

# Client-side integration: renderer + graphics -> DrawCall -> render_frame.
# A headless Server build contains no renderer (systemplan/18).
if(NOT POKO_SERVER_CONFIG)
    add_executable(poko_game_demo src/game_main.cpp)
    target_link_libraries(poko_game_demo PRIVATE
        poko_runtime
        poko_renderer
        poko_render_null
        poko_graphics
        poko_physics_null
        poko_assets_null)
    poko_apply_common(poko_game_demo)
    poko_set_impl_label(poko_game_demo LOCAL_DEV)
    add_test(NAME runtime.game COMMAND poko_game_demo)
endif()
