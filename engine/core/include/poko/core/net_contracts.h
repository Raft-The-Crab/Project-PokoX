#pragma once

// Network transport contracts (systemplan/10). POD-only endpoint/packet
// descriptors so the INetworkTransport seam stays vendor-free. Asio is the
// planned production transport; a MOCK NullNetworkTransport implements the
// seam for offline tests.
//
// Reliability model: channels guarantee ordering per channel; packet payloads
// are opaque bytes (serialization is the caller's job).

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "poko/core/types.h"

namespace poko {

using EndpointId = TypedHandle;
using PacketId = std::uint64_t;
inline constexpr PacketId kInvalidPacket = 0;

// IPv4/IPv6-agnostic textual endpoint, e.g. "127.0.0.1:7777" or "[::1]:7777".
struct EndpointAddress {
    std::array<char, 64> text{};
    std::uint16_t port = 0;
};

// Well-known channel ids. Games may define their own above kUserChannelBase.
enum ChannelId : std::uint8_t {
    ChannelReliableOrdered = 0, // TCP-like: ordered + guaranteed
    ChannelUnreliable,          // UDP-like: may drop/reorder
    kUserChannelBase = 32,
};

struct PacketDesc {
    EndpointId from;
    EndpointId to; // invalid = broadcast/any on receive
    ChannelId channel = ChannelReliableOrdered;
    std::uint32_t flags = 0;
};

enum class TransportState : std::uint8_t {
    Disconnected = 0,
    Connecting,
    Connected,
    Closing,
};

// Input validation at the module boundary.
inline bool valid_endpoint_text(std::string_view text) noexcept {
    // Skeleton validation: non-empty, no whitespace, sane length.
    if (text.empty() || text.size() > 63) {
        return false;
    }
    for (const char c : text) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            return false;
        }
    }
    return true;
}

} // namespace poko
