#include "poko_test_harness.h"

#include "poko/ai/ai.h"

POKO_TEST(blackboard_typed_values) {
    poko::ai::Blackboard board;
    board.set_float("health", 0.75f);
    board.set_int("ammo", 12);
    board.set_bool("alerted", true);
    board.set_string("state", "patrol");

    POKO_CHECK_EQ(board.get_float("health"), 0.75f);
    POKO_CHECK_EQ(board.get_int("ammo"), 12);
    POKO_CHECK(board.get_bool("alerted"));
    POKO_CHECK_EQ(board.get_string("state"), "patrol");
    POKO_CHECK_EQ(board.get_float("missing", 9.0f), 9.0f); // fallback

    board.erase("ammo");
    POKO_CHECK(!board.has("ammo"));
}

POKO_TEST(fsm_transitions_on_condition) {
    poko::ai::StateMachine fsm;
    const auto idle = fsm.add_state("idle");
    const auto chase = fsm.add_state("chase");
    fsm.add_transition(
        idle, {chase, [](const poko::ai::Blackboard& b) { return b.get_bool("seen_player"); }});

    poko::ai::Blackboard board;
    POKO_CHECK_EQ(fsm.update(0.016f, board), idle); // no flag -> stays
    board.set_bool("seen_player", true);
    POKO_CHECK_EQ(fsm.update(0.016f, board), chase); // transitions
}

POKO_TEST(enter_exit_hooks_fire) {
    int enters = 0, exits = 0;
    poko::ai::StateMachine fsm;
    const auto a = fsm.add_state(
        "a", {}, [&](poko::ai::Blackboard&) { ++enters; }, [&](poko::ai::Blackboard&) { ++exits; });
    const auto b = fsm.add_state("b");
    fsm.add_transition(a, {b, [](const poko::ai::Blackboard&) { return true; }});

    poko::ai::Blackboard board;
    fsm.update(0.016f, board); // enters a
    POKO_CHECK_EQ(enters, 1);
    fsm.update(0.016f, board); // exits a, enters b
    POKO_CHECK_EQ(exits, 1);
}

POKO_TEST_MAIN()
