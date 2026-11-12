# Audio device seam (systemplan/07). MOCK backend kept for deterministic
# tests; the PRODUCTION backend is a real software PCM mixer. miniaudio is NOT
# VENDORED (third_party/DEPENDENCIES.md) so OS output is deferred, not guessed.
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

# PRODUCTION software mixer: deterministic PCM synthesis, voice state, linear
# resampling, positional attenuation and mono mixdown. Standard library only.
add_library(poko_audio_mixer STATIC
    src/mixer_device.cpp
    src/mixer_render.cpp)

target_include_directories(poko_audio_mixer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_audio_mixer PUBLIC poko_core)
poko_apply_common(poko_audio_mixer)
poko_set_impl_label(poko_audio_mixer PRODUCTION)

poko_add_unit_tests(poko_audio_mixer
    TEST_SOURCES
        audio_mixer_device_test.cpp)
