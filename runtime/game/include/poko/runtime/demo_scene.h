#pragma once

#include "poko/core/result.h"
#include "poko/runtime/game_runtime.h"

namespace poko::runtime {

// Populates the runtime with a deterministic sample scene (systemplan/27
// "stable local demo scene"): hierarchy, entities, lights, post-process,
// camera rig, one physics body, one asset request, held movement input and a
// small render list. RenderItem mesh_id/material_id both index slot 0 — the
// client demo supplies one shared mesh + material pair. Call once per runtime.
Status build_demo_scene(GameRuntime& runtime);

} // namespace poko::runtime
