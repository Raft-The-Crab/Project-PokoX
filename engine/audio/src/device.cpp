#include "poko/audio/device.h"

namespace poko {

bool NullAudioDevice::initialize() noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    if (initialized_) {
        return true;
    }
    initialized_ = true;
    return true;
}

NullAudioDevice::Sound* NullAudioDevice::find(SoundHandle handle) noexcept {
    if (static_cast<std::size_t>(handle.index) >= sounds_.size()) {
        return nullptr;
    }
    return &sounds_[handle.index];
}

const NullAudioDevice::Sound* NullAudioDevice::find(SoundHandle handle) const noexcept {
    if (static_cast<std::size_t>(handle.index) >= sounds_.size()) {
        return nullptr;
    }
    return &sounds_[handle.index];
}

bool NullAudioDevice::handle_live(SoundHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < sounds_.size() &&
           generations_[handle.index] == handle.generation && sounds_[handle.index].alive;
}

Result<SoundHandle> NullAudioDevice::load_sound(const SoundDesc& desc) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Result<SoundHandle>::fail(ErrorCode::FailedPrecondition,
                                         "audio device is not initialized");
    }
    if (!valid(desc)) {
        return Result<SoundHandle>::fail(ErrorCode::InvalidArgument, "invalid sound descriptor");
    }
    HandleIndex index;
    if (!free_.empty()) {
        index = free_.back();
        free_.pop_back();
        sounds_[index] = Sound{};
    } else {
        index = static_cast<HandleIndex>(sounds_.size());
        sounds_.push_back(Sound{});
        generations_.push_back(0);
    }
    auto& sound = sounds_[index];
    sound.alive = true;
    sound.desc = desc;
    sound.volume = desc.volume;
    return Result<SoundHandle>::success(make_handle(index, generations_[index]));
}

Status NullAudioDevice::unload_sound(SoundHandle sound) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(sound)) {
        return Status::fail(ErrorCode::NotFound, "sound handle is not live");
    }
    sounds_[sound.index].alive = false;
    sounds_[sound.index].playing = false;
    ++generations_[sound.index];
    free_.push_back(sound.index);
    return Status::success();
}

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
