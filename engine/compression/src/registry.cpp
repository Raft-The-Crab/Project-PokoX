#include "poko/core/compression.h"

#include <cstring>

#include "store_codec.h"

namespace poko {

CodecRegistry::CodecRegistry() {
    register_codec(store_codec());
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
