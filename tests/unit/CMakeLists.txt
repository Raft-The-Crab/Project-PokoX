set(_poko_unit_sources
    core_version_test.cpp
    core_types_test.cpp
    core_log_test.cpp
    core_assert_test.cpp
    core_result_test.cpp
    core_math_test.cpp
    core_clock_event_job_test.cpp
    core_diagnostics_settings_ownership_test.cpp
    core_platform_fs_test.cpp)

foreach(_poko_src IN LISTS _poko_unit_sources)
    get_filename_component(_poko_name "${_poko_src}" NAME_WE)
    add_executable(poko_test_${_poko_name} ${_poko_src})
    target_link_libraries(poko_test_${_poko_name} PRIVATE poko_core poko_platform)
    target_include_directories(poko_test_${_poko_name} PRIVATE
        ${PROJECT_SOURCE_DIR}/tests/harness)
    poko_apply_common(poko_test_${_poko_name})
    poko_set_impl_label(poko_test_${_poko_name} LOCAL_DEV)
    add_test(NAME unit.${_poko_name} COMMAND poko_test_${_poko_name})
    set_tests_properties(unit.${_poko_name} PROPERTIES LABELS unit)
endforeach()
