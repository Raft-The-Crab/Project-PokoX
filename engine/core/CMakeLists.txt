add_library(poko_core STATIC
    src/poko_version.cpp
    src/poko_log.cpp
    src/poko_assert.cpp)

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
