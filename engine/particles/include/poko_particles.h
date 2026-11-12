#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "poko_math.h"
#include "poko_result.h"

namespace poko::particles {

// Particle skeleton (Phase 1, systemplan/04/07): CPU-side emitter state and
// deterministic simulation. GPU billboard rendering comes later; this owns
// spawn rules, lifetime integration and per-frame snapshots.

struct Particle {
    math::Vec3 position;
    math::Vec3 velocity;
    float lifetime = 0.0f; // seconds remaining
    float max_lifetime = 1.0f;
    float size = 0.1f;     // metres
    float rotation = 0.0f; // radians
    float angular_velocity = 0.0f;
    math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f}; // linear RGBA
};

struct EmitterDesc {
    std::string name;
    math::Vec3 origin;
    math::Vec3 direction{0.0f, 1.0f, 0.0f}; // unit spawn cone axis
    float spread = 0.5f;                    // radians, cone half-angle
    float rate = 10.0f;                     // particles per second
    float lifetime = 1.0f;                  // seconds
    float speed_min = 1.0f;
    float speed_max = 3.0f;
    float size = 0.1f;
    math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec3 gravity{0.0f, -9.81f, 0.0f};
    bool looping = true;
    std::uint32_t max_particles = 512;
};

// Deterministic emitter. update() spawns per accumulated rate and integrates
// all live particles; no RNG state means identical inputs -> identical output.
class Emitter {
public:
    explicit Emitter(const EmitterDesc& desc) noexcept;

    void update(float delta_seconds) noexcept;

    const std::vector<Particle>& particles() const noexcept {
        return particles_;
    }
    const EmitterDesc& desc() const noexcept {
        return desc_;
    }
    bool empty() const noexcept {
        return particles_.empty();
    }

private:
    // Deterministic hash-based pseudo random in [0,1) — stable across runs.
    static float rand01(std::uint32_t& state) noexcept;
    void spawn(std::uint32_t& rng) noexcept;

    EmitterDesc desc_;
    std::vector<Particle> particles_;
    float spawn_accumulator_ = 0.0f;
    std::uint32_t spawn_counter_ = 0;
    std::uint32_t rng_state_ = 0x9E3779B9u;
};

inline bool valid(const EmitterDesc& desc) noexcept {
    return desc.rate >= 0.0f && desc.lifetime > 0.0f && desc.speed_min >= 0.0f &&
           desc.speed_max >= desc.speed_min && desc.spread >= 0.0f && desc.max_particles > 0;
}

} // namespace poko::particles
