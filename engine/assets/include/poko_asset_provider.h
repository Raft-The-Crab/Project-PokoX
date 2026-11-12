#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "poko_api_seams.h"

namespace poko {

// MOCK — test-only fake asset provider (systemplan/05 skeleton). It records
// load requests, synthesizes deterministic payloads per AssetKind on
// update(), and tracks asset lifetime. It performs NO file I/O, decoding,
// caching or eviction policy — those belong to the real backend.
class NullAssetProvider final : public api::IAssetProvider {
public:
    NullAssetProvider() = default;

    bool initialize() noexcept override;

    Result<LoadRequestId> request_load(const AssetDesc& desc) override;
    Status cancel_load(LoadRequestId id) override;
    bool poll_load(LoadRequestId id, LoadResult& out) override;
    AssetState asset_state(AssetHandle asset) const override;
    Result<LoadResult> get_asset(AssetHandle asset) const override;
    Status release_asset(AssetHandle asset) override;
    void update() override;

private:
    struct Asset {
        bool alive = false;
        AssetDesc desc;
        AssetState state = AssetState::Unloaded;
        AssetPayload payload{};
        LoadRequestId pending_request = kInvalidLoadRequest;
    };

    struct Request {
        bool alive = false;
        AssetHandle asset;
        bool done = false;
        LoadResult result{};
    };

    bool handle_live(AssetHandle handle) const noexcept; // caller holds mutex_
    Asset* find_asset(AssetHandle handle) noexcept;
    const Asset* find_asset(AssetHandle handle) const noexcept;
    Request* find_request(LoadRequestId id) noexcept;

    // Synthesizes the deterministic MOCK payload for a kind.
    static AssetPayload synthesize_payload(AssetKind kind) noexcept;

    mutable std::mutex mutex_;
    bool initialized_ = false;
    std::vector<Asset> assets_;
    std::vector<Generation> asset_generations_;
    std::vector<HandleIndex> asset_free_;
    std::vector<Request> requests_;
    LoadRequestId next_request_id_ = 1;
};

} // namespace poko
