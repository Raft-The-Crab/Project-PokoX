#include "poko/physics/world.h"

namespace poko {

void NullPhysicsWorld::step(float delta_seconds) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_ || delta_seconds <= 0.0f) {
        return;
    }
    // MOCK integrator: semi-implicit Euler on dynamic bodies only. No
    // collisions, constraints or sleeping — a real backend replaces this.
    for (auto& body : bodies_) {
        if (!body.alive || body.type != BodyType::Dynamic) {
            continue;
        }
        body.state.velocity = body.state.velocity + desc_.gravity * delta_seconds;
        body.state.position = body.state.position + body.state.velocity * delta_seconds;
    }
}

} // namespace poko
