# Audio device seam (systemplan/07). Only the MOCK backend exists so far;
# miniaudio is the planned production backend.
add_library(poko_audio_null STATIC
    src/sounds.cpp
    src/playback.cpp)

target_include_directories(poko_audio_null PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_audio_null PUBLIC poko_core)
poko_apply_common(poko_audio_null)
poko_set_impl_label(poko_audio_null MOCK)

poko_add_unit_tests(poko_audio_null
    TEST_SOURCES
        audio_contracts_test.cpp
        audio_null_device_test.cpp)
