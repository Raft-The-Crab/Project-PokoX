#include "poko/platform/fs.h"

#include <stdexcept>
#include <system_error>

namespace poko {
namespace {

bool contains_parent_component(const std::filesystem::path& path) {
    for (const auto& component : path) {
        if (component == "..") {
            return true;
        }
    }
    return false;
}

} // namespace

LocalFileSystem::LocalFileSystem(std::filesystem::path root)
    : root_(std::filesystem::absolute(std::move(root)).lexically_normal()) {}

std::filesystem::path LocalFileSystem::resolve(std::string_view path) const {
    if (path.empty()) {
        throw std::invalid_argument("empty path");
    }
    const std::filesystem::path relative{std::string(path)};
    // Reject root-relative, drive-relative and rooted paths: all of them can
    // make operator/ replace root_ instead of appending to it.
    if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
        throw std::invalid_argument("rooted paths are not permitted");
    }
    if (contains_parent_component(relative)) {
        throw std::invalid_argument("parent traversal is not permitted");
    }
    return (root_ / relative).lexically_normal();
}

std::string LocalFileSystem::absolute(std::string_view path) const {
    return resolve(path).string();
}

} // namespace poko
