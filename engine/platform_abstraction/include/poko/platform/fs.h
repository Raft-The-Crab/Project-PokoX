#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "poko/core/api_seams.h"

namespace poko {

// LOCAL_DEV implementation of api::IPlatformFileSystem. It is root-scoped:
// every path is resolved under root_, absolute paths and ".." components are
// rejected, so untrusted package paths cannot escape (systemplan/13,
// masterplan/17). This is a local development filesystem, not the production
// package I/O path; the engine core itself stays OS-agnostic.
//
// Thread-safety contract: all methods are const and safe to call concurrently.
class LocalFileSystem final : public api::IPlatformFileSystem {
public:
    explicit LocalFileSystem(std::filesystem::path root);

    bool exists(std::string_view path) const override;
    bool read_file(std::string_view path, std::string& out) const override;
    std::uintmax_t file_size(std::string_view path) const override;
    std::string absolute(std::string_view path) const override;

    const std::filesystem::path& root() const noexcept {
        return root_;
    }

private:
    std::filesystem::path resolve(std::string_view path) const;

    std::filesystem::path root_;
};

} // namespace poko
