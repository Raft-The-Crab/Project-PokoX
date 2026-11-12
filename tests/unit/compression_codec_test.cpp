#include "poko_test_harness.h"

#include <cstring>

#include "poko_compression.h"

POKO_TEST(store_codec_roundtrip) {
    const char text[] = "poko compression test payload";
    std::byte buf[64]{};
    std::byte out[64]{};

    poko::CodecRegistry registry;
    POKO_CHECK_EQ(registry.count(), 1u);
    auto* codec = registry.find(poko::codec::kStore);
    POKO_CHECK(codec != nullptr);
    POKO_CHECK(registry.find_by_name("store") == codec);
    POKO_CHECK(registry.find(poko::codec::kZstd) == nullptr);

    const auto written = codec->compress(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(text), sizeof(text)), buf, 1);
    POKO_CHECK(written.ok());
    POKO_CHECK_EQ(written.value(), sizeof(text));

    const auto read = codec->decompress(std::span<const std::byte>(buf, written.value()),
                                        std::span<std::byte>(out, sizeof(out)));
    POKO_CHECK(read.ok());
    POKO_CHECK(std::memcmp(out, text, sizeof(text)) == 0);
}

POKO_TEST(store_codec_rejects_small_output) {
    std::byte in[8]{};
    std::byte out[4]{};
    poko::CodecRegistry registry;
    auto* codec = registry.find(poko::codec::kStore);
    POKO_CHECK(!codec->compress(std::span<const std::byte>(in, 8), out, 1).ok());
}

POKO_TEST_MAIN()
