#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "poko/core/api_seams.h"

namespace poko {

// PRODUCTION filesystem-backed asset provider (systemplan/05). Reads real
// bytes through an injected IPlatformFileSystem, decodes the simple PKX1
// container (see src/fs_detail.h), and caches decoded payloads by logical
// path so repeat loads are served without I/O. Loads are queued by
// request_load() and completed in update(), matching the seam contract.
//
// The filesystem is never owned here; paths stay package-relative and are
// resolved by the injected seam (LocalFileSystem rejects traversal).
// Thread-safety contract: synchronised; any thread may call any method.
class FilesystemAssetProvider final : public api::IAssetProvider {
public:
    explicit FilesystemAssetProvider(api::IPlatformFileSystem& fs) noexcept;

    bool initialize() noexcept override;
    Result<LoadRequestId> request_load(const AssetDesc& desc) override;
    Status cancel_load(LoadRequestId id) override;
    bool poll_load(LoadRequestId id, LoadResult& out) override;
    AssetState asset_state(AssetHandle asset) const override;
    Result<LoadResult> get_asset(AssetHandle asset) const override;
    Status release_asset(AssetHandle asset) override;
    void update() override;

    // Backend-specific introspection, outside the seam.
    std::uint32_t cache_size() const noexcept;
    std::uint32_t failed_load_count() const noexcept;
    void evict(std::string_view logical_path) noexcept;

private:
    struct Asset {
        bool alive = false;
        AssetDesc desc{};
        std::string logical_path;
        AssetState state = AssetState::Unloaded;
        AssetPayload payload{};
        LoadRequestId pending_request = kInvalidLoadRequest;
    };
    struct Request {
        bool alive = false;
        AssetHandle asset{};
        bool done = false;
        LoadResult result{};
        std::string logical_path;
    };

    bool handle_live(AssetHandle handle) const noexcept; // caller holds mutex_
    Asset* find_asset(AssetHandle handle) noexcept;      // caller holds mutex_
    const Asset* find_asset(AssetHandle handle) const noexcept;
    Request* find_request(LoadRequestId id) noexcept;    // caller holds mutex_
    HandleIndex allocate_asset();                        // caller holds mutex_
    void finish_request(Request& request, Asset& asset, bool succeeded,
                        const AssetPayload& payload);      // caller holds mutex_

    api::IPlatformFileSystem& fs_;
    mutable std::mutex mutex_;
    bool initialized_ = false;
    std::vector<Asset> assets_;
    std::vector<Generation> asset_generations_;
    std::vector<HandleIndex> asset_free_;
    std::vector<Request> requests_;
    std::unordered_map<std::string, AssetPayload> cache_;
    std::uint32_t failed_loads_ = 0;
    LoadRequestId next_request_id_ = 1;
};

} // namespace poko
