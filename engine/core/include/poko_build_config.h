#pragma once

// Compile-time description of the active build configuration. The POKO_*
// definitions are supplied by config/cmake/PokoTargets.cmake, never by hand.

namespace poko::config {

#if defined(POKO_CONFIG_DEBUG)
inline constexpr bool kDebug = true;
#else
inline constexpr bool kDebug = false;
#endif

#if defined(POKO_CONFIG_DEVELOPMENT)
inline constexpr bool kDevelopment = true;
#else
inline constexpr bool kDevelopment = false;
#endif

#if defined(POKO_CONFIG_RELEASE)
inline constexpr bool kRelease = true;
#else
inline constexpr bool kRelease = false;
#endif

#if defined(POKO_CONFIG_SERVER)
inline constexpr bool kServer = true;
#else
inline constexpr bool kServer = false;
#endif

#if defined(POKO_ENABLE_ASSERTS)
inline constexpr bool kAssertsEnabled = true;
#else
inline constexpr bool kAssertsEnabled = false;
#endif

#if defined(POKO_DEV_TOOLS)
inline constexpr bool kDevTools = true;
#else
inline constexpr bool kDevTools = false;
#endif

#if defined(POKO_HEADLESS)
inline constexpr bool kHeadless = true;
#else
inline constexpr bool kHeadless = false;
#endif

inline constexpr bool kReleaseLike = kRelease || kServer;
inline constexpr bool kDevelopmentLike = kDebug || kDevelopment;

constexpr const char* name() noexcept {
    if constexpr (kDebug) {
        return "Debug";
    } else if constexpr (kDevelopment) {
        return "Development";
    } else if constexpr (kRelease) {
        return "Release";
    } else if constexpr (kServer) {
        return "Server";
    } else {
        return "Unknown";
    }
}

}  // namespace poko::config
