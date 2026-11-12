# Async file IO skeleton (Phase 1): queued reads over IPlatformFileSystem.
add_library(poko_io STATIC
    src/poko_async_io.cpp)

target_include_directories(poko_io PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_io PUBLIC poko_core)
poko_apply_common(poko_io)
poko_set_impl_label(poko_io PRODUCTION)
