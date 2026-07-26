# Player-authored audible markers specification

## Status

**Engineering implementation present; runtime and blind-user acceptance
pending.** This document specifies a four-slot audible marker system for
blind-player exploration and orientation. A successful build proves
integration only, not accessibility validation.

The first implementation targets single-player missions and one-local-player
Combat Simulator sessions. It must preserve existing gameplay behavior and use
the existing accessibility oscillator and spatial-audio infrastructure.

## Purpose and scope

Players need a way to create their own recognizable landmarks in areas that
have few unique sounds. A marker lets a player recognize that they have
returned to a location, deliberately mark a junction or room, and orient toward
a previously visited point.

The system provides four independent in-memory marker slots:

| Slot | Place or move | Remove | Identity |
| --- | --- | --- | --- |
| 1 | F9 | Shift+F9 | One high chirp |
| 2 | F10 | Shift+F10 | Two high chirps |
| 3 | F11 | Shift+F11 | Three high chirps |
| 4 | F12 | Shift+F12 | Four high chirps |

Pressing a slot key stores the player's current world position. Pressing it
again moves that slot to the new position. Shift plus the same key removes the
marker. Active markers sound whenever the player is within their audible
range, including when more than one marker is nearby.

This is a player-authored landmark system. It does not identify mission goals,
choose routes, reveal unexplored locations, label geometry, or provide semantic
information that the player did not supply. Goal guidance is a separate
roadmap item.

## Design goals

- Make revisited locations and self-created landmarks recognizable.
- Make all four slots distinguishable without speech.
- Preserve useful direction and distance cues in stereo.
- Keep marker state stable across temporary menus and interruptions.
- Respond immediately without allocating memory or blocking the game loop.
- Coexist with combat, scanners, the virtual cane, speech, and native audio.
- Keep engine hooks small and policy inside the accessibility modules.

## Non-goals

- Saving markers in profiles, save files, or `pd.ini`.
- Carrying markers between stages or mission attempts.
- Pathfinding, breadcrumb generation, or automatic waypoint placement.
- Naming markers or announcing them through a screen reader.
- Multiplayer audio composition beyond one local player.
- Replacing the virtual cane or object scanners.

## Confirmed input availability

The current port input definitions name F9 as SDL scancode 66. F10 through F12
are the consecutive SDL scancodes 67 through 69 but do not yet have named
constants in `port/include/input.h`. The current source has no gameplay or
accessibility checks for F9 through F12. These keys are therefore available in
the current default control scheme.

The implementation must add explicit `VK_F10`, `VK_F11`, and `VK_F12`
constants rather than embedding numeric scancodes. It must continue to use
edge-triggered input so holding a key cannot repeatedly place or delete a
marker.

These are raw accessibility shortcuts and do not consume the underlying input.
A player-created custom binding could therefore assign the same key to another
action. That limitation must be documented until accessibility shortcuts are
integrated with the configurable binding system.

## Input behavior

When gameplay is eligible:

- F9, F10, F11, or F12 places the corresponding inactive marker.
- Pressing the same key for an active slot moves it to the player's current
  position and replaces its previous coordinates.
- Holding either Shift key while pressing the slot key removes that slot.
- Removing an empty slot plays no sound and records an ignored command in the
  accessibility log.
- Alt- or Control-modified function-key presses are ignored to avoid accidental
  activation through operating-system or debugging shortcuts.
- A key held across a gameplay or menu transition cannot trigger when gameplay
  resumes; a new key-down edge is required.

The position must be taken from the active accessibility observer. In ordinary
gameplay this is Joanna's camera position; while controlling a CamSpy it is the
CamSpy camera position. Placement, removal, spatialization, range, and
line-of-sight tests therefore switch together when the active perspective
changes.

Input is temporarily inactive during menus, pause screens, dialogs, cutscenes,
death states, text entry, stage transitions, and unsupported multiplayer
configurations. Remote-camera control is eligible gameplay. Temporary
suppression preserves all marker slots.

### Placement and removal feedback

Placement and movement start the marker's audible signature immediately. At
the placement point it will be centered and at its near-field volume, which
provides confirmation without adding speech.

Removal plays a short centered confirmation:

1. The slot's one-to-four 800 Hz identity chirps.
2. One 400 Hz deletion chirp immediately after the identity pattern.

Each confirmation chirp is 35 ms with a 5 ms attack and release. Adjacent
chirps have a 75 ms silent gap. Removal confirmation uses the marker identity
scheduler at center and must not leave a continuous voice active.

## Marker state

The accessibility core owns a fixed four-element array. A slot contains only
stable copied values:

```c
struct AccessibilityMarker {
    bool active;
    struct coord position;
    RoomNum rooms[2];
    s32 stage_id;
    u32 placed_tick;
    u32 revision;
};
```

The first room entry copies the active observer camera room and the second is
the engine's `-1` terminator. The implementation must not retain pointers to a
player, prop, room list, stage object, or temporary collision result.

`revision` increments whenever a slot is placed, moved, or removed. It makes
commands and diagnostic snapshots unambiguous without pointer ownership; the
audio boundary uses its own atomic sequence counters.

Markers are:

- initialized empty at accessibility startup;
- preserved through pause, menus, computers, cutscenes, ordinary death
  suppression, and temporary loss of gameplay scope;
- cleared silently on stage teardown, stage restart, accessibility shutdown,
  or when the marker feature is disabled;
- never serialized into the game profile, save data, or configuration file.

The first implementation should retain markers while the player is dead and
resume them if gameplay resumes within the same mission attempt. A full stage
restart clears them. If the engine cannot reliably distinguish those cases,
stage lifecycle boundaries take precedence: suppress at death and clear at the
existing stage teardown hook.

## Audible signature

Each active marker uses a continuous two-oscillator base plus a periodic
slot-identity chirp.

### Base tone

The oscillators use opposed triangular frequency sweeps:

- Oscillator A: 300 Hz to 600 Hz to 300 Hz.
- Oscillator B: 600 Hz to 300 Hz to 600 Hz.
- One complete out-and-back cycle lasts 2,000 ms.
- Both oscillators begin at opposite ends of the sweep and remain
  phase-locked.

The two samples are mixed at equal gain and normalized before the marker's
distance and master gains are applied. This prevents two oscillators from
doubling the lane amplitude or clipping. Each oscillator uses the envelope and
phase-continuity practices of the existing accessibility tone system; moving a
marker must not reset audio state in a way that creates a click.

Distance is represented by volume and spatial position, not by changing this
base pitch pattern. This keeps every marker recognizable and leaves a
distance-pitch experiment for later blind-user testing.

### Identity chirps

An 800 Hz chirp repeats the slot number:

- Slot 1: one chirp.
- Slot 2: two chirps.
- Slot 3: three chirps.
- Slot 4: four chirps.

Each chirp is 35 ms with a 5 ms attack and release, followed by a 75 ms gap.
Each slot's pattern begins once every 2,000 ms.

A small marker identity scheduler guarantees at least 500 ms between pattern
starts, including immediately after placing a marker. Because the longest
four-chirp pattern lasts 365 ms, patterns cannot begin at the same instant or
overlap under normal scheduling. Each completed slot pattern becomes due again
after 2,000 ms.

The base tones for multiple markers may sound simultaneously. Their identity
patterns must remain staggered so the player can tell which marker is which.

## Audio voices and real-time constraints

Reserve four dedicated marker mixer voices at audio initialization, one per
slot. Do not borrow combat, virtual-cane, tracker, hazard, scanner, or general
earcon voices.

Each marker voice internally renders:

- oscillator A;
- oscillator B;
- its current identity-chirp envelope;
- the slot's spatial pan, distance gain, and master gain.

All buffers, voice state, scheduler state, and command storage are
preallocated. Gameplay code publishes small state snapshots to the audio side
using the same lock-free or bounded synchronization approach as the existing
tone subsystem. The audio callback must not allocate, free, log, access mutable
game objects, acquire a game-thread lock, enumerate rooms, or perform collision
queries.

When no marker is active and audible, marker synthesis should reduce to a
constant-time inactive check. At the maximum of four audible markers, the
additional work is eight base oscillators plus at most one active identity
pattern. This should be measured with the existing audio and frame diagnostics
rather than assumed harmless.

## Spatialization and range

Both base tones and identity chirps use the same marker position and pan.
Spatial values are calculated on the gameplay thread from the active
accessibility observer's current camera position and orientation, then
published to the audio voices.

The first implementation uses full three-dimensional Euclidean distance:

```text
d = sqrt(dx² + dy² + dz²)
```

This reduces false proximity between vertically separated floors. Marker room
IDs should be supplied to the existing engine spatialization helper when it
accepts stable copied rooms. If those rooms later become invalid, fall back to
geometric bearing and distance instead of dereferencing stale state.

Every active marker requires a clear line-of-sight ray from the active
observer's camera position to the stored marker position. Test against
background collision and doors using wall, block-sight, and block-shoot
geometry flags. A wall or closed door therefore silences the marker. The
marker does not need to be inside the viewport or in front of the observer:
visibility flags, projection bounds, and facing direction must not participate
in eligibility.

Run line-of-sight tests on the gameplay thread only. The stored marker room and
current observer room seed the engine's portal-aware collision query. A failed,
invalid, or indeterminate query is treated as blocked. Re-evaluate each
in-range marker once per logical gameplay tick so opening a door, moving around
a corner, or changing to or from CamSpy updates audibility promptly. Markers
outside the bounded range skip the collision query.

Default range behavior:

- Audible radius: 1,200 world units.
- Full gain through the inner 100 world units.
- Smooth squared fade between the inner radius and audible radius.
- Enter the audible set at 1,200 units.
- Leave it at 1,275 units to prevent boundary chatter.

For distance `d`, inner radius `N`, and outer radius `R`, the base distance gain
is:

```text
1                                      when d <= N
((R - d) / (R - N))²                   when N < d < R
0                                      when d >= R
```

While hysteresis keeps a voice assigned between `R` and `R + 75`, its rendered
gain remains zero outside `R`. The configured master volume is applied after
distance gain and before the mixer's final safety clamp.

The existing stereo pan convention must be verified with a marker placed on
the player's left and right. No additional front/rear modulation is included
in version one because it could obscure the requested crossing-sweep timbre.
Stereo-only front/rear ambiguity is a known limitation and should be evaluated
in acceptance testing.

All active markers inside range are rendered. Version one must not select only
the nearest marker. If four simultaneous bases prove overwhelming, density
management can be designed from test evidence without changing marker storage
or input semantics.

## Configuration

Add these keys to the accessibility section of `pd.ini`:

```ini
AudibleMarkers=1
MarkerRange=1200
MarkerVolume=1.0
```

- `AudibleMarkers` enables input, stored marker state, and marker audio.
- `MarkerRange` is the outer audible radius in world units.
- `MarkerVolume` is a linear master multiplier for all marker voices.

Acceptance builds default implemented accessibility features on, but all four
slots begin empty. Recommended validation clamps are:

- `MarkerRange`: 100 through 10000.
- `MarkerVolume`: 0.0 through 4.0.

Invalid, non-finite, or out-of-range values are replaced or clamped using the
configuration conventions already used by other accessibility audio settings,
and the effective value is logged at startup. Sweep frequencies, sweep period,
chirp frequency, chirp timing, and hysteresis remain named compile-time
constants for the first prototype. Promote them to settings only if acceptance
testing shows a real need.

Disabling the feature at runtime clears all markers silently. Re-enabling it
does not restore them.

## Gameplay scope and coexistence

Marker audio is eligible only when:

- accessibility and audible markers are enabled;
- a supported one-local-player gameplay mode is active;
- Joanna has a valid player prop and observer state;
- the stage is not pausing, stopping, or transitioning;
- no menu, dialog, computer interface, cutscene, or death screen owns the
  presentation;
- the audio subsystem is initialized and not shutting down.

Temporary ineligibility mutes voices and resets their due state without
discarding marker locations. On resumption, identity patterns re-enter the
normal staggered scheduler rather than allowing all due patterns to fire at
once.

Marker mixing must preserve headroom with combat target tones, enemy beacons,
object scanners, the virtual cane, AR/IR/X-ray feedback, native dialogue, and
screen-reader output. It must use the tone system's existing final clamp or
saturation behavior. It must not reduce native audio volume or suppress other
accessibility features.

## Implementation boundaries

Keep state and policy in a new platform-independent marker module:

- `src/accessibility/accessibility_marker.c`
- corresponding public header in `src/include/accessibility/`

Responsibilities:

- fixed slot state;
- eligible input commands supplied by the port coordinator;
- stage identity and lifecycle reset;
- active-observer line of sight, distance, bearing, range hysteresis, and
  published voice snapshots;
- diagnostic events and summary state.

Extend the port-facing oscillator backend:

- `port/src/accessibility/accessibility_tone.c`
- `port/include/accessibility/accessibility_tone.h`

Responsibilities:

- four preallocated marker voices;
- two-oscillator sweep synthesis;
- chirp envelopes and stagger scheduler;
- bounded game-thread-to-audio-thread updates;
- final per-voice pan and gain application.

Expected established-file changes:

- `port/include/input.h`: name F10 through F12 scancodes.
- the existing accessibility coordinator or `port/src/pdmain.c`: one marker
  tick in the established accessibility update location and lifecycle reset
  calls.
- accessibility initialization/configuration code: register settings and
  initialize/shut down the subsystem.
- build lists only if the repository does not automatically collect the new
  source file.

Do not put input policy or persistent slot state in `pdmain.c`, and do not call
the tone backend directly from unrelated gameplay systems. Update the upstream
hook ledger in `ACCESSIBILITY_ARCHITECTURE.md` for every established source
file that gains or loses a hook.

## Update order

Once per logical gameplay tick:

1. Determine supported gameplay scope and obtain the active observer, including
   CamSpy or another supported remote camera.
2. Detect fresh F9 through F12 edges and modifier state.
3. Apply at most one command per slot from that tick.
4. Copy the player's position, room list, stage identity, and tick for a place
   or move.
5. Calculate each active marker's distance and bearing.
6. Ray-test each in-range marker from the active observer against background
   and door collision.
7. Apply audible-range hysteresis, line-of-sight eligibility, and master gain.
8. Publish a four-slot immutable audio snapshot.
9. Log only state changes or rate-limited diagnostics.

If several marker keys are pressed on the same tick, process them in slot order.
Each affected slot receives the same current player position. The audio
scheduler still serializes their identity-pattern starts.

## Logging and diagnostics

Use the accessibility session log, not the general `pd.log`. At startup log:

- feature enabled state;
- effective range and volume;
- key mapping;
- supported-player policy;
- oscillator and chirp constants;
- reserved voice count.

For every command log:

- slot;
- action: place, move, remove, or ignored;
- old and new coordinates;
- copied rooms;
- stage identifier;
- game tick and marker revision;
- modifier state;
- ignore or suppression reason.

Log scope transitions and marker resets once per transition. Add a rate-limited
summary, no faster than once per second while markers exist, containing:

- active-slot mask;
- audible-slot mask;
- player coordinates and rooms;
- per-slot distance, pan, gain, and revision;
- advanced-diagnostic marker voice count and collision-query duration;
- marker audio callback high-water timing when advanced profiling is enabled.

Never log per sample or once per frame merely to repeat unchanged values. Logs
may include complete feature-relevant local state under this project's
accessibility logging policy, but must not include ROM contents or unrelated
secrets.

## Failure behavior

- If tone initialization fails, gameplay continues and marker input may log
  that audio is unavailable.
- If the player or stage state is invalid, mute all voices for that tick without
  dereferencing it.
- Atomic slot fields are sampled into one audio-buffer snapshot; a newer
  gameplay update replaces the prior values without blocking.
- On shutdown, silence marker voices before releasing the shared tone backend.
- Invalid configuration cannot create negative ranges, non-finite gains, or
  oscillator frequencies outside the backend's safe limits.

## Verification plan

### Static and build verification

- Confirm F9 through F12 have no default gameplay binding at implementation
  time.
- Confirm F10 through F12 use named constants and no magic scancodes.
- Confirm the marker module owns no live engine pointers.
- Confirm four voices are preallocated and the audio callback allocates
  nothing.
- Build the default `ntsc-final` target in the required MinGW64 environment.
- Check warnings, configuration documentation, the architecture hook ledger,
  and repository status.

### Functional tests

1. Place slot 1, walk away, and return from several bearings. Verify its
   continuous base is positioned at the stored point and one chirp identifies
   it.
2. Repeat for slots 2 through 4 and distinguish all identity patterns without
   speech.
3. Move an active slot. Verify the old position falls silent and the new
   position starts immediately.
4. Remove each slot with Shift plus its key. Verify the centered slot identity
   plus deletion chirp and no remaining spatial audio.
5. Press Shift plus an empty slot, hold a key, and use Alt/Control combinations.
   Verify no repeated or accidental placements.
6. Place all four markers near one location. Verify all bases remain audible,
   identity patterns are staggered, and no two patterns start simultaneously.
7. Cross the configured range boundary repeatedly. Verify smooth gain and no
   rapid voice chatter.
8. Place a marker across a wall, behind a closed door, and on another floor.
   Verify walls and closed doors silence it, opening or rounding the obstruction
   restores it, off-screen markers remain audible, and full 3D distance
   prevents misleading proximity.
9. Enter and leave pause, menus, computers, dialogs, cutscenes, and ordinary
   death states. Verify sound is suppressed, state survives, and patterns
   resume staggered.
10. Restart or leave the stage. Verify all slots clear and cannot sound in the
    next attempt or stage.
11. Enter CamSpy control. Verify placement, removal, spatialization, range, and
    line of sight use the CamSpy perspective, then switch seamlessly back to
    Joanna's perspective without losing markers.
12. Mix four markers with enemies, target tones, scanners, the virtual cane,
    native effects, music, and screen-reader speech. Check intelligibility and
    clipping.

### Performance and stability tests

- Run at least 20 minutes with four nearby markers, the fast virtual cane,
  enemy scanning, and combat targeting active.
- Exercise repeated place, move, remove, pause, computer, CamSpy, death, and
  stage-transition cycles.
- Compare frame-time and audio-callback diagnostics with zero, one, and four
  audible markers.
- Confirm no growing allocations, voice count, command backlog, stale-update
  count, or accessibility log rate.
- Repeat the project's graphics-slowdown capture procedure if the 30 FPS gate
  trips or audio/video choppiness returns.

### Accessibility acceptance

A blind or screen-reader-dependent tester should be able to:

- deliberately mark and later recognize a junction or room;
- distinguish which of four landmarks is nearby;
- tell the left/right direction and useful relative distance to a marker;
- move and remove markers without sighted confirmation;
- retain orientation through ordinary menus and interactions;
- notice no unacceptable loss of combat information or performance.

Record successes, confusion, masking, front/rear ambiguity, preferred range,
preferred volume, and whether four simultaneous bases are manageable. Do not
call the feature complete until the roadmap acceptance criteria and independent
playtest evidence are updated.

## Provisional choices requiring acceptance feedback

The following are implementation-ready defaults, not immutable decisions:

- 1,200-unit audible radius and 1.0 master volume.
- Two-second base sweep and identity interval.
- Removal confirmation pattern.
- Marker retention through ordinary death suppression.
- No front/rear modulation.
- Rendering every marker inside range instead of density limiting.

If testing changes these values, preserve the four-slot input and lifecycle
contract unless the test evidence shows that contract itself is unusable.

## Documentation required with implementation

The implementation change must update:

- `ACCESSIBILITY.md` with the player-facing purpose and shortcuts;
- `ACCESSIBILITY_ARCHITECTURE.md` with module ownership, tone voices, and the
  established-file hook ledger;
- `ACCESSIBILITY_ROADMAP.md` with milestone placement and acceptance status;
- `ACCESSIBILITY_TESTING.md` with the marker test matrix and session evidence;
- `pd.ini` documentation with all marker settings and defaults.

The handoff must identify the exact build tested, session log ID, effective
configuration, established files touched, runtime evidence, known limitations,
and rollback path.
