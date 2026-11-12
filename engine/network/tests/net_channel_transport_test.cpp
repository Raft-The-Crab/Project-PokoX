#include "poko_test_harness.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "poko/network/channel_transport.h"

namespace {
using poko::ChannelNetworkTransport;
using poko::EndpointId;
using poko::PacketDesc;
using poko::TransportState;

constexpr auto kReliable = poko::ChannelReliableOrdered;
constexpr auto kUnreliable = poko::ChannelUnreliable;

std::span<const std::byte> bytes_of(const std::vector<std::byte>& data) {
    return {data.data(), data.size()};
}
} // namespace

POKO_TEST(validation_handshake_and_endpoint_lifetime) {
    ChannelNetworkTransport transport;
    POKO_CHECK(!transport.listen("", 1234).ok());
    POKO_CHECK(!transport.connect("127.0.0.1", 0).ok());

    auto server = transport.listen("127.0.0.1", 9101);
    POKO_CHECK(server.ok());
    auto client = transport.connect("127.0.0.1", 9101);
    POKO_CHECK(client.ok());
    POKO_CHECK(transport.state(client.value()) == TransportState::Connecting);
    POKO_CHECK_EQ(transport.endpoint_count(), 2u);

    transport.update();
    POKO_CHECK(transport.state(client.value()) == TransportState::Connected);
    POKO_CHECK(transport.state(server.value()) == TransportState::Connected);

    POKO_CHECK(transport.close(client.value()).ok());
    POKO_CHECK(!transport.close(client.value()).ok());
    POKO_CHECK(transport.state(client.value()) == TransportState::Disconnected);
    POKO_CHECK_EQ(transport.endpoint_count(), 1u);

    auto reused = transport.connect("127.0.0.1", 9101);
    POKO_CHECK(reused.ok());
    POKO_CHECK(!poko::same_handle(client.value(), reused.value()));
}

POKO_TEST(framing_and_reliable_ordered_delivery) {
    ChannelNetworkTransport transport;
    auto server = transport.listen("127.0.0.1", 9102);
    auto client = transport.connect("127.0.0.1", 9102);
    POKO_CHECK(server.ok() && client.ok());
    transport.update();

    const std::uint8_t values[3] = {0x11u, 0x22u, 0x33u};
    for (const std::uint8_t value : values) {
        const std::vector<std::byte> payload{static_cast<std::byte>(value)};
        POKO_CHECK(transport.send(client.value(), kReliable, bytes_of(payload)).ok());
    }
    POKO_CHECK_EQ(transport.framed_count(), 3u);

    transport.set_reorder(true); // force out-of-order arrival
    transport.update();

    std::vector<std::uint8_t> delivered;
    std::vector<PacketDesc> descs;
    const std::uint32_t count = transport.receive(
        [&](const PacketDesc& desc, std::span<const std::byte> payload) {
            descs.push_back(desc);
            if (!payload.empty()) {
                delivered.push_back(static_cast<std::uint8_t>(payload[0]));
            }
        });
    POKO_CHECK_EQ(count, 3u);
    POKO_CHECK_EQ(delivered.size(), std::size_t{3});
    POKO_CHECK_EQ(delivered[0], 0x11u); // restored ascending sequence order
    POKO_CHECK_EQ(delivered[1], 0x22u);
    POKO_CHECK_EQ(delivered[2], 0x33u);
    POKO_CHECK(transport.reordered_count() > 0u);
    POKO_CHECK(descs[0].channel == kReliable);
    POKO_CHECK_EQ(descs[0].flags, 1u);
    POKO_CHECK(poko::same_handle(descs[0].from, server.value()));
    POKO_CHECK(poko::same_handle(descs[0].to, client.value()));
    POKO_CHECK_EQ(transport.receive([](const PacketDesc&, std::span<const std::byte>) {}), 0u);
}

POKO_TEST(unreliable_channel_applies_deterministic_loss) {
    ChannelNetworkTransport transport;
    auto server = transport.listen("127.0.0.1", 9103);
    auto client = transport.connect("127.0.0.1", 9103);
    POKO_CHECK(server.ok() && client.ok());
    transport.update();

    transport.set_drop_every(2); // packet ids 2 and 4 are lost
    for (std::uint8_t i = 0; i < 4; ++i) {
        const std::vector<std::byte> payload{static_cast<std::byte>(i)};
        POKO_CHECK(transport.send(client.value(), kUnreliable, bytes_of(payload)).ok());
    }
    transport.update();
    std::uint32_t received = 0;
    POKO_CHECK_EQ(
        transport.receive([&](const PacketDesc&, std::span<const std::byte>) { ++received; }),
        2u);
    POKO_CHECK_EQ(received, 2u);
    POKO_CHECK_EQ(transport.dropped_count(), 2u);
}

POKO_TEST(frames_defer_until_handshake_completes) {
    ChannelNetworkTransport transport;
    auto client = transport.connect("127.0.0.1", 9104);
    POKO_CHECK(client.ok());
    const std::vector<std::byte> payload{std::byte{0x7f}};
    POKO_CHECK(transport.send(client.value(), kReliable, bytes_of(payload)).ok());

    transport.update(); // no listener yet: still connecting, nothing delivered
    POKO_CHECK(transport.state(client.value()) == TransportState::Connecting);
    POKO_CHECK_EQ(transport.receive([](const PacketDesc&, std::span<const std::byte>) {}), 0u);

    auto server = transport.listen("127.0.0.1", 9104);
    POKO_CHECK(server.ok());
    transport.update(); // handshake completes, deferred frame delivered
    POKO_CHECK(transport.state(client.value()) == TransportState::Connected);
    POKO_CHECK_EQ(transport.receive([](const PacketDesc&, std::span<const std::byte>) {}), 1u);
}

POKO_TEST(send_validates_endpoint_and_payload) {
    ChannelNetworkTransport transport;
    auto server = transport.listen("127.0.0.1", 9105);
    POKO_CHECK(server.ok());
    const std::vector<std::byte> payload{std::byte{0x01}};
    POKO_CHECK(!transport.send(EndpointId{}, kReliable, bytes_of(payload)).ok());
    POKO_CHECK(!transport.send(server.value(), kReliable, {}).ok());
}

POKO_TEST_MAIN()
