#include "poko/audio/device.h"

namespace poko {

Status NullAudioDevice::play(SoundHandle sound) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "audio device is not initialized");
    }
    auto* found = handle_live(sound) ? find(sound) : nullptr;
    if (found == nullptr) {
        return Status::fail(ErrorCode::NotFound, "sound handle is not live");
    }
    if (!found->playing) {
        ++voices_played_;
    }
    found->playing = true;
    found->paused = false;
    return Status::success();
}

Status NullAudioDevice::stop(SoundHandle sound) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto* found = handle_live(sound) ? find(sound) : nullptr;
    if (found == nullptr) {
        return Status::fail(ErrorCode::NotFound, "sound handle is not live");
    }
    found->playing = false;
    found->paused = false;
    return Status::success();
}

Status NullAudioDevice::set_volume(SoundHandle sound, float volume) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (volume < 0.0f || volume > 1.0f) {
        return Status::fail(ErrorCode::InvalidArgument, "volume out of range");
    }
    auto* found = handle_live(sound) ? find(sound) : nullptr;
    if (found == nullptr) {
        return Status::fail(ErrorCode::NotFound, "sound handle is not live");
    }
    found->volume = volume;
    return Status::success();
}

Status NullAudioDevice::set_paused(SoundHandle sound, bool paused) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto* found = handle_live(sound) ? find(sound) : nullptr;
    if (found == nullptr) {
        return Status::fail(ErrorCode::NotFound, "sound handle is not live");
    }
    found->paused = paused;
    return Status::success();
}

bool NullAudioDevice::is_playing(SoundHandle sound) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const auto* found = handle_live(sound) ? find(sound) : nullptr;
    return found != nullptr && found->playing && !found->paused;
}

Status NullAudioDevice::set_listener(const ListenerState& listener) {
    std::lock_guard<std::mutex> guard(mutex_);
    listener_ = listener;
    return Status::success();
}

AudioStats NullAudioDevice::stats() const {
    std::lock_guard<std::mutex> guard(mutex_);
    AudioStats out;
    for (const auto& sound : sounds_) {
        if (sound.alive && sound.playing && !sound.paused) {
            ++out.active_voices;
        }
    }
    out.total_voices_played = static_cast<std::uint32_t>(voices_played_);
    return out;
}

} // namespace poko
