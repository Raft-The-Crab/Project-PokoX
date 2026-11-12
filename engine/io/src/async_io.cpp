#include "poko/io/async_io.h"

namespace poko::io {

Result<IoRequestId> AsyncIo::request_read(std::string_view path) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (path.empty()) {
        return Result<IoRequestId>::fail(ErrorCode::InvalidArgument, "empty path");
    }
    const IoRequestId id = next_id_++;
    Request request;
    request.alive = true;
    request.path = std::string(path);
    if (requests_.size() < id) {
        requests_.resize(id);
    }
    requests_[id - 1] = std::move(request);
    return Result<IoRequestId>::success(id);
}

bool AsyncIo::poll(IoRequestId id, IoResult& out) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (id == kInvalidIoRequest || id > requests_.size()) {
        return false;
    }
    auto& request = requests_[id - 1];
    if (!request.alive || !request.done) {
        return false;
    }
    out = std::move(request.result);
    request.alive = false;
    return true;
}

void AsyncIo::update() {
    std::lock_guard<std::mutex> guard(mutex_);
    for (std::size_t i = 0; i < requests_.size(); ++i) {
        auto& request = requests_[i];
        if (!request.alive || request.done) {
            continue;
        }
        request.done = true;
        IoResult result;
        if (!fs_.exists(request.path)) {
            result.status =
                Status::fail(ErrorCode::NotFound, "file does not exist: " + request.path);
        } else if (!fs_.read_file(request.path, result.data)) {
            result.status =
                Status::fail(ErrorCode::FailedPrecondition, "failed to read file: " + request.path);
        } else {
            result.status = Status::success();
        }
        request.result = std::move(result);
    }
}

} // namespace poko::io
