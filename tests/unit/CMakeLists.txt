set(_poko_unit_sources
    core_version_test.cpp
    core_types_test.cpp
    core_log_test.cpp
    core_assert_test.cpp
    core_result_test.cpp
    core_math_test.cpp
    core_clock_event_job_test.cpp
    core_diagnostics_settings_ownership_test.cpp
    core_platform_fs_test.cpp
    physics_contracts_test.cpp
    physics_null_world_test.cpp
    asset_contracts_test.cpp
    assets_null_provider_test.cpp
    scene_graph_test.cpp
    ecs_registry_test.cpp
    animation_clip_test.cpp
    input_system_test.cpp
    audio_contracts_test.cpp
    audio_null_device_test.cpp
    light_contracts_test.cpp
    light_list_test.cpp
    camera_rig_test.cpp
    particles_emitter_test.cpp
    memory_allocators_test.cpp)

# The renderer is excluded from the headless Server configuration, so its
# tests are conditional on the target existing.
if(TARGET poko_renderer)
    list(APPEND _poko_unit_sources
        renderer_contracts_test.cpp
        renderer_null_device_test.cpp
        renderer_shader_cache_test.cpp
        renderer_camera_test.cpp
        renderer_submission_test.cpp
        renderer_frame_stats_facade_test.cpp)
endif()

foreach(_poko_src IN LISTS _poko_unit_sources)
    get_filename_component(_poko_name "${_poko_src}" NAME_WE)
    add_executable(poko_test_${_poko_name} ${_poko_src})
    target_link_libraries(poko_test_${_poko_name} PRIVATE poko_core poko_platform)
    if(TARGET poko_renderer)
        target_link_libraries(poko_test_${_poko_name} PRIVATE
            poko_renderer poko_render_null)
    endif()
    if(TARGET poko_physics_null)
        target_link_libraries(poko_test_${_poko_name} PRIVATE poko_physics_null)
    endif()
    if(TARGET poko_assets_null)
        target_link_libraries(poko_test_${_poko_name} PRIVATE poko_assets_null)
    endif()
    if(TARGET poko_scene)
        target_link_libraries(poko_test_${_poko_name} PRIVATE poko_scene)
    endif()
    if(TARGET poko_ecs)
        target_link_libraries(poko_test_${_poko_name} PRIVATE poko_ecs)
    endif()
    if(TARGET poko_animation)
        target_link_libraries(poko_test_${_poko_name} PRIVATE poko_animation)
    endif()
    if(TARGET poko_input)
        target_link_libraries(poko_test_${_poko_name} PRIVATE poko_input)
    endif()
    if(TARGET poko_audio_null)
        target_link_libraries(poko_test_${_poko_name} PRIVATE poko_audio_null)
    endif()
    if(TARGET poko_lighting)
        target_link_libraries(poko_test_${_poko_name} PRIVATE poko_lighting)
    endif()
    if(TARGET poko_camera)
        target_link_libraries(poko_test_${_poko_name} PRIVATE poko_camera)
    endif()
    if(TARGET poko_particles)
        target_link_libraries(poko_test_${_poko_name} PRIVATE poko_particles)
    endif()
    if(TARGET poko_memory)
        target_link_libraries(poko_test_${_poko_name} PRIVATE poko_memory)
    endif()
    target_include_directories(poko_test_${_poko_name} PRIVATE
        ${PROJECT_SOURCE_DIR}/tests/harness)
    poko_apply_common(poko_test_${_poko_name})
    poko_set_impl_label(poko_test_${_poko_name} LOCAL_DEV)
    add_test(NAME unit.${_poko_name} COMMAND poko_test_${_poko_name})
    set_tests_properties(unit.${_poko_name} PROPERTIES LABELS unit)
endforeach()
