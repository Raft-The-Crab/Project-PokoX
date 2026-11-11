add_executable(poko_headless src/headless_main.cpp)
target_link_libraries(poko_headless PRIVATE poko_core)
poko_apply_common(poko_headless)
poko_set_impl_label(poko_headless LOCAL_DEV)

add_test(NAME runtime.headless COMMAND poko_headless)
