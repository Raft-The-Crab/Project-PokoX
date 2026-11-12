#include "poko/assets/filesystem_provider.h"

#include <cstddef>
#include <utility>

namespace poko {

FilesystemAssetProvider::FilesystemAssetProvider(api::IPlatformFileSystem& fs) noexcept : fs_(fs) {}

bool FilesystemAssetProvider::initialize() noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    initialized_ = true;
    return true;
}

bool FilesystemAssetProvider::handle_live(AssetHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < assets_.size() &&
           asset_generations_[handle.index] == handle.generation && assets_[handle.index].alive;
}

FilesystemAssetProvider::Asset* FilesystemAssetProvider::find_asset(AssetHandle handle) noexcept {
    if (!is_valid(handle) || static_cast<std::size_t>(handle.index) >= assets_.size()) {
        return nullptr;
    }
    return &assets_[handle.index];
}

const FilesystemAssetProvider::Asset*
FilesystemAssetProvider::find_asset(AssetHandle handle) const noexcept {
    if (!is_valid(handle) || static_cast<std::size_t>(handle.index) >= assets_.size()) {
        return nullptr;
    }
    return &assets_[handle.index];
}

FilesystemAssetProvider::Request*
FilesystemAssetProvider::find_request(LoadRequestId id) noexcept {
    if (id == kInvalidLoadRequest || id > requests_.size()) {
        return nullptr;
    }
    Request& request = requests_[static_cast<std::size_t>(id - 1)];
    return request.alive ? &request : nullptr;
}

HandleIndex FilesystemAssetProvider::allocate_asset() {
    if (!asset_free_.empty()) {
        const HandleIndex index = asset_free_.back();
        asset_free_.pop_back();
        assets_[index] = Asset{};
        return index;
    }
    const HandleIndex index = static_cast<HandleIndex>(assets_.size());
    assets_.push_back(Asset{});
    asset_generations_.push_back(0);
    return index;
}

AssetState FilesystemAssetProvider::asset_state(AssetHandle asset) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const Asset* found = find_asset(asset);
    if (found == nullptr || !handle_live(asset)) {
        return AssetState::Unknown;
    }
    return found->state;
}

Result<LoadResult> FilesystemAssetProvider::get_asset(AssetHandle asset) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const Asset* found = find_asset(asset);
    if (found == nullptr || !handle_live(asset)) {
        return Result<LoadResult>::fail(ErrorCode::NotFound, "asset handle is not live");
    }
    if (found->state != AssetState::Ready) {
        return Result<LoadResult>::fail(ErrorCode::FailedPrecondition, "asset is not ready");
    }
    return Result<LoadResult>::success(LoadResult{asset, AssetState::Ready, found->payload});
}

Status FilesystemAssetProvider::release_asset(AssetHandle asset) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(asset)) {
        return Status::fail(ErrorCode::NotFound, "asset handle is not live");
    }
    Asset& entry = assets_[asset.index];
    if (entry.pending_request != kInvalidLoadRequest) {
        if (Request* request = find_request(entry.pending_request)) {
            request->alive = false;
        }
    }
    entry.alive = false;
    entry.state = AssetState::Unloaded;
    ++asset_generations_[asset.index];
    asset_free_.push_back(asset.index);
    return Status::success();
}

std::uint32_t FilesystemAssetProvider::cache_size() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return static_cast<std::uint32_t>(cache_.size());
}

std::uint32_t FilesystemAssetProvider::failed_load_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return failed_loads_;
}

void FilesystemAssetProvider::evict(std::string_view logical_path) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    cache_.erase(std::string(logical_path));
}

} // namespace poko
