#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace poko::reflect {

// Reflection skeleton (Phase 1): name -> type registry for serialization,
// scene editing and (later) MUTE bindings. Members are described by offset +
// kind so POD structs can be walked without codegen.

enum class MemberKind : std::uint8_t {
    Bool,
    Int32,
    UInt32,
    Float,
    String, // std::string / std::string_view
    Vec3,   // math::Vec3 (12 bytes)
    Quat,   // math::Quat (16 bytes)
    Handle, // TypedHandle (8 bytes)
    Enum8,  // std::uint8_t enum
    User,   // nested registered type
};

struct TypeInfo;

struct MemberDesc {
    std::string_view name;
    MemberKind kind = MemberKind::User;
    std::size_t offset = 0;              // byte offset in parent struct
    std::size_t size = 0;                // byte size of member
    const TypeInfo* user_type = nullptr; // for MemberKind::User
};

struct TypeInfo {
    std::string_view name;
    std::type_index cpp_type;
    std::size_t size = 0;
    std::vector<MemberDesc> members;
};

// Global registry; populated at startup by REGISTER_TYPE macros or manual
// registration. Lookup is by name or std::type_index.
class TypeRegistry {
public:
    static TypeRegistry& instance() noexcept;

    // Registers a type description. Last registration wins on name clash.
    void register_type(const TypeInfo& info);

    const TypeInfo* find(std::string_view name) const noexcept;
    const TypeInfo* find(std::type_index cpp_type) const noexcept;

    std::size_t size() const noexcept;

private:
    TypeRegistry() = default;
    std::unordered_map<std::string, const TypeInfo*> by_name_;
    std::unordered_map<std::type_index, const TypeInfo*> by_cpp_;
    std::vector<const TypeInfo*> owned_;
};

} // namespace poko::reflect
