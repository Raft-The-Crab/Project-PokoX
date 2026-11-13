#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace poko::replication {

// Little-endian append-only writer.
class ByteWriter {
public:
    void u8(std::uint8_t v) { bytes_.push_back(static_cast<std::byte>(v)); }
    void u16(std::uint16_t v) { put(v); }
    void u32(std::uint32_t v) { put(v); }
    void f32(float v) { put(std::bit_cast<std::uint32_t>(v)); }

    const std::vector<std::byte>& bytes() const noexcept { return bytes_; }

private:
    template <typename T>
    void put(T v) {
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            bytes_.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFF));
        }
    }
    std::vector<std::byte> bytes_;
};

// Bounds-checked reader. An over-read latches ok() == false and yields zeros,
// so decoders check once at the end and never read out of range.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> data) noexcept : data_(data) {}

    std::uint8_t u8() { return take<std::uint8_t>(); }
    std::uint16_t u16() { return take<std::uint16_t>(); }
    std::uint32_t u32() { return take<std::uint32_t>(); }
    float f32() { return std::bit_cast<float>(take<std::uint32_t>()); }

    bool ok() const noexcept { return ok_; }
    bool at_end() const noexcept { return pos_ == data_.size(); }
    std::size_t remaining() const noexcept { return data_.size() - pos_; }

private:
    template <typename T>
    T take() {
        if (!ok_ || remaining() < sizeof(T)) {
            ok_ = false;
            return T{};
        }
        T v{};
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            v = static_cast<T>(v | (static_cast<T>(data_[pos_ + i]) << (8 * i)));
        }
        pos_ += sizeof(T);
        return v;
    }
    std::span<const std::byte> data_;
    std::size_t pos_ = 0;
    bool ok_ = true;
};

} // namespace poko::replication
