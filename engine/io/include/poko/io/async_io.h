#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "poko/core/api_seams.h"
#include "poko/core/result.h"

namespace poko::io {

// Async IO skeleton (Phase 1): queued file reads over IPlatformFileSystem.
// The API is async-shaped; the skeleton completes requests inside update()
// (a worker-pool backend slots in later without changing callers).

using IoRequestId = std::uint64_t;
inline constexpr IoRequestId kInvalidIoRequest = 0;

struct IoResult {
    Status status = Status::success();
    std::string data; // file bytes on success
};

class AsyncIo {
public:
    explicit AsyncIo(api::IPlatformFileSystem& fs) noexcept : fs_(fs) {}

    // Queues a read; returns a request id for poll().
    Result<IoRequestId> request_read(std::string_view path);

    // Returns true once the request finished; `out` holds status + bytes.
    // One-shot: the request is removed after a successful poll.
    bool poll(IoRequestId id, IoResult& out);

    // Drains queued reads through the filesystem.
    void update();

private:
    struct Request {
        bool alive = false;
        std::string path;
        bool done = false;
        IoResult result{};
    };

    api::IPlatformFileSystem& fs_;
    mutable std::mutex mutex_;
    std::vector<Request> requests_;
    IoRequestId next_id_ = 1;
};

} // namespace poko::io
