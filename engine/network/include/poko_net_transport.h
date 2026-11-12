#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "poko_api_seams.h"

namespace poko {

// MOCK — test-only fake network transport (systemplan/10 skeleton). It
// records endpoints/packets and delivers sends to loopback peers registered
// on the same instance. It opens NO sockets — the production transport
// (planned: Asio) owns real I/O.
class NullNetworkTransport final : public api::INetworkTransport {
public:
    NullNetworkTransport() = default;

    Result<EndpointId> listen(std::string_view address, std::uint16_t port) override;
    Result<EndpointId> connect(std::string_view address, std::uint16_t port) override;
    Status close(EndpointId endpoint) override;
    TransportState state(EndpointId endpoint) const override;
    Result<PacketId> send(EndpointId to, ChannelId channel,
                          std::span<const std::byte> payload) override;
    std::uint32_t
    receive(const std::function<void(const PacketDesc&, std::span<const std::byte>)>& out) override;
    void update() override;

private:
    struct Endpoint {
        bool alive = false;
        TransportState state = TransportState::Disconnected;
        std::string address;
        std::uint16_t port = 0;
        bool listening = false;
    };

    struct Inbound {
        PacketDesc desc;
        std::vector<std::byte> payload;
    };

    bool live(EndpointId id) const noexcept; // caller holds mutex_
    Endpoint* find(EndpointId id) noexcept;
    const Endpoint* find(EndpointId id) const noexcept;

    mutable std::mutex mutex_;
    std::vector<Endpoint> endpoints_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    std::vector<Inbound> inbound_;
    PacketId next_packet_id_ = 1;
    EndpointId loopback_peer_{}; // set when a listener exists; connects bind to it
};

} // namespace poko
