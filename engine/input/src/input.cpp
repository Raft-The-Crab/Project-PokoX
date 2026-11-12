#include "poko/input/input.h"

#include <algorithm>

namespace poko::input {
namespace {

void append_utf8(char32_t cp, std::string& out) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

} // namespace

Status InputSystem::feed_event(const InputEvent& event) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid(event)) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid input event");
    }
    switch (event.type) {
    case EventType::KeyDown: {
        const auto i = key_index(event.key);
        if (!keys_down_[i]) {
            keys_pressed_[i] = true;
            any_pressed_ = true;
        }
        keys_down_[i] = true;
        break;
    }
    case EventType::KeyUp: {
        const auto i = key_index(event.key);
        if (keys_down_[i]) {
            keys_released_[i] = true;
        }
        keys_down_[i] = false;
        break;
    }
    case EventType::MouseButtonDown: {
        const auto i = button_index(event.button);
        if (!buttons_down_[i]) {
            buttons_pressed_[i] = true;
        }
        buttons_down_[i] = true;
        break;
    }
    case EventType::MouseButtonUp: {
        const auto i = button_index(event.button);
        buttons_down_[i] = false;
        break;
    }
    case EventType::MouseMove:
        mouse_pos_ = event.position;
        break;
    case EventType::MouseWheel:
        wheel_delta_ += event.wheel;
        break;
    case EventType::TextInput:
        pending_text_.push_back(event.codepoint);
        break;
    }
    return Status::success();
}

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
        append_utf8(cp, out);
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
