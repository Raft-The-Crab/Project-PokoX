# Material/mesh -> DrawCall layer on top of the IRenderDevice seam
# (systemplan/04 steps 4-5, 8). CPU-side authoring descriptors are uploaded
# into device handles; DrawCallBuilder assembles validated DrawCalls.
add_library(poko_graphics STATIC
    src/mesh.cpp
    src/material.cpp
    src/draw_call.cpp)

target_include_directories(poko_graphics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_graphics PUBLIC poko_core)
poko_apply_common(poko_graphics)
poko_set_impl_label(poko_graphics PRODUCTION)

poko_add_unit_tests(poko_graphics
    TEST_SOURCES
        graphics_mesh_test.cpp
        graphics_material_test.cpp
        graphics_draw_call_test.cpp
    LINK_LIBS poko_render_null)
