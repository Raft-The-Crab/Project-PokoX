#include "poko_test_harness.h"

#include <cstring>

#include "poko/network/transport.h"

POKO_TEST(listen_connect_send_receive) {
    poko::NullNetworkTransport net;

    const auto listener = net.listen("127.0.0.1", 7777);
    POKO_CHECK(listener.ok());
    POKO_CHECK(net.state(listener.value()) == poko::TransportState::Connected);

    const auto client = net.connect("127.0.0.1", 7777);
    POKO_CHECK(client.ok());
    POKO_CHECK(net.state(client.value()) == poko::TransportState::Connected);

    const char bytes[] = "ping";
    const auto packet =
        net.send(listener.value(), poko::ChannelReliableOrdered,
                 std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes), 4));
    POKO_CHECK(packet.ok());

    std::uint32_t received = 0;
    std::string payload;
    net.receive([&](const poko::PacketDesc& desc, std::span<const std::byte> data) {
        ++received;
        payload.assign(reinterpret_cast<const char*>(data.data()), data.size());
        POKO_CHECK(desc.channel == poko::ChannelReliableOrdered);
    });
    POKO_CHECK_EQ(received, 1u);
    POKO_CHECK_EQ(payload, "ping");

    // receive is one-shot: second drain yields nothing.
    std::uint32_t again = 0;
    net.receive([&](const poko::PacketDesc&, std::span<const std::byte>) { ++again; });
    POKO_CHECK_EQ(again, 0u);
}

POKO_TEST(invalid_operations_rejected) {
    poko::NullNetworkTransport net;
    POKO_CHECK(!net.listen("", 0).ok());
    POKO_CHECK(!net.connect("bad host", 0).ok());
    POKO_CHECK(!net.send({}, poko::ChannelReliableOrdered, {}).ok());
    POKO_CHECK(!net.close({}).ok());
}

POKO_TEST_MAIN()
