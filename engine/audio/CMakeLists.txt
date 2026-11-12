# Audio device seam (systemplan/07). Only the MOCK backend exists so far;
# miniaudio is the planned production backend.
add_library(poko_audio_null STATIC
    src/poko_audio_device.cpp)

target_include_directories(poko_audio_null PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_audio_null PUBLIC poko_core)
poko_apply_common(poko_audio_null)
poko_set_impl_label(poko_audio_null MOCK)
