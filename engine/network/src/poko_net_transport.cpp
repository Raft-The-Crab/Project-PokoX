#include "poko_net_transport.h"

#include <cstring>

namespace poko {

bool NullNetworkTransport::live(EndpointId id) const noexcept {
    return is_valid(id) && static_cast<std::size_t>(id.index) < endpoints_.size() &&
           generations_[id.index] == id.generation && endpoints_[id.index].alive;
}

NullNetworkTransport::Endpoint* NullNetworkTransport::find(EndpointId id) noexcept {
    if (static_cast<std::size_t>(id.index) >= endpoints_.size()) {
        return nullptr;
    }
    return &endpoints_[id.index];
}

const NullNetworkTransport::Endpoint* NullNetworkTransport::find(EndpointId id) const noexcept {
    if (static_cast<std::size_t>(id.index) >= endpoints_.size()) {
        return nullptr;
    }
    return &endpoints_[id.index];
}

Result<EndpointId> NullNetworkTransport::listen(std::string_view address, std::uint16_t port) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid_endpoint_text(address) || port == 0) {
        return Result<EndpointId>::fail(ErrorCode::InvalidArgument,
                                        "invalid listen address or port");
    }
    // Reuse a dead listener slot if present, else append.
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        endpoints_[index] = Endpoint{};
    } else {
        index = static_cast<HandleIndex>(endpoints_.size());
        endpoints_.push_back(Endpoint{});
        generations_.push_back(0);
    }
    auto& ep = endpoints_[index];
    ep.alive = true;
    ep.state = TransportState::Connected; // MOCK: instant "connected"
    ep.address = std::string(address);
    ep.port = port;
    ep.listening = true;
    const EndpointId id = make_handle(index, generations_[index]);
    loopback_peer_ = id; // loopback connects target the (first) listener
    return Result<EndpointId>::success(id);
}

Result<EndpointId> NullNetworkTransport::connect(std::string_view address, std::uint16_t port) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid_endpoint_text(address) || port == 0) {
        return Result<EndpointId>::fail(ErrorCode::InvalidArgument,
                                        "invalid connect address or port");
    }
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        endpoints_[index] = Endpoint{};
    } else {
        index = static_cast<HandleIndex>(endpoints_.size());
        endpoints_.push_back(Endpoint{});
        generations_.push_back(0);
    }
    auto& ep = endpoints_[index];
    ep.alive = true;
    ep.state = is_valid(loopback_peer_) ? TransportState::Connected : TransportState::Connecting;
    ep.address = std::string(address);
    ep.port = port;
    return Result<EndpointId>::success(make_handle(index, generations_[index]));
}

Status NullNetworkTransport::close(EndpointId endpoint) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(endpoint)) {
        return Status::fail(ErrorCode::NotFound, "endpoint is not live");
    }
    endpoints_[endpoint.index].alive = false;
    endpoints_[endpoint.index].state = TransportState::Disconnected;
    ++generations_[endpoint.index];
    free_.push_back(endpoint.index);
    if (same_handle(loopback_peer_, endpoint)) {
        loopback_peer_ = {};
    }
    return Status::success();
}

TransportState NullNetworkTransport::state(EndpointId endpoint) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const auto* ep = live(endpoint) ? find(endpoint) : nullptr;
    return ep != nullptr ? ep->state : TransportState::Disconnected;
}

Result<PacketId> NullNetworkTransport::send(EndpointId to, ChannelId channel,
                                            std::span<const std::byte> payload) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(to) || payload.empty()) {
        return Result<PacketId>::fail(ErrorCode::InvalidArgument,
                                      "invalid endpoint or empty payload");
    }
    // MOCK loopback: if the target is our listener, queue to the connect-side
    // inbound of every other connected endpoint (simplest deterministic rule:
    // delivers to the first non-listener connected endpoint).
    const PacketId id = next_packet_id_++;
    if (live(loopback_peer_)) {
        // Deliver to all connected non-listener endpoints.
        for (std::size_t i = 0; i < endpoints_.size(); ++i) {
            auto& ep = endpoints_[i];
            if (!ep.alive || ep.listening || ep.state != TransportState::Connected) {
                continue;
            }
            const EndpointId from = make_handle(static_cast<HandleIndex>(i), generations_[i]);
            inbound_.push_back(Inbound{PacketDesc{from, to, channel, 0},
                                       std::vector<std::byte>(payload.begin(), payload.end())});
        }
    }
    return Result<PacketId>::success(id);
}

std::uint32_t NullNetworkTransport::receive(
    const std::function<void(const PacketDesc&, std::span<const std::byte>)>& out) {
    std::lock_guard<std::mutex> guard(mutex_);
    const std::uint32_t count = static_cast<std::uint32_t>(inbound_.size());
    for (const auto& packet : inbound_) {
        out(packet.desc, std::span<const std::byte>(packet.payload.data(), packet.payload.size()));
    }
    inbound_.clear();
    return count;
}

void NullNetworkTransport::update() {
    // MOCK: no background work; sends are queued eagerly.
}

} // namespace poko
