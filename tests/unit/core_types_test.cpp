#include "poko_test_harness.h"

#include <cstddef>

#include "poko_types.h"

POKO_TEST(invalid_ids_are_rejected) {
    POKO_CHECK(!poko::is_valid(poko::kInvalidEntityId));
    POKO_CHECK(poko::is_valid(poko::EntityId{42}));
    POKO_CHECK(!poko::is_valid(poko::TypedHandle{}));
}

POKO_TEST(handles_require_matching_generation) {
    const auto first = poko::make_handle(7, 1);
    const auto second = poko::make_handle(7, 2);
    POKO_CHECK(poko::is_valid(first));
    POKO_CHECK(!poko::same_handle(first, second));
    POKO_CHECK(poko::same_handle(first, poko::make_handle(7, 1)));
    POKO_CHECK(!poko::same_handle(first, poko::TypedHandle{}));
}

POKO_TEST(serialized_widths_are_stable) {
    POKO_CHECK_EQ(sizeof(poko::EntityId), std::size_t{8});
    POKO_CHECK_EQ(sizeof(poko::TimestampUs), std::size_t{8});
    POKO_CHECK_EQ(poko::kInvalidHandleIndex, 0xFFFFFFFFu);
}

POKO_TEST_MAIN()
