#include "poko/network/transport.h"

namespace poko {

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
