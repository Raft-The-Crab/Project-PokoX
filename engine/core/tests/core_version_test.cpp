#include "poko_test_harness.h"

#include <cstring>

#include "poko/core/build_config.h"
#include "poko/core/version.h"

POKO_TEST(version_string_matches_constant) {
    POKO_CHECK(std::strcmp(poko::version_string(), "0.1.0") == 0);
    POKO_CHECK_EQ(poko::kVersion.major, 0);
    POKO_CHECK_EQ(poko::kVersion.minor, 1);
    POKO_CHECK_EQ(poko::kVersion.patch, 0);
}

POKO_TEST(build_config_is_known) {
    POKO_CHECK(std::strcmp(poko::build_config_name(), poko::config::name()) == 0);
    POKO_CHECK(std::strlen(poko::build_config_name()) > 0);
    POKO_CHECK(std::strcmp(poko::build_config_name(), "Unknown") != 0);
}

POKO_TEST(platform_is_reported) {
    POKO_CHECK(std::strlen(poko::platform_name()) > 0);
}

POKO_TEST_MAIN()
