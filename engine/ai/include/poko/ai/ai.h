#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "poko/core/result.h"

namespace poko::ai {

// AI skeleton (Phase 1, systemplan/07): finite-state behavior with a shared
// blackboard. Utility AI / behavior trees come later; this owns state
// transitions, entry/exit hooks and per-tick updates. Deterministic: states
// run in registration order, transitions evaluate in declared order.

using StateId = std::uint32_t;
inline constexpr StateId kInvalidState = 0xFFFFFFFFu;

class Blackboard {
public:
    void set_float(const std::string& key, float value);
    void set_int(const std::string& key, int value);
    void set_bool(const std::string& key, bool value);
    void set_string(const std::string& key, const std::string& value);

    float get_float(const std::string& key, float fallback = 0.0f) const;
    int get_int(const std::string& key, int fallback = 0) const;
    bool get_bool(const std::string& key, bool fallback = false) const;
    std::string get_string(const std::string& key, const std::string& fallback = {}) const;
    bool has(const std::string& key) const;
    void erase(const std::string& key);

private:
    struct Value {
        enum class Type : std::uint8_t { Float, Int, Bool, String };
        Type type = Type::Float;
        float f = 0.0f;
        int i = 0;
        bool b = false;
        std::string s;
    };
    std::unordered_map<std::string, Value> values_;
};

struct Transition {
    StateId target = kInvalidState;
    std::function<bool(const Blackboard&)> condition;
};

class StateMachine {
public:
    StateMachine() = default;

    StateId add_state(std::string name, std::function<void(float, Blackboard&)> on_update = {},
                      std::function<void(Blackboard&)> on_enter = {},
                      std::function<void(Blackboard&)> on_exit = {});
    Status add_transition(StateId from, Transition transition);
    Status set_initial(StateId state);

    // Advances one tick; returns the active state after transitions.
    StateId update(float delta_seconds, Blackboard& blackboard);
    StateId current() const noexcept {
        return current_;
    }
    const std::string& current_name() const;

private:
    struct State {
        std::string name;
        std::function<void(float, Blackboard&)> on_update;
        std::function<void(Blackboard&)> on_enter;
        std::function<void(Blackboard&)> on_exit;
        std::vector<Transition> transitions;
    };

    std::vector<State> states_;
    StateId current_ = kInvalidState;
    StateId initial_ = kInvalidState;
};

} // namespace poko::ai
