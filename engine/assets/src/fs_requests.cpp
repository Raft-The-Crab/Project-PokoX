#include "poko/assets/filesystem_provider.h"

#include <cstddef>
#include <exception>
#include <span>
#include <string>
#include <utility>

#include "fs_detail.h"

namespace poko {

void FilesystemAssetProvider::finish_request(Request& request, Asset& asset, bool succeeded,
                                             const AssetPayload& payload) {
    request.done = true;
    if (succeeded) {
        asset.state = AssetState::Ready;
        asset.payload = payload;
        cache_[request.logical_path] = payload;
        request.result = LoadResult{request.asset, AssetState::Ready, payload};
    } else {
        asset.state = AssetState::Failed;
        ++failed_loads_;
        request.result = LoadResult{request.asset, AssetState::Failed, AssetPayload{}};
    }
    asset.pending_request = kInvalidLoadRequest;
}

Result<LoadRequestId> FilesystemAssetProvider::request_load(const AssetDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<LoadRequestId>::fail(ErrorCode::FailedPrecondition,
                                           "asset provider is not initialized");
    }
    if (!valid(desc)) {
        return Result<LoadRequestId>::fail(ErrorCode::InvalidArgument, "invalid asset descriptor");
    }

    const HandleIndex index = allocate_asset();
    const HandleIndex generation = asset_generations_[index];
    const AssetHandle handle = make_handle(index, generation);
    Asset& asset = assets_[index];
    asset.alive = true;
    asset.desc = desc;
    asset.logical_path = std::string(desc.logical_path);
    asset.state = AssetState::Loading;

    const LoadRequestId id = next_request_id_++;
    requests_.resize(static_cast<std::size_t>(id));
    Request& request = requests_[static_cast<std::size_t>(id - 1)];
    request = Request{};
    request.alive = true;
    request.asset = handle;
    request.logical_path = asset.logical_path;
    asset.pending_request = id;
    return Result<LoadRequestId>::success(id);
}

Status FilesystemAssetProvider::cancel_load(LoadRequestId id) {
    std::lock_guard<std::mutex> guard(mutex_);
    Request* request = find_request(id);
    if (request == nullptr) {
        return Status::fail(ErrorCode::NotFound, "load request is not live");
    }
    if (request->done) {
        return Status::fail(ErrorCode::FailedPrecondition, "load request already finished");
    }
    if (Asset* asset = find_asset(request->asset)) {
        if (asset->pending_request == id) {
            asset->state = AssetState::Unloaded;
            asset->pending_request = kInvalidLoadRequest;
        }
    }
    request->alive = false;
    return Status::success();
}

bool FilesystemAssetProvider::poll_load(LoadRequestId id, LoadResult& out) {
    std::lock_guard<std::mutex> guard(mutex_);
    Request* request = find_request(id);
    if (request == nullptr || !request->done) {
        return false;
    }
    out = request->result;
    request->alive = false; // one-shot
    return true;
}

void FilesystemAssetProvider::update() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return;
    }
    for (auto& request : requests_) {
        if (!request.alive || request.done) {
            continue;
        }
        Asset* asset = find_asset(request.asset);
        if (asset == nullptr || !handle_live(request.asset)) {
            request.alive = false; // asset was released while loading
            continue;
        }

        // Cache hit: no filesystem I/O. Copy first — finish_request writes to
        // cache_ and would invalidate the iterator.
        const auto cached = cache_.find(request.logical_path);
        if (cached != cache_.end()) {
            const AssetPayload payload = cached->second;
            finish_request(request, *asset, true, payload);
            continue;
        }

        std::string data;
        bool read_ok = false;
        try {
            read_ok = fs_.exists(request.logical_path) && fs_.read_file(request.logical_path, data);
        } catch (const std::exception&) {
            // LocalFileSystem::resolve rejects rooted/traversal paths by throw.
            read_ok = false;
        }
        if (!read_ok) {
            finish_request(request, *asset, false, AssetPayload{});
            continue;
        }

        AssetPayload payload;
        const std::span<const std::byte> bytes{
            reinterpret_cast<const std::byte*>(data.data()), data.size()};
        if (!assets::detail::decode(bytes, asset->desc.kind, payload)) {
            finish_request(request, *asset, false, AssetPayload{});
            continue;
        }
        finish_request(request, *asset, true, payload);
    }
}

} // namespace poko
