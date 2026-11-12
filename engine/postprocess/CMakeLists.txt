# Post-process chain skeleton (systemplan/04): ordered screen-space effects.
add_library(poko_postprocess STATIC
    src/poko_postprocess.cpp)

target_include_directories(poko_postprocess PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_postprocess PUBLIC poko_core)
poko_apply_common(poko_postprocess)
poko_set_impl_label(poko_postprocess PRODUCTION)
