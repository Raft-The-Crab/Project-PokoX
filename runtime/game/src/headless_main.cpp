#include <atomic>
#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>

#include "poko/core/assert.h"
#include "poko/core/build_config.h"
#include "poko/core/clock.h"
#include "poko/core/diagnostics.h"
#include "poko/core/event.h"
#include "poko/core/job.h"
#include "poko/core/log.h"
#include "poko/core/math.h"
#include "poko/core/ownership.h"
#include "poko/core/settings.h"
#include "poko/core/version.h"

// Console test application for engine core (systemplan/03). A headless demo
// that runs without any graphics device and exercises the core subsystems.
namespace {

struct TickEvent {
    std::size_t step;
};

int run_core_self_check() {
    poko::Logger::instance().set_level(poko::LogLevel::Debug);

    POKO_LOG_INFO("runtime", "headless runtime " << poko::version_string()
                                                 << " config=" << poko::build_config_name()
                                                 << " platform=" << poko::platform_name());

    POKO_ASSERT(poko::version_string() != nullptr, "version string must exist");
    POKO_ASSERT(!poko::config::kDevTools || poko::config::kDevelopmentLike,
                "dev tools are restricted to development configurations");

    if constexpr (poko::config::kHeadless) {
        POKO_LOG_INFO("runtime", "headless mode: no graphics, audio or player UI");
    }

    poko::Settings settings = poko::Settings::parse("# engine core smoke settings\n"
                                                    "sim.fixed_step = 0.016666\n"
                                                    "diagnostics.enabled = on\n");
    POKO_ASSERT(settings.has("sim.fixed_step"), "settings must parse");

    poko::Diagnostics diagnostics;
    poko::EventBus<TickEvent> bus;
    std::atomic<int> ticks{0};
    [[maybe_unused]] const auto subscription =
        bus.subscribe([&ticks](const TickEvent&) { ticks.fetch_add(1); });

    poko::JobSystem jobs(2);
    std::atomic<int> completed{0};
    for (int i = 0; i < 8; ++i) {
        jobs.submit([&completed] { completed.fetch_add(1); });
    }
    jobs.wait_idle();
    POKO_ASSERT(completed.load() == 8, "all jobs must complete");

    poko::FixedTimestep timestep(settings.get_double("sim.fixed_step", 1.0 / 60.0));
    const std::size_t steps = timestep.advance(1.0 / 30.0);
    for (std::size_t step = 0; step < steps; ++step) {
        bus.publish(TickEvent{step});
        diagnostics.increment("ticks");
    }
    POKO_ASSERT(ticks.load() == static_cast<int>(steps), "ticks must match steps");

    poko::ResourceRegistry<int> registry;
    [[maybe_unused]] const auto handle = registry.add(std::make_unique<int>(42));
    POKO_ASSERT(registry.get(handle) != nullptr, "resource must be live");
    POKO_ASSERT(registry.release(handle), "resource must release");
    POKO_ASSERT(registry.get(handle) == nullptr, "stale handle must be invalid");

    [[maybe_unused]] const poko::math::Vec3 rotated = poko::math::transform_direction(
        poko::math::Mat4::rotation_y(poko::math::kHalfPi), poko::math::Vec3{1.0f, 0.0f, 0.0f});
    POKO_ASSERT(poko::math::nearly_equal(rotated.z, -1.0f, 1.0e-4f),
                "rotation must be deterministic");

    POKO_ASSERT(bus.unsubscribe(subscription), "subscription must unsubscribe");
    diagnostics.set_gauge("jobs.completed", static_cast<double>(jobs.completed()));

    POKO_LOG_DEBUG("runtime", "self-check complete, ticks=" << diagnostics.counter("ticks")
                                                            << " jobs=" << jobs.completed());
    std::cout << "headless runtime self-check: OK\n";
    return 0;
}

} // namespace

int main() {
    // Contain any unexpected exception so the harness reports a clean failure.
    try {
        return run_core_self_check();
    } catch (const std::exception& error) {
        POKO_LOG_ERROR("runtime", "self-check failed: " << error.what());
        return 1;
    }
}
