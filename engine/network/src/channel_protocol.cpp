#include "poko/network/channel_transport.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "channel_detail.h"

namespace poko {

void ChannelNetworkTransport::deliver(const Frame& frame) {
    net::detail::PkxFrameHeader header;
    std::span<const std::byte> payload;
    if (!net::detail::parse(frame.bytes, header, payload)) {
        return;
    }
    PacketDesc desc;
    desc.from = frame.from;
    desc.to = frame.to;
    desc.channel = frame.channel;
    desc.flags = header.flags;
    delivered_.push_back(
        Delivered{desc, std::vector<std::byte>(payload.begin(), payload.end())});
}

Result<PacketId> ChannelNetworkTransport::send(EndpointId to, ChannelId channel,
                                               std::span<const std::byte> payload) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(to) || payload.empty()) {
        return Result<PacketId>::fail(ErrorCode::InvalidArgument,
                                      "invalid endpoint or empty payload");
    }
    const std::uint64_t key = (static_cast<std::uint64_t>(to.index) << 8) |
                              static_cast<std::uint8_t>(channel);
    const std::uint32_t sequence = ++sequences_[key];
    const PacketId packet_id = next_packet_id_++;

    net::detail::PkxFrameHeader header;
    header.channel = static_cast<std::uint8_t>(channel);
    header.flags = net::detail::reliable(channel) ? 1u : 0u;
    header.sequence = sequence;
    header.payload_length = static_cast<std::uint32_t>(payload.size());

    Frame frame;
    frame.from = peer_for(endpoints_[to.index]);
    frame.to = to;
    frame.channel = channel;
    frame.sequence = sequence;
    frame.packet_id = packet_id;
    frame.bytes = net::detail::serialize(header, payload);
    queued_.push_back(std::move(frame));
    ++framed_;
    return Result<PacketId>::success(packet_id);
}

void ChannelNetworkTransport::update() {
    std::lock_guard<std::mutex> guard(mutex_);

    // Handshake: a connecting endpoint becomes connected once a listener on
    // its port exists. This is the in-process analogue of a real handshake.
    for (auto& endpoint : endpoints_) {
        if (!endpoint.alive || endpoint.state != TransportState::Connecting) {
            continue;
        }
        for (const auto& listener : endpoints_) {
            if (listener.alive && listener.listening && listener.port == endpoint.port) {
                endpoint.state = TransportState::Connected;
                break;
            }
        }
    }

    if (queued_.empty()) {
        return;
    }
    std::vector<Frame> reliable_frames;
    std::vector<Frame> unreliable_frames;
    std::vector<Frame> deferred;
    for (auto& frame : queued_) {
        if (!live(frame.to)) {
            continue; // endpoint torn down
        }
        if (find(frame.to)->state != TransportState::Connected) {
            deferred.push_back(std::move(frame)); // wait for handshake
        } else if (net::detail::reliable(frame.channel)) {
            reliable_frames.push_back(std::move(frame));
        } else {
            unreliable_frames.push_back(std::move(frame));
        }
    }
    queued_ = std::move(deferred);

    // Reliable-ordered streams: the reorder hook deliberately shuffles arrival
    // order so delivery must restore ascending sequence per (to, channel).
    if (reorder_) {
        std::reverse(reliable_frames.begin(), reliable_frames.end());
    }
    const std::vector<std::uint32_t> before = [&] {
        std::vector<std::uint32_t> out;
        out.reserve(reliable_frames.size());
        for (const auto& frame : reliable_frames) {
            out.push_back(frame.sequence);
        }
        return out;
    }();
    std::stable_sort(reliable_frames.begin(), reliable_frames.end(),
                     [](const Frame& a, const Frame& b) {
                         if (a.to.index != b.to.index) {
                             return a.to.index < b.to.index;
                         }
                         if (a.channel != b.channel) {
                             return a.channel < b.channel;
                         }
                         return a.sequence < b.sequence;
                     });
    for (std::size_t i = 0; i < reliable_frames.size(); ++i) {
        if (reliable_frames[i].sequence != before[i]) {
            ++reordered_;
        }
    }
    for (const auto& frame : reliable_frames) {
        deliver(frame);
    }

    // Unreliable streams: delivered in arrival order, with deterministic loss.
    for (const auto& frame : unreliable_frames) {
        if (drop_every_ > 0 && frame.packet_id % drop_every_ == 0) {
            ++dropped_;
            continue;
        }
        deliver(frame);
    }
}

std::uint32_t ChannelNetworkTransport::receive(
    const std::function<void(const PacketDesc&, std::span<const std::byte>)>& out) {
    std::lock_guard<std::mutex> guard(mutex_);
    const std::uint32_t count = static_cast<std::uint32_t>(delivered_.size());
    for (const auto& entry : delivered_) {
        out(entry.desc, std::span<const std::byte>(entry.payload.data(), entry.payload.size()));
    }
    delivered_.clear();
    return count;
}

} // namespace poko
