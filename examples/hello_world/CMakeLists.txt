# Windowed demo. With POKO_RENDER_OPENGL=ON this is the real SDL3+OpenGL
# clear-screen-and-triangle (systemplan/04 steps 1-4, DEC-004); otherwise it
# stays a labelled STUB so offline/CI builds keep a trivial target.
if(TARGET poko_render_gl)
    add_executable(poko_windowed_demo windowed_demo.cpp)
    target_link_libraries(poko_windowed_demo PRIVATE
        poko_renderer poko_render_gl SDL3::SDL3)
    poko_apply_common(poko_windowed_demo)
    poko_set_impl_label(poko_windowed_demo LOCAL_DEV)

    # Automated smoke run: 60 frames then exit. Requires a GL 3.3 context, so
    # it only exists in opt-in graphics builds.
    add_test(NAME demo.windowed COMMAND poko_windowed_demo --frames 60)

    if(TARGET poko_render_vulkan)
        add_executable(poko_vulkan_demo vulkan_demo.cpp)
        target_link_libraries(poko_vulkan_demo PRIVATE
            poko_renderer poko_render_vulkan SDL3::SDL3 Vulkan::Headers)
        if(TARGET Vulkan::VulkanLoader)
            target_link_libraries(poko_vulkan_demo PRIVATE Vulkan::VulkanLoader)
        elseif(TARGET vulkan)
            target_link_libraries(poko_vulkan_demo PRIVATE vulkan)
        endif()
        poko_apply_common(poko_vulkan_demo)
        poko_set_impl_label(poko_vulkan_demo LOCAL_DEV)
        add_test(NAME demo.vulkan COMMAND poko_vulkan_demo --frames 60)
    endif()
else()
    add_executable(poko_windowed_demo main.cpp)
    target_link_libraries(poko_windowed_demo PRIVATE poko_core)
    poko_apply_common(poko_windowed_demo)
    poko_set_impl_label(poko_windowed_demo STUB)

    add_test(NAME demo.windowed COMMAND poko_windowed_demo)
endif()

# Console renderer demo. Only built when the renderer exists (not in Server).
if(TARGET poko_renderer)
    add_executable(poko_render_demo render_demo.cpp)
    target_link_libraries(poko_render_demo PRIVATE poko_renderer poko_render_null)
    poko_apply_common(poko_render_demo)
    poko_set_impl_label(poko_render_demo LOCAL_DEV)
    add_test(NAME demo.render COMMAND poko_render_demo)
endif()
