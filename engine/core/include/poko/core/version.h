#pragma once

#include <cstdint>

namespace poko {

struct Version {
    std::uint16_t major;
    std::uint16_t minor;
    std::uint16_t patch;
};

inline constexpr Version kVersion{0, 1, 0};

const char* version_string() noexcept;
const char* build_config_name() noexcept;
const char* platform_name() noexcept;

} // namespace poko
