add_executable(poko_windowed_demo main.cpp)
target_link_libraries(poko_windowed_demo PRIVATE poko_core)
poko_apply_common(poko_windowed_demo)
poko_set_impl_label(poko_windowed_demo STUB)

add_test(NAME demo.windowed COMMAND poko_windowed_demo)

# Console renderer demo. Only built when the renderer exists (not in Server).
if(TARGET poko_renderer)
    add_executable(poko_render_demo render_demo.cpp)
    target_link_libraries(poko_render_demo PRIVATE poko_renderer poko_render_null)
    poko_apply_common(poko_render_demo)
    poko_set_impl_label(poko_render_demo LOCAL_DEV)
    add_test(NAME demo.render COMMAND poko_render_demo)
endif()
