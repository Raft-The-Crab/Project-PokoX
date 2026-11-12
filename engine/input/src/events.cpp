#include "poko/input/input.h"

#include <algorithm>

#include "utf8.h"

namespace poko::input {

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

} // namespace poko::input
