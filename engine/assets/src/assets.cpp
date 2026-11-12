#include "poko/assets/provider.h"

namespace poko {

bool NullAssetProvider::initialize() noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    if (initialized_) {
        return true;
    }
    initialized_ = true;
    return true;
}

bool NullAssetProvider::handle_live(AssetHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < assets_.size() &&
           asset_generations_[handle.index] == handle.generation && assets_[handle.index].alive;
}

NullAssetProvider::Asset* NullAssetProvider::find_asset(AssetHandle handle) noexcept {
    if (static_cast<std::size_t>(handle.index) >= assets_.size()) {
        return nullptr;
    }
    return &assets_[handle.index];
}

const NullAssetProvider::Asset* NullAssetProvider::find_asset(AssetHandle handle) const noexcept {
    if (static_cast<std::size_t>(handle.index) >= assets_.size()) {
        return nullptr;
    }
    return &assets_[handle.index];
}

NullAssetProvider::Request* NullAssetProvider::find_request(LoadRequestId id) noexcept {
    if (id == kInvalidLoadRequest || id > requests_.size()) {
        return nullptr;
    }
    auto& request = requests_[id - 1];
    return request.alive ? &request : nullptr;
}

AssetState NullAssetProvider::asset_state(AssetHandle asset) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const auto* found = find_asset(asset);
    if (found == nullptr || !handle_live(asset)) {
        return AssetState::Unknown;
    }
    return found->state;
}

Result<LoadResult> NullAssetProvider::get_asset(AssetHandle asset) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(asset)) {
        return Result<LoadResult>::fail(ErrorCode::NotFound, "asset handle is not live");
    }
    const auto& found = assets_[asset.index];
    if (found.state != AssetState::Ready) {
        return Result<LoadResult>::fail(ErrorCode::FailedPrecondition, "asset is not ready");
    }
    LoadResult result;
    result.asset = asset;
    result.state = AssetState::Ready;
    result.payload = found.payload;
    return Result<LoadResult>::success(result);
}

Status NullAssetProvider::release_asset(AssetHandle asset) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(asset)) {
        return Status::fail(ErrorCode::NotFound, "asset handle is not live");
    }
    auto& found = assets_[asset.index];
    if (found.pending_request != kInvalidLoadRequest) {
        if (auto* request = find_request(found.pending_request)) {
            request->alive = false;
        }
        found.pending_request = kInvalidLoadRequest;
    }
    found.alive = false;
    found.state = AssetState::Unloaded;
    ++asset_generations_[asset.index];
    asset_free_.push_back(asset.index);
    return Status::success();
}

void NullAssetProvider::update() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return;
    }
    // MOCK: every pending request completes on the next update().
    for (auto& asset : assets_) {
        if (!asset.alive || asset.state != AssetState::Loading ||
            asset.pending_request == kInvalidLoadRequest) {
            continue;
        }
        auto* request = find_request(asset.pending_request);
        if (request == nullptr) {
            asset.state = AssetState::Failed;
            asset.pending_request = kInvalidLoadRequest;
            continue;
        }
        asset.payload = synthesize_payload(asset.desc.kind);
        asset.state = AssetState::Ready;
        request->done = true;
        request->result.asset =
            make_handle(static_cast<HandleIndex>(&asset - assets_.data()),
                        asset_generations_[static_cast<HandleIndex>(&asset - assets_.data())]);
        request->result.state = AssetState::Ready;
        request->result.payload = asset.payload;
        asset.pending_request = kInvalidLoadRequest;
    }
}

} // namespace poko
