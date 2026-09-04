# Accessibility Milestone 11: Virtual Cane Prototype Plan

Status: **Implemented; iterative project-owner acceptance passed, broader milestone validation pending**

Current-status note: this file preserves the original implementation handoff.
Later project-owner testing accepted the core sweep, distance pitch, terrain
transition sweeps, increased/configurable level, and longer reach. The original
request for no mode-change confirmation was later superseded by accepted F4
earcons for Off, Slow, and Fast. A 2026-08-06 extension also superseded the
original drop-off non-goal: the implementation now refines large floor drops
to a spatial edge cue and compares standing with full-squat collision to
identify crouch-passable openings. Those extensions await project-owner
runtime acceptance. Follow-up diagnostics showed rising stairs could mask a
co-located under-stair opening; the current implementation tests squat
clearance within one player radius of that terrain and uses a distinct
descending double chirp. A 2026-08-30 Deep Sea diagnostic also showed an
untraversable rise immediately before a wall incorrectly winning terrain-cue
precedence. Positive terrain now keeps the rising contour only after the live
player-envelope traversal check succeeds; otherwise the cane reports the rise
as an ordinary barrier. Follow-up captures showed the same ambiguity for short
declines ending at walls. Non-drop terrain in either direction now requires at
least two live player diameters of usable runway before a detected barrier;
shorter pockets report the terminal barrier instead. These refinements await
project-owner runtime acceptance. A 2026-09-04 capture found lower floor only
two units beyond a wall collision, causing an inaccessible drop to win by
distance. Refined drops now cede to a barrier within one live player collision
radius of their edge, while open ledges and wider gaps retain drop priority.
See the root `ACCESSIBILITY.md`, `ACCESSIBILITY_ROADMAP.md`,
and `ACCESSIBILITY_TESTING.md` for current behavior and status.

An independent 2026-09-04 compass extension uses Shift+F4 to cycle Off,
speech-plus-sonification, and sonification-only modes. Exact cardinal crossings
use one through four dedicated centered clicks clockwise from North, with
optional simultaneous speech. This extension awaits project-owner runtime
acceptance and does not change the cane sweep schedule.

Planning baseline: `accessibility` branch at `2ca16241f`
Prepared: 2026-07-21

## Purpose and milestone relationship

This document specifies a self-contained virtual-cane prototype for handoff to a coding agent. It is a prioritized slice of roadmap Milestone 11, **Navigation and orientation assistance**. Completing this plan does not complete the rest of Milestone 11: route guidance, landmarks, recovery from disorientation, and broader navigation settings remain separate work.

The virtual cane sweeps nine collision probes from left to right in front of the current player. A short spatialized tone is emitted at the impact point of every probe that encounters a movement-blocking environmental surface. The resulting pattern should let a blind player perceive broad geometry such as flat and angled walls, doorways, pillars, and intermittent obstacles while continuing to move and turn.

This is an orientation cue, not an automated movement system. It must expose geometry already available through the game's movement collision system and must not alter movement, collision, aim, enemies, or gameplay state.

## Approved player experience

### Modes and input

The feature has three modes:

| Value | Mode | Complete sweep cycle |
| --- | --- | --- |
| `0` | Off | No probes or cane tones |
| `1` | Slow | 2.0 seconds, including the end pause |
| `2` | Fast | 1.0 second, including the end pause |

Pressing `F4` cycles `Off -> Slow -> Fast -> Off`.

There is deliberately no speech, earcon, or other mode-change confirmation. The presence and rate of the cane sweep itself are the feedback. An unobstructed sweep is silent. The setting defaults to `Slow` for blind-user acceptance builds, consistent with the repository rule that implemented accessibility features be enabled by default for acceptance testing.

The key handler must ignore `F4` while either Alt key is held so that `Alt+F4` cannot also change the cane mode. It must also ignore the command while menus, pause screens, cutscenes, or other non-gameplay contexts own input. `F4` appears unused in the current default bindings, but the coding agent must repeat that search immediately before implementation and record any conflict found.

A permanent controller binding and an options-menu control are outside this prototype. They belong with the later input-remapping and settings work.

### Probe angles and order

Each sweep samples these camera-relative horizontal angles in this exact order:

```text
-60, -45, -30, -15, 0, +15, +30, +45, +60 degrees
```

The observed sound must move left to right. Do not assume the sign convention from the mathematical rotation alone: verify in the game that `-60` pans left and `+60` pans right, and reverse the rotation signs if the engine coordinate system requires it.

Each probe originally had a maximum horizontal distance of 600 world units. Blind-user acceptance testing increased the default reach by 50 percent to 900 world units. Reach and attenuation are now bounded `pd.ini` settings, so later testing can tune them without recompilation. A miss produces silence but still consumes its position in the sweep. There is no distance-to-pitch mapping in this prototype.

### Timing

Use region-correct game ticks (`TICKS` or the project's equivalent) rather than assuming every build runs at 60 logical ticks per second.

The nominal NTSC-final schedules are:

| Mode | Cycle | Probe offsets from cycle start | End pause |
| --- | --- | --- | --- |
| Slow | 120 ticks / 2.0 s | `0, 11, 23, 34, 45, 56, 68, 79, 90` | 30 ticks / 0.5 s |
| Fast | 60 ticks / 1.0 s | `0, 6, 11, 17, 23, 28, 34, 39, 45` | 15 ticks / 0.25 s |

The alternating 5/6-tick intervals in Fast approximate 94 ms between samples. The cycle restarts at the leftmost probe after its end pause.

Movement and camera rotation must **not** cancel, restart, or freeze a sweep. Immediately before each scheduled probe, read the current player position, current stance bounds, and current camera direction. This keeps the cane live during combat even when the player turns or moves between successive samples.

If a hitch skips more than one scheduled offset, do not issue several collision queries or sounds in one tick. Skip overdue samples or resynchronize to the current cycle phase, increment a diagnostic missed-sample count, and preserve a hard maximum of one cane query per logical tick.

### Sound

Each collision emits a brief procedural oscillator chirp at the spatial impact position:

- Provisional frequency: 330 Hz.
- Provisional duration: 35 ms.
- Provisional envelope: 3 ms attack and 8 ms release.
- Lane/master volume: 0.115 before spatial attenuation (a 15% increase from the prototype's 0.10 gain after blind-user acceptance testing).
- Pan: derived from the actual collision point using the existing spatial-audio calculation.
- Volume: derived from collision distance. The original provisional thresholds were full volume at 75 units or closer, fading through 500 units, and silent at 650 units. The 900-unit acceptance-testing defaults scale them to 112.5, 750, and 975 units respectively; all four distances are configurable and cross-field validated at runtime.

The 330 Hz frequency is intentionally separated from existing accessibility cues near 220 Hz (laser hazard), 440 Hz (doors and combat), 660 Hz and above (fine aiming), 880 Hz (interactables), and 1000 Hz (weapon-function confirmation). All sound constants are tuning values, not semantic contracts. Put them together in the cane implementation rather than scattering literals across hooks.

Because the revised probe stops at 900 units, the attenuation curve's silent threshold must be farther than 900 so a maximum-range hit remains faintly audible. Use the collision X/Z coordinates for direction and distance. For playback, use camera/player ear height as the audio source Y coordinate so crouching or a collision polygon's vertical coordinate does not accidentally encode elevation in this horizontal-only prototype. Preserve both the raw collision position and final audio-source position in diagnostics.

The user requested one dedicated channel for each angle. Implement this as nine preallocated virtual-cane voices in the accessibility procedural mixer, not nine scarce native game sound channels. Ordinarily only one short voice will be audible because samples are scheduled sequentially. Fixed slot ownership prevents one probe from stealing another probe's voice and isolates the cane from beacons, hazards, combat cues, fine aim, and weapon-function feedback.

## Gameplay scope

Run the virtual cane only when all of the following are true:

- Global accessibility is enabled.
- `Accessibility.VirtualCaneMode` is `1` or `2`.
- A valid local current player and player prop exist.
- The player is alive and in ordinary walk movement mode.
- Normal gameplay simulation is advancing (`lvupdate` is positive or its current equivalent).
- No menu, pause screen, cutscene, stage transition, or other non-gameplay context is active.
- The mode is single-player for this prototype.

On scope loss, immediately stop all cane voices and reset the sweep scheduler, but preserve the configured mode. On scope regain, begin a fresh left-to-right sweep at `-60` degrees. A stage change or player-context change must do the same. Turning, walking, crouching, or changing rooms is not a scope loss.

Multiplayer and split-screen behavior are explicitly deferred. Do not accidentally use the wrong player's camera or combine geometry from several player contexts.

## Collision semantics

### What must produce a cane tone

The probe should represent surfaces that currently prevent the player capsule from moving horizontally:

- Background/world collision.
- Movement-blocking doors, including closed or partially open doors when their current collision state blocks the probe.
- Solid objects.
- Path-blocking objects.

Doors remain cane obstacles even when the separate door-beacon feature is enabled. The two systems answer different questions: the beacon identifies an interactable category, while the cane exposes the current physical boundary.

### What must not produce a cane tone

Exclude these from the prototype:

- Characters, enemies, and other players. Combat audio already represents enemies.
- Pickups and non-solid decorative props.
- Floor/drop-off detection.
- Objects hidden behind a nearer blocking surface.
- Semantic route targets, objectives, or landmarks.

The collision query must stop at the nearest qualifying movement blocker. It must not reveal barriers through walls or provide information unavailable through ordinary movement collision.

### Player-sized sweep, not a thin ray

Use the current player's movement cylinder rather than a zero-width line-of-sight ray. This gives the player useful information about whether their body can pass through an opening and naturally reflects current standing or crouched bounds.

Obtain the radius and vertical extents through the existing `playerGetBbox` path. Preserve its current stance handling and step-up allowance. Do not invent a second hard-coded player capsule for accessibility.

The target horizontal position for a sample is:

```text
end = start + rotate(normalize(camera_forward_xz), sample_angle) * 900
end.y = start.y
```

Use the live camera look vector's X/Z components rather than pitch. If the horizontal component is degenerate, record a skipped sample and emit no sound.

### Read-only collision-query adapter

Do not call `bwalkCalculateNewPosition`, even with an apparent non-applying option. That routine mutates movement/collision state and does not expose a clean, accessibility-safe result.

Instead, build a small read-only cane collision adapter using the same collision primitives and ordering as player walking. The coding agent must confirm exact signatures at the implementation baseline, but the intended flow is:

1. Capture the current player start position and rooms.
2. Call `playerGetBbox` for current radius and vertical bounds.
3. Build room traversal for the 900-unit horizontal destination with `func0f065dfc`.
4. Resolve entered destination rooms with `bmoveFindEnteredRoomsByPos`.
5. Run the same long-move cylinder checks used by `bwalkCalculateNewPosition`: first `cdExamCylMove06`, and, if clear, `cdExamCylMove02`.
6. Use `CDTYPE_BG | CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER`; deliberately omit character and player types.
7. Capture the collision result immediately: `cdGetPos`, obstacle prop, geometry flags, and obstacle normal.

The collision subsystem exposes its result through shared global state. Therefore, the complete query and result capture must run on the main game thread at a safe point in the logical tick. Never call it from the audio callback, a worker thread, or concurrently with another collision operation. Do not retain pointers to collision scratch data after the query.

Match the game's current `bondcollisions` behavior. If collision cheats/debug settings reduce normal movement collision to background geometry only, the cane should not contradict that state by reporting disabled object collision. Confirm and document the exact rule used.

Use fixed-size stack room arrays sized consistently with the existing movement code. Do not allocate memory per sample or per sweep.

### Impact-position validation requirement

Before treating `cdGetPos` as an audible surface impact, validate it against controlled walls. Some cylinder collision routines may report a capsule stopping center or an edge-related position rather than the literal polygon impact. Diagnostics must capture start, requested end, raw returned position, normal, and distance.

If the returned point is unsuitable, derive the barrier contact point from existing collision edge/normal data or use a bounded refinement supported by the engine. Do not add an iterative binary search until profiling proves it necessary and safe; that would multiply the most expensive part of the feature.

## Architecture and file ownership

### New core module

Create:

- `src/include/accessibility/accessibility_cane.h`
- `src/accessibility/accessibility_cane.c`

Add the source to `SRC_ACCESSIBILITY` in `CMakeLists.txt`.

This module owns:

- Runtime mode and sweep phase.
- Gameplay-scope checks.
- F4 mode cycling or the semantic command invoked by the input hook.
- Live sample scheduling.
- Camera-relative direction construction.
- Read-only collision adapter.
- Spatial volume/pan request construction.
- Aggregated cane diagnostics.
- Reset and shutdown behavior.

Keep established game-file changes to small calls into this module. Do not put scheduler policy or logging in player movement, collision, or input internals.

Suggested public contract, with naming adjusted to current repository conventions:

```c
void accessibilityCaneTick(void);
void accessibilityCaneCycleMode(void);
void accessibilityCaneReset(const char *reason);
```

If input handling needs a predicate, expose a semantic `accessibilityCaneHandleKey` or `accessibilityCaneCanCycle` function rather than leaking menu/state policy into `pdmain.c`.

### Configuration

Extend `src/accessibility/accessibility.c` and its public header with a clamped integer setting:

```ini
Accessibility.VirtualCaneMode=1
```

Meanings are `0=off`, `1=slow`, and `2=fast`. Invalid values must clamp or fall back safely and be recorded in the session-start log. The effective mode must appear in the existing accessibility startup/configuration diagnostics.

Reset runtime scheduler state during accessibility initialization and shutdown. Disabling global accessibility or setting mode `0` must synchronously enqueue a stop for all cane voices; it must not leave a tone active until its natural envelope finishes.

### Main-loop hook

The expected integration point is `port/src/pdmain.c`, beside the existing accessibility beacon and hazard ticks after `lvTick`, where the current player and logical game state are stable and the query remains on the main thread.

Add a reset call before or during the existing stage-stop path so no stale sweep crosses a stage transition. Verify ordering against the current implementation rather than relying only on this planning baseline.

Document every added or removed established-file hook in the upstream hook ledger in `ACCESSIBILITY_ARCHITECTURE.md`.

### Input hook

Follow the current context-sensitive `F5`/`F6` accessibility-key pattern, while keeping the cane command independent of beacon policy. Add `VK_F4` to the platform key definitions if it is absent. Add or confirm a right-Alt code or an Alt modifier test so both `Left Alt+F4` and `Right Alt+F4` are ignored.

Do not consume F4 in a context where the game or menu already owns it. Log actual mode changes, not every polled key state.

### Procedural mixer extension

Extend the existing files:

- `port/include/accessibility/accessibility_tone.h`
- `port/src/accessibility/accessibility_tone.c`

Add nine fixed cane slots, for example:

```c
#define ACCESSIBILITY_TONE_CANE_SLOT_COUNT 9

void accessibilityTonePlayCaneSlot(
    s32 slot,
    f32 frequency,
    f32 volume,
    f32 pan);

void accessibilityToneStopCane(void);
```

The exact public parameters may be narrowed if the frequency and envelope remain fixed. Each slot needs only preallocated command state shared through the mixer's existing atomic pattern and audio-thread-owned phase/envelope state. A new command sequence for a slot restarts its 35 ms chirp from phase zero. Stopping the cane disables and clears every cane slot.

The audio callback must:

- Perform no heap allocation, file I/O, logging, locking, or collision work.
- Skip cane sample generation cheaply when no slot is active and no command changed.
- Sum cane voices with the existing accessibility mix and clamp only through the established final path.
- Keep cane sequence and mixed-sample counters available for diagnostics.

Do not use `sndStart`, native game sound handles, or nine native sound channels. The procedural mixer is already designed to keep accessibility cues isolated from the game's channel pool.

## Scheduler state machine

The implementation should behave as this state machine:

1. **Disabled:** mode is Off or global accessibility is disabled. No scheduler work and no active voices.
2. **Out of scope:** mode is Slow/Fast, but gameplay scope is invalid. Preserve mode, stop voices, and wait.
3. **Begin sweep:** scope becomes valid or mode changes to Slow/Fast. Set cursor to `-60`, assign a new sweep identifier, and allow the first sample immediately.
4. **Sample:** at the scheduled offset, capture the live pose and bbox, perform one query, optionally play that slot, record the result, and advance the cursor.
5. **End pause:** after `+60`, make no queries until the cycle boundary.
6. **Next sweep:** assign a new sweep identifier and return to `-60` using the current live pose.

Changing Slow to Fast or Fast to Slow starts a new left-to-right sweep immediately. Changing to Off stops all cane voices and discards partial diagnostic aggregation after emitting a reset/mode-change record.

Use wrap-safe tick comparisons. Do not use wall-clock sleeps, audio playback completion, or frame rendering to drive the scheduler.

## Diagnostics and logging

Accessibility logs may include all feature-relevant local state, but must continue to exclude ROM contents, extracted assets, credentials, and unrelated operating-system secrets.

Add these structured events or equivalent fields under current logging conventions:

### `cane/command`

- Input source/key.
- Old and new effective mode.
- Alt modifier state.
- Stage, player index, logical tick, and player pointer/identifier.

Emit only for accepted changes. Rejected `Alt+F4` may be counted in aggregate diagnostics without noisy per-frame logging.

### `cane/scope` and `cane/reset`

- Scope entered/lost.
- Stable reason code: global disabled, mode off, no player, dead, wrong movement mode, paused, menu, cutscene, stage transition, multiplayer, or shutdown.
- Old/new stage and player context where relevant.

Emit on transitions, not every tick.

### `cane/sweep`

Prefer one aggregate record per completed 1-second or 2-second sweep over nine independently flushed lines. It should contain:

- Sweep identifier, mode, cycle start/end tick, intended and actual duration.
- Per-slot angle, scheduled tick, actual tick, lateness, and skipped state.
- Live origin, horizontal camera direction/yaw, rotated probe direction, and requested end.
- Player radius and vertical bbox values.
- Hit/miss/result code.
- Raw collision position and playback source position.
- Horizontal distance, obstacle prop pointer/identifier and type, geometry flags, and collision normal.
- Requested volume and pan.
- Query duration when performance diagnostics are compiled in.
- Total skipped or resynchronized samples.

If a sweep is interrupted, emit a partial aggregate with its reset reason. Keep log construction and formatting outside the audio callback.

### Mixer/performance diagnostics

Extend `accessibilitytonediagnostics` and the existing performance window with:

- Cane requested-slot mask and audio-active-slot mask.
- Per-slot command sequence and mixed sequence, or compact mismatch counts.
- Commands requested, tones started, samples mixed, and stop count.
- Collision queries, hits, misses, skipped samples, total query microseconds, and maximum query microseconds.

When `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS` is enabled, measure query duration with the existing microsecond clock and aggregate it. Do not synchronously flush a line for every query; prior acceptance testing has shown that excessive diagnostic I/O can itself cause choppiness.

## Performance risks and required mitigations

Performance is the principal implementation risk in this prototype.

### 1. Player-cylinder collision checks are not cheap rays

The proposed query traverses rooms and tests background, object, door, and path-blocker collision with a player-sized cylinder. This is intentionally more useful than a thin visual ray, but more expensive. Fast mode schedules nine queries per second; Slow schedules 4.5 per second. Never run nine queries at once, never query every render frame, and never probe unscheduled angles speculatively.

Measure a baseline before enabling the cane, then measure the same controlled CI/holo route with Slow and Fast. Treat an individual query above 2 ms or a sweep average above 0.5 ms as an investigation trigger, not as an automatically acceptable cost. These are provisional diagnostic thresholds, not proof of a universal budget.

If the cost is excessive, optimize in this order:

1. Confirm no accidental duplicate tick call or catch-up burst.
2. Confirm room lists and collision masks match the minimum required movement semantics.
3. Reuse fixed scheduler state and eliminate redundant calculations.
4. Profile door/object/background portions before changing semantics.
5. Consider a cheaper engine-supported swept-cylinder query only if it preserves the same accessible information.

Do not silently reduce range, omit doors/objects, or substitute a thin ray without user review.

### 2. Collision results use shared state

The collision API's shared result storage makes parallelization unsafe. Keep the query and immediate result capture on the main thread. A future optimization must not move it to the audio callback or a background worker unless the collision subsystem is first made explicitly reentrant, which is outside this plan.

### 3. Audio callback overhead

Nine persistent slots must not mean nine unconditional oscillators calculated for every output sample. Maintain a fast inactive path. With the approved sequential schedule, normally zero or one cane voice is active for only about 35 ms. Use fixed arrays and atomic command transfer; allocate nothing at runtime.

### 4. Hitch recovery

A delayed frame must not trigger a burst of overdue probes and tones. Limit work to one query per logical tick and skip/resynchronize missed offsets. This protects both performance and intelligibility.

### 5. Diagnostic I/O can imitate a gameplay regression

Normal builds need comprehensive but aggregated cane records. Detailed per-query timing belongs behind `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS`, and even there should be accumulated in memory and flushed with the existing performance cadence. Preserve the compile-time diagnostic flag and document any new cane fields beside it.

### Performance acceptance evidence

Before handoff for user testing, provide:

- Query rate, mean, maximum, hit/miss count, and skipped count for Off, Slow, and Fast.
- Frame-window comparison from the same route with the cane Off and Fast.
- Accessibility mixer active/requested counts showing no stuck slot.
- Process memory observations over at least 15 minutes of repeated movement, combat, menu transitions, and several holo-training sessions.
- Confirmation of zero per-sample, per-probe, and per-sweep heap allocations.
- Confirmation that no native game sound channel is allocated by the cane.

Any sustained frame-time regression, recurring new maximum frame gaps, increasing memory use, stuck audio slots, or speech/audio choppiness blocks acceptance even if the feature functions.

## Implementation sequence

The coding agent should use this order so each layer can be verified independently:

1. Re-read `README.md`, all accessibility guidance, this plan, and the current collision/audio/input implementations.
2. Confirm branch/status and repeat the F4/Alt binding search.
3. Add configuration parsing, validation, effective-mode startup logging, and initialization/shutdown reset.
4. Add the nine preallocated mixer slots, stop path, counters, and a temporary internal diagnostic trigger if needed. Verify there are no allocations or native sound handles.
5. Implement the scheduler with a stub miss result. Verify exact Slow/Fast timing, mode cycling, scope transitions, no movement reset, and hitch skipping from logs.
6. Implement and instrument the read-only movement-cylinder adapter on the main thread.
7. Validate raw collision positions and left/right sign in simple geometry before enabling sound requests.
8. Connect hit position to existing spatial pan/volume helpers and the matching dedicated slot.
9. Add aggregate sweep and compile-time performance diagnostics.
10. Add the main-loop and input hooks, then update the architecture hook ledger, roadmap note if appropriate, configuration docs, testing docs, and shortcut documentation.
11. Build in the required MinGW64 environment.
12. Launch only from that initialized MinGW64 environment and run the functional/performance test matrix.
13. Inspect the effective `pd.ini` beside the executable and the session-start log to confirm global accessibility and `VirtualCaneMode=1` are active for acceptance testing.

Do not leave temporary developer keybindings or unconditional verbose logs in the normal build.

## Verification matrix

### Geometry behavior

- Flat wall viewed head-on: a consistent left-to-right sequence with approximately symmetric pan and sensible distances.
- Angled wall: progressively changing spatial position/volume matching its slope.
- Inside and outside corner: distinct patterns without reporting through the nearer wall.
- Doorway/narrow opening: missing or more distant central samples expose the opening while neighboring wall samples remain.
- Pillar or crate: intermittent occupied angles rather than a false continuous wall.
- Closed, partly open, and fully open doors: cues follow current movement-blocking state.
- Pickup and non-solid decoration: no cane tone unless the object genuinely blocks player movement.
- Open space: silent complete sweeps at the selected cadence.
- Crouched versus standing beneath low geometry: current bbox changes the result where movement semantics change.
- Small step within the player's normal step allowance: cane behavior matches whether the player can actually move through it.

### Live behavior

- Move continuously during a Slow sweep: samples use their individual live positions and the sweep does not restart.
- Turn rapidly during Fast: each sample follows current camera yaw and no stale frozen fan remains.
- Induce a hitch or pause: there is no catch-up burst and the missed count is recorded.
- Cross rooms and stage boundaries: no stale collision pointer, tone, or scheduler state survives.
- Die, enter cutscene, open menu, pause, or leave walk movement mode: cane stops immediately; it restarts from the left when ordinary gameplay resumes.

### Input/configuration

- F4 cycles Off/Slow/Fast/Off once per press without speech or confirmation sounds.
- Holding F4 does not race through modes unless the existing input policy intentionally repeats discrete keys; use an edge-trigger if needed.
- Left Alt+F4 and Right Alt+F4 do not change cane mode.
- F4 in menus or paused gameplay does not change the cane.
- `VirtualCaneMode=0/1/2` loads correctly; invalid values fail safely and log the effective fallback.
- Global accessibility Off suppresses all queries and tones regardless of cane mode.

### Audio coexistence

- Exercise cane with door/interactable beacons, laser hazard, combat target tones, fine aim, weapon-function beeps, speech, music, and ordinary sound effects.
- No cane sound steals, stops, retunes, or spatially relocates another accessibility cue.
- Cane tones remain distinguishable from the 220/440/660+/880/1000 Hz cue families.
- No two cane probes start simultaneously, including after a hitch.
- Mode Off and scope loss leave all nine requested and active slot masks clear.

### Build/runtime

- Configure and compile only in the MSYS2 MinGW64 environment with the documented commands.
- Launch `build/pd.x86_64.exe` only through the initialized MinGW64 environment.
- Record build configuration, executable, ROM configuration without logging ROM contents, session ID, and diagnostic flag state.
- Run a normal build and an `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS` build.

## Acceptance criteria

Engineering acceptance requires all of the following:

- The nine angles, ordering, maximum range, and complete-cycle timing match this specification.
- F4 mode cycling and default-on acceptance configuration work without conflicting with Alt+F4 or game/menu controls.
- Every sample uses live pose and stance data without restarting on movement.
- The nearest qualifying player-blocking environment produces a spatialized fixed-pitch chirp; misses are silent.
- Characters, players, pickups, and drop-offs are not represented.
- The implementation uses nine preallocated accessibility mixer slots, performs no runtime allocation in the sample/audio paths, and consumes no native game sound channels.
- The collision query runs at most once per logical tick and never on the audio thread.
- No query bursts, stuck voices, unbounded logs, memory growth, or measurable sustained stability regression are present.
- Configuration, shortcut, diagnostics, testing guidance, and the upstream hook ledger are updated.

Accessibility acceptance additionally requires blind-user testing in controlled geometry. The tester should be able to distinguish at least a flat wall, an angled wall, an opening, and an intermittent obstacle pattern, and should be able to keep Fast mode active while moving in a combat-like situation without the cane becoming unusably stale or overwhelming. Compilation and sighted inspection do not satisfy this criterion.

## Explicit non-goals

- Distance encoded by pitch.
- Floor, stair, ledge, pit, or drop-off warnings beyond whatever blocks the horizontal movement cylinder.
- Enemy, character, or multiplayer-player detection.
- Route guidance, waypoints, landmarks, objective direction, or automatic movement.
- Controller binding, rebinding UI, or settings-menu presentation.
- Vertical/elevation encoding, HRTF changes, or front/back sound redesign.
- A frozen nine-ray snapshot or restarting sweeps whenever the player moves.
- Spoken mode feedback.
- Completing all of roadmap Milestone 11.

## Rollback and failure containment

The immediate user rollback is `Accessibility.VirtualCaneMode=0`. Global accessibility disable must also suppress the feature.

The code should remain separable enough that a source rollback consists of removing the cane tick/reset/input hooks, removing the cane module from `SRC_ACCESSIBILITY`, and removing its mixer slots and configuration key without changing existing beacons, hazards, combat audio, fine aiming, speech, collision, or movement behavior.

If the movement-cylinder query cannot be made read-only or creates a reproducible performance/stability regression, stop implementation at the instrumented adapter and report evidence. Do not ship a query that changes collision globals outside the documented result scratch state, modifies player movement, or causes gameplay/audio choppiness.

## Handoff report requirements

The implementing agent's final report must include:

- Files changed and user-visible behavior.
- Every established game/port file touched and why the hook was necessary.
- The final collision mask, bbox source, room-list path, and validated meaning of the reported hit position.
- Final sound frequency, envelope, attenuation thresholds, and any values changed during testing.
- Query and mixer performance evidence, memory/stability evidence, and diagnostic flag state.
- Build configuration and exact executable tested.
- Runtime session ID and relevant `cane/*`, tone-diagnostic, and frame-window fields.
- Blind-user evidence separately from engineering evidence.
- Assumptions, unresolved questions, known regressions, and exact rollback steps.

Do not mark the broader Navigation and orientation milestone complete when only this virtual-cane slice has been implemented.
