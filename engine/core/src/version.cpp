#include "poko/core/version.h"

#include "poko/core/build_config.h"

namespace poko {

const char* version_string() noexcept {
    return "0.1.0";
}

const char* build_config_name() noexcept {
    return config::name();
}

const char* platform_name() noexcept {
#if defined(_WIN32)
    return "windows";
#elif defined(__linux__)
    return "linux";
#elif defined(__APPLE__)
    return "macos";
#else
    return "unknown";
#endif
}

static_assert(kVersion.major == 0 && kVersion.minor == 1 && kVersion.patch == 0,
              "version_string() must match kVersion");

} // namespace poko
