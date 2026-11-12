#include "poko_test_harness.h"

#include <cstddef>
#include <cstdint>

#include "poko/renderer/scene_submission.h"

namespace {
poko::DrawCall make_call(std::uint32_t pipeline_index, std::uint32_t sort_key) {
    poko::DrawCall call;
    call.pipeline = poko::make_handle(pipeline_index, 0);
    call.index_count = 6;
    call.sort_key = sort_key;
    return call;
}
} // namespace

POKO_TEST(invalid_draws_are_ignored) {
    poko::SceneSubmission submission;
    submission.submit(poko::DrawCall{});
    POKO_CHECK_EQ(submission.size(), std::size_t{0});
}

POKO_TEST(batch_sorts_merges_and_counts_batches) {
    poko::SceneSubmission submission;
    submission.submit(make_call(2, 0));
    submission.submit(make_call(1, 0));
    submission.submit(make_call(1, 0));
    submission.submit(make_call(1, 1));
    POKO_CHECK_EQ(submission.size(), std::size_t{4});

    submission.batch();
    POKO_CHECK_EQ(submission.draws().size(), std::size_t{3});
    POKO_CHECK_EQ(submission.draws()[0].pipeline.index, 1u);
    POKO_CHECK_EQ(submission.draws()[0].sort_key, 0u);
    POKO_CHECK_EQ(submission.draws()[0].instance_count, 2u);
    POKO_CHECK_EQ(submission.batch_count(), std::size_t{2});
    POKO_CHECK_EQ(submission.total_triangles(), std::uint64_t{8});

    // Idempotent.
    submission.batch();
    POKO_CHECK_EQ(submission.draws().size(), std::size_t{3});
    POKO_CHECK_EQ(submission.total_triangles(), std::uint64_t{8});
}

POKO_TEST(clear_resets_everything) {
    poko::SceneSubmission submission;
    submission.submit(make_call(1, 0));
    submission.batch();
    POKO_CHECK_EQ(submission.batch_count(), std::size_t{1});
    submission.clear();
    POKO_CHECK_EQ(submission.size(), std::size_t{0});
    POKO_CHECK_EQ(submission.batch_count(), std::size_t{0});
    POKO_CHECK_EQ(submission.total_triangles(), std::uint64_t{0});
}

POKO_TEST_MAIN()
