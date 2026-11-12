# Animation clip sampling skeleton (systemplan/07): deterministic keyframes.
add_library(poko_animation STATIC
    src/poko_animation.cpp)

target_include_directories(poko_animation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_animation PUBLIC poko_core poko_scene)
poko_apply_common(poko_animation)
poko_set_impl_label(poko_animation PRODUCTION)
