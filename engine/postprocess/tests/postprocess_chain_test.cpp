#include "poko_test_harness.h"

#include "poko/postprocess/chain.h"

POKO_TEST(chain_order_and_lifecycle) {
    poko::postprocess::PostProcessChain chain;
    poko::PostProcessDesc bloom;
    bloom.kind = poko::PostProcessKind::Bloom;
    const auto b = chain.add(bloom);
    POKO_CHECK(b.ok());

    poko::PostProcessDesc grade;
    grade.kind = poko::PostProcessKind::ColorGrade;
    const auto g = chain.add(grade);
    POKO_CHECK(g.ok());
    POKO_CHECK_EQ(chain.count(), 2u);

    const auto frame = chain.frame_chain();
    POKO_CHECK_EQ(frame.size(), 2u);
    POKO_CHECK(frame[0].kind == poko::PostProcessKind::Bloom);
    POKO_CHECK(frame[1].kind == poko::PostProcessKind::ColorGrade);

    POKO_CHECK(chain.remove(b.value()).ok());
    POKO_CHECK_EQ(chain.count(), 1u);
    POKO_CHECK(!chain.get(b.value()).ok());
}

POKO_TEST(invalid_rejected) {
    poko::postprocess::PostProcessChain chain;
    POKO_CHECK(!chain.add(poko::PostProcessDesc{}).ok());
}

POKO_TEST_MAIN()
