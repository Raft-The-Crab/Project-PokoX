#pragma once

#include "poko/core/compression.h"

namespace poko {

// Internal: the built-in store (passthrough) codec instance. Not part of the
// public API — CodecRegistry seeds itself with it.
ICodec* store_codec() noexcept;

} // namespace poko
