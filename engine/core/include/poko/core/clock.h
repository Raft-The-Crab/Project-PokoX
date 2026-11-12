#pragma once

#include <chrono>
#include <cmath>
#include <cstddef>

namespace poko {

// Monotonic timing. Never use system_clock for simulation time; it can jump.
class Stopwatch {
public:
    using Clock = std::chrono::steady_clock;

    Stopwatch() : start_(Clock::now()) {}

    void restart() noexcept {
        start_ = Clock::now();
    }

    double elapsed_seconds() const noexcept {
        return std::chrono::duration<double>(Clock::now() - start_).count();
    }

    double elapsed_milliseconds() const noexcept {
        return elapsed_seconds() * 1000.0;
    }

private:
    Clock::time_point start_;
};

// Fixed-timestep accumulator (systemplan/06: fixed timestep and determinism).
// advance() returns how many simulation steps to run for a frame. A cap
// prevents the "spiral of death" when a frame is very late.
class FixedTimestep {
public:
    static constexpr double kDefaultStep = 1.0 / 60.0;
    static constexpr double kMaxFrameSeconds = 0.25;
    static constexpr std::size_t kMaxStepsPerFrame = 8;

    explicit FixedTimestep(double step_seconds = kDefaultStep,
                           std::size_t max_steps = kMaxStepsPerFrame)
        : step_(step_seconds > 0.0 ? step_seconds : kDefaultStep),
          max_steps_(max_steps == 0 ? kMaxStepsPerFrame : max_steps) {}

    std::size_t advance(double frame_seconds) noexcept {
        if (!(frame_seconds > 0.0)) {
            return 0;
        }
        if (frame_seconds > kMaxFrameSeconds) {
            frame_seconds = kMaxFrameSeconds;
        }
        accumulator_ += frame_seconds;

        double raw_steps = std::floor(accumulator_ / step_);
        double cap = static_cast<double>(max_steps_);
        if (raw_steps > cap) {
            raw_steps = cap;
        }
        const auto steps = static_cast<std::size_t>(raw_steps);

        accumulator_ -= static_cast<double>(steps) * step_;
        if (accumulator_ < 0.0) {
            accumulator_ = 0.0;
        }
        return steps;
    }

    // Fraction of a step not yet simulated, for interpolation [0, 1).
    double interpolation_alpha() const noexcept {
        return accumulator_ / step_;
    }

    double step_seconds() const noexcept {
        return step_;
    }
    std::size_t max_steps() const noexcept {
        return max_steps_;
    }
    double accumulator() const noexcept {
        return accumulator_;
    }

    void reset() noexcept {
        accumulator_ = 0.0;
    }

private:
    double step_;
    std::size_t max_steps_;
    double accumulator_ = 0.0;
};

} // namespace poko
