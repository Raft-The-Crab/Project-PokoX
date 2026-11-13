#include "poko_test_harness.h"

#include <cmath>
#include <vector>

#include "poko/replication/protocol.h"

namespace {
using namespace poko::replication;

EntityState entity(NetEntityId id, float x) {
    EntityState s;
    s.id = id;
    s.position = poko::math::Vec3{x, 1.0f, -2.0f};
    s.yaw = 0.5f;
    s.velocity = poko::math::Vec3{0.0f, 0.0f, 3.0f};
    s.flags = 7;
    return s;
}

std::span<const std::byte> view(const std::vector<std::byte>& v) {
    return {v.data(), v.size()};
}
} // namespace

POKO_TEST(control_messages_round_trip) {
    const auto hello = decode_hello(view(encode(Hello{1, 42})));
    POKO_CHECK(hello.has_value());
    POKO_CHECK_EQ(hello->client, 42u);

    const auto reply = decode_hello_reply(view(encode(HelloReply{1, HelloStatus::VersionMismatch})));
    POKO_CHECK(reply.has_value());
    POKO_CHECK(reply->status == HelloStatus::VersionMismatch);

    const auto ack = decode_ack(view(encode(Ack{3, 99})));
    POKO_CHECK(ack.has_value());
    POKO_CHECK_EQ(ack->tick, 99u);

    const auto input = decode_input(view(encode(InputCommand{3, 8, 0.5f, -0.25f, 1.5f, 5})));
    POKO_CHECK(input.has_value());
    POKO_CHECK_EQ(input->sequence, 8u);
    POKO_CHECK(input->move_z == -0.25f);
    POKO_CHECK(peek_type(view(encode(Ack{}))) == MessageType::Ack);
}

POKO_TEST(snapshot_round_trip_full_and_delta) {
    Snapshot full;
    full.tick = 5;
    full.entities.push_back({entity(1, 10.0f), kFieldAll});
    full.entities.push_back({entity(2, 20.0f), kFieldAll});
    const auto full_bytes = encode(full);
    const auto decoded = decode_snapshot(view(full_bytes));
    POKO_CHECK(decoded.has_value());
    POKO_CHECK_EQ(decoded->entities.size(), 2u);
    POKO_CHECK(decoded->entities[1].state == entity(2, 20.0f));

    const auto moved = [] { auto e = entity(1, 11.0f); return e; }();
    const auto base = entity(1, 10.0f);
    POKO_CHECK_EQ(static_cast<unsigned>(diff_mask(moved, &base)), static_cast<unsigned>(kFieldPosition));
    POKO_CHECK_EQ(static_cast<unsigned>(diff_mask(base, &base)), 0u);

    Snapshot delta;
    delta.tick = 6;
    delta.baseline_tick = 5;
    delta.entities.push_back({moved, kFieldPosition});
    delta.removed.push_back(2);
    const auto delta_bytes = encode(delta);
    POKO_CHECK(delta_bytes.size() < full_bytes.size());
    const auto back = decode_snapshot(view(delta_bytes));
    POKO_CHECK(back.has_value());
    POKO_CHECK_EQ(back->removed.size(), 1u);
    POKO_CHECK(apply_delta(base, back->entities[0]) == moved);
}

POKO_TEST(decoders_reject_every_truncation) {
    Snapshot snap;
    snap.tick = 2;
    snap.entities.push_back({entity(1, 1.0f), kFieldAll});
    const auto bytes = encode(snap);
    for (std::size_t n = 0; n < bytes.size(); ++n) {
        POKO_CHECK(!decode_snapshot({bytes.data(), n}).has_value());
    }
    auto trailing = bytes;
    trailing.push_back(std::byte{0});
    POKO_CHECK(!decode_snapshot(view(trailing)).has_value());

    const auto input = encode(InputCommand{1, 1, 0, 0, 0, 0});
    for (std::size_t n = 0; n < input.size(); ++n) {
        POKO_CHECK(!decode_input({input.data(), n}).has_value());
    }
}

POKO_TEST(decoders_reject_hostile_content) {
    auto bytes = encode(Ack{1, 1});
    bytes[0] = std::byte{0};
    POKO_CHECK(!decode_ack(view(bytes)).has_value());
    POKO_CHECK(!peek_type(view(bytes)).has_value());

    POKO_CHECK(!decode_input(view(encode(InputCommand{1, 1, std::nanf(""), 0, 0, 0}))).has_value());
    POKO_CHECK(!decode_input(view(encode(InputCommand{1, 1, 0, INFINITY, 0, 0}))).has_value());

    EntityState nan_state = entity(1, 0.0f);
    nan_state.position.x = std::nanf("");
    Snapshot nan_snap;
    nan_snap.tick = 1;
    nan_snap.entities.push_back({nan_state, kFieldAll});
    POKO_CHECK(!decode_snapshot(view(encode(nan_snap))).has_value());

    // Huge counts must fail before allocating.
    Snapshot huge;
    huge.tick = 1;
    auto big = encode(huge);
    big[13] = std::byte{0xFF};
    big[14] = std::byte{0xFF};
    big[15] = std::byte{0xFF};
    big[16] = std::byte{0x7F};
    POKO_CHECK(!decode_snapshot(view(big)).has_value());

    // Unknown mask bit, and a partial mask in a full (baseline 0) snapshot.
    Snapshot partial;
    partial.tick = 1;
    partial.entities.push_back({entity(1, 0.0f), kFieldPosition});
    POKO_CHECK(!decode_snapshot(view(encode(partial))).has_value());
    Snapshot unknown;
    unknown.tick = 2;
    unknown.baseline_tick = 1;
    unknown.entities.push_back({entity(1, 0.0f), static_cast<std::uint8_t>(0x80)});
    POKO_CHECK(!decode_snapshot(view(encode(unknown))).has_value());
}

POKO_TEST_MAIN()
