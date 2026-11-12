#include "poko_test_harness.h"

#include "poko_memory.h"

POKO_TEST(arena_allocates_and_aligns) {
    poko::memory::Arena arena(1024);
    void* a = arena.allocate(10, 4);
    void* b = arena.allocate(20, 8);
    POKO_CHECK(a != nullptr);
    POKO_CHECK(b != nullptr);
    POKO_CHECK(reinterpret_cast<std::uintptr_t>(b) % 8 == 0);
    POKO_CHECK(arena.used() <= arena.capacity());
}

POKO_TEST(arena_reset_reuses) {
    poko::memory::Arena arena(256);
    POKO_CHECK(arena.allocate(200) != nullptr);
    POKO_CHECK(arena.allocate(100) == nullptr); // out of space
    arena.reset();
    POKO_CHECK(arena.allocate(200) != nullptr);
}

POKO_TEST(pool_alloc_release) {
    poko::memory::Pool<int> pool(4);
    const auto h1 = pool.acquire();
    const auto h2 = pool.acquire();
    const auto h3 = pool.acquire();
    const auto h4 = pool.acquire();
    POKO_CHECK(h1 != 0 && h2 != 0 && h3 != 0 && h4 != 0);
    POKO_CHECK(pool.acquire() == 0); // capacity exhausted

    *pool.get(h1) = 42;
    POKO_CHECK_EQ(*pool.get(h1), 42);

    pool.release(h2);
    const auto h5 = pool.acquire(); // reuses the freed slot
    POKO_CHECK(h5 != 0);
    POKO_CHECK_EQ(h5, h2);
    POKO_CHECK(pool.acquire() == 0);
}

POKO_TEST_MAIN()
