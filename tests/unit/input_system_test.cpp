#include "poko_test_harness.h"

#include "poko_input.h"

POKO_TEST(key_state_and_edges) {
    poko::input::InputSystem input;
    const auto down =
        poko::input::InputEvent{poko::input::EventType::KeyDown, poko::input::KeyCode::W};
    POKO_CHECK(input.feed_event(down).ok());
    POKO_CHECK(input.key_down(poko::input::KeyCode::W));
    POKO_CHECK(input.key_pressed(poko::input::KeyCode::W));
    POKO_CHECK(input.any_key_pressed());

    input.begin_frame();
    POKO_CHECK(input.key_down(poko::input::KeyCode::W));
    POKO_CHECK(!input.key_pressed(poko::input::KeyCode::W)); // edge cleared
    POKO_CHECK(!input.any_key_pressed());

    const auto up = poko::input::InputEvent{poko::input::EventType::KeyUp, poko::input::KeyCode::W};
    POKO_CHECK(input.feed_event(up).ok());
    POKO_CHECK(!input.key_down(poko::input::KeyCode::W));
    POKO_CHECK(input.key_released(poko::input::KeyCode::W));
}

POKO_TEST(invalid_events_rejected) {
    poko::input::InputSystem input;
    POKO_CHECK(!input.feed_event({}).ok()); // KeyDown with Unknown key
    auto wheel = poko::input::InputEvent{};
    wheel.type = poko::input::EventType::MouseWheel;
    POKO_CHECK(!input.feed_event(wheel).ok()); // zero wheel delta
}

POKO_TEST(mouse_state) {
    poko::input::InputSystem input;
    auto move = poko::input::InputEvent{};
    move.type = poko::input::EventType::MouseMove;
    move.position = {12.0f, 34.0f};
    POKO_CHECK(input.feed_event(move).ok());
    POKO_CHECK_EQ(input.mouse_position().x, 12.0f);

    auto click = poko::input::InputEvent{};
    click.type = poko::input::EventType::MouseButtonDown;
    click.button = poko::input::MouseButton::Right;
    POKO_CHECK(input.feed_event(click).ok());
    POKO_CHECK(input.mouse_button_down(poko::input::MouseButton::Right));
    POKO_CHECK(input.mouse_button_pressed(poko::input::MouseButton::Right));

    auto wheel = poko::input::InputEvent{};
    wheel.type = poko::input::EventType::MouseWheel;
    wheel.wheel = -1.0f;
    POKO_CHECK(input.feed_event(wheel).ok());
    wheel.wheel = 1.0f;
    POKO_CHECK(input.feed_event(wheel).ok());
    POKO_CHECK_EQ(input.wheel_delta(), 0.0f);

    input.begin_frame();
    POKO_CHECK_EQ(input.wheel_delta(), 0.0f);
}

POKO_TEST(text_input_encodes_utf8) {
    poko::input::InputSystem input;
    auto ev = poko::input::InputEvent{};
    ev.type = poko::input::EventType::TextInput;
    ev.codepoint = U'a';
    POKO_CHECK(input.feed_event(ev).ok());
    ev.codepoint = 0x20AC; // euro sign, 3-byte UTF-8
    POKO_CHECK(input.feed_event(ev).ok());

    const std::string text = input.take_text();
    POKO_CHECK_EQ(text.size(), 4u);        // 'a' + 3 bytes
    POKO_CHECK(input.take_text().empty()); // drained
}

POKO_TEST_MAIN()
