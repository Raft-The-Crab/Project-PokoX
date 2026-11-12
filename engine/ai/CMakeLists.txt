# AI skeleton (Phase 1): FSM + blackboard; utility AI/BT come later.
add_library(poko_ai STATIC
    src/blackboard.cpp
    src/state_machine.cpp)

target_include_directories(poko_ai PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_ai PUBLIC poko_core)
poko_apply_common(poko_ai)
poko_set_impl_label(poko_ai PRODUCTION)

poko_add_unit_tests(poko_ai TEST_SOURCES ai_fsm_test.cpp)
