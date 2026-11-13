#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "poko/core/math.h"
#include "poko/replication/wire.h"

namespace poko::replication {

inline constexpr std::uint32_t kMagic = 0x52584B50; // "PKXR"
inline constexpr std::uint16_t kProtocolVersion = 1;
inline constexpr std::uint32_t kMaxSnapshotEntities = 1024;

using math::Vec3;
using ClientId = std::uint32_t;
using NetEntityId = std::uint32_t;

enum class MessageType : std::uint8_t {
    Hello = 1,
    HelloReply = 2,
    Snapshot = 3,
    Ack = 4,
    Input = 5,
};

struct EntityState {
    NetEntityId id = 0;
    Vec3 position{};
    float yaw = 0.0f;
    Vec3 velocity{};
    std::uint16_t flags = 0;

    friend bool operator==(const EntityState& a, const EntityState& b) noexcept {
        return a.id == b.id && a.position.x == b.position.x && a.position.y == b.position.y &&
               a.position.z == b.position.z && a.yaw == b.yaw && a.velocity.x == b.velocity.x &&
               a.velocity.y == b.velocity.y && a.velocity.z == b.velocity.z && a.flags == b.flags;
    }
};

enum FieldBit : std::uint8_t {
    kFieldPosition = 1u << 0,
    kFieldYaw = 1u << 1,
    kFieldVelocity = 1u << 2,
    kFieldFlags = 1u << 3,
    kFieldAll = kFieldPosition | kFieldYaw | kFieldVelocity | kFieldFlags,
};

// One entity in a snapshot. Only fields named in `mask` are meaningful and
// sent on the wire; the receiver copies the rest from the baseline.
struct EntityDelta {
    EntityState state;
    std::uint8_t mask = kFieldAll;
};

// `baseline_tick == 0` means a full snapshot (every mask is kFieldAll).
// Otherwise entities carry only fields that changed against that baseline and
// `removed` lists ids in the baseline that are no longer visible.
struct Snapshot {
    std::uint32_t tick = 0;
    std::uint32_t baseline_tick = 0;
    std::vector<EntityDelta> entities;
    std::vector<NetEntityId> removed;
};

struct Hello {
    std::uint16_t version = kProtocolVersion;
    ClientId client = 0;
};

enum class HelloStatus : std::uint8_t { Accepted = 0, VersionMismatch = 1, Rejected = 2 };

struct HelloReply {
    std::uint16_t server_version = kProtocolVersion;
    HelloStatus status = HelloStatus::Accepted;
};

struct InputCommand {
    ClientId client = 0;
    std::uint32_t sequence = 0;
    float move_x = 0.0f;
    float move_z = 0.0f;
    float yaw = 0.0f;
    std::uint16_t buttons = 0;
};

struct Ack {
    ClientId client = 0;
    std::uint32_t tick = 0;
};

// Mask of fields where `now` differs from `base`; kFieldAll when base is null.
std::uint8_t diff_mask(const EntityState& now, const EntityState* base) noexcept;
// Copies the fields named in `mask` from `delta` over `base`.
EntityState apply_delta(const EntityState& base, const EntityDelta& delta) noexcept;

// Encoders produce a framed message (magic, type, body). Decoders are total
// over arbitrary bytes: they return nullopt on truncation, trailing bytes,
// wrong magic or type, limit violations, unknown mask bits or non-finite floats.
std::vector<std::byte> encode(const Hello& m);
std::vector<std::byte> encode(const HelloReply& m);
std::vector<std::byte> encode(const Snapshot& m);
std::vector<std::byte> encode(const Ack& m);
std::vector<std::byte> encode(const InputCommand& m);

std::optional<MessageType> peek_type(std::span<const std::byte> data) noexcept;
std::optional<Hello> decode_hello(std::span<const std::byte> data);
std::optional<HelloReply> decode_hello_reply(std::span<const std::byte> data);
std::optional<Snapshot> decode_snapshot(std::span<const std::byte> data);
std::optional<Ack> decode_ack(std::span<const std::byte> data);
std::optional<InputCommand> decode_input(std::span<const std::byte> data);

} // namespace poko::replication
