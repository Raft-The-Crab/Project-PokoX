#include "poko/replication/server.h"

#include <algorithm>
#include <cmath>

namespace poko::replication {
namespace {

bool finite_state(const EntityState& s) noexcept {
    return std::isfinite(s.position.x) && std::isfinite(s.position.y) &&
           std::isfinite(s.position.z) && std::isfinite(s.yaw) && std::isfinite(s.velocity.x) &&
           std::isfinite(s.velocity.y) && std::isfinite(s.velocity.z);
}

float distance_sq(const Vec3& a, const Vec3& b) noexcept {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

} // namespace

std::uint32_t pump(INetworkTransport& transport, std::span<PacketSink* const> sinks) {
    transport.update();
    // Drain first, dispatch after: sinks send replies, and a transport may hold
    // its lock while invoking the receive callback.
    struct Inbound {
        PacketDesc desc;
        std::vector<std::byte> payload;
    };
    std::vector<Inbound> inbound;
    const auto count = transport.receive([&](const PacketDesc& desc, std::span<const std::byte> payload) {
        inbound.push_back({desc, std::vector<std::byte>(payload.begin(), payload.end())});
    });
    for (const auto& packet : inbound) {
        for (auto* sink : sinks) {
            if (sink != nullptr && sink->owns(packet.desc.to)) {
                sink->on_packet(packet.desc, packet.payload);
                break;
            }
        }
    }
    return count;
}

ReplicationServer::ReplicationServer(INetworkTransport& transport, EndpointId listener,
                                     ServerConfig config)
    : transport_(transport), listener_(listener), config_(config) {}

Result<ClientId> ReplicationServer::add_client(EndpointId client_endpoint) {
    if (!is_valid(client_endpoint)) {
        return Result<ClientId>::fail(ErrorCode::InvalidArgument, "invalid client endpoint");
    }
    for (const auto& [id, client] : clients_) {
        if (client.endpoint.index == client_endpoint.index &&
            client.endpoint.generation == client_endpoint.generation) {
            return Result<ClientId>::fail(ErrorCode::AlreadyExists, "endpoint already registered");
        }
    }
    const ClientId id = next_client_++;
    Client client;
    client.endpoint = client_endpoint;
    client.last_heard = tick_;
    clients_.emplace(id, std::move(client));
    return Result<ClientId>::success(id);
}

Status ReplicationServer::remove_client(ClientId client) {
    if (clients_.erase(client) == 0) {
        return Status::fail(ErrorCode::NotFound, "unknown client");
    }
    return Status::success();
}

bool ReplicationServer::has_client(ClientId client) const noexcept {
    return clients_.contains(client);
}

Status ReplicationServer::set_view(ClientId client, const Vec3& position) {
    auto* c = client_for(client);
    if (c == nullptr) {
        return Status::fail(ErrorCode::NotFound, "unknown client");
    }
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
        return Status::fail(ErrorCode::InvalidArgument, "non-finite view position");
    }
    c->view = position;
    return Status::success();
}

Result<NetEntityId> ReplicationServer::spawn(const EntityState& state) {
    if (!finite_state(state)) {
        return Result<NetEntityId>::fail(ErrorCode::InvalidArgument, "non-finite entity state");
    }
    EntityState stored = state;
    stored.id = next_entity_++;
    entities_.emplace(stored.id, stored);
    return Result<NetEntityId>::success(stored.id);
}

Status ReplicationServer::despawn(NetEntityId id) {
    if (entities_.erase(id) == 0) {
        return Status::fail(ErrorCode::NotFound, "unknown entity");
    }
    return Status::success();
}

Status ReplicationServer::set_state(const EntityState& state) {
    auto it = entities_.find(state.id);
    if (it == entities_.end()) {
        return Status::fail(ErrorCode::NotFound, "unknown entity");
    }
    if (!finite_state(state)) {
        return Status::fail(ErrorCode::InvalidArgument, "non-finite entity state");
    }
    it->second = state;
    return Status::success();
}

const EntityState* ReplicationServer::find(NetEntityId id) const noexcept {
    const auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}

ReplicationServer::Client* ReplicationServer::client_for(ClientId id) noexcept {
    const auto it = clients_.find(id);
    return it == clients_.end() ? nullptr : &it->second;
}

bool ReplicationServer::owns(EndpointId endpoint) const noexcept {
    return endpoint.index == listener_.index && endpoint.generation == listener_.generation;
}

void ReplicationServer::on_packet(const PacketDesc&, std::span<const std::byte> payload) {
    const auto type = peek_type(payload);
    if (!type) {
        ++stats_.malformed_dropped;
        return;
    }
    switch (*type) {
    case MessageType::Hello:
        if (const auto m = decode_hello(payload)) handle_hello(*m); else ++stats_.malformed_dropped;
        break;
    case MessageType::Ack:
        if (const auto m = decode_ack(payload)) handle_ack(*m); else ++stats_.malformed_dropped;
        break;
    case MessageType::Input:
        if (const auto m = decode_input(payload)) handle_input(*m); else ++stats_.malformed_dropped;
        break;
    default: // clients never legitimately send server-to-client messages
        ++stats_.malformed_dropped;
        break;
    }
}

void ReplicationServer::handle_hello(const Hello& hello) {
    auto* client = client_for(hello.client);
    if (client == nullptr) {
        ++stats_.unknown_client_dropped;
        return;
    }
    client->last_heard = tick_;
    HelloReply reply;
    if (hello.version != kProtocolVersion) {
        reply.status = HelloStatus::VersionMismatch;
        ++stats_.version_mismatches;
    } else {
        client->handshaken = true;
    }
    const auto bytes = encode(reply);
    (void)transport_.send(client->endpoint, ChannelReliableOrdered, bytes);
}

void ReplicationServer::handle_ack(const Ack& ack) {
    auto* client = client_for(ack.client);
    if (client == nullptr || !client->handshaken) {
        ++stats_.unknown_client_dropped;
        return;
    }
    client->last_heard = tick_;
    // Only ticks actually sent to this client can be acknowledged.
    const auto known = std::any_of(client->history.begin(), client->history.end(),
                                   [&](const auto& entry) { return entry.first == ack.tick; });
    if (!known || ack.tick <= client->acked_tick) {
        return;
    }
    client->acked_tick = ack.tick;
    while (!client->history.empty() && client->history.front().first < ack.tick) {
        client->history.pop_front();
    }
}

void ReplicationServer::handle_input(const InputCommand& input) {
    auto* client = client_for(input.client);
    if (client == nullptr || !client->handshaken) {
        ++stats_.unknown_client_dropped;
        return;
    }
    client->last_heard = tick_;
    if (input.sequence <= client->last_input) { // replayed or reordered
        ++stats_.inputs_rejected;
        return;
    }
    client->last_input = input.sequence;
    InputCommand clean = input;
    const float mag = std::sqrt(input.move_x * input.move_x + input.move_z * input.move_z);
    if (mag > config_.max_input_magnitude) {
        const float scale = config_.max_input_magnitude / mag;
        clean.move_x *= scale;
        clean.move_z *= scale;
    }
    ++stats_.inputs_accepted;
    if (input_handler_) {
        input_handler_(input.client, clean);
    }
}

void ReplicationServer::tick() {
    ++tick_;
    std::vector<ClientId> timed_out;
    for (const auto& [id, client] : clients_) {
        if (tick_ - client.last_heard > config_.timeout_ticks) {
            timed_out.push_back(id);
        }
    }
    for (const auto id : timed_out) {
        clients_.erase(id);
        ++stats_.timeouts;
    }
    for (auto& [id, client] : clients_) {
        if (client.handshaken) {
            send_snapshot(id, client);
        }
    }
}

void ReplicationServer::send_snapshot(ClientId, Client& client) {
    // Visible set: within the interest radius, nearest first, capped.
    const float radius_sq = config_.interest_radius * config_.interest_radius;
    std::vector<const EntityState*> visible;
    for (const auto& [id, state] : entities_) {
        if (distance_sq(state.position, client.view) <= radius_sq) {
            visible.push_back(&state);
        }
    }
    std::sort(visible.begin(), visible.end(), [&](const EntityState* a, const EntityState* b) {
        const float da = distance_sq(a->position, client.view);
        const float db = distance_sq(b->position, client.view);
        return da != db ? da < db : a->id < b->id;
    });
    const auto cap = std::min<std::size_t>(config_.max_entities_per_snapshot, kMaxSnapshotEntities);
    if (visible.size() > cap) {
        visible.resize(cap);
    }

    const std::map<NetEntityId, EntityState>* baseline = nullptr;
    for (const auto& [t, states] : client.history) {
        if (t == client.acked_tick && t != 0) baseline = &states;
    }

    Snapshot snap;
    snap.tick = tick_;
    snap.baseline_tick = baseline != nullptr ? client.acked_tick : 0;
    std::map<NetEntityId, EntityState> sent;
    for (const auto* state : visible) {
        sent.emplace(state->id, *state);
        const EntityState* base = nullptr;
        if (baseline != nullptr) {
            const auto it = baseline->find(state->id);
            if (it != baseline->end()) base = &it->second;
        }
        const auto mask = diff_mask(*state, base);
        if (baseline != nullptr && base != nullptr && mask == 0) {
            continue;
        }
        // A new entity against a delta baseline still needs every field.
        snap.entities.push_back(EntityDelta{*state, base == nullptr ? std::uint8_t{kFieldAll} : mask});
    }
    if (baseline != nullptr) {
        for (const auto& [id, state] : *baseline) {
            if (!sent.contains(id)) snap.removed.push_back(id);
        }
    }

    const auto bytes = encode(snap);
    if (transport_.send(client.endpoint, ChannelUnreliable, bytes).ok()) {
        ++stats_.snapshots_sent;
        stats_.bytes_sent += bytes.size();
    }
    client.history.emplace_back(tick_, std::move(sent));
    while (client.history.size() > config_.history_ticks) {
        client.history.pop_front();
    }
}

} // namespace poko::replication
