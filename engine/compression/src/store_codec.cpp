#include "store_codec.h"

#include <cstring>

namespace poko {
namespace {

// Store codec: passthrough copy. Deterministic by construction.
class StoreCodec final : public ICodec {
public:
    CodecId id() const noexcept override {
        return codec::kStore;
    }
    const char* name() const noexcept override {
        return "store";
    }
    Result<std::size_t> compress(std::span<const std::byte> in, std::span<std::byte> out,
                                 int level) const override {
        (void)level;
        if (out.size() < in.size()) {
            return Result<std::size_t>::fail(ErrorCode::ResourceExhausted,
                                             "store codec needs output >= input");
        }
        std::memcpy(out.data(), in.data(), in.size());
        return Result<std::size_t>::success(in.size());
    }
    Result<std::size_t> decompress(std::span<const std::byte> in,
                                   std::span<std::byte> out) const override {
        if (out.size() < in.size()) {
            return Result<std::size_t>::fail(ErrorCode::ResourceExhausted,
                                             "store codec needs output >= input");
        }
        std::memcpy(out.data(), in.data(), in.size());
        return Result<std::size_t>::success(in.size());
    }
};

StoreCodec g_store_codec;

} // namespace

ICodec* store_codec() noexcept {
    return &g_store_codec;
}

} // namespace poko
