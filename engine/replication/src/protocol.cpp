#include "poko/replication/protocol.h"

#include <cmath>

namespace poko::replication {
namespace {

ByteWriter begin(MessageType type) {
    ByteWriter w;
    w.u32(kMagic);
    w.u8(static_cast<std::uint8_t>(type));
    return w;
}

bool open(ByteReader& r, MessageType type) {
    const auto magic = r.u32();
    const auto kind = r.u8();
    return r.ok() && magic == kMagic && kind == static_cast<std::uint8_t>(type);
}

void put_vec(ByteWriter& w, const Vec3& v) {
    w.f32(v.x);
    w.f32(v.y);
    w.f32(v.z);
}

Vec3 get_vec(ByteReader& r) {
    Vec3 v;
    v.x = r.f32();
    v.y = r.f32();
    v.z = r.f32();
    return v;
}

bool finite(const Vec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool same(const Vec3& a, const Vec3& b) noexcept {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

} // namespace

std::uint8_t diff_mask(const EntityState& now, const EntityState* base) noexcept {
    if (base == nullptr) {
        return kFieldAll;
    }
    std::uint8_t mask = 0;
    if (!same(now.position, base->position)) mask |= kFieldPosition;
    if (now.yaw != base->yaw) mask |= kFieldYaw;
    if (!same(now.velocity, base->velocity)) mask |= kFieldVelocity;
    if (now.flags != base->flags) mask |= kFieldFlags;
    return mask;
}

EntityState apply_delta(const EntityState& base, const EntityDelta& delta) noexcept {
    EntityState out = base;
    out.id = delta.state.id;
    if (delta.mask & kFieldPosition) out.position = delta.state.position;
    if (delta.mask & kFieldYaw) out.yaw = delta.state.yaw;
    if (delta.mask & kFieldVelocity) out.velocity = delta.state.velocity;
    if (delta.mask & kFieldFlags) out.flags = delta.state.flags;
    return out;
}

std::vector<std::byte> encode(const Hello& m) {
    auto w = begin(MessageType::Hello);
    w.u16(m.version);
    w.u32(m.client);
    return w.bytes();
}

std::vector<std::byte> encode(const HelloReply& m) {
    auto w = begin(MessageType::HelloReply);
    w.u16(m.server_version);
    w.u8(static_cast<std::uint8_t>(m.status));
    return w.bytes();
}

std::vector<std::byte> encode(const Ack& m) {
    auto w = begin(MessageType::Ack);
    w.u32(m.client);
    w.u32(m.tick);
    return w.bytes();
}

std::vector<std::byte> encode(const InputCommand& m) {
    auto w = begin(MessageType::Input);
    w.u32(m.client);
    w.u32(m.sequence);
    w.f32(m.move_x);
    w.f32(m.move_z);
    w.f32(m.yaw);
    w.u16(m.buttons);
    return w.bytes();
}

std::vector<std::byte> encode(const Snapshot& m) {
    auto w = begin(MessageType::Snapshot);
    w.u32(m.tick);
    w.u32(m.baseline_tick);
    w.u32(static_cast<std::uint32_t>(m.entities.size()));
    w.u32(static_cast<std::uint32_t>(m.removed.size()));
    for (const auto& d : m.entities) {
        w.u32(d.state.id);
        w.u8(d.mask);
        if (d.mask & kFieldPosition) put_vec(w, d.state.position);
        if (d.mask & kFieldYaw) w.f32(d.state.yaw);
        if (d.mask & kFieldVelocity) put_vec(w, d.state.velocity);
        if (d.mask & kFieldFlags) w.u16(d.state.flags);
    }
    for (const auto id : m.removed) {
        w.u32(id);
    }
    return w.bytes();
}

std::optional<MessageType> peek_type(std::span<const std::byte> data) noexcept {
    ByteReader r(data);
    const auto magic = r.u32();
    const auto kind = r.u8();
    if (!r.ok() || magic != kMagic || kind < 1 || kind > 5) {
        return std::nullopt;
    }
    return static_cast<MessageType>(kind);
}

std::optional<Hello> decode_hello(std::span<const std::byte> data) {
    ByteReader r(data);
    if (!open(r, MessageType::Hello)) return std::nullopt;
    Hello m;
    m.version = r.u16();
    m.client = r.u32();
    if (!r.ok() || !r.at_end()) return std::nullopt;
    return m;
}

std::optional<HelloReply> decode_hello_reply(std::span<const std::byte> data) {
    ByteReader r(data);
    if (!open(r, MessageType::HelloReply)) return std::nullopt;
    HelloReply m;
    m.server_version = r.u16();
    const auto status = r.u8();
    if (!r.ok() || !r.at_end() || status > static_cast<std::uint8_t>(HelloStatus::Rejected)) {
        return std::nullopt;
    }
    m.status = static_cast<HelloStatus>(status);
    return m;
}

std::optional<Ack> decode_ack(std::span<const std::byte> data) {
    ByteReader r(data);
    if (!open(r, MessageType::Ack)) return std::nullopt;
    Ack m;
    m.client = r.u32();
    m.tick = r.u32();
    if (!r.ok() || !r.at_end()) return std::nullopt;
    return m;
}

std::optional<InputCommand> decode_input(std::span<const std::byte> data) {
    ByteReader r(data);
    if (!open(r, MessageType::Input)) return std::nullopt;
    InputCommand m;
    m.client = r.u32();
    m.sequence = r.u32();
    m.move_x = r.f32();
    m.move_z = r.f32();
    m.yaw = r.f32();
    m.buttons = r.u16();
    if (!r.ok() || !r.at_end()) return std::nullopt;
    if (!std::isfinite(m.move_x) || !std::isfinite(m.move_z) || !std::isfinite(m.yaw)) {
        return std::nullopt;
    }
    return m;
}

std::optional<Snapshot> decode_snapshot(std::span<const std::byte> data) {
    ByteReader r(data);
    if (!open(r, MessageType::Snapshot)) return std::nullopt;
    Snapshot m;
    m.tick = r.u32();
    m.baseline_tick = r.u32();
    const auto entity_count = r.u32();
    const auto removed_count = r.u32();
    if (!r.ok() || entity_count > kMaxSnapshotEntities || removed_count > kMaxSnapshotEntities) {
        return std::nullopt;
    }
    // Each entity needs at least 5 bytes and each removal 4: reject counts the
    // payload cannot possibly hold before allocating.
    if (static_cast<std::uint64_t>(entity_count) * 5 + static_cast<std::uint64_t>(removed_count) * 4 >
        r.remaining()) {
        return std::nullopt;
    }
    m.entities.reserve(entity_count);
    for (std::uint32_t i = 0; i < entity_count; ++i) {
        EntityDelta d;
        d.state.id = r.u32();
        d.mask = r.u8();
        if (d.mask & ~static_cast<std::uint8_t>(kFieldAll)) return std::nullopt;
        if (m.baseline_tick == 0 && d.mask != kFieldAll) return std::nullopt;
        if (d.mask & kFieldPosition) d.state.position = get_vec(r);
        if (d.mask & kFieldYaw) d.state.yaw = r.f32();
        if (d.mask & kFieldVelocity) d.state.velocity = get_vec(r);
        if (d.mask & kFieldFlags) d.state.flags = r.u16();
        if (!r.ok()) return std::nullopt;
        if (!finite(d.state.position) || !finite(d.state.velocity) || !std::isfinite(d.state.yaw)) {
            return std::nullopt;
        }
        m.entities.push_back(d);
    }
    m.removed.reserve(removed_count);
    for (std::uint32_t i = 0; i < removed_count; ++i) {
        m.removed.push_back(r.u32());
    }
    if (!r.ok() || !r.at_end()) return std::nullopt;
    return m;
}

} // namespace poko::replication
