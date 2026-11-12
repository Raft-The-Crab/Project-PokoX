# audio

`api::IAudioDevice` seam. Two implementations, separate targets so a MOCK is
never confused with production (systemplan/30 rule 12):

- `poko_audio_null` — **MOCK** `NullAudioDevice`
  (`include/poko/audio/device.h`). Voice state only, no samples.
- `poko_audio_mixer` — **PRODUCTION** `MixerAudioDevice`
  (`include/poko/audio/mixer_device.h`). A real software PCM mixer using only
  the C++20 standard library.

`MixerAudioDevice` synthesizes a deterministic mono waveform at load time
(fundamental + soft second harmonic with attack/release envelope), maintains
voice state (play/stop/pause/volume/loop), linearly resamples from a 48 kHz
internal rate, applies distance attenuation against the listener for
positional sounds, and mixes the active voices into a caller-supplied float
buffer via `render()` (output clamped to [-1, 1]). `peak_level()` exposes the
last render peak; `sample_count()` exposes the synthesized length. All timing
is deterministic and testable without an audio device.

**Deferred:** OS audio output. miniaudio is `NOT VENDORED` in
`third_party/DEPENDENCIES.md` with a `TBD` revision/hash; pinning it requires
a real immutable revision, SHA-256 and licence notice, which must not be
guessed (see the deferral recorded in `systemplan/28`). `render()` is the
boundary a real device callback will drive once that dependency is pinned.

Public headers live under `include/poko/audio/`; sources under `src/`
(`mixer_detail.h` holds shared `detail::` helpers); tests under `tests/`.
