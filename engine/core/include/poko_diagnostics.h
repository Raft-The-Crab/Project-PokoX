#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace poko {

// Instance-based diagnostics (no global mutable state). The engine owns one
// and exposes it through IPlatformServices later; Dear ImGui viewers are
// development-only and live outside engine core (systemplan/09).
//
// Thread-safety contract: every method is safe to call concurrently. Timings
// are last-value-wins, not aggregated; aggregate in a later telemetry layer.
class Diagnostics {
public:
    void increment(std::string_view name, std::int64_t delta = 1) {
        std::lock_guard<std::mutex> guard(mutex_);
        counters_[std::string(name)] += delta;
    }

    std::int64_t counter(std::string_view name) const {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto it = counters_.find(std::string(name));
        return it == counters_.end() ? 0 : it->second;
    }

    void set_gauge(std::string_view name, double value) {
        std::lock_guard<std::mutex> guard(mutex_);
        gauges_[std::string(name)] = value;
    }

    double gauge(std::string_view name) const {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto it = gauges_.find(std::string(name));
        return it == gauges_.end() ? 0.0 : it->second;
    }

    void record_time(std::string_view name, double seconds) {
        std::lock_guard<std::mutex> guard(mutex_);
        timings_[std::string(name)] = seconds;
    }

    double last_time(std::string_view name) const {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto it = timings_.find(std::string(name));
        return it == timings_.end() ? 0.0 : it->second;
    }

    std::size_t counter_count() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return counters_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> guard(mutex_);
        counters_.clear();
        gauges_.clear();
        timings_.clear();
    }

    // RAII scope timer. Records elapsed seconds on destruction.
    class ScopedTimer {
    public:
        ScopedTimer(Diagnostics& diagnostics, std::string_view name)
            : diagnostics_(diagnostics), name_(name), start_(Clock::now()) {}

        ~ScopedTimer() {
            diagnostics_.record_time(name_,
                                     std::chrono::duration<double>(Clock::now() - start_).count());
        }

        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;

    private:
        using Clock = std::chrono::steady_clock;
        Diagnostics& diagnostics_;
        std::string name_;
        Clock::time_point start_;
    };

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::int64_t> counters_;
    std::unordered_map<std::string, double> gauges_;
    std::unordered_map<std::string, double> timings_;
};

} // namespace poko
