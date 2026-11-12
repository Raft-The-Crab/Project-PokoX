#include "poko_test_harness.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "poko/io/async_io.h"
#include "poko/platform/fs.h"

namespace {

std::filesystem::path make_temp_dir() {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("poko_io_test_" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    return dir;
}

} // namespace

POKO_TEST(async_read_roundtrip) {
    const auto dir = make_temp_dir();
    {
        std::ofstream out(dir / "data.txt", std::ios::binary);
        out << "hello io";
    }
    poko::LocalFileSystem fs(dir);
    poko::io::AsyncIo io(fs);

    const auto request = io.request_read("data.txt");
    POKO_CHECK(request.ok());

    poko::io::IoResult result;
    POKO_CHECK(!io.poll(request.value(), result)); // pending until update

    io.update();
    POKO_CHECK(io.poll(request.value(), result));
    POKO_CHECK(result.status.ok());
    POKO_CHECK_EQ(result.data, "hello io");

    std::filesystem::remove_all(dir);
}

POKO_TEST(missing_file_reports_not_found) {
    const auto dir = make_temp_dir();
    poko::LocalFileSystem fs(dir);
    poko::io::AsyncIo io(fs);

    const auto request = io.request_read("nope.bin");
    POKO_CHECK(request.ok());
    io.update();

    poko::io::IoResult result;
    POKO_CHECK(io.poll(request.value(), result));
    POKO_CHECK(!result.status.ok());
    POKO_CHECK(result.data.empty());

    std::filesystem::remove_all(dir);
}

POKO_TEST(empty_path_rejected) {
    const auto dir = make_temp_dir();
    poko::LocalFileSystem fs(dir);
    poko::io::AsyncIo io(fs);
    POKO_CHECK(!io.request_read("").ok());
    std::filesystem::remove_all(dir);
}

POKO_TEST_MAIN()
