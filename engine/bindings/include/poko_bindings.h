#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "poko_result.h"

namespace poko::bindings {

// Script binding skeleton (Phase 1, systemplan/11): name -> callable registry
// that MUTE (and later Studio tooling) uses to expose engine APIs. Values are
// type-erased variant-lite PODs for now; a real variant arrives with MUTE.

enum class ValueKind : std::uint8_t {
    Null = 0,
    Bool,
    Int,
    Float,
    String,
    Handle, // opaque u64 (entity/object ids)
};

struct Value {
    ValueKind kind = ValueKind::Null;
    bool b = false;
    std::int64_t i = 0;
    double f = 0.0;
    std::string s;
    std::uint64_t h = 0;
};

using NativeFn = std::function<Result<Value>(const std::vector<Value>& args)>;

// Flat function registry. Names are dot-namespaced ("world.spawn").
class BindingRegistry {
public:
    BindingRegistry() = default;

    // Registers fn under `name`; last registration wins on clash.
    Status register_function(std::string name, NativeFn fn);
    NativeFn find(const std::string& name) const;

    // Lists registered names sorted (stable output for docs/UI).
    std::vector<std::string> names() const;
    std::uint32_t count() const noexcept;

private:
    std::unordered_map<std::string, NativeFn> functions_;
};

} // namespace poko::bindings
