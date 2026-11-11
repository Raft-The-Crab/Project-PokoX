#include "poko_test_harness.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <thread>

#include "poko_clock.h"
#include "poko_event.h"
#include "poko_job.h"
#include "poko_log.h"

POKO_TEST(stopwatch_measures_elapsed) {
    poko::Stopwatch watch;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    POKO_CHECK(watch.elapsed_seconds() > 0.0);
    watch.restart();
    POKO_CHECK(watch.elapsed_seconds() >= 0.0);
    POKO_CHECK(watch.elapsed_milliseconds() >= 0.0);
}

POKO_TEST(fixed_timestep_accumulates_and_caps) {
    poko::FixedTimestep timestep(1.0 / 60.0);
    POKO_CHECK_EQ(timestep.advance(1.0 / 60.0), std::size_t{1});
    POKO_CHECK_EQ(timestep.advance(1.0 / 30.0), std::size_t{2});
    POKO_CHECK(timestep.interpolation_alpha() >= 0.0);
    POKO_CHECK(timestep.interpolation_alpha() < 1.0);
    POKO_CHECK_EQ(timestep.advance(100.0), std::size_t{8});
    POKO_CHECK_EQ(timestep.advance(0.0), std::size_t{0});
    timestep.reset();
    POKO_CHECK_EQ(timestep.accumulator(), 0.0);
    const double expected = 1.0 / 60.0;
    POKO_CHECK(timestep.step_seconds() >= expected - 1.0e-9);
    POKO_CHECK(timestep.step_seconds() <= expected + 1.0e-9);
}

namespace {
struct TestEvent {
    int value;
};
}  // namespace

POKO_TEST(event_bus_dispatches_and_unsubscribes) {
    poko::EventBus<TestEvent> bus;
    int first = 0;
    int second = 0;
    const auto subscription =
        bus.subscribe([&first](const TestEvent& event) { first += event.value; });
    bus.subscribe([&second](const TestEvent& event) { second += event.value; });
    POKO_CHECK_EQ(bus.subscriber_count(), std::size_t{2});

    bus.publish(TestEvent{3});
    POKO_CHECK_EQ(first, 3);
    POKO_CHECK_EQ(second, 3);

    POKO_CHECK(bus.unsubscribe(subscription));
    POKO_CHECK(!bus.unsubscribe(subscription));
    POKO_CHECK_EQ(bus.subscriber_count(), std::size_t{1});

    bus.publish(TestEvent{2});
    POKO_CHECK_EQ(first, 3);
    POKO_CHECK_EQ(second, 5);

    bus.clear();
    POKO_CHECK_EQ(bus.subscriber_count(), std::size_t{0});
}

POKO_TEST(event_handlers_can_publish_without_deadlock) {
    poko::EventBus<int> bus;
    int calls = 0;
    bus.subscribe([&bus, &calls](const int& value) {
        ++calls;
        if (value < 2) {
            bus.publish(value + 1);
        }
    });
    bus.publish(0);
    POKO_CHECK_EQ(calls, 3);
}

POKO_TEST(job_system_executes_all_jobs) {
    poko::JobSystem jobs(3);
    POKO_CHECK_EQ(jobs.worker_count(), std::size_t{3});
    std::atomic<int> total{0};
    for (int i = 0; i < 50; ++i) {
        jobs.submit([&total] { total.fetch_add(1); });
    }
    jobs.wait_idle();
    POKO_CHECK_EQ(total.load(), 50);
    POKO_CHECK_EQ(jobs.completed(), std::size_t{50});
    POKO_CHECK_EQ(jobs.pending(), std::size_t{0});
}

POKO_TEST(job_system_contains_exceptions) {
    poko::JobSystem jobs(2);
    poko::Logger::instance().set_sink([](const poko::LogRecord&) {});
    jobs.submit([] { throw std::runtime_error("expected"); });
    jobs.wait_idle();
    poko::Logger::instance().clear_sink();
    POKO_CHECK_EQ(jobs.completed(), std::size_t{1});
}

POKO_TEST_MAIN()
