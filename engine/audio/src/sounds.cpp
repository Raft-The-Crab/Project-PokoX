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

} // namespace poko
