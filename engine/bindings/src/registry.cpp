#include "poko/bindings/registry.h"

#include <algorithm>

namespace poko::bindings {

Status BindingRegistry::register_function(std::string name, NativeFn fn) {
    if (name.empty() || !fn) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid binding name or function");
    }
    functions_[std::move(name)] = std::move(fn);
    return Status::success();
}

NativeFn BindingRegistry::find(const std::string& name) const {
    const auto it = functions_.find(name);
    return it != functions_.end() ? it->second : NativeFn{};
}

std::vector<std::string> BindingRegistry::names() const {
    std::vector<std::string> out;
    out.reserve(functions_.size());
    for (const auto& [name, fn] : functions_) {
        (void)fn;
        out.push_back(name);
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::uint32_t BindingRegistry::count() const noexcept {
    return static_cast<std::uint32_t>(functions_.size());
}

} // namespace poko::bindings
