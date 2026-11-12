#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "poko/core/api_seams.h"

namespace poko {

// PRODUCTION in-process datagram protocol implementation (systemplan/10).
// Implements the real parts the MOCK loopback omits: a versioned PKXN frame
// header, per (destination, channel) sequence numbers, a connect handshake
// pumped by update(), reliable-ordered delivery that reorders out-of-order
// frames, deterministic loss for unreliable channels, and generation-checked
// endpoint lifetime.
//
// OS socket I/O is deliberately deferred: Asio is NOT VENDORED in
// third_party/DEPENDENCIES.md and may not be pinned speculatively (see the
// deferral recorded in systemplan/28). This backend owns the protocol; a
// socket backend will drive the same framing once pinned.
//
// Thread-safety contract: synchronised; any thread may call any method.
class ChannelNetworkTransport final : public api::INetworkTransport {
public:
    ChannelNetworkTransport() = default;

    Result<EndpointId> listen(std::string_view address, std::uint16_t port) override;
    Result<EndpointId> connect(std::string_view address, std::uint16_t port) override;
    Status close(EndpointId endpoint) override;
    TransportState state(EndpointId endpoint) const override;
    Result<PacketId> send(EndpointId to, ChannelId channel,
                          std::span<const std::byte> payload) override;
    std::uint32_t
    receive(const std::function<void(const PacketDesc&, std::span<const std::byte>)>& out) override;
    void update() override;

    // Backend-specific controls and introspection, outside the seam.
    void set_drop_every(std::uint32_t every) noexcept; // unreliable loss model
    void set_reorder(bool enabled) noexcept;           // deterministic reorder test hook
    std::uint32_t endpoint_count() const noexcept;
    std::uint32_t framed_count() const noexcept;
    std::uint32_t reordered_count() const noexcept;
    std::uint32_t dropped_count() const noexcept;

private:
    struct Endpoint {
        bool alive = false;
        TransportState state = TransportState::Disconnected;
        std::string address;
        std::uint16_t port = 0;
        bool listening = false;
    };
    struct Frame {
        EndpointId from{};
        EndpointId to{};
        ChannelId channel = ChannelReliableOrdered;
        std::uint32_t sequence = 0;
        std::uint64_t packet_id = 0;
        std::vector<std::byte> bytes;
    };
    struct Delivered {
        PacketDesc desc{};
        std::vector<std::byte> payload;
    };

    bool live(EndpointId id) const noexcept; // caller holds mutex_
    Endpoint* find(EndpointId id) noexcept;  // caller holds mutex_
    const Endpoint* find(EndpointId id) const noexcept;
    HandleIndex allocate_endpoint();         // caller holds mutex_
    EndpointId peer_for(const Endpoint& to) const; // caller holds mutex_
    void deliver(const Frame& frame);        // caller holds mutex_

    mutable std::mutex mutex_;
    std::vector<Endpoint> endpoints_;
    std::vector<Generation> generations_;
    std::vector<HandleIndex> free_;
    std::vector<Frame> queued_;
    std::vector<Delivered> delivered_;
    std::unordered_map<std::uint64_t, std::uint32_t> sequences_;
    PacketId next_packet_id_ = 1;
    std::uint32_t drop_every_ = 0;
    bool reorder_ = false;
    std::uint32_t framed_ = 0;
    std::uint32_t reordered_ = 0;
    std::uint32_t dropped_ = 0;
};

} // namespace poko
