#pragma once

#include <stdexcept>
#include <string_view>

namespace poko {

class AssertionError : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

namespace detail {

void assert_check(bool condition, const char* expression, const char* file, int line,
                  std::string_view message);

[[noreturn]] void assert_failed(const char* expression, const char* file, int line,
                                std::string_view message);

} // namespace detail
} // namespace poko

// Assertions exist only in Debug and Development builds. In Release and
// Server, POKO_ASSERT compiles away entirely (systemplan/00).
#if defined(POKO_ENABLE_ASSERTS)
#define POKO_ASSERT(condition, message)                                                            \
    ::poko::detail::assert_check(static_cast<bool>(condition), #condition, __FILE__, __LINE__,     \
                                 (message))
#else
#define POKO_ASSERT(condition, message) ((void)0)
#endif
