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

} // namespace poko::ai
