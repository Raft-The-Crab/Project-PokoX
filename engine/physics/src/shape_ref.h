#pragma once

#include "geometry.h"
#include "poko/core/physics_contracts.h"

namespace poko::physics::detail {

// A shape plus its collision geometry (null for primitives).
struct ShapeRef {
    const ShapeDesc& desc;
    const Geometry* geo = nullptr;
};

} // namespace poko::physics::detail
