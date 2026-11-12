#include "poko_test_harness.h"

#include <cstring>
#include <string>

#include "poko/core/result.h"

POKO_TEST(status_ok_and_fail) {
    const auto ok = poko::Status::success();
    POKO_CHECK(ok.ok());
    POKO_CHECK(ok.code() == poko::ErrorCode::None);

    const auto fail = poko::Status::fail(poko::ErrorCode::NotFound, "missing");
    POKO_CHECK(!fail.ok());
    POKO_CHECK(fail.code() == poko::ErrorCode::NotFound);
    POKO_CHECK_EQ(fail.error().message, std::string("missing"));
}

POKO_TEST(result_ok_and_fail) {
    const auto value = poko::Result<int>::success(42);
    POKO_CHECK(value.has_value());
    POKO_CHECK_EQ(value.value(), 42);

    const auto error = poko::Result<int>::fail(poko::ErrorCode::OutOfRange, "too big");
    POKO_CHECK(!error.has_value());
    POKO_CHECK(error.error().code == poko::ErrorCode::OutOfRange);
    POKO_CHECK_EQ(error.error().message, std::string("too big"));
}

POKO_TEST(error_codes_have_stable_names) {
    POKO_CHECK(std::strcmp(poko::to_string(poko::ErrorCode::None), "none") == 0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::ErrorCode::InvalidArgument), "invalid_argument") ==
               0);
    POKO_CHECK(std::strcmp(poko::to_string(poko::ErrorCode::Unimplemented), "unimplemented") == 0);
}

POKO_TEST_MAIN()
