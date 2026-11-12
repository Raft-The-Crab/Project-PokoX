#include "poko/core/compression.h"

#include <algorithm>
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

CodecRegistry::CodecRegistry() {
    register_codec(&g_store_codec);
}

void CodecRegistry::register_codec(ICodec* codec) {
    if (codec == nullptr) {
        return;
    }
    // Replace same-id codec (last registration wins).
    for (auto* existing : codecs_) {
        if (existing->id() == codec->id()) {
            existing = codec;
            return;
        }
    }
    codecs_.push_back(codec);
}

ICodec* CodecRegistry::find(CodecId id) const noexcept {
    for (auto* codec : codecs_) {
        if (codec->id() == id) {
            return codec;
        }
    }
    return nullptr;
}

ICodec* CodecRegistry::find_by_name(const char* name) const noexcept {
    for (auto* codec : codecs_) {
        if (std::strcmp(codec->name(), name) == 0) {
            return codec;
        }
    }
    return nullptr;
}

std::uint32_t CodecRegistry::count() const noexcept {
    return static_cast<std::uint32_t>(codecs_.size());
}

} // namespace poko
