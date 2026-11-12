#include "poko/io/async_io.h"

namespace poko::io {

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
