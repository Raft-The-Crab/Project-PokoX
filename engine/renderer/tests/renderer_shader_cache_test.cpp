#include "poko_test_harness.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "poko/core/render_contracts.h"
#include "poko/renderer/shader_cache.h"

POKO_TEST(hash_is_deterministic_and_sensitive_to_inputs) {
    poko::ShaderDesc vertex;
    vertex.stage = poko::ShaderStage::Vertex;
    vertex.source = "void main() {}";

    poko::ShaderDesc fragment = vertex;
    fragment.stage = poko::ShaderStage::Fragment;

    poko::ShaderDesc different_source = vertex;
    different_source.source = "void main() { int x = 1; }";

    POKO_CHECK_EQ(poko::ShaderCache::hash(vertex), poko::ShaderCache::hash(vertex));
    POKO_CHECK(poko::ShaderCache::hash(vertex) != poko::ShaderCache::hash(fragment));
    POKO_CHECK(poko::ShaderCache::hash(vertex) != poko::ShaderCache::hash(different_source));
}

POKO_TEST(cache_compiles_once_and_then_hits) {
    poko::ShaderCache cache;
    poko::ShaderDesc desc;
    desc.source = "void main() {}";
    desc.entry_point = "main";

    const auto first = cache.get_or_compile(desc);
    POKO_CHECK(first.key != 0);
    POKO_CHECK_EQ(first.source_bytes, desc.source.size());
    POKO_CHECK_EQ(first.entry_point, std::string("main"));
    POKO_CHECK(!first.artifact.empty());
    POKO_CHECK_EQ(cache.size(), std::size_t{1});
    POKO_CHECK_EQ(cache.compile_count(), std::size_t{1});
    POKO_CHECK_EQ(cache.hit_count(), std::size_t{0});

    const auto second = cache.get_or_compile(desc);
    POKO_CHECK_EQ(second.key, first.key);
    POKO_CHECK_EQ(cache.compile_count(), std::size_t{1});
    POKO_CHECK_EQ(cache.hit_count(), std::size_t{1});
    POKO_CHECK(cache.contains(first.key));
    POKO_CHECK(!cache.contains(0));

    cache.clear();
    POKO_CHECK_EQ(cache.size(), std::size_t{0});
    POKO_CHECK_EQ(cache.compile_count(), std::size_t{0});
}

POKO_TEST(different_shaders_get_different_keys) {
    poko::ShaderCache cache;
    poko::ShaderDesc vertex;
    vertex.source = "void main() {}";
    poko::ShaderDesc fragment = vertex;
    fragment.stage = poko::ShaderStage::Fragment;
    const auto a = cache.get_or_compile(vertex);
    const auto b = cache.get_or_compile(fragment);
    POKO_CHECK(a.key != b.key);
    POKO_CHECK_EQ(cache.size(), std::size_t{2});
}

POKO_TEST_MAIN()
