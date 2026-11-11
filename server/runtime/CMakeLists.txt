add_executable(poko_server src/main.cpp)
target_link_libraries(poko_server PRIVATE poko_core)
poko_apply_common(poko_server)
poko_set_impl_label(poko_server STUB)

add_test(NAME server.version COMMAND poko_server --version)
