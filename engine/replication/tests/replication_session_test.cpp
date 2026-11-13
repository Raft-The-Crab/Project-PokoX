#include "poko_test_harness.h"

#include <cmath>
#include <memory>
#include <vector>

#include "poko/network/channel_transport.h"
#include "poko/replication/client.h"

namespace {
using namespace poko::replication;
using poko::ChannelNetworkTransport;
using poko::EndpointId;

struct World {
    ChannelNetworkTransport transport;
    EndpointId listener{};
    std::vector<EndpointId> client_endpoints;
    std::unique_ptr<ReplicationServer> server;
    std::vector<std::unique_ptr<ReplicationClient>> clients;

    explicit World(std::uint16_t port, std::size_t client_count = 1, ServerConfig cfg = {}) {
        listener = transport.listen("127.0.0.1", port).value();
        server = std::make_unique<ReplicationServer>(transport, listener, cfg);
        for (std::size_t i = 0; i < client_count; ++i) {
            const auto endpoint = transport.connect("127.0.0.1", port).value();
            client_endpoints.push_back(endpoint);
            const auto id = server->add_client(endpoint).value();
            clients.push_back(std::make_unique<ReplicationClient>(transport, endpoint, listener, id));
        }
    }

    void step() {
        std::vector<PacketSink*> sinks{server.get()};
        for (auto& c : clients) sinks.push_back(c.get());
        pump(transport, sinks);
        server->tick();
        pump(transport, sinks);
    }

    void run(int ticks) {
        for (int i = 0; i < ticks; ++i) step();
    }

    void connect_all() {
        for (auto& c : clients) POKO_CHECK(c->connect().ok());
        run(2);
    }
};

EntityState at(float x, float z = 0.0f) {
    EntityState s;
    s.position = poko::math::Vec3{x, 0.0f, z};
    return s;
}
} // namespace

POKO_TEST(handshake_and_replication_converge) {
    World w(9301);
    POKO_CHECK(w.clients[0]->state() == ClientState::Idle);
    w.connect_all();
    POKO_CHECK(w.clients[0]->state() == ClientState::Connected);

    const auto id = w.server->spawn(at(5.0f)).value();
    w.run(3);
    const auto& seen = w.clients[0]->latest();
    POKO_CHECK_EQ(seen.size(), 1u);
    POKO_CHECK(seen.at(id).position.x == 5.0f);

    auto moved = *w.server->find(id);
    moved.position.x = 9.0f;
    POKO_CHECK(w.server->set_state(moved).ok());
    w.run(3);
    POKO_CHECK(w.clients[0]->latest().at(id).position.x == 9.0f);
}

POKO_TEST(deltas_shrink_steady_state_and_removals_replicate) {
    World w(9302);
    w.connect_all();
    const auto a = w.server->spawn(at(1.0f)).value();
    const auto b = w.server->spawn(at(2.0f)).value();
    w.run(3);
    POKO_CHECK_EQ(w.clients[0]->latest().size(), 2u);

    const auto before = w.server->stats().bytes_sent;
    const auto snaps_before = w.server->stats().snapshots_sent;
    w.run(4);
    const auto per_snapshot = (w.server->stats().bytes_sent - before) /
                              (w.server->stats().snapshots_sent - snaps_before);
    // An unchanged world is just the header: no entity payload.
    POKO_CHECK(per_snapshot < 25);

    POKO_CHECK(w.server->despawn(a).ok());
    w.run(3);
    POKO_CHECK_EQ(w.clients[0]->latest().size(), 1u);
    POKO_CHECK(w.clients[0]->latest().contains(b));
}

POKO_TEST(interest_management_filters_per_client) {
    ServerConfig cfg;
    cfg.interest_radius = 10.0f;
    World w(9303, 2, cfg);
    w.connect_all();
    const auto near_id = w.server->spawn(at(0.0f)).value();
    const auto far_id = w.server->spawn(at(100.0f)).value();
    POKO_CHECK(w.server->set_view(w.clients[0]->id(), poko::math::Vec3{0, 0, 0}).ok());
    POKO_CHECK(w.server->set_view(w.clients[1]->id(), poko::math::Vec3{100, 0, 0}).ok());
    w.run(3);
    POKO_CHECK(w.clients[0]->latest().contains(near_id) && !w.clients[0]->latest().contains(far_id));
    POKO_CHECK(w.clients[1]->latest().contains(far_id) && !w.clients[1]->latest().contains(near_id));

    // Moving the view swaps visibility, replicated as removal plus addition.
    POKO_CHECK(w.server->set_view(w.clients[0]->id(), poko::math::Vec3{100, 0, 0}).ok());
    w.run(3);
    POKO_CHECK(w.clients[0]->latest().contains(far_id) && !w.clients[0]->latest().contains(near_id));
}

POKO_TEST(entity_cap_keeps_nearest) {
    ServerConfig cfg;
    cfg.max_entities_per_snapshot = 2;
    World w(9304, 1, cfg);
    w.connect_all();
    w.server->spawn(at(30.0f)).value();
    const auto n1 = w.server->spawn(at(1.0f)).value();
    const auto n2 = w.server->spawn(at(2.0f)).value();
    w.run(3);
    POKO_CHECK_EQ(w.clients[0]->latest().size(), 2u);
    POKO_CHECK(w.clients[0]->latest().contains(n1) && w.clients[0]->latest().contains(n2));
}

POKO_TEST(survives_packet_loss) {
    World w(9305);
    w.transport.set_drop_every(3); // acks, snapshots and inputs are unreliable
    w.connect_all();
    const auto id = w.server->spawn(at(0.0f)).value();
    for (int i = 1; i <= 40; ++i) {
        auto s = *w.server->find(id);
        s.position.x = static_cast<float>(i);
        POKO_CHECK(w.server->set_state(s).ok());
        w.step();
    }
    w.run(6);
    POKO_CHECK(w.transport.dropped_count() > 0);
    POKO_CHECK(w.clients[0]->latest().at(id).position.x == 40.0f);
    POKO_CHECK_EQ(w.clients[0]->stats().malformed_dropped, 0u);
}

POKO_TEST(server_validates_inputs) {
    World w(9306);
    std::vector<InputCommand> received;
    w.server->set_input_handler([&](ClientId, const InputCommand& c) { received.push_back(c); });

    // Inputs before the handshake are refused.
    POKO_CHECK(!w.clients[0]->send_input(1, 0, 0, 0).ok());
    w.connect_all();

    POKO_CHECK(w.clients[0]->send_input(10.0f, 0.0f, 0.0f, 1).ok());
    POKO_CHECK(!w.clients[0]->send_input(std::nanf(""), 0.0f, 0.0f, 0).ok());
    w.run(2);
    POKO_CHECK_EQ(received.size(), 1u);
    const float mag = std::hypot(received[0].move_x, received[0].move_z);
    POKO_CHECK(std::fabs(mag - 1.0f) < 1e-5f);

    // A replayed sequence number is rejected.
    const auto replay = encode(InputCommand{w.clients[0]->id(), 1, 0.1f, 0.0f, 0.0f, 0});
    POKO_CHECK(w.transport.send(w.listener, poko::ChannelUnreliable, replay).ok());
    w.run(2);
    POKO_CHECK_EQ(received.size(), 1u);
    POKO_CHECK_EQ(w.server->stats().inputs_rejected, 1u);
}

POKO_TEST(malformed_and_spoofed_packets_are_dropped) {
    World w(9307);
    w.connect_all();
    const std::vector<std::byte> junk(32, std::byte{0xAB});
    POKO_CHECK(w.transport.send(w.listener, poko::ChannelUnreliable, junk).ok());
    // Server-to-client message types are invalid from a client.
    POKO_CHECK(w.transport.send(w.listener, poko::ChannelUnreliable, encode(Snapshot{})).ok());
    // Unknown client id.
    POKO_CHECK(w.transport.send(w.listener, poko::ChannelUnreliable, encode(Ack{777, 1})).ok());
    POKO_CHECK(w.transport.send(w.client_endpoints[0], poko::ChannelUnreliable, junk).ok());
    w.run(2);
    POKO_CHECK_EQ(w.server->stats().malformed_dropped, 2u);
    POKO_CHECK_EQ(w.server->stats().unknown_client_dropped, 1u);
    POKO_CHECK_EQ(w.clients[0]->stats().malformed_dropped, 1u);
    POKO_CHECK(w.clients[0]->state() == ClientState::Connected);
}

POKO_TEST(protocol_version_mismatch_is_rejected) {
    World w(9308);
    const auto hello = encode(Hello{static_cast<std::uint16_t>(kProtocolVersion + 1), w.clients[0]->id()});
    POKO_CHECK(w.transport.send(w.listener, poko::ChannelReliableOrdered, hello).ok());
    w.server->spawn(at(0.0f)).value();
    w.run(3);
    POKO_CHECK_EQ(w.server->stats().version_mismatches, 1u);
    POKO_CHECK_EQ(w.server->stats().snapshots_sent, 0u);
}

POKO_TEST(silent_clients_time_out) {
    ServerConfig cfg;
    cfg.timeout_ticks = 5;
    World w(9310, 1, cfg);
    POKO_CHECK_EQ(w.server->client_count(), 1u);
    w.run(10); // never says hello
    POKO_CHECK_EQ(w.server->client_count(), 0u);
    POKO_CHECK_EQ(w.server->stats().timeouts, 1u);
}

POKO_TEST(interpolation_blends_between_snapshots) {
    World w(9311);
    w.connect_all();
    const auto id = w.server->spawn(at(0.0f)).value();
    w.run(2);
    auto s = *w.server->find(id);
    s.position.x = 10.0f;
    s.yaw = 0.0f;
    POKO_CHECK(w.server->set_state(s).ok());
    w.step();
    const auto newest = w.clients[0]->latest_tick();
    const auto mid = w.clients[0]->sample(static_cast<double>(newest) - 0.5);
    POKO_CHECK_EQ(mid.size(), 1u);
    POKO_CHECK(std::fabs(mid[0].position.x - 5.0f) < 1e-4f);
    // Beyond the buffer clamps to the newest authoritative state.
    POKO_CHECK(w.clients[0]->sample(1.0e6)[0].position.x == 10.0f);
}

POKO_TEST_MAIN()
