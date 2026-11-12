#include "poko/network/channel_transport.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace poko {

bool ChannelNetworkTransport::live(EndpointId id) const noexcept {
    return is_valid(id) && static_cast<std::size_t>(id.index) < endpoints_.size() &&
           generations_[id.index] == id.generation && endpoints_[id.index].alive;
}

ChannelNetworkTransport::Endpoint* ChannelNetworkTransport::find(EndpointId id) noexcept {
    if (!is_valid(id) || static_cast<std::size_t>(id.index) >= endpoints_.size()) {
        return nullptr;
    }
    return &endpoints_[id.index];
}

const ChannelNetworkTransport::Endpoint*
ChannelNetworkTransport::find(EndpointId id) const noexcept {
    if (!is_valid(id) || static_cast<std::size_t>(id.index) >= endpoints_.size()) {
        return nullptr;
    }
    return &endpoints_[id.index];
}

HandleIndex ChannelNetworkTransport::allocate_endpoint() {
    if (!free_.empty()) {
        const HandleIndex index = free_.back();
        free_.pop_back();
        endpoints_[index] = Endpoint{};
        return index;
    }
    const HandleIndex index = static_cast<HandleIndex>(endpoints_.size());
    endpoints_.push_back(Endpoint{});
    generations_.push_back(0);
    return index;
}

EndpointId ChannelNetworkTransport::peer_for(const Endpoint& to) const {
    for (std::size_t i = 0; i < endpoints_.size(); ++i) {
        const Endpoint& candidate = endpoints_[i];
        if (candidate.alive && candidate.listening && candidate.port == to.port &&
            &candidate != &to) {
            return make_handle(static_cast<HandleIndex>(i), generations_[i]);
        }
    }
    return EndpointId{};
}

Result<EndpointId> ChannelNetworkTransport::listen(std::string_view address, std::uint16_t port) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid_endpoint_text(address) || port == 0) {
        return Result<EndpointId>::fail(ErrorCode::InvalidArgument,
                                        "invalid listen address or port");
    }
    const HandleIndex index = allocate_endpoint();
    Endpoint& endpoint = endpoints_[index];
    endpoint.alive = true;
    endpoint.state = TransportState::Connected; // a bound listener is ready
    endpoint.address = std::string(address);
    endpoint.port = port;
    endpoint.listening = true;
    return Result<EndpointId>::success(make_handle(index, generations_[index]));
}

Result<EndpointId> ChannelNetworkTransport::connect(std::string_view address, std::uint16_t port) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid_endpoint_text(address) || port == 0) {
        return Result<EndpointId>::fail(ErrorCode::InvalidArgument,
                                        "invalid connect address or port");
    }
    const HandleIndex index = allocate_endpoint();
    Endpoint& endpoint = endpoints_[index];
    endpoint.alive = true;
    endpoint.state = TransportState::Connecting; // completed by update()
    endpoint.address = std::string(address);
    endpoint.port = port;
    endpoint.listening = false;
    return Result<EndpointId>::success(make_handle(index, generations_[index]));
}

Status ChannelNetworkTransport::close(EndpointId endpoint) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(endpoint)) {
        return Status::fail(ErrorCode::NotFound, "endpoint is not live");
    }
    endpoints_[endpoint.index].alive = false;
    endpoints_[endpoint.index].state = TransportState::Disconnected;
    ++generations_[endpoint.index];
    free_.push_back(endpoint.index);
    // Frames addressed to a torn-down endpoint are discarded.
    queued_.erase(std::remove_if(queued_.begin(), queued_.end(),
                                 [endpoint](const Frame& frame) {
                                     return frame.to.index == endpoint.index;
                                 }),
                  queued_.end());
    return Status::success();
}

TransportState ChannelNetworkTransport::state(EndpointId endpoint) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const Endpoint* found = find(endpoint);
    if (found == nullptr || !live(endpoint)) {
        return TransportState::Disconnected;
    }
    return found->state;
}

std::uint32_t ChannelNetworkTransport::endpoint_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    std::uint32_t count = 0;
    for (const auto& endpoint : endpoints_) {
        if (endpoint.alive) {
            ++count;
        }
    }
    return count;
}

void ChannelNetworkTransport::set_drop_every(std::uint32_t every) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    drop_every_ = every;
}

void ChannelNetworkTransport::set_reorder(bool enabled) noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    reorder_ = enabled;
}

std::uint32_t ChannelNetworkTransport::framed_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return framed_;
}

std::uint32_t ChannelNetworkTransport::reordered_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return reordered_;
}

std::uint32_t ChannelNetworkTransport::dropped_count() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return dropped_;
}

} // namespace poko
