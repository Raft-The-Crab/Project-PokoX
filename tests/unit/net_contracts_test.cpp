#include "poko_test_harness.h"

#include "poko_net_contracts.h"

POKO_TEST(endpoint_text_validation) {
    POKO_CHECK(!poko::valid_endpoint_text(""));
    POKO_CHECK(!poko::valid_endpoint_text("has space"));
    POKO_CHECK(poko::valid_endpoint_text("127.0.0.1"));
    POKO_CHECK(poko::valid_endpoint_text("[::1]"));
}

POKO_TEST_MAIN()
