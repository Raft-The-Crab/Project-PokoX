# Gameplay camera rig skeleton (Phase 1): orbit/top-down/fly pose source.
add_library(poko_camera STATIC
    src/poko_camera_rig.cpp)

target_include_directories(poko_camera PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_camera PUBLIC poko_core)
poko_apply_common(poko_camera)
poko_set_impl_label(poko_camera PRODUCTION)
