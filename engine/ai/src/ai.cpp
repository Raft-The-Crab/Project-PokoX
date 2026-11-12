#include "poko/ai/ai.h"

namespace poko::ai {

void Blackboard::set_float(const std::string& key, float value) {
    values_[key] = Value{Value::Type::Float, value, 0, false, {}};
}
void Blackboard::set_int(const std::string& key, int value) {
    values_[key] = Value{Value::Type::Int, 0.0f, value, false, {}};
}
void Blackboard::set_bool(const std::string& key, bool value) {
    values_[key] = Value{Value::Type::Bool, 0.0f, 0, value, {}};
}
void Blackboard::set_string(const std::string& key, const std::string& value) {
    values_[key] = Value{Value::Type::String, 0.0f, 0, false, value};
}

float Blackboard::get_float(const std::string& key, float fallback) const {
    const auto it = values_.find(key);
    return it != values_.end() && it->second.type == Value::Type::Float ? it->second.f : fallback;
}
int Blackboard::get_int(const std::string& key, int fallback) const {
    const auto it = values_.find(key);
    return it != values_.end() && it->second.type == Value::Type::Int ? it->second.i : fallback;
}
bool Blackboard::get_bool(const std::string& key, bool fallback) const {
    const auto it = values_.find(key);
    return it != values_.end() && it->second.type == Value::Type::Bool ? it->second.b : fallback;
}
std::string Blackboard::get_string(const std::string& key, const std::string& fallback) const {
    const auto it = values_.find(key);
    return it != values_.end() && it->second.type == Value::Type::String ? it->second.s : fallback;
}
bool Blackboard::has(const std::string& key) const {
    return values_.find(key) != values_.end();
}
void Blackboard::erase(const std::string& key) {
    values_.erase(key);
}

StateId StateMachine::add_state(std::string name, std::function<void(float, Blackboard&)> on_update,
                                std::function<void(Blackboard&)> on_enter,
                                std::function<void(Blackboard&)> on_exit) {
    State state;
    state.name = std::move(name);
    state.on_update = std::move(on_update);
    state.on_enter = std::move(on_enter);
    state.on_exit = std::move(on_exit);
    states_.push_back(std::move(state));
    const StateId id = static_cast<StateId>(states_.size() - 1);
    if (initial_ == kInvalidState) {
        initial_ = id;
    }
    return id;
}

Status StateMachine::add_transition(StateId from, Transition transition) {
    if (from >= states_.size() || transition.target >= states_.size()) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid state id");
    }
    states_[from].transitions.push_back(std::move(transition));
    return Status::success();
}

Status StateMachine::set_initial(StateId state) {
    if (state >= states_.size()) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid state id");
    }
    initial_ = state;
    return Status::success();
}

const std::string& StateMachine::current_name() const {
    static const std::string kNone = "<none>";
    return current_ < states_.size() ? states_[current_].name : kNone;
}

StateId StateMachine::update(float delta_seconds, Blackboard& blackboard) {
    if (current_ == kInvalidState && initial_ != kInvalidState) {
        current_ = initial_;
        if (states_[current_].on_enter) {
            states_[current_].on_enter(blackboard);
        }
    }
    if (current_ == kInvalidState || current_ >= states_.size()) {
        return current_;
    }
    // Transitions evaluate before update (first match wins).
    for (const auto& transition : states_[current_].transitions) {
        if (transition.condition && transition.condition(blackboard)) {
            if (states_[current_].on_exit) {
                states_[current_].on_exit(blackboard);
            }
            current_ = transition.target;
            if (states_[current_].on_enter) {
                states_[current_].on_enter(blackboard);
            }
            break;
        }
    }
    if (states_[current_].on_update) {
        states_[current_].on_update(delta_seconds, blackboard);
    }
    return current_;
}

} // namespace poko::ai
