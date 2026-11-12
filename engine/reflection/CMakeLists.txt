# Type reflection registry skeleton (Phase 1): name/type lookup for
# serialization, scene editing and later MUTE bindings.
add_library(poko_reflection STATIC
    src/poko_reflection.cpp)

target_include_directories(poko_reflection PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_reflection PUBLIC poko_core)
poko_apply_common(poko_reflection)
poko_set_impl_label(poko_reflection PRODUCTION)
