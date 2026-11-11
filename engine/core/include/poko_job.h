#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "poko_api_seams.h"

namespace poko {

// Phase 1 implementation of api::IJobSystem.
//
// Ownership contract: the JobSystem owns its worker threads and queued jobs.
// Submitted callables are moved into the queue and destroyed after execution.
// The destructor stops the workers and joins them; it must NOT be called from
// a job running on one of its own workers.
//
// Thread-safety contract: submit() and wait_idle() may be called from any
// thread. Jobs may run concurrently on different workers and must synchronise
// their own shared state. Exceptions escaping a job are logged and contained;
// they never terminate a worker.
class JobSystem final : public api::IJobSystem {
public:
    using Job = std::function<void()>;

    // worker_count == 0 selects a hardware-based default (at least 2).
    explicit JobSystem(std::size_t worker_count = 0);
    ~JobSystem() override;

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void submit(Job job) override;
    void wait_idle() override;

    std::size_t worker_count() const noexcept { return workers_.size(); }
    std::size_t pending() const;
    std::size_t completed() const noexcept {
        return completed_.load(std::memory_order_relaxed);
    }

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    mutable std::mutex mutex_;
    std::condition_variable work_cv_;
    std::condition_variable idle_cv_;
    std::deque<Job> queue_;
    std::size_t active_ = 0;
    bool stopping_ = false;
    std::atomic<std::size_t> completed_{0};
};

}  // namespace poko
