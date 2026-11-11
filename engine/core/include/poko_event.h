#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace poko {

using SubscriptionId = std::uint64_t;
inline constexpr SubscriptionId kInvalidSubscriptionId = 0;

// Instance-based (no global mutable state, systemplan/03). Synchronous
// dispatch: handlers are copied under the lock and invoked outside it, so a
// handler may subscribe, unsubscribe or publish without deadlocking.
//
// Thread-safety contract: subscribe, unsubscribe, publish, clear and
// subscriber_count are safe to call concurrently. Handlers may be invoked from
// any worker thread; they must synchronise their own shared state.
template <typename Event>
class EventBus {
public:
    using Handler = std::function<void(const Event&)>;

    SubscriptionId subscribe(Handler handler) {
        std::lock_guard<std::mutex> guard(mutex_);
        const SubscriptionId id = next_id_++;
        handlers_.emplace_back(id, std::move(handler));
        return id;
    }

    bool unsubscribe(SubscriptionId id) {
        std::lock_guard<std::mutex> guard(mutex_);
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if (it->first == id) {
                handlers_.erase(it);
                return true;
            }
        }
        return false;
    }

    std::size_t subscriber_count() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return handlers_.size();
    }

    void publish(const Event& event) {
        std::vector<Handler> snapshot;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            snapshot.reserve(handlers_.size());
            for (const auto& entry : handlers_) {
                snapshot.push_back(entry.second);
            }
        }
        for (auto& handler : snapshot) {
            handler(event);
        }
    }

    void clear() {
        std::lock_guard<std::mutex> guard(mutex_);
        handlers_.clear();
    }

private:
    mutable std::mutex mutex_;
    SubscriptionId next_id_ = 1;
    std::vector<std::pair<SubscriptionId, Handler>> handlers_;
};

}  // namespace poko
