#include "poko/platform/fs.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>

namespace poko {

bool LocalFileSystem::exists(std::string_view path) const {
    std::error_code error;
    return std::filesystem::exists(resolve(path), error) && !error;
}

bool LocalFileSystem::read_file(std::string_view path, std::string& out) const {
    std::ifstream stream(resolve(path), std::ios::binary);
    if (!stream) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return stream.good() || stream.eof();
}

std::uintmax_t LocalFileSystem::file_size(std::string_view path) const {
    std::error_code error;
    const auto size = std::filesystem::file_size(resolve(path), error);
    return error ? 0 : size;
}

} // namespace poko
