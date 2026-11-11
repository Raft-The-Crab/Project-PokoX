add_executable(poko_windowed_demo main.cpp)
target_link_libraries(poko_windowed_demo PRIVATE poko_core)
poko_apply_common(poko_windowed_demo)
poko_set_impl_label(poko_windowed_demo STUB)

add_test(NAME demo.windowed COMMAND poko_windowed_demo)
