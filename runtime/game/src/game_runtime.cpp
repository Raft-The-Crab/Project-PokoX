#include "poko/runtime/game_runtime.h"

#include <cmath>
#include <utility>

namespace poko::runtime {
namespace {

particles::EmitterDesc default_emitter() {
    particles::EmitterDesc desc;
    desc.name = "runtime.sparks";
    desc.origin = math::Vec3{0.0f, 1.0f, 0.0f};
    desc.rate = 8.0f;
    desc.lifetime = 1.5f;
    desc.speed_min = 0.5f;
    desc.speed_max = 1.5f;
    desc.size = 0.08f;
    desc.max_particles = 64;
    return desc;
}

// Flat walkable world plane at y = 0, matching the MOCK physics world. The
// motor probes downward from feet + up*ground_probe; returning a hit within
// range keeps the character grounded and deterministic.
bool flat_ground_probe(const math::Vec3& origin, float distance, math::Vec3& hit_point,
                       math::Vec3& normal, void*) {
    if (origin.y - distance <= 0.0f && origin.y >= 0.0f) {
        hit_point = math::Vec3{origin.x, 0.0f, origin.z};
        normal = math::Vec3{0.0f, 1.0f, 0.0f};
        return true;
    }
    return false;
}

character::MoveInput read_move_input(const input::InputSystem& input) {
    character::MoveInput move;
    if (input.key_down(input::KeyCode::W)) {
        move.wish_direction = math::Vec3{0.0f, 0.0f, -1.0f};
    } else if (input.key_down(input::KeyCode::S)) {
        move.wish_direction = math::Vec3{0.0f, 0.0f, 1.0f};
    }
    if (input.key_down(input::KeyCode::A)) {
        move.wish_direction = move.wish_direction + math::Vec3{-1.0f, 0.0f, 0.0f};
    } else if (input.key_down(input::KeyCode::D)) {
        move.wish_direction = move.wish_direction + math::Vec3{1.0f, 0.0f, 0.0f};
    }
    move.sprint = input.key_down(input::KeyCode::LeftShift);
    move.jump = input.key_pressed(input::KeyCode::Space);
    return move;
}

} // namespace

GameRuntime::GameRuntime(api::IPhysicsWorld& physics, api::IAssetProvider& assets,
                         const RuntimeDesc& desc)
    : physics_(physics),
      assets_(assets),
      desc_(desc),
      timestep_(desc.fixed_step_seconds, desc.max_steps_per_frame),
      particles_(default_emitter()) {
    character_.configure(character::CharacterDesc{});
    character_.teleport(math::Vec3{0.0f, 0.0f, 0.0f});
    diagnostics_.set_gauge("runtime.fixed_step", desc_.fixed_step_seconds);
}

void GameRuntime::set_render_fn(RenderFn fn) {
    render_fn_ = std::move(fn);
}

void GameRuntime::set_render_items(std::vector<RenderItem> items) {
    render_items_ = std::move(items);
}

void GameRuntime::simulate_step(double step_seconds) {
    input_.begin_frame();
    physics_.step(static_cast<float>(step_seconds));
    character_.update(static_cast<float>(step_seconds), read_move_input(input_),
                      flat_ground_probe, nullptr);
    particles_.update(static_cast<float>(step_seconds));
    assets_.update();
    diagnostics_.increment("runtime.steps");
    ++total_steps_;
    simulation_time_ += step_seconds;
}

FrameContext GameRuntime::build_frame(std::uint32_t steps) const {
    FrameContext ctx;
    ctx.frame_index = frame_index_;
    ctx.simulation_steps = steps;
    ctx.simulation_time_seconds = simulation_time_;
    const camera::CameraPose pose = camera_rig_.pose();
    ctx.camera_position = pose.position;
    ctx.camera_target = pose.look_target;
    ctx.camera_up = pose.up;
    ctx.light_count = lights_.count();
    ctx.postprocess_count = postprocess_.count();
    ctx.render_items = render_items_;
    return ctx;
}

Status GameRuntime::tick(double frame_seconds) {
    if (!(frame_seconds > 0.0) || !std::isfinite(frame_seconds)) {
        return Status::fail(ErrorCode::InvalidArgument, "frame_seconds must be finite and > 0");
    }
    const std::size_t steps = timestep_.advance(frame_seconds);
    for (std::size_t i = 0; i < steps; ++i) {
        simulate_step(timestep_.step_seconds());
    }

    FrameContext ctx = build_frame(static_cast<std::uint32_t>(steps));
    diagnostics_.increment("runtime.frames");
    diagnostics_.set_gauge("runtime.simulation_time", simulation_time_);

    if (render_fn_) {
        Status status = render_fn_(ctx);
        if (!status.ok()) {
            return status;
        }
    }

    last_frame_ = std::move(ctx);
    ++frame_index_;
    return Status::success();
}

Status GameRuntime::run(std::uint32_t frames, double frame_seconds) {
    for (std::uint32_t i = 0; i < frames; ++i) {
        Status status = tick(frame_seconds);
        if (!status.ok()) {
            return status;
        }
    }
    return Status::success();
}

} // namespace poko::runtime
