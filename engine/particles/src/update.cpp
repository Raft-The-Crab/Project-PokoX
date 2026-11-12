#include "poko/particles/particles.h"

#include <algorithm>

namespace poko::particles {

void Emitter::update(float delta_seconds) noexcept {
    if (delta_seconds <= 0.0f) {
        return;
    }
    std::uint32_t rng = rng_state_ + spawn_counter_;

    // Spawn per accumulated rate.
    spawn_accumulator_ += desc_.rate * delta_seconds;
    const std::uint32_t due = static_cast<std::uint32_t>(spawn_accumulator_);
    spawn_accumulator_ -= static_cast<float>(due);
    for (std::uint32_t i = 0; i < due; ++i) {
        spawn(rng);
    }
    spawn_counter_ += due;

    // Integrate + kill expired.
    const math::Vec3 dragless = desc_.gravity * delta_seconds;
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                    [&](Particle& p) {
                                        p.lifetime -= delta_seconds;
                                        if (p.lifetime <= 0.0f) {
                                            return true;
                                        }
                                        p.velocity = p.velocity + dragless;
                                        p.position = p.position + p.velocity * delta_seconds;
                                        p.rotation += p.angular_velocity * delta_seconds;
                                        return false;
                                    }),
                     particles_.end());
}

} // namespace poko::particles
