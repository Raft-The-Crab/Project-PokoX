#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "poko/core/math.h"
#include "poko/core/result.h"

namespace poko::input {

// Input skeleton (Phase 1): platform-agnostic keyboard/mouse/text events and
// per-frame state. Gamepad support is deferred; SDL3 is the planned source.

enum class KeyCode : std::uint16_t {
    Unknown = 0,
    Escape,
    Enter,
    Tab,
    Space,
    Backspace,
    Delete,
    Insert,
    Home,
    End,
    PageUp,
    PageDown,
    Left,
    Right,
    Up,
    Down,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Num0,
    Num1,
    Num2,
    Num3,
    Num4,
    Num5,
    Num6,
    Num7,
    Num8,
    Num9,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
};

inline constexpr std::size_t kKeyCount = static_cast<std::size_t>(KeyCode::F12) + 1;

enum class MouseButton : std::uint8_t { Left = 0, Right, Middle, Count };

enum class EventType : std::uint8_t {
    KeyDown,
    KeyUp,
    MouseButtonDown,
    MouseButtonUp,
    MouseMove,
    MouseWheel,
    TextInput, // carries codepoint
};

struct InputEvent {
    EventType type = EventType::KeyDown;
    KeyCode key = KeyCode::Unknown;
    MouseButton button = MouseButton::Left;
    math::Vec2 position;    // window pixels, for mouse events
    float wheel = 0.0f;     // wheel delta, for MouseWheel
    char32_t codepoint = 0; // for TextInput
};

inline bool valid(const InputEvent& event) noexcept {
    switch (event.type) {
    case EventType::KeyDown:
    case EventType::KeyUp:
        return event.key != KeyCode::Unknown;
    case EventType::MouseButtonDown:
    case EventType::MouseButtonUp:
        return static_cast<std::uint8_t>(event.button) <
               static_cast<std::uint8_t>(MouseButton::Count);
    case EventType::MouseMove:
        return true;
    case EventType::MouseWheel:
        return event.wheel != 0.0f;
    case EventType::TextInput:
        return event.codepoint != 0;
    }
    return false;
}

// Per-frame input snapshot. Feed events, then call begin_frame() once per
// tick to rotate edge-triggered state.
class InputSystem {
public:
    InputSystem() = default;

    Status feed_event(const InputEvent& event);
    void begin_frame() noexcept;

    bool key_down(KeyCode key) const noexcept;
    bool key_pressed(KeyCode key) const noexcept; // edge: went down this frame
    bool key_released(KeyCode key) const noexcept;
    bool any_key_pressed() const noexcept;

    bool mouse_button_down(MouseButton button) const noexcept;
    bool mouse_button_pressed(MouseButton button) const noexcept;
    math::Vec2 mouse_position() const noexcept;
    float wheel_delta() const noexcept; // accumulated since last begin_frame

    // Drains buffered text input as UTF-8.
    std::string take_text();

    void clear() noexcept;

private:
    static std::size_t key_index(KeyCode key) noexcept {
        return static_cast<std::size_t>(key);
    }
    static std::size_t button_index(MouseButton button) noexcept {
        return static_cast<std::size_t>(button);
    }

    mutable std::mutex mutex_;
    std::array<bool, kKeyCount> keys_down_{};
    std::array<bool, kKeyCount> keys_pressed_{};
    std::array<bool, kKeyCount> keys_released_{};
    std::array<bool, static_cast<std::size_t>(MouseButton::Count)> buttons_down_{};
    std::array<bool, static_cast<std::size_t>(MouseButton::Count)> buttons_pressed_{};
    math::Vec2 mouse_pos_{};
    float wheel_delta_ = 0.0f;
    bool any_pressed_ = false;
    std::u32string pending_text_;
};

} // namespace poko::input
