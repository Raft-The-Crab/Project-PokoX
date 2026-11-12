#include "poko/assets/provider.h"

namespace poko {

Result<LoadRequestId> NullAssetProvider::request_load(const AssetDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<LoadRequestId>::fail(ErrorCode::FailedPrecondition,
                                           "asset provider is not initialized");
    }
    if (!valid(desc)) {
        return Result<LoadRequestId>::fail(ErrorCode::InvalidArgument, "invalid asset descriptor");
    }
    // Register (or reuse) the asset slot in Unloaded state.
    HandleIndex index;
    if (!asset_free_.empty()) {
        index = asset_free_.back();
        asset_free_.pop_back();
        assets_[index] = Asset{};
    } else {
        index = static_cast<HandleIndex>(assets_.size());
        assets_.push_back(Asset{});
        asset_generations_.push_back(0);
    }
    auto& asset = assets_[index];
    asset.alive = true;
    asset.desc = desc;
    asset.state = AssetState::Loading;

    const LoadRequestId id = next_request_id_++;
    Request request;
    request.alive = true;
    request.asset = make_handle(index, asset_generations_[index]);
    if (requests_.size() < id) {
        requests_.resize(id);
    }
    requests_[id - 1] = request;
    asset.pending_request = id;
    return Result<LoadRequestId>::success(id);
}

Status NullAssetProvider::cancel_load(LoadRequestId id) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto* request = find_request(id);
    if (request == nullptr) {
        return Status::fail(ErrorCode::NotFound, "load request is not live");
    }
    if (request->done) {
        return Status::fail(ErrorCode::FailedPrecondition, "load request already finished");
    }
    auto* asset = find_asset(request->asset);
    if (asset != nullptr && asset->pending_request == id) {
        asset->state = AssetState::Unloaded;
        asset->pending_request = kInvalidLoadRequest;
    }
    request->alive = false;
    return Status::success();
}

bool NullAssetProvider::poll_load(LoadRequestId id, LoadResult& out) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto* request = find_request(id);
    if (request == nullptr || !request->done) {
        return false;
    }
    out = request->result;
    request->alive = false; // one-shot result
    return true;
}

} // namespace poko
