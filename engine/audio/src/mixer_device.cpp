#include "poko/audio/mixer_device.h"

#include <cmath>
#include <cstddef>
#include <utility>

#include "mixer_detail.h"

namespace poko {

bool MixerAudioDevice::initialize() noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    initialized_ = true;
    return true;
}

bool MixerAudioDevice::handle_live(SoundHandle handle) const noexcept {
    return is_valid(handle) && static_cast<std::size_t>(handle.index) < sounds_.size() &&
           generations_[handle.index] == handle.generation && sounds_[handle.index].alive;
}

MixerAudioDevice::Sound* MixerAudioDevice::find(SoundHandle handle) noexcept {
    if (!is_valid(handle) || static_cast<std::size_t>(handle.index) >= sounds_.size()) {
        return nullptr;
    }
    return &sounds_[handle.index];
}

const MixerAudioDevice::Sound* MixerAudioDevice::find(SoundHandle handle) const noexcept {
    if (!is_valid(handle) || static_cast<std::size_t>(handle.index) >= sounds_.size()) {
        return nullptr;
    }
    return &sounds_[handle.index];
}

Result<SoundHandle> MixerAudioDevice::load_sound(const SoundDesc& desc) {
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
    Sound& sound = sounds_[index];
    sound.alive = true;
    sound.desc = desc;
    sound.volume = desc.volume;
    sound.pcm = audio::detail::synthesize(desc);
    return Result<SoundHandle>::success(make_handle(index, generations_[index]));
}

Status MixerAudioDevice::unload_sound(SoundHandle sound) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(sound)) {
        return Status::fail(ErrorCode::NotFound, "sound handle is not live");
    }
    Sound& entry = sounds_[sound.index];
    entry.alive = false;
    entry.playing = false;
    entry.paused = false;
    ++generations_[sound.index];
    free_.push_back(sound.index);
    return Status::success();
}

Status MixerAudioDevice::play(SoundHandle sound) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!initialized_) {
        return Status::fail(ErrorCode::FailedPrecondition, "audio device is not initialized");
    }
    Sound* entry = find(sound);
    if (entry == nullptr || !handle_live(sound)) {
        return Status::fail(ErrorCode::NotFound, "sound handle is not live");
    }
    if (!entry->playing) {
        entry->cursor = 0.0;
        ++voices_played_;
    }
    entry->playing = true;
    entry->paused = false;
    return Status::success();
}

Status MixerAudioDevice::stop(SoundHandle sound) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(sound)) {
        return Status::fail(ErrorCode::NotFound, "sound handle is not live");
    }
    Sound& entry = sounds_[sound.index];
    entry.playing = false;
    entry.paused = false;
    return Status::success();
}

Status MixerAudioDevice::set_volume(SoundHandle sound, float volume) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (volume < 0.0f || volume > 1.0f) {
        return Status::fail(ErrorCode::InvalidArgument, "volume out of range");
    }
    if (!handle_live(sound)) {
        return Status::fail(ErrorCode::NotFound, "sound handle is not live");
    }
    sounds_[sound.index].volume = volume;
    return Status::success();
}

Status MixerAudioDevice::set_paused(SoundHandle sound, bool paused) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!handle_live(sound)) {
        return Status::fail(ErrorCode::NotFound, "sound handle is not live");
    }
    sounds_[sound.index].paused = paused;
    return Status::success();
}

bool MixerAudioDevice::is_playing(SoundHandle sound) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const Sound* entry = find(sound);
    return entry != nullptr && handle_live(sound) && entry->playing && !entry->paused;
}

Status MixerAudioDevice::set_listener(const ListenerState& listener) {
    std::lock_guard<std::mutex> guard(mutex_);
    const float values[6] = {listener.x, listener.y, listener.z,
                             listener.fx, listener.fy, listener.fz};
    for (const float value : values) {
        if (!std::isfinite(value)) {
            return Status::fail(ErrorCode::InvalidArgument, "listener contains a non-finite value");
        }
    }
    listener_ = listener;
    return Status::success();
}

AudioStats MixerAudioDevice::stats() const {
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

std::uint32_t MixerAudioDevice::sample_count(SoundHandle sound) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const Sound* entry = find(sound);
    if (entry == nullptr || !handle_live(sound)) {
        return 0;
    }
    return static_cast<std::uint32_t>(entry->pcm.size());
}

float MixerAudioDevice::peak_level() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return peak_;
}

} // namespace poko
