#include "poko_asset_provider.h"

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

AssetPayload NullAssetProvider::synthesize_payload(AssetKind kind) noexcept {
    AssetPayload payload;
    switch (kind) {
    case AssetKind::Mesh:
        payload.mesh.vertex_count = 3;
        payload.mesh.index_count = 3;
        payload.mesh.submesh_count = 1;
        payload.mesh.index_type = IndexType::UInt32;
        payload.mesh.vertex_layout.attribute_count = 1;
        payload.mesh.vertex_layout.attributes[0] = {0, 3, 12, 0};
        payload.byte_size = 36;
        break;
    case AssetKind::Texture:
        payload.texture.width = 4;
        payload.texture.height = 4;
        payload.texture.format = PixelFormat::RGBA8;
        payload.texture.mip_levels = 1;
        payload.byte_size = 64;
        break;
    case AssetKind::Audio:
        payload.audio.sample_rate = 44100;
        payload.audio.channels = 2;
        payload.audio.bits_per_sample = 16;
        payload.audio.duration_seconds = 1.0f;
        payload.byte_size = 176400;
        break;
    case AssetKind::Material:
    case AssetKind::Shader:
    case AssetKind::Font:
    case AssetKind::Animation:
    case AssetKind::Scene:
    case AssetKind::Skeleton:
    case AssetKind::UISprite:
    case AssetKind::Raw:
        payload.byte_size = 256;
        break;
    case AssetKind::Unknown:
        break;
    }
    return payload;
}

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
