#include "poko/replication/client.h"

#include <algorithm>
#include <cmath>

namespace poko::replication {
namespace {

float lerp(float a, float b, float t) noexcept {
    return a + (b - a) * t;
}

float lerp_angle(float a, float b, float t) noexcept {
    float delta = std::fmod(b - a, math::kTau);
    if (delta > math::kPi) delta -= math::kTau;
    if (delta < -math::kPi) delta += math::kTau;
    return a + delta * t;
}

EntityState blend(const EntityState& a, const EntityState& b, float t) noexcept {
    EntityState out = b;
    out.position = Vec3{lerp(a.position.x, b.position.x, t), lerp(a.position.y, b.position.y, t),
                        lerp(a.position.z, b.position.z, t)};
    out.velocity = Vec3{lerp(a.velocity.x, b.velocity.x, t), lerp(a.velocity.y, b.velocity.y, t),
                        lerp(a.velocity.z, b.velocity.z, t)};
    out.yaw = lerp_angle(a.yaw, b.yaw, t);
    return out;
}

} // namespace

ReplicationClient::ReplicationClient(INetworkTransport& transport, EndpointId local,
                                     EndpointId server, ClientId id, ClientConfig config)
    : transport_(transport), local_(local), server_(server), id_(id), config_(config) {}

Status ReplicationClient::connect() {
    if (state_ != ClientState::Idle) {
        return Status::fail(ErrorCode::FailedPrecondition, "already connecting");
    }
    const auto sent = transport_.send(server_, ChannelReliableOrdered, encode(Hello{kProtocolVersion, id_}));
    if (!sent.ok()) {
        return Status::fail(sent.error().code, sent.error().message);
    }
    state_ = ClientState::Handshaking;
    return Status::success();
}

Result<std::uint32_t> ReplicationClient::send_input(float move_x, float move_z, float yaw,
                                                    std::uint16_t buttons) {
    if (state_ != ClientState::Connected) {
        return Result<std::uint32_t>::fail(ErrorCode::FailedPrecondition, "not connected");
    }
    if (!std::isfinite(move_x) || !std::isfinite(move_z) || !std::isfinite(yaw)) {
        return Result<std::uint32_t>::fail(ErrorCode::InvalidArgument, "non-finite input");
    }
    InputCommand cmd{id_, input_sequence_ + 1, move_x, move_z, yaw, buttons};
    const auto sent = transport_.send(server_, ChannelUnreliable, encode(cmd));
    if (!sent.ok()) {
        return Result<std::uint32_t>::fail(sent.error().code, sent.error().message);
    }
    return Result<std::uint32_t>::success(++input_sequence_);
}

const std::map<NetEntityId, EntityState>& ReplicationClient::latest() const noexcept {
    static const std::map<NetEntityId, EntityState> kEmpty;
    return history_.empty() ? kEmpty : history_.back().second;
}

bool ReplicationClient::owns(EndpointId endpoint) const noexcept {
    return endpoint.index == local_.index && endpoint.generation == local_.generation;
}

void ReplicationClient::on_packet(const PacketDesc&, std::span<const std::byte> payload) {
    const auto type = peek_type(payload);
    if (type == MessageType::HelloReply) {
        const auto reply = decode_hello_reply(payload);
        if (!reply) {
            ++stats_.malformed_dropped;
            return;
        }
        if (state_ == ClientState::Handshaking) {
            state_ = reply->status == HelloStatus::Accepted ? ClientState::Connected
                                                            : ClientState::Rejected;
        }
    } else if (type == MessageType::Snapshot) {
        const auto snap = decode_snapshot(payload);
        if (!snap) {
            ++stats_.malformed_dropped;
            return;
        }
        if (state_ == ClientState::Connected) {
            apply(*snap);
        }
    } else {
        ++stats_.malformed_dropped;
    }
}

void ReplicationClient::apply(const Snapshot& snapshot) {
    if (snapshot.tick <= latest_tick_) {
        ++stats_.stale_dropped;
        return;
    }
    std::map<NetEntityId, EntityState> world;
    if (snapshot.baseline_tick != 0) {
        const auto it = std::find_if(history_.begin(), history_.end(), [&](const auto& entry) {
            return entry.first == snapshot.baseline_tick;
        });
        if (it == history_.end()) {
            ++stats_.missing_baseline; // server will fall back to full once acks stall
            return;
        }
        world = it->second;
        for (const auto id : snapshot.removed) world.erase(id);
    }
    for (const auto& delta : snapshot.entities) {
        const auto it = world.find(delta.state.id);
        if (it == world.end()) {
            if (delta.mask != kFieldAll) { // delta for an entity we never had
                ++stats_.malformed_dropped;
                return;
            }
            world.emplace(delta.state.id, delta.state);
        } else {
            it->second = apply_delta(it->second, delta);
        }
    }
    latest_tick_ = snapshot.tick;
    history_.emplace_back(snapshot.tick, std::move(world));
    while (history_.size() > config_.history_ticks) {
        history_.pop_front();
    }
    ++stats_.snapshots_applied;
    (void)transport_.send(server_, ChannelUnreliable, encode(Ack{id_, snapshot.tick}));
}

std::vector<EntityState> ReplicationClient::sample(double render_tick) const {
    std::vector<EntityState> out;
    if (history_.empty()) {
        return out;
    }
    const auto& first = history_.front();
    const auto& last = history_.back();
    if (render_tick <= first.first) {
        for (const auto& [id, s] : first.second) out.push_back(s);
        return out;
    }
    if (render_tick >= last.first) {
        for (const auto& [id, s] : last.second) out.push_back(s);
        return out;
    }
    auto upper = std::find_if(history_.begin(), history_.end(), [&](const auto& entry) {
        return static_cast<double>(entry.first) >= render_tick;
    });
    const auto& b = *upper;
    const auto& a = *(upper - 1);
    const float t = static_cast<float>((render_tick - a.first) / double(b.first - a.first));
    for (const auto& [id, to] : b.second) {
        const auto from = a.second.find(id);
        out.push_back(from == a.second.end() ? to : blend(from->second, to, t));
    }
    return out;
}

std::vector<EntityState> ReplicationClient::sample_delayed() const {
    return sample(static_cast<double>(latest_tick_) - config_.interpolation_delay_ticks);
}

} // namespace poko::replication
