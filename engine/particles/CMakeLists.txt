# CPU particle emitter skeleton (Phase 1): deterministic simulation.
add_library(poko_particles STATIC
    src/particles.cpp)

target_include_directories(poko_particles PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_particles PUBLIC poko_core)
poko_apply_common(poko_particles)
poko_set_impl_label(poko_particles PRODUCTION)

poko_add_unit_tests(poko_particles TEST_SOURCES particles_emitter_test.cpp)
