# Kinematic character motor skeleton (Phase 1): move/jump/gravity/probe.
add_library(poko_character STATIC
    src/poko_character.cpp)

target_include_directories(poko_character PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_character PUBLIC poko_core)
poko_apply_common(poko_character)
poko_set_impl_label(poko_character PRODUCTION)
