#include "poko/input/input.h"

#include "utf8.h"

namespace poko::input {

void InputSystem::begin_frame() noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    keys_pressed_.fill(false);
    keys_released_.fill(false);
    buttons_pressed_.fill(false);
    wheel_delta_ = 0.0f;
    any_pressed_ = false;
}

bool InputSystem::key_down(KeyCode key) const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return key_index(key) < kKeyCount && keys_down_[key_index(key)];
}

bool InputSystem::key_pressed(KeyCode key) const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return key_index(key) < kKeyCount && keys_pressed_[key_index(key)];
}

bool InputSystem::key_released(KeyCode key) const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return key_index(key) < kKeyCount && keys_released_[key_index(key)];
}

bool InputSystem::any_key_pressed() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return any_pressed_;
}

bool InputSystem::mouse_button_down(MouseButton button) const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return buttons_down_[button_index(button)];
}

bool InputSystem::mouse_button_pressed(MouseButton button) const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return buttons_pressed_[button_index(button)];
}

math::Vec2 InputSystem::mouse_position() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return mouse_pos_;
}

float InputSystem::wheel_delta() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return wheel_delta_;
}

std::string InputSystem::take_text() {
    std::lock_guard<std::mutex> guard(mutex_);
    std::string out;
    out.reserve(pending_text_.size());
    for (char32_t cp : pending_text_) {
        detail::append_utf8(cp, out);
    }
    pending_text_.clear();
    return out;
}

void InputSystem::clear() noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    keys_down_.fill(false);
    keys_pressed_.fill(false);
    keys_released_.fill(false);
    buttons_down_.fill(false);
    buttons_pressed_.fill(false);
    mouse_pos_ = {};
    wheel_delta_ = 0.0f;
    any_pressed_ = false;
    pending_text_.clear();
}

} // namespace poko::input
