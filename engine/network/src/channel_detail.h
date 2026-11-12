#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "poko/core/net_contracts.h"

namespace poko::net::detail {

// Versioned PokoX datagram frame. 16-byte little-endian header followed by an
// opaque payload. A socket backend will put this on the wire unchanged.
struct PkxFrameHeader {
    std::array<char, 4> magic{'P', 'K', 'X', 'N'};
    std::uint8_t channel = 0;
    std::uint8_t flags = 0;
    std::uint16_t reserved = 0;
    std::uint32_t sequence = 0;
    std::uint32_t payload_length = 0;
};

static_assert(sizeof(PkxFrameHeader) == 16, "PKXN header must be 16 bytes");

inline std::vector<std::byte> serialize(const PkxFrameHeader& header,
                                        std::span<const std::byte> payload) {
    std::vector<std::byte> out(sizeof(PkxFrameHeader) + payload.size());
    std::memcpy(out.data(), &header, sizeof(header));
    if (!payload.empty()) {
        std::memcpy(out.data() + sizeof(header), payload.data(), payload.size());
    }
    return out;
}

inline bool parse(std::span<const std::byte> bytes, PkxFrameHeader& header,
                  std::span<const std::byte>& payload) noexcept {
    if (bytes.size() < sizeof(PkxFrameHeader)) {
        return false;
    }
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (header.magic != std::array<char, 4>{'P', 'K', 'X', 'N'} ||
        header.payload_length > bytes.size() - sizeof(PkxFrameHeader)) {
        return false;
    }
    payload = bytes.subspan(sizeof(PkxFrameHeader), header.payload_length);
    return true;
}

inline bool reliable(ChannelId channel) noexcept {
    return channel == ChannelReliableOrdered;
}

} // namespace poko::net::detail
