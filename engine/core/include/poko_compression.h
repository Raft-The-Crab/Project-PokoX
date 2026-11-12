#pragma once

// Compression contracts (systemplan/13): codec identifiers and buffer-level
// operations for package/asset payloads. Zstd is the planned production
// codec; the skeleton defines the seam + a store-only codec for tests.

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "poko_result.h"

namespace poko {

using CodecId = std::uint16_t;

namespace codec {
inline constexpr CodecId kStore = 0; // uncompressed passthrough
inline constexpr CodecId kZstd = 1;  // planned production codec
inline constexpr CodecId kLz4 = 2;   // optional fast codec
} // namespace codec

struct CompressedBlock {
    CodecId codec = codec::kStore;
    std::span<const std::byte> data;
    std::size_t original_size = 0; // bytes before compression (0 = unknown)
};

struct DecompressedBlock {
    std::span<std::byte> out;
    std::size_t written = 0;
};

// Codec seam (systemplan/13). Backends must be deterministic: same input +
// level -> same output bytes. Errors return Status, never throw.
class ICodec {
public:
    virtual ~ICodec() = default;
    virtual CodecId id() const noexcept = 0;
    virtual const char* name() const noexcept = 0;

    // Compress `in` into `out`; returns bytes written or an error Status via
    // the Result overload below.
    virtual Result<std::size_t> compress(std::span<const std::byte> in, std::span<std::byte> out,
                                         int level) const = 0;
    virtual Result<std::size_t> decompress(std::span<const std::byte> in,
                                           std::span<std::byte> out) const = 0;
};

// Registry of available codecs. Seeds with the Store codec; production
// codecs (Zstd etc.) register at startup.
class CodecRegistry {
public:
    CodecRegistry();
    void register_codec(ICodec* codec);
    ICodec* find(CodecId id) const noexcept;
    ICodec* find_by_name(const char* name) const noexcept;
    std::uint32_t count() const noexcept;

private:
    std::vector<ICodec*> codecs_;
};

} // namespace poko
