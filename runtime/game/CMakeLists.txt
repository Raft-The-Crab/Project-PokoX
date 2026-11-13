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

# Integration: the PRODUCTION backends are drop-in behind the same seams.
poko_add_unit_tests(poko_runtime
    TEST_SOURCES runtime_real_backends_test.cpp
    LINK_LIBS poko_physics_custom poko_assets_fs poko_platform)

# Phase 1 performance baseline (systemplan/27 exit criterion). Uses only the
# PRODUCTION CPU backends so it builds in every configuration, including the
# headless Server build.
add_executable(poko_engine_bench src/engine_bench_main.cpp)
target_link_libraries(poko_engine_bench PRIVATE
    poko_core
    poko_runtime
    poko_benchmarks
    poko_physics_custom
    poko_assets_fs
    poko_audio_mixer
    poko_net_channel
    poko_platform)
poko_apply_common(poko_engine_bench)
poko_set_impl_label(poko_engine_bench LOCAL_DEV)
add_test(NAME benchmark.engine COMMAND poko_engine_bench)

# PRODUCTION engine facade + client integration: renderer + graphics ->
# DrawCall -> render_frame. A headless Server build contains no renderer
# (systemplan/18), so these targets are gated the same way.
if(NOT POKO_SERVER_CONFIG)
    add_library(poko_engine STATIC
        src/engine.cpp
        src/engine_backends.cpp
        src/engine_render.cpp)

    target_include_directories(poko_engine PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_link_libraries(poko_engine PUBLIC
        poko_core
        poko_runtime
        poko_physics_custom
        poko_assets_fs
        poko_audio_mixer
        poko_net_channel
        poko_platform
        poko_renderer
        poko_render_null
        poko_graphics)
    poko_apply_common(poko_engine)
    poko_set_impl_label(poko_engine PRODUCTION)

    poko_add_unit_tests(poko_engine
        TEST_SOURCES engine_test.cpp)

    add_executable(poko_engine_demo src/engine_demo_main.cpp)
    target_link_libraries(poko_engine_demo PRIVATE poko_engine)
    poko_apply_common(poko_engine_demo)
    poko_set_impl_label(poko_engine_demo LOCAL_DEV)
    add_test(NAME runtime.engine COMMAND poko_engine_demo)

    # Software-rendered physics showcase; writes PPM snapshots to the cwd.
    add_executable(poko_physics_showcase src/physics_showcase_main.cpp)
    target_link_libraries(poko_physics_showcase PRIVATE
        poko_graphics
        poko_renderer
        poko_render_software
        poko_physics_custom)
    poko_apply_common(poko_physics_showcase)
    poko_set_impl_label(poko_physics_showcase LOCAL_DEV)
    add_test(NAME runtime.physics_showcase COMMAND poko_physics_showcase ${CMAKE_CURRENT_BINARY_DIR})

    # Original MOCK-backed integration harness, kept as a regression check
    # that the seams still work with the deterministic fakes.
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
