#include "poko/network/transport.h"

namespace poko {

bool NullNetworkTransport::live(EndpointId id) const noexcept {
    return is_valid(id) && static_cast<std::size_t>(id.index) < endpoints_.size() &&
           generations_[id.index] == id.generation && endpoints_[id.index].alive;
}

NullNetworkTransport::Endpoint* NullNetworkTransport::find(EndpointId id) noexcept {
    if (static_cast<std::size_t>(id.index) >= endpoints_.size()) {
        return nullptr;
    }
    return &endpoints_[id.index];
}

const NullNetworkTransport::Endpoint* NullNetworkTransport::find(EndpointId id) const noexcept {
    if (static_cast<std::size_t>(id.index) >= endpoints_.size()) {
        return nullptr;
    }
    return &endpoints_[id.index];
}

Result<EndpointId> NullNetworkTransport::listen(std::string_view address, std::uint16_t port) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid_endpoint_text(address) || port == 0) {
        return Result<EndpointId>::fail(ErrorCode::InvalidArgument,
                                        "invalid listen address or port");
    }
    // Reuse a dead listener slot if present, else append.
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        endpoints_[index] = Endpoint{};
    } else {
        index = static_cast<HandleIndex>(endpoints_.size());
        endpoints_.push_back(Endpoint{});
        generations_.push_back(0);
    }
    auto& ep = endpoints_[index];
    ep.alive = true;
    ep.state = TransportState::Connected; // MOCK: instant "connected"
    ep.address = std::string(address);
    ep.port = port;
    ep.listening = true;
    const EndpointId id = make_handle(index, generations_[index]);
    loopback_peer_ = id; // loopback connects target the (first) listener
    return Result<EndpointId>::success(id);
}

Result<EndpointId> NullNetworkTransport::connect(std::string_view address,
                                                 std::uint16_t port) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!valid_endpoint_text(address) || port == 0) {
        return Result<EndpointId>::fail(ErrorCode::InvalidArgument,
                                        "invalid connect address or port");
    }
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        endpoints_[index] = Endpoint{};
    } else {
        index = static_cast<HandleIndex>(endpoints_.size());
        endpoints_.push_back(Endpoint{});
        generations_.push_back(0);
    }
    auto& ep = endpoints_[index];
    ep.alive = true;
    ep.state = is_valid(loopback_peer_) ? TransportState::Connected : TransportState::Connecting;
    ep.address = std::string(address);
    ep.port = port;
    return Result<EndpointId>::success(make_handle(index, generations_[index]));
}

Status NullNetworkTransport::close(EndpointId endpoint) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!live(endpoint)) {
        return Status::fail(ErrorCode::NotFound, "endpoint is not live");
    }
    endpoints_[endpoint.index].alive = false;
    endpoints_[endpoint.index].state = TransportState::Disconnected;
    ++generations_[endpoint.index];
    free_.push_back(endpoint.index);
    if (same_handle(loopback_peer_, endpoint)) {
        loopback_peer_ = {};
    }
    return Status::success();
}

TransportState NullNetworkTransport::state(EndpointId endpoint) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const auto* ep = live(endpoint) ? find(endpoint) : nullptr;
    return ep != nullptr ? ep->state : TransportState::Disconnected;
}

} // namespace poko
