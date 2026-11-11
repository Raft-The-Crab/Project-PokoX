#include "poko_job.h"

#include <algorithm>
#include <exception>
#include <utility>

#include "poko_log.h"

namespace poko {

JobSystem::JobSystem(std::size_t worker_count) {
    const std::size_t hardware =
        static_cast<std::size_t>(std::thread::hardware_concurrency());
    const std::size_t count =
        worker_count == 0 ? std::max<std::size_t>(std::size_t{2}, hardware)
                          : worker_count;
    workers_.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

JobSystem::~JobSystem() {
    {
        std::lock_guard<std::mutex> guard(mutex_);
        stopping_ = true;
    }
    work_cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void JobSystem::submit(Job job) {
    if (!job) {
        POKO_LOG_WARN("jobs", "ignoring an empty job");
        return;
    }
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (stopping_) {
            POKO_LOG_WARN("jobs", "submit called while stopping");
            return;
        }
        queue_.push_back(std::move(job));
    }
    work_cv_.notify_one();
}

void JobSystem::wait_idle() {
    std::unique_lock<std::mutex> lock(mutex_);
    idle_cv_.wait(lock, [this] { return queue_.empty() && active_ == 0; });
}

std::size_t JobSystem::pending() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return queue_.size();
}

void JobSystem::worker_loop() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            work_cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) {
                return;
            }
            job = std::move(queue_.front());
            queue_.pop_front();
            ++active_;
        }

        try {
            job();
        } catch (const std::exception& error) {
            POKO_LOG_ERROR("jobs", "job threw: " << error.what());
        } catch (...) {
            POKO_LOG_ERROR("jobs", "job threw an unknown exception");
        }

        {
            std::lock_guard<std::mutex> guard(mutex_);
            --active_;
            completed_.fetch_add(1, std::memory_order_relaxed);
        }
        idle_cv_.notify_all();
    }
}

}  // namespace poko
