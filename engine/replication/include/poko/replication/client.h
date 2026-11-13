#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <vector>

#include "poko/replication/server.h"

namespace poko::replication {

enum class ClientState : std::uint8_t { Idle, Handshaking, Connected, Rejected };

struct ClientConfig {
    std::uint32_t history_ticks = 64;
    // Interpolation renders this many ticks behind the newest snapshot.
    float interpolation_delay_ticks = 2.0f;
};

struct ClientStats {
    std::uint64_t snapshots_applied = 0;
    std::uint64_t malformed_dropped = 0;
    std::uint64_t stale_dropped = 0;
    std::uint64_t missing_baseline = 0;
};

// Receives authoritative snapshots, acknowledges them so the server can delta
// against them, and exposes an interpolated view of the world.
class ReplicationClient final : public PacketSink {
public:
    ReplicationClient(INetworkTransport& transport, EndpointId local, EndpointId server,
                      ClientId id, ClientConfig config = {});

    Status connect();
    ClientState state() const noexcept { return state_; }
    ClientId id() const noexcept { return id_; }

    // Sends a movement/button command; the server validates and may clamp it.
    Result<std::uint32_t> send_input(float move_x, float move_z, float yaw, std::uint16_t buttons);

    std::uint32_t latest_tick() const noexcept { return latest_tick_; }
    // Latest authoritative state, without interpolation.
    const std::map<NetEntityId, EntityState>& latest() const noexcept;
    // State interpolated at `render_tick` (fractional ticks, already including
    // any delay the caller wants). Clamps to the buffered range.
    std::vector<EntityState> sample(double render_tick) const;
    // Convenience: sample() at latest_tick() - interpolation_delay_ticks.
    std::vector<EntityState> sample_delayed() const;
    const ClientStats& stats() const noexcept { return stats_; }

    bool owns(EndpointId endpoint) const noexcept override;
    void on_packet(const PacketDesc& desc, std::span<const std::byte> payload) override;

private:
    void apply(const Snapshot& snapshot);

    INetworkTransport& transport_;
    EndpointId local_;
    EndpointId server_;
    ClientId id_;
    ClientConfig config_;
    ClientState state_ = ClientState::Idle;
    std::uint32_t input_sequence_ = 0;
    std::uint32_t latest_tick_ = 0;
    std::deque<std::pair<std::uint32_t, std::map<NetEntityId, EntityState>>> history_;
    ClientStats stats_;
};

} // namespace poko::replication
