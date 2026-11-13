#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

#include "poko/core/api_seams.h"
#include "poko/replication/protocol.h"

namespace poko::replication {

using api::INetworkTransport;

// Something that consumes inbound transport packets addressed to one endpoint.
// The transport has a single receive queue, so a process drains it once and
// routes each packet to the sink that owns the destination endpoint.
class PacketSink {
public:
    virtual ~PacketSink() = default;
    virtual bool owns(EndpointId endpoint) const noexcept = 0;
    virtual void on_packet(const PacketDesc& desc, std::span<const std::byte> payload) = 0;
};

// Pumps the transport and routes every received packet to the sink owning its
// destination. Packets nobody owns are dropped. Returns packets received.
std::uint32_t pump(INetworkTransport& transport, std::span<PacketSink* const> sinks);

struct ServerConfig {
    float interest_radius = 100.0f;
    std::uint32_t max_entities_per_snapshot = 256;
    std::uint32_t history_ticks = 64;     // sent snapshots retained per client for delta baselines
    std::uint32_t timeout_ticks = 300;    // silent clients are dropped after this many ticks
    float max_input_magnitude = 1.0f;     // authoritative clamp on client move vectors
};

struct ServerStats {
    std::uint64_t snapshots_sent = 0;
    std::uint64_t bytes_sent = 0;
    std::uint64_t malformed_dropped = 0;
    std::uint64_t unknown_client_dropped = 0;
    std::uint64_t inputs_accepted = 0;
    std::uint64_t inputs_rejected = 0;
    std::uint64_t version_mismatches = 0;
    std::uint64_t timeouts = 0;
};

// Server-authoritative replication. The server owns entity state; clients only
// send inputs, which are validated and handed to the game via the input
// handler. Trust boundary: a ClientId is only as trustworthy as the transport
// binding between endpoint and client (add_client); a socket backend must
// establish that binding from the authenticated connection.
class ReplicationServer final : public PacketSink {
public:
    using InputHandler = std::function<void(ClientId, const InputCommand&)>;

    ReplicationServer(INetworkTransport& transport, EndpointId listener, ServerConfig config = {});

    Result<ClientId> add_client(EndpointId client_endpoint);
    Status remove_client(ClientId client);
    bool has_client(ClientId client) const noexcept;
    std::uint32_t client_count() const noexcept { return static_cast<std::uint32_t>(clients_.size()); }
    Status set_view(ClientId client, const Vec3& position);

    Result<NetEntityId> spawn(const EntityState& state);
    Status despawn(NetEntityId id);
    Status set_state(const EntityState& state);
    const EntityState* find(NetEntityId id) const noexcept;
    std::uint32_t entity_count() const noexcept { return static_cast<std::uint32_t>(entities_.size()); }

    void set_input_handler(InputHandler handler) { input_handler_ = std::move(handler); }

    // Advances the replication tick: drops timed-out clients and sends each
    // handshaken client one delta snapshot.
    void tick();
    std::uint32_t current_tick() const noexcept { return tick_; }
    const ServerStats& stats() const noexcept { return stats_; }

    bool owns(EndpointId endpoint) const noexcept override;
    void on_packet(const PacketDesc& desc, std::span<const std::byte> payload) override;

private:
    struct Client {
        EndpointId endpoint{};
        bool handshaken = false;
        Vec3 view{};
        std::uint32_t last_input = 0;
        std::uint32_t last_heard = 0;
        std::uint32_t acked_tick = 0;
        // Sent snapshots by tick: the visible state at that tick.
        std::deque<std::pair<std::uint32_t, std::map<NetEntityId, EntityState>>> history;
    };

    void handle_hello(const Hello& hello);
    void handle_ack(const Ack& ack);
    void handle_input(const InputCommand& input);
    void send_snapshot(ClientId id, Client& client);
    Client* client_for(ClientId id) noexcept;

    INetworkTransport& transport_;
    EndpointId listener_;
    ServerConfig config_;
    std::map<NetEntityId, EntityState> entities_;
    std::map<ClientId, Client> clients_;
    InputHandler input_handler_;
    NetEntityId next_entity_ = 1;
    ClientId next_client_ = 1;
    std::uint32_t tick_ = 0;
    ServerStats stats_;
};

} // namespace poko::replication
