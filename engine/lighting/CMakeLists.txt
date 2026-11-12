# Scene light list skeleton (systemplan/04): forward lighting contracts.
add_library(poko_lighting STATIC
    src/poko_light_list.cpp)

target_include_directories(poko_lighting PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_lighting PUBLIC poko_core)
poko_apply_common(poko_lighting)
poko_set_impl_label(poko_lighting PRODUCTION)
