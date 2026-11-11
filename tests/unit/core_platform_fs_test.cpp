#include "poko_test_harness.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "poko_ownership.h"
#include "poko_platform_fs.h"

POKO_TEST(local_filesystem_reads_within_root) {
    const std::filesystem::path root = std::filesystem::current_path();
    const std::string filename = "poko_platform_fs_test.txt";
    {
        std::ofstream out(root / filename, std::ios::binary);
        out << "hello pokox";
    }
    poko::ScopeExit cleanup([&root, &filename] {
        std::error_code error;
        std::filesystem::remove(root / filename, error);
    });

    poko::LocalFileSystem fs(root);
    POKO_CHECK(fs.exists(filename));
    POKO_CHECK(!fs.exists("does_not_exist.txt"));

    std::string contents;
    POKO_CHECK(fs.read_file(filename, contents));
    POKO_CHECK_EQ(contents, std::string("hello pokox"));
    POKO_CHECK_EQ(fs.file_size(filename), std::uintmax_t{11});
    POKO_CHECK(fs.absolute(filename).size() > filename.size());

    std::string missing;
    POKO_CHECK(!fs.read_file("does_not_exist.txt", missing));
}

POKO_TEST(local_filesystem_rejects_traversal_and_rooted_paths) {
    const std::filesystem::path root = std::filesystem::current_path();
    poko::LocalFileSystem fs(root);

    bool threw = false;
    try {
        (void)fs.absolute("../outside.txt");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    POKO_CHECK(threw);

    threw = false;
    try {
        (void)fs.absolute((root / "escape.txt").string());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    POKO_CHECK(threw);

    threw = false;
    try {
        (void)fs.absolute("");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    POKO_CHECK(threw);
}

POKO_TEST_MAIN()
