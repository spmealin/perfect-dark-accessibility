# Accessibility mix and overload protection

## Behavior

The port mixer now separates three floating-point buses before final PCM output:

- Original game audio: the already combined music, dialogue and effects stream.
- World cues: scanner/enemy presence and directional guidance, cane, markers,
  landmarks, radar, trackers, pickups and hazard/threat sounds, after any HRTF.
- Feedback cues: centered lock-on/range-accuracy tones, compass, mode/toggle,
  stance and weapon-function confirmations.

This does not introduce prioritization, voice stealing, new cue scheduling,
speech ducking, or new visibility/geometry queries. Existing individual cue
volume settings still apply before their bus gain. Prism/screen-reader audio is
external to this mixer and is not compressed or attenuated here.

By default, audible accessibility cues reserve 3 dB of headroom in the original
game bus. Game gain ramps down over approximately 10 ms, stays reserved for
250 ms after the last non-negligible cue sample, and returns over approximately
100 ms. This avoids reacting to individual waveform zero crossings and short
beep gaps. The original game bus includes dialogue as well as effects/music;
this is not selective music-only ducking. Cue gain is not lowered to create
headroom. Reducing `GameAudioHeadroomDb` to zero disables this attenuation.

The combined signal passes through a stereo-linked sample-peak limiter, with
an instantaneous attack, 100 ms exponential release and a 0.98 full-scale
ceiling while cues/hold are active. The same gain applies to both channels,
preserving their relative balance instead of clipping each ear independently.
There is no lookahead, buffering, device change or added audio delay. This is
not a true-peak/inter-sample limiter, loudness normalizer, or guarantee about
clipping that already exists in original game assets. A final integer guard
remains as a safety net. Nonfinite samples are sanitized and counted.

When cues stop, recovery completes before returning to the original unchanged
game buffer. With no active cues/history, the existing direct passthrough is
bit-exact. Disabled protection restores the previous mixer, including its
clamping and PCM behavior. Preview boundaries reset gain history; protected
glossary demonstrations fade the cue before limiting, not the game's stream.
All state is fixed and mixer-owned; no allocation, disk I/O, locks or speech
API calls occur in this DSP stage.

## Settings

Under `[Accessibility]` in `pd.ini` (restart after editing):

| Key | Default | Range | Meaning |
| --- | --- | --- | --- |
| `MixProtection` | `1` | 0–1 | Enable the new bus mix/headroom/limiter. Zero restores legacy mixing; new bus gains/headroom are then ignored. |
| `WorldCueVolume` | `1.0` | 0–2 | Multiplier for the world-cue bus, in addition to existing per-feature controls. |
| `FeedbackCueVolume` | `1.0` | 0–2 | Multiplier for centered alignment and confirmation cues. |
| `GameAudioHeadroomDb` | `3.0` | 0–12 | Original-game attenuation while cues/hold are active. Zero leaves game gain unchanged. |

The limiter ceiling/timings are fixed implementation parameters, not additional
INI keys. No shortcut or menu item is added. Settings are validated; malformed
nonfinite floating-point values fall back to defaults.

## Diagnostics and verification

`lifecycle/audio_mix_start_state` records effective settings, timing and
stereo-link/no-lookahead policy. Shift+F2 adds `incident/audio_mix`, including
processed/limited frame totals, nonfinite input count, peak mixed signal before
limiting, output peak and minimum limiter gain. Values are normalized (1.0 is
full scale). Meters are cumulative for this process, not per-capture or measured
perceptual loudness. Counters are unsigned 32-bit and may wrap in very long
sessions. Meter publication uses fixed atomic mailboxes once per audio buffer;
formatting and logging occur only outside DSP. Ordinary frame/performance
diagnostics remain available for gameplay-overhead comparison.

`accessibility_mix_test.c` exercises quiet unity behavior, independent buses,
linked limiting under sustained overload, output bounds, release/passthrough,
headroom, invalid inputs/settings, sample/block partition invariance and the
production mixer with crowded cues over loud game audio. The legacy 93-cue
PCM fingerprint remains unchanged with protection off. The HRTF mixer smoke
also checks protected output and retained source direction with no additional
SDK allocation. All 36 regression tests (diagnostics off/on), the expanded HRTF
checks and the Windows x64 `ntsc-final` RelWithDebInfo build passed on
2026-11-04. A pure DSP stress loop processed one second of stereo samples in
approximately 0.33 ms wall time on this machine. Synthetic timing is not a
gameplay-FPS measurement. Full performance diagnostics and the current 3D
backend remain enabled; no game was launched or player session log overwritten.

Owner listening acceptance is pending. Test crowded combat with the cane and
several scanners, close/distant cue balance, lock-on/weapon confirmations,
glossary playback, pause/resume and quiet gameplay after cues stop. Listen for
distortion, conspicuous background pumping, cues becoming too quiet, or loss
of directional clarity. Capture Shift+F2 if the mix is confusing.

Rollback: set `MixProtection=0` and restart. No original-game hooks or assets
are changed; implementation lives in the accessibility core/port modules.
