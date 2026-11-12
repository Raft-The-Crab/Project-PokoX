#include "poko_particles.h"

#include <algorithm>
#include <cmath>

namespace poko::particles {
namespace {

// Builds an orthonormal basis around the cone axis for random direction spread.
void basis_around(const math::Vec3& axis, math::Vec3& t1, math::Vec3& t2) {
    const math::Vec3 up =
        std::fabs(axis.y) < 0.9f ? math::Vec3{0.0f, 1.0f, 0.0f} : math::Vec3{1.0f, 0.0f, 0.0f};
    t1 = math::normalize(math::cross(axis, up));
    t2 = math::normalize(math::cross(axis, t1));
}

} // namespace

float Emitter::rand01(std::uint32_t& state) noexcept {
    // xorshift32 — deterministic, no <random> dependency.
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

Emitter::Emitter(const EmitterDesc& desc) noexcept : desc_(desc) {
    if (!valid(desc_)) {
        desc_.rate = 0.0f;
        desc_.max_particles = 1;
    }
}

void Emitter::spawn(std::uint32_t& rng) noexcept {
    if (particles_.size() >= desc_.max_particles) {
        return;
    }
    Particle p;
    p.position = desc_.origin;
    p.max_lifetime = desc_.lifetime;
    p.lifetime = desc_.lifetime;
    p.size = desc_.size;
    p.color = desc_.color;

    // Random direction inside the cone around desc_.direction.
    const math::Vec3 axis = math::normalize(desc_.direction);
    math::Vec3 t1, t2;
    basis_around(axis, t1, t2);
    const float cos_spread = std::cos(desc_.spread);
    const float cos_theta = 1.0f + rand01(rng) * (cos_spread - 1.0f);
    const float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));
    const float phi = rand01(rng) * math::kTau;
    const math::Vec3 dir = axis * cos_theta + (t1 * std::cos(phi) + t2 * std::sin(phi)) * sin_theta;

    const float speed = desc_.speed_min + rand01(rng) * (desc_.speed_max - desc_.speed_min);
    p.velocity = dir * speed;
    p.angular_velocity = (rand01(rng) - 0.5f) * 4.0f;
    particles_.push_back(p);
}

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
