#include "poko_assert.h"

#include <sstream>

#include "poko_log.h"

namespace poko::detail {

void assert_check(bool condition, const char* expression, const char* file,
                  int line, std::string_view message) {
    if (!condition) {
        assert_failed(expression, file, line, message);
    }
}

[[noreturn]] void assert_failed(const char* expression, const char* file,
                                int line, std::string_view message) {
    std::ostringstream out;
    out << "assertion failed: " << expression << " at " << file << ':' << line;
    if (!message.empty()) {
        out << " (" << message << ')';
    }
    POKO_LOG_ERROR("assert", out.str());
    throw AssertionError(out.str());
}

}  // namespace poko::detail
