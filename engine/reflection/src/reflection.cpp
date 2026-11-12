#include "poko/reflection/reflection.h"

namespace poko::reflect {

TypeRegistry& TypeRegistry::instance() noexcept {
    static TypeRegistry registry;
    return registry;
}

void TypeRegistry::register_type(const TypeInfo& info) {
    // Caller-owned: TypeInfo (and its name/members strings) must outlive the
    // process — static registration is the expected usage.
    by_name_[std::string(info.name)] = &info;
    by_cpp_[info.cpp_type] = &info;
    owned_.push_back(&info);
}

const TypeInfo* TypeRegistry::find(std::string_view name) const noexcept {
    auto it = by_name_.find(std::string(name));
    return it != by_name_.end() ? it->second : nullptr;
}

const TypeInfo* TypeRegistry::find(std::type_index cpp_type) const noexcept {
    auto it = by_cpp_.find(cpp_type);
    return it != by_cpp_.end() ? it->second : nullptr;
}

std::size_t TypeRegistry::size() const noexcept {
    return owned_.size();
}

} // namespace poko::reflect
