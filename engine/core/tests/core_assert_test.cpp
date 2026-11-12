#include "poko_test_harness.h"

#include "poko/core/assert.h"
#include "poko/core/build_config.h"
#include "poko/core/log.h"

#if defined(POKO_ENABLE_ASSERTS)

POKO_TEST(assert_passes_on_true) {
    POKO_ASSERT(true, "must not fire");
    POKO_CHECK(poko::config::kAssertsEnabled);
}

POKO_TEST(assert_throws_on_false) {
    // Suppress console noise while still exercising the logging path.
    poko::Logger::instance().set_sink([](const poko::LogRecord&) {});
    POKO_CHECK_THROWS(POKO_ASSERT(false, "expected"), poko::AssertionError);
    poko::Logger::instance().clear_sink();
}

#else

POKO_TEST(asserts_are_compiled_out) {
    POKO_CHECK(!poko::config::kAssertsEnabled);
    // Must be a no-op: a failing condition here must not abort the test.
    POKO_ASSERT(false, "compiled out");
}

#endif

POKO_TEST_MAIN()
