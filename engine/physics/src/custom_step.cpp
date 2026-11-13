#include "poko/physics/custom_world.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <numeric>

#include "joint_rows.h"
#include "narrowphase.h"

namespace poko {
namespace {

using math::Vec3;

constexpr int kVelocityIterations = 12;
constexpr int kPositionIterations = 4;
constexpr float kSlop = 0.005f;
constexpr float kCorrection = 0.8f;
constexpr float kRestitutionThreshold = 1.0f;
constexpr float kLinearDamping = 0.02f;
constexpr float kAngularDamping = 0.1f;
constexpr float kRestSpeed = 0.05f;
constexpr float kSleepTime = 0.5f;
constexpr float kMatchDistance = 0.05f;
constexpr float kRestDamping = 4.0f;
constexpr float kRestDampSpeed = 0.5f;

struct Constraint {
    int a = -1;
    int b = -1; // -1: immovable ground
    Vec3 n{};
    Vec3 point{};
    std::uint64_t key = 0;
    Vec3 ra{};
    Vec3 rb{};
    Vec3 t1{};
    Vec3 t2{};
    float depth = 0.0f;
    float mass_n = 0.0f;
    float mass_t1 = 0.0f;
    float mass_t2 = 0.0f;
    float bounce = 0.0f;
    float mu = 0.0f;
    float jn = 0.0f;
    float jt1 = 0.0f;
    float jt2 = 0.0f;
};

struct Pair {
    int a;
    int b;
};

Vec3 perpendicular(const Vec3& n) noexcept {
    const Vec3 helper = std::abs(n.x) < 0.57f ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
    return math::normalize(math::cross(n, helper));
}

} // namespace

void CustomPhysicsWorld::substep(float h) {
    using namespace physics::detail;
    const std::size_t count = bodies_.size();

    // 1. Integrate velocities of awake dynamic bodies.
    for (auto& body : bodies_) {
        if (!body.alive || body.type != BodyType::Dynamic || body.sleeping) {
            continue;
        }
        BodyState& s = body.state;
        s.velocity = (s.velocity + desc_.gravity * h) * (1.0f / (1.0f + kLinearDamping * h));
        s.angular_velocity = s.angular_velocity * (1.0f / (1.0f + kAngularDamping * h));
    }

    // 2. Broadphase: sweep and prune on x, then y/z overlap and filters.
    struct Entry {
        Vec3 lo;
        Vec3 hi;
        int index;
    };
    std::vector<Entry> entries;
    entries.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Body& body = bodies_[i];
        if (!body.alive) continue;
        const Vec3 half = aabb_half_extents(body.shape, body.state.rotation);
        entries.push_back({body.state.position - half, body.state.position + half, static_cast<int>(i)});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& l, const Entry& r) {
        return l.lo.x != r.lo.x ? l.lo.x < r.lo.x : l.index < r.index;
    });
    const auto awake_dynamic = [&](const Body& b) {
        return b.type == BodyType::Dynamic && !b.sleeping;
    };
    std::vector<std::uint64_t> joined; // body pairs that must not collide
    for (const Joint& joint : joints_) {
        if (joint.alive && !joint.collide_connected && joint.b >= 0) {
            const auto lo = static_cast<std::uint64_t>(std::min(joint.a, joint.b));
            const auto hi = static_cast<std::uint64_t>(std::max(joint.a, joint.b));
            joined.push_back(lo << 32 | hi);
        }
    }
    std::sort(joined.begin(), joined.end());
    std::vector<Pair> pairs;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        for (std::size_t j = i + 1; j < entries.size() && entries[j].lo.x <= entries[i].hi.x; ++j) {
            const Entry& e = entries[i];
            const Entry& f = entries[j];
            if (e.lo.y > f.hi.y || f.lo.y > e.hi.y || e.lo.z > f.hi.z || f.lo.z > e.hi.z) continue;
            const int lo = std::min(e.index, f.index);
            const int hi = std::max(e.index, f.index);
            if (std::binary_search(joined.begin(), joined.end(),
                                   static_cast<std::uint64_t>(lo) << 32 | static_cast<std::uint64_t>(hi))) {
                continue;
            }
            const Body& ba = bodies_[static_cast<std::size_t>(lo)];
            const Body& bb = bodies_[static_cast<std::size_t>(hi)];
            if (!awake_dynamic(ba) && !awake_dynamic(bb)) continue;
            if ((ba.collision_layer & bb.collision_mask) == 0 ||
                (bb.collision_layer & ba.collision_mask) == 0) {
                continue;
            }
            pairs.push_back({lo, hi});
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](const Pair& l, const Pair& r) {
        return l.a != r.a ? l.a < r.a : l.b < r.b;
    });

    // 3. Wake sleepers touched by something that is moving.
    const auto moving = [&](const Body& b) {
        if (b.type == BodyType::Kinematic) {
            return math::length_squared(b.state.velocity) > 1.0e-6f ||
                   math::length_squared(b.state.angular_velocity) > 1.0e-6f;
        }
        return awake_dynamic(b) && b.sleep_timer == 0.0f;
    };
    for (const Pair& p : pairs) {
        Body& ba = bodies_[static_cast<std::size_t>(p.a)];
        Body& bb = bodies_[static_cast<std::size_t>(p.b)];
        if (ba.sleeping && moving(bb)) {
            ba.sleeping = false;
            ba.sleep_timer = 0.0f;
        } else if (bb.sleeping && moving(ba)) {
            bb.sleeping = false;
            bb.sleep_timer = 0.0f;
        }
    }
    // A joint keeps its bodies in one island: waking one wakes the chain.
    for (bool changed = true; changed;) {
        changed = false;
        for (const Joint& joint : joints_) {
            if (!joint.alive || joint.b < 0) continue;
            Body& ba = bodies_[static_cast<std::size_t>(joint.a)];
            Body& bb = bodies_[static_cast<std::size_t>(joint.b)];
            if (ba.sleeping && awake_dynamic(bb)) {
                ba.sleeping = false;
                ba.sleep_timer = 0.0f;
                changed = true;
            } else if (bb.sleeping && awake_dynamic(ba)) {
                bb.sleeping = false;
                bb.sleep_timer = 0.0f;
                changed = true;
            }
        }
    }

    std::vector<float> inv_m(count, 0.0f);
    for (std::size_t i = 0; i < count; ++i) {
        const Body& body = bodies_[i];
        if (body.alive && awake_dynamic(body) && body.mass > 0.0f) inv_m[i] = 1.0f / body.mass;
    }
    const auto inv_inertia_mul = [&](int i, const Vec3& v) {
        const Body& body = bodies_[static_cast<std::size_t>(i)];
        if (inv_m[static_cast<std::size_t>(i)] == 0.0f) return Vec3{};
        const Vec3 local = math::rotate(math::conjugate(body.state.rotation), v);
        return math::rotate(body.state.rotation,
                            Vec3{local.x * body.inv_inertia.x, local.y * body.inv_inertia.y,
                                 local.z * body.inv_inertia.z});
    };

    // 4. Narrow phase and constraint construction.
    std::vector<Constraint> constraints;
    const auto add_constraints = [&](int a, int b, const Manifold& m, float mu, float e) {
        for (int k = 0; k < m.count; ++k) {
            const ContactPoint& cp = m.points[k];
            Constraint c;
            c.a = a;
            c.b = b;
            c.n = cp.normal;
            c.point = cp.point;
            c.key = (static_cast<std::uint64_t>(a + 1) << 32) | static_cast<std::uint64_t>(b + 1);
            c.depth = cp.depth;
            c.ra = cp.point - bodies_[static_cast<std::size_t>(a)].state.position;
            c.rb = b >= 0 ? cp.point - bodies_[static_cast<std::size_t>(b)].state.position : Vec3{};
            c.t1 = perpendicular(c.n);
            c.t2 = math::cross(c.n, c.t1);
            c.mu = mu;
            c.bounce = e; // resolved to a target speed in the pre-step
            constraints.push_back(c);
        }
        contacts_ += static_cast<std::uint32_t>(m.count);
    };
    for (const Pair& p : pairs) {
        const Body& ba = bodies_[static_cast<std::size_t>(p.a)];
        const Body& bb = bodies_[static_cast<std::size_t>(p.b)];
        Manifold m;
        if (!collide(ba.shape, Pose{ba.state.position, ba.state.rotation}, bb.shape,
                     Pose{bb.state.position, bb.state.rotation}, m)) {
            continue;
        }
        add_constraints(p.a, p.b, m, std::sqrt(ba.friction * bb.friction),
                        std::min(ba.restitution, bb.restitution));
    }
    if (ground_enabled_) {
        for (std::size_t i = 0; i < count; ++i) {
            const Body& body = bodies_[i];
            if (!body.alive || !awake_dynamic(body)) continue;
            Manifold m;
            if (collide_ground(body.shape, Pose{body.state.position, body.state.rotation},
                               ground_height_, m)) {
                add_constraints(static_cast<int>(i), -1, m, std::sqrt(body.friction),
                                body.restitution);
            }
        }
    }

    // 5. Position correction: linear, mass-weighted, tracking per-body
    // displacement so multi-point manifolds are not corrected repeatedly.
    std::vector<Vec3> shift(count);
    for (int iter = 0; iter < kPositionIterations; ++iter) {
        for (const Constraint& c : constraints) {
            const auto ia = inv_m[static_cast<std::size_t>(c.a)];
            const auto ib = c.b >= 0 ? inv_m[static_cast<std::size_t>(c.b)] : 0.0f;
            if (ia + ib <= 0.0f) continue;
            const Vec3 sb = c.b >= 0 ? shift[static_cast<std::size_t>(c.b)] : Vec3{};
            const float depth = c.depth - math::dot(sb - shift[static_cast<std::size_t>(c.a)], c.n);
            const float corr = std::max(0.0f, depth - kSlop) * kCorrection / (ia + ib);
            shift[static_cast<std::size_t>(c.a)] = shift[static_cast<std::size_t>(c.a)] - c.n * (corr * ia);
            if (c.b >= 0) shift[static_cast<std::size_t>(c.b)] = shift[static_cast<std::size_t>(c.b)] + c.n * (corr * ib);
        }
    }

    // 6. Velocity solve (sequential impulses).
    const auto velocity_at = [&](int i, const Vec3& r) {
        if (i < 0) return Vec3{};
        const BodyState& s = bodies_[static_cast<std::size_t>(i)].state;
        return s.velocity + math::cross(s.angular_velocity, r);
    };
    const auto apply = [&](const Constraint& c, const Vec3& impulse) {
        const auto ia = inv_m[static_cast<std::size_t>(c.a)];
        BodyState& sa = bodies_[static_cast<std::size_t>(c.a)].state;
        sa.velocity = sa.velocity - impulse * ia;
        sa.angular_velocity = sa.angular_velocity - inv_inertia_mul(c.a, math::cross(c.ra, impulse));
        if (c.b >= 0) {
            const auto ib = inv_m[static_cast<std::size_t>(c.b)];
            BodyState& sb = bodies_[static_cast<std::size_t>(c.b)].state;
            sb.velocity = sb.velocity + impulse * ib;
            sb.angular_velocity = sb.angular_velocity + inv_inertia_mul(c.b, math::cross(c.rb, impulse));
        }
    };
    const auto effective_mass = [&](const Constraint& c, const Vec3& dir) {
        const auto ia = inv_m[static_cast<std::size_t>(c.a)];
        const Vec3 ra_n = math::cross(c.ra, dir);
        float k = ia + math::dot(math::cross(inv_inertia_mul(c.a, ra_n), c.ra), dir);
        if (c.b >= 0) {
            const auto ib = inv_m[static_cast<std::size_t>(c.b)];
            const Vec3 rb_n = math::cross(c.rb, dir);
            k += ib + math::dot(math::cross(inv_inertia_mul(c.b, rb_n), c.rb), dir);
        }
        return k > 1.0e-9f ? 1.0f / k : 0.0f;
    };
    for (Constraint& c : constraints) {
        c.mass_n = effective_mass(c, c.n);
        c.mass_t1 = effective_mass(c, c.t1);
        c.mass_t2 = effective_mass(c, c.t2);
        const float vn = math::dot(velocity_at(c.b, c.rb) - velocity_at(c.a, c.ra), c.n);
        c.bounce = vn < -kRestitutionThreshold ? -c.bounce * vn : 0.0f;
    }
    // Joint rows: geometry from the current pose, impulses from the last step.
    std::vector<JointRow> jrows;
    build_joint_rows(h, inv_m, jrows);
    const auto body_velocity = [&](int i) {
        return i < 0 ? Vec3{} : bodies_[static_cast<std::size_t>(i)].state.velocity;
    };
    const auto body_spin = [&](int i) {
        return i < 0 ? Vec3{} : bodies_[static_cast<std::size_t>(i)].state.angular_velocity;
    };
    const auto row_inv_mass = [&](int i) { return i < 0 ? 0.0f : inv_m[static_cast<std::size_t>(i)]; };
    const auto row_inertia = [&](int i, const Vec3& v) { return i < 0 ? Vec3{} : inv_inertia_mul(i, v); };
    const auto apply_row = [&](const JointRow& r, float lambda) {
        if (r.a >= 0) {
            BodyState& s = bodies_[static_cast<std::size_t>(r.a)].state;
            s.velocity = s.velocity - r.lin * (row_inv_mass(r.a) * lambda);
            s.angular_velocity = s.angular_velocity - row_inertia(r.a, r.ang_a * lambda);
        }
        if (r.b >= 0) {
            BodyState& s = bodies_[static_cast<std::size_t>(r.b)].state;
            s.velocity = s.velocity + r.lin * (row_inv_mass(r.b) * lambda);
            s.angular_velocity = s.angular_velocity + row_inertia(r.b, r.ang_b * lambda);
        }
    };
    for (JointRow& r : jrows) {
        float k = (row_inv_mass(r.a) + row_inv_mass(r.b)) * math::dot(r.lin, r.lin) +
                  math::dot(row_inertia(r.a, r.ang_a), r.ang_a) +
                  math::dot(row_inertia(r.b, r.ang_b), r.ang_b);
        r.mass = k > 1.0e-9f ? 1.0f / k : 0.0f;
        apply_row(r, r.impulse); // warm start
    }

    // Contact warm starting: match each contact to last step's nearest one.
    std::map<std::uint64_t, std::vector<char>> claimed;
    for (Constraint& c : constraints) {
        const auto cached = contact_cache_.find(c.key);
        if (cached == contact_cache_.end()) continue;
        auto& taken = claimed[c.key];
        taken.resize(cached->second.size(), 0);
        int best = -1;
        float best_d2 = kMatchDistance * kMatchDistance;
        for (std::size_t i = 0; i < cached->second.size(); ++i) {
            const float d2 = math::length_squared(cached->second[i].point - c.point);
            if (!taken[i] && d2 < best_d2) {
                best_d2 = d2;
                best = static_cast<int>(i);
            }
        }
        if (best < 0) continue;
        taken[static_cast<std::size_t>(best)] = 1;
        const auto& old = cached->second[static_cast<std::size_t>(best)];
        c.jn = old.normal_impulse;
        c.jt1 = math::dot(old.friction_impulse, c.t1);
        c.jt2 = math::dot(old.friction_impulse, c.t2);
    }
    for (const Constraint& c : constraints) {
        apply(c, c.n * c.jn + c.t1 * c.jt1 + c.t2 * c.jt2);
    }

    for (int iter = 0; iter < kVelocityIterations; ++iter) {
        for (JointRow& r : jrows) {
            const float jv = math::dot(r.lin, body_velocity(r.b) - body_velocity(r.a)) +
                             math::dot(r.ang_b, body_spin(r.b)) - math::dot(r.ang_a, body_spin(r.a));
            const float old = r.impulse;
            r.impulse = math::clamp(old + r.mass * (r.bias - jv), r.lo, r.hi);
            apply_row(r, r.impulse - old);
        }
        for (Constraint& c : constraints) {
            const Vec3 rel = velocity_at(c.b, c.rb) - velocity_at(c.a, c.ra);
            // Friction, bounded by the Coulomb cone of the current normal impulse.
            const float limit = c.mu * c.jn;
            const float d1 = -math::dot(rel, c.t1) * c.mass_t1;
            const float old1 = c.jt1;
            c.jt1 = math::clamp(old1 + d1, -limit, limit);
            const float d2 = -math::dot(rel, c.t2) * c.mass_t2;
            const float old2 = c.jt2;
            c.jt2 = math::clamp(old2 + d2, -limit, limit);
            apply(c, c.t1 * (c.jt1 - old1) + c.t2 * (c.jt2 - old2));

            const Vec3 rel_n = velocity_at(c.b, c.rb) - velocity_at(c.a, c.ra);
            const float vn = math::dot(rel_n, c.n);
            const float dn = (c.bounce - vn) * c.mass_n;
            const float old = c.jn;
            c.jn = std::max(0.0f, old + dn);
            apply(c, c.n * (c.jn - old));
        }
    }

    // Rest damping: slow bodies in contact shed residual sway so stacks settle
    // and can sleep instead of creeping indefinitely.
    {
        std::vector<char> touching(count, 0);
        for (const Constraint& c : constraints) {
            touching[static_cast<std::size_t>(c.a)] = 1;
            if (c.b >= 0) touching[static_cast<std::size_t>(c.b)] = 1;
        }
        const float factor = 1.0f / (1.0f + kRestDamping * h);
        for (std::size_t i = 0; i < count; ++i) {
            Body& body = bodies_[i];
            if (!touching[i] || !body.alive || !awake_dynamic(body)) continue;
            BodyState& s = body.state;
            if (math::length_squared(s.velocity) < kRestDampSpeed * kRestDampSpeed &&
                math::length_squared(s.angular_velocity) < kRestDampSpeed * kRestDampSpeed) {
                s.velocity = s.velocity * factor;
                s.angular_velocity = s.angular_velocity * factor;
            }
        }
    }

    for (const JointRow& r : jrows) {
        joints_[static_cast<std::size_t>(r.joint)].impulses[r.slot] = r.impulse;
    }
    contact_cache_.clear();
    for (const Constraint& c : constraints) {
        contact_cache_[c.key].push_back({c.point, c.jn, c.t1 * c.jt1 + c.t2 * c.jt2});
    }

    // 7. Integrate positions and orientation.
    for (std::size_t i = 0; i < count; ++i) {
        Body& body = bodies_[i];
        if (!body.alive || !awake_dynamic(body)) continue;
        BodyState& s = body.state;
        s.position = s.position + s.velocity * h + shift[i];
        const float spin = math::length(s.angular_velocity);
        if (spin > 1.0e-5f) {
            s.rotation = math::multiply(
                math::quat_from_axis_angle(s.angular_velocity / spin, spin * h), s.rotation);
        }
    }

    // 8. Sleeping: islands of dynamic bodies at rest.
    if (sleep_enabled_) {
        std::vector<int> parent(count);
        std::iota(parent.begin(), parent.end(), 0);
        const auto find_root = [&](int x) {
            while (parent[static_cast<std::size_t>(x)] != x) {
                parent[static_cast<std::size_t>(x)] = parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])];
                x = parent[static_cast<std::size_t>(x)];
            }
            return x;
        };
        for (const Constraint& c : constraints) {
            if (c.b >= 0 && inv_m[static_cast<std::size_t>(c.a)] > 0.0f &&
                inv_m[static_cast<std::size_t>(c.b)] > 0.0f) {
                parent[static_cast<std::size_t>(find_root(c.a))] = find_root(c.b);
            }
        }
        for (const Joint& joint : joints_) {
            if (joint.alive && joint.b >= 0 && inv_m[static_cast<std::size_t>(joint.a)] > 0.0f &&
                inv_m[static_cast<std::size_t>(joint.b)] > 0.0f) {
                parent[static_cast<std::size_t>(find_root(joint.a))] = find_root(joint.b);
            }
        }
        std::vector<char> ready(count, 1);
        for (std::size_t i = 0; i < count; ++i) {
            Body& body = bodies_[i];
            if (!body.alive || !awake_dynamic(body)) continue;
            const bool slow = math::length_squared(body.state.velocity) < kRestSpeed * kRestSpeed &&
                              math::length_squared(body.state.angular_velocity) < kRestSpeed * kRestSpeed;
            body.sleep_timer = slow ? body.sleep_timer + h : 0.0f;
            if (body.sleep_timer < kSleepTime) {
                ready[static_cast<std::size_t>(find_root(static_cast<int>(i)))] = 0;
            }
        }
        for (std::size_t i = 0; i < count; ++i) {
            Body& body = bodies_[i];
            if (!body.alive || !awake_dynamic(body)) continue;
            if (ready[static_cast<std::size_t>(find_root(static_cast<int>(i)))]) {
                body.sleeping = true;
                body.state.velocity = Vec3{};
                body.state.angular_velocity = Vec3{};
            }
        }
    }
    ++substeps_;
}

void CustomPhysicsWorld::step(float delta_seconds) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_ || !(delta_seconds > 0.0f) || !std::isfinite(delta_seconds)) {
        return;
    }
    contacts_ = 0;
    const float h = desc_.fixed_delta_seconds;
    accumulator_ += std::min(delta_seconds, 0.25f);
    std::uint32_t iterations = 0;
    while (accumulator_ >= h && iterations < 8u) {
        substep(h);
        accumulator_ -= h;
        ++iterations;
    }
}

} // namespace poko
