add_library(poko_core STATIC
    src/version.cpp
    src/log.cpp
    src/assert.cpp
    src/job.cpp)

target_include_directories(poko_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

poko_apply_common(poko_core)
poko_set_impl_label(poko_core PRODUCTION)

# Header-only declaration of the Phase 0 interface seams (STUB).
add_library(poko_api_seams INTERFACE)
target_include_directories(poko_api_seams INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_api_seams INTERFACE poko_core)
set_property(TARGET poko_api_seams PROPERTY POKO_IMPL_LABEL STUB)

poko_add_unit_tests(poko_core
    TEST_SOURCES
        core_version_test.cpp
        core_types_test.cpp
        core_log_test.cpp
        core_assert_test.cpp
        core_result_test.cpp
        core_math_test.cpp
        core_clock_event_job_test.cpp
        core_diagnostics_settings_ownership_test.cpp)
