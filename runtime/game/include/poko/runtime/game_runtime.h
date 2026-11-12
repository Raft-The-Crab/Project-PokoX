#pragma once

#include <cstdint>
#include <vector>

#include "poko/camera/rig.h"
#include "poko/character/character.h"
#include "poko/core/api_seams.h"
#include "poko/core/clock.h"
#include "poko/core/diagnostics.h"
#include "poko/core/result.h"
#include "poko/ecs/registry.h"
#include "poko/input/input.h"
#include "poko/lighting/light_list.h"
#include "poko/particles/particles.h"
#include "poko/postprocess/chain.h"
#include "poko/runtime/frame_context.h"
#include "poko/scene/graph.h"

namespace poko::runtime {

struct RuntimeDesc {
    double fixed_step_seconds = 1.0 / 60.0;
    std::uint32_t max_steps_per_frame = 8;
};

// Integrates the Phase 1 CPU subsystems into one deterministic fixed-step
// frame loop (Phase 1 exit, systemplan/27). Rendering is an injected callback
// so this class has no graphics dependency and is buildable in every
// configuration, including the headless Server build; the client demo supplies
// a RenderFn backed by the renderer and graphics modules.
//
// Per tick: FixedTimestep::advance -> N simulation steps (input edges,
// physics, character motor, particles, asset pump) -> FrameContext -> RenderFn.
// No wall clock is read, so identical inputs give identical output.
//
// Thread-safety contract: owned and driven by one frame thread. Individual
// subsystems keep their own contracts.
class GameRuntime {
public:
    GameRuntime(api::IPhysicsWorld& physics, api::IAssetProvider& assets,
                const RuntimeDesc& desc = {});

    // Advances by frame_seconds of wall time; invokes render_fn once if set.
    Status tick(double frame_seconds);
    // Runs `frames` deterministic frames of `frame_seconds` each.
    Status run(std::uint32_t frames, double frame_seconds);

    void set_render_fn(RenderFn fn);
    void set_render_items(std::vector<RenderItem> items);

    input::InputSystem& input() noexcept {
        return input_;
    }
    ecs::Registry& entities() noexcept {
        return entities_;
    }
    SceneGraph& scene() noexcept {
        return scene_;
    }
    lighting::LightList& lights() noexcept {
        return lights_;
    }
    camera::CameraRig& camera_rig() noexcept {
        return camera_rig_;
    }
    postprocess::PostProcessChain& postprocess() noexcept {
        return postprocess_;
    }
    particles::Emitter& particles() noexcept {
        return particles_;
    }
    character::CharacterMotor& character() noexcept {
        return character_;
    }
    Diagnostics& diagnostics() noexcept {
        return diagnostics_;
    }
    api::IPhysicsWorld& physics() noexcept {
        return physics_;
    }
    api::IAssetProvider& assets() noexcept {
        return assets_;
    }

    const FrameContext& last_frame() const noexcept {
        return last_frame_;
    }
    std::uint32_t frame_index() const noexcept {
        return frame_index_;
    }
    std::uint32_t simulation_step_count() const noexcept {
        return total_steps_;
    }
    double simulation_time() const noexcept {
        return simulation_time_;
    }

private:
    void simulate_step(double step_seconds);
    FrameContext build_frame(std::uint32_t steps) const;

    api::IPhysicsWorld& physics_;
    api::IAssetProvider& assets_;
    RuntimeDesc desc_;
    FixedTimestep timestep_;
    input::InputSystem input_;
    ecs::Registry entities_;
    SceneGraph scene_;
    lighting::LightList lights_;
    camera::CameraRig camera_rig_;
    postprocess::PostProcessChain postprocess_;
    particles::Emitter particles_;
    character::CharacterMotor character_;
    Diagnostics diagnostics_;
    RenderFn render_fn_;
    std::vector<RenderItem> render_items_;
    FrameContext last_frame_{};
    std::uint32_t frame_index_ = 0;
    std::uint32_t total_steps_ = 0;
    double simulation_time_ = 0.0;
};

} // namespace poko::runtime
