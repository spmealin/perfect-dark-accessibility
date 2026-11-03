# Accessibility binaural experiment

## Scope and controls

Steam Audio 4.8.1 renders 62 fixed voices through its built-in RMS-normalized
HRTF: ten enemies, pickup and interactable/movable-object lanes, three doors,
three friendly/neutral people, ten cane slots, one drop edge, ten R-Tracker
contacts, one combat-radar contact, one King of the Hill guide, four player
markers, eleven mission landmarks, four rockets, one moving laser and one
threat alert. Visibility,
LOS, distance attenuation, voice budgets and scheduling remain authoritative.
Original game sound, speech,
range-target presence, glossary previews and exact lock-on are unchanged.
Manual precision guidance retains its existing exaggerated stereo mapping.

Combat-radar contacts retain the world position from their queued native-radar
observation and transform it against the observer when played. Radar launch,
empty and unavailable confirmations remain centered. R-Tracker targets refresh
from existing observations and retain their round-robin scheduler. Hill cues use
the existing hill center and radar/LOS rules. All use the existing 3D option;
there are no new keys, collision queries or runtime SDK allocations. IR tracking
now shares those ten 3D voices, using its existing faster pattern and native
highlight/viewport rules. Switching tracker source stops and restarts those
voices without allocating. Each new family has production-mixer HRTF tests;
listening acceptance remains pending.

Player-marker drones and their ID chirps share each marker's actual world
source. Mission landmarks (including vulnerable targets and pillars) have their
own HRTF voices at existing authored positions. Marker deletion confirmations
remain centered, because the marker is gone. Existing ID cadence, mission-state
suppression, LOS, volume, range and placement rules are unchanged. Source
capture records identify marker versus landmark lanes. Mixer tests cover both
families and centered removal; owner listening acceptance remains pending.

Rockets, moving-laser sweeps and threat-detector alerts now use independent
3D voices. Rockets refresh at the existing observed projectile positions;
lasers retain the actual interpolated beam point, including height; threat
alerts use the detected object's position at emission. Their selection, range,
hostility, visibility and original patterns remain unchanged. Projectile and
one-shot threat histories reset on replacement. No new collision queries or
runtime SDK allocations are introduced. Production-mixer tests cover all three,
including moving a laser source left-to-right; listening acceptance is pending.

`ACCESSIBILITY_BINAURAL_EXPERIMENT` defaults OFF in CMake. The local test build
has it ON. In such builds `Accessibility.BinauralAudio=1` is the startup default;
use Options → Extended → Audio → `3D Accessibility Audio` for a live A/B
comparison. The native checkbox uses existing fonts, speech, layout and Back
conventions and saves its choice through the same INI key. It is disabled if
the backend is unavailable. The new English label follows existing extended
option literals and should join their string table when those port settings
receive localization. Missing or incompatible adjacent `phonon.dll`
falls back to legacy stereo. No Steam client, GPU simulation, new raycasting,
scene baking or Internet access is required to play after building.

## Build and dependency

Configure/build through MinGW64:

```sh
cmake -G"Unix Makefiles" -Bbuild -DACCESSIBILITY_BINAURAL_EXPERIMENT=ON .
cmake --build build -j4 -- -O
cmake --build build --target accessibility_spatial_checks -j4 -- -O
```

The option downloads Valve's official 181 MB SDK archive if its headers are
not already available under `build/deps/sdk/steamaudio`; archive SHA-256 is
`4a0aa5ec1176f38f0b0993a37c2259d9e86f27e22d5e24f83ec4c3cb9a1d5449`.
The runtime DLL is copied beside the executable on build. This explicitly
requires ignored build-directory dependency files and third-party generated
output, but copies no game/ROM assets. Nothing from the SDK is committed.
The SDK public header and license links are documented in third-party notices.
Public `pd_zip` packaging deliberately fails while the experiment is enabled;
do not distribute this prototype without completing dependency/license packaging.

## Implementation and lifetime

- `port/src/accessibility/accessibility_spatial.c` dynamically loads the C API
  from the executable directory, creates one context/HRTF plus 62 effects,
  and warms all effects before gameplay, even with the saved setting off.
  Live toggling only changes routing/reset state and never creates an SDK
  context or allocates. Initialization failure tears down
  partially created resources. SSE2 is used as the conservative x64 baseline.
- A 128-frame 48 kHz block adapter runs in the existing tone mixer. Steam Audio
  rejects this port's 22,020 Hz rate, so only cue streams are linearly resampled
  to 48 kHz and back. Game output/device rate is unchanged. Known block and
  resampling buffering is approximately 2.733 ms; this excludes HRTF filter
  delay and the existing SDL/device queue. The linear resampler is a prototype,
  not a claim of production-quality arbitrary-bandwidth sample conversion.
- Mono sources enter HRTF before stereo summation. Other families retain their
  original PCM path. Source positions are transformed into observer-relative
  right/up/back coordinates; looking up/down changes elevation. Roll and head
  tracking are not modeled. At a degenerate vertical look basis, stereo is used.
- Cane path points carry actual floor height and terminal barrier position into
  the same interpolation fractions as the contour; corridor cues follow their
  existing all-left-then-all-right stations. Drop-edge sources follow the same
  moving edge point, now retaining its floor height. Fixed cane contacts and
  hoverbike blocked-movement cues use their existing source. No path or collision
  decision changes. The F7 source is at the character's body center.
- SDK effects, buffers and resampler history are fixed, with no DSP-time
  allocation observed in the tests. Histories reset on slot restart, routing
  changes, pickup replacement/stop and preview boundaries; inactive tails drain
  or are discarded on complete suspension. Existing ownership gates still
  prevent gameplay from disturbing a glossary preview.
- Startup creates resources on the main thread. Mixing remains in the existing
  audio production path. `audioShutdown` pauses/closes the SDL device before
  releasing all effects, HRTF and context and unloading the DLL. No audio worker,
  asynchronous shutdown or second output device is introduced.

## Diagnostics and collected evidence

`spatial_audio/init` reports ready/fallback status, initialization step/error,
rates, block buffering and allocation count. Once per second, `window` reports
processed blocks, total/max DSP microseconds, allocations after warm-up and
current queued SDL frames/milliseconds. Timing excludes ordinary mixer cost;
queue snapshots are not end-to-end latency measurements. Logging stays local.

Shift+F2 also writes capture-only `incident/spatial_state`, `spatial_source`
and `spatial_point` records. Fixed main-thread snapshots retain each slot's last
published world points, listener camera/look, publication age and initial
camera-relative vector. Captures add the backend's current normalized direction,
the points transformed against the current observer, azimuth/elevation, and cane
point frequency/volume/pattern. `requested` and `spatial_valid` are command state,
not proof that a short beep is currently audible; stopped slots can retain old
snapshots, and ages must be checked. Backend axes are sampled atomically but are
not a transactional PCM-frame snapshot. Path directions are captured at phrase
publication, not continuously reoriented as the listener moves.

`incident/door_audio_anchor` links each assigned door voice to its prop identity,
model, current prop position and fixed closed-doorway center. `door_audio_panel`
adds each bounded sibling's closed center and opening fraction. Paired panels
share the averaged anchor. These records distinguish visual panel movement from
the chosen navigation anchor. No new key or INI flag is required; normal
accessibility logging must be enabled. Detailed formatting/I/O occurs only on
capture, never in DSP. Snapshots use fixed storage and perform no raycasts or
SDK allocations. Existing visibility, positions, sounds and scheduling are
unchanged by the instrumentation.

Verification collected without launching Perfect Dark or replacing player logs:

- All 34 regression tests pass with the experimental option OFF, including
  the unchanged 93-cue PCM fingerprint and tone-command ownership contract.
- Real SDK smoke checks verify left/right energy, distinct above/below filters,
  29 simultaneously active voices, no allocations after warm-up, balanced
  shutdown and idempotent cleanup. One synthetic run rendered one second of
  29 continuously active voices in about 39 ms wall time (33 ms in DSP);
  4,510 tracked startup allocations were matched by 4,510 frees. This is not
  a gameplay FPS or perceptual-localization result.
- Production mixer smoke checks verify native right/left world orientation,
  every routed family, cane-path direction changes, live A/B switching with no
  allocations, pickup stopping and preview isolation. The real Extended Audio
  item/handler is checked for presence, default-on, get/set and unavailable state.
- Temporarily withholding the DLL confirmed clean stereo fallback, then the
  original DLL was restored. No game executable was launched.
- The current Windows x64 `ntsc-final` RelWithDebInfo build keeps full performance
  diagnostics ON for the ongoing graphics investigation. VSync is OFF with a
  60 FPS software limit. These diagnostic/personal settings are not new defaults.
- The expanded 62-effect synthetic workload rendered one second with every
  voice continuously active in approximately 81 ms wall time (69 ms DSP), with
  5,302 startup allocations balanced by 5,302 frees and zero after warm-up.
  Actual cues are sparse; this is an all-voices stress benchmark, not gameplay
  frame timing or listening acceptance. Mixer checks exercise each added family,
  centered non-world confirmations, source changes, stop, previews and fallback.

## Owner listening tests and limitations

Use stereo headphones, with OS/headset virtual surround disabled for comparison.
First compare an enemy on the left and right, then above/below while rotating
and changing vertical look without holding aim. Verify distant and close enemy
rhythms, exact lock-on, death/removal and transitions into held aim. For pickups,
check the three chirps, moving their source across the viewport, and silence
off-screen/behind a wall. Try pause, radial menus and the audio glossary, then
several level transitions. Capture Shift+F2 and report confusing directions,
unexpected volume, latency or choppiness; include whether held aim was active.

Generic HRTFs differ between listeners. Pure pickup tones may provide weak
elevation cues despite correct filtering. Existing pitch-based elevation is
retained for comparison, and a richer cue may be a later experiment. Pickup
direction is captured at each short three-chirp event; enemy direction refreshes
with normal observations. The owner reported the initial enemy/pickup prototype
"working really well" on 2026-11-02. This is listening acceptance of that first
scope, not the newly added radar, marker, hazard and IR routes. Later captures
in sessions `1793659964` and `1793664599` informed timing/graphics diagnosis,
not acceptance of these new routes. The agent has not launched the game for
this extension. Persistent graphics-driver instability is not claimed fixed.

Rollback: turn the menu option off live, or rebuild with
`-DACCESSIBILITY_BINAURAL_EXPERIMENT=OFF`. To remove the implementation through
Git, revert the spatial-extension commits newest-first, followed by the base
`28a5c6e9c` spatial-audio commit. Preserve unrelated targeting/scheduler fixes
and commits `adf434db7` (ranking formatting) and `9afbf0927` (pickup viewport).
