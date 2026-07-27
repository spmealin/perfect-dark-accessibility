# Accessibility Milestone 5 plan — Carrington Institute interactable beacons

## Status and handoff contract

This is the implementation handoff for the next accessibility milestone. It is deliberately narrower than the later generalized interaction scanner: prove that a blind player can find two high-value classes of things in Carrington Institute training before expanding the semantic model.

The implementer should follow this document without inventing broader scope. If repository facts differ from the audit below, preserve the user contract, log the discrepancy, and make the smallest safe adjustment. Do not silently add more categories, stages, assets, automatic movement, or interaction.

Milestone 5 is complete only after the MinGW64 build passes, the runtime checks are recorded, and blind-user acceptance confirms that the two category sounds are distinguishable and spatially useful. A successful compile alone is not accessibility completion.

## User outcome

While freely moving through Carrington Institute training, the player can:

1. press one provisional gameplay key to start a nearby-prop scan and beacon the nearest eligible result;
2. hear a short cue positioned at the selected prop;
3. distinguish an interactable object from a door by the cue itself;
4. turn and move toward that prop while repeated pulses remain attached to its world position;
5. independently enable or disable the interactable-object and door categories; and
6. run neither, either, or both category beacons according to player preference.

The first blind-user acceptance route must include at least one Carrington Institute laptop or terminal and one door. The player, rather than the feature, still moves, aims, and activates the target.

## Locked scope decisions

### Stage and player scope

- Enable this prototype only when `g_Vars.stagenum == STAGE_CITRAINING`.
- Support one local player only. If `PLAYERCOUNT() != 1`, reject the command nonfatally, stop any active beacon, and log the reason.
- Run only during ordinary first-person gameplay. Menus, pause, cutscenes, death, stage transitions, and missing player/prop state silence the beacon.
- Do not scan the title screens, campaign stages, multiplayer, Combat Simulator, or other training stages in this milestone.

### Exactly two categories

1. **Interactable object:** a healthy object or weapon prop whose existing object semantics say it is deliberately actionable in Carrington Institute. This includes the existing CI-tagged terminals/laptop and objects marked with interaction flags. It does not mean every pickup, weapon, decoration, character, or object on screen.
2. **Door:** a healthy door prop which still represents a usable physical door. Locked doors remain discoverable because the game permits the player to attempt them and receive a locked result. Linked door pieces are one logical result.

Do not add pickups, weapons merely because they can be collected, characters, enemies, lifts without a door prop, mission objectives, alarms outside the scoped interaction rules, or decorative props as new categories.

### Original existing-sound selection (superseded)

The initial accepted prototype below used existing samples. A later cue-quality revision replaces those two samples with a 100 ms procedural sine chirp: 440 Hz for doors and 880 Hz for interactable objects. It retains the same round-robin cadence, spatial pan, distance attenuation, category toggles, and one-at-a-time rule while moving pulse ownership from a property-sound channel to the fixed accessibility mixer. This revision adds no external asset and does not change the semantic scan policy.

No synthesized tone and no external audio file is needed for this milestone.

| Category | Existing sound | Reason |
| --- | --- | --- |
| Interactable object | `SFX_MENU_FOCUS` | Short, clean, affirmative cue already used for menu focus changes. In the prototype it communicates “a deliberate control/object.” |
| Door | `SFX_MENU_SUBFOCUS` | Short cue already used for subordinate focus/toggle-off feedback. Its contrast with the focus cue provides a second category without falsely playing a door movement sound. |

The cues are defined in `src/include/sfx.h` and already played by `menuPlaySound` in `src/game/menu.c`. Beacon playback must use the existing positional prop-sound path, not `menuPlaySound`, so left/right level and distance follow the selected world prop.

Do not use an existing open, close, lock, switch, pickup, success, or failure sound as the beacon. Those sounds carry a false state-change meaning. Do not use `SFX_SLAYER_BEEP`; its existing lifetime is tied to the continuously managed Slayer rocket sound set rather than a neutral one-shot notification.

The two locked cue choices may be revised only if runtime listening or blind-user evidence shows that they are not reliably distinguishable. Record both the original and replacement IDs if that happens.

### Why existing samples win for the first proof

The port's normal audio endpoint receives the already mixed stereo stream; it is not an arbitrary WAV/OGG asset service. The game already has `psCreate`, prop-following channels, room-aware attenuation, and stereo pan. Reusing two short in-ROM samples therefore exercises the real spatial path with minimal packaging and licensing risk.

Procedural synthesis remains feasible later by generating PCM and adding it to the mixer, but that would require a cue generator, resampling/format policy, channel ownership, and mixing integration. Loading external files would additionally require decoding, distribution, lookup, and failure policy. Neither helps answer this milestone's central question: can the game's existing spatial audio guide a blind player to an interactable?

## Audited repository facts

The implementer should recheck these symbols before editing:

- `src/game/propsnd.c:psCreate` can attach a sound channel to a `struct prop` and accepts explicit distance parameters.
- `psCalculateVol` and `psCalculatePan` update prop-relative attenuation and stereo position. The current stereo pan is principally left/right; front/rear may require the player to turn and compare the cue.
- `src/game/prop.c:propFindForInteract` delegates object and door checks but mutates `g_InteractProp` and searches the rendered interaction list. It must not be called by the beacon scanner.
- `src/game/propobj.c:objTestForInteract` mixes semantic actionability with immediate interaction requirements such as being on screen, close, facing the player, and sometimes line of sight.
- `propobjGetCiTagId` recognizes Carrington Institute terminal/object tags. `propobjInteract` maps those tags to training, lists, firing range, and the main CI computer behavior.
- `doorTestForInteract` contains immediate door-facing/range/visibility checks, and `propdoorInteract` performs the actual action. Neither action function belongs in a read-only scan.
- Active props can be traversed from `g_Vars.activeprops` until `g_Vars.pausedprops`. Prop lists and prop pointers can change during stage/runtime transitions.
- The port names `VK_F5`, `VK_F6`, and `VK_F9`. F5/F6 already have menu-only accessibility meanings, so gameplay beacon meanings must be gated strictly to the no-menu CI gameplay scope.
- The accessibility service, logger, speech backend, and menu observer already exist under `src/accessibility` and `port/src/accessibility`.

## Interaction model

### Provisional commands

Use the following PC-only development actions:

- **F5 — toggle interactable-object beacon.** If inactive, scan and select the nearest valid object. If active, stop only the object beacon. The door category is unaffected.
- **F6 — toggle door beacon.** If inactive, scan and select the nearest valid door. If active, stop only the door beacon. The object category is unaffected.
- **Menu context remains unchanged.** While a menu is open, the beacon coordinator returns before reading gameplay beacon commands; F5 repeats menu narration and F6 cancels speech as established in Milestone 4.

Read F5/F6 through the existing input abstraction. These bindings are provisional; Milestone 6 owns narrated settings, collision checking, remapping, and controller access.

The commands must do nothing to gameplay controls beyond observing these just-pressed keys. They must not press Use, turn the player, aim, alter camera orientation, or consume unrelated input.

### Periodic nearest-target refresh with stable handoff

On scan:

1. Capture the current player prop, position, orientation, and rooms.
2. Traverse active props once.
3. Classify every considered prop and record an inclusion or exclusion reason.
4. Collapse linked door pieces to one canonical logical door.
5. Compute exact three-dimensional distance and player-relative bearing for eligible canonical results.
6. Sort by ascending distance, then category, then stable prop index/identity as a deterministic tie-breaker.
7. Store a bounded snapshot and retain up to three valid results separately for each enabled category.

Do not rebuild the target set every frame. While either category is enabled, rebuild the shared bounded snapshot every 30 logical ticks. Retain up to three targets per enabled category; keep existing members unless they disappear or an unscheduled candidate is at least 150 units nearer than the farthest retained member. This lets cues follow the player through CI while preventing moving objects or tiny distance changes from thrashing schedule membership.

Interleave retained object and door targets on one global round-robin timeline. Calculate the slot gap as the 45-tick base interval divided by target count, clamped to the current 18-tick minimum. Before every pulse, validate the scheduled identity, stop the prior accessibility cue, transfer the tracked sound slot, and only then start the new positioned cue. If no valid targets remain, keep the player's category preference active but silent; periodic refresh must automatically discover later eligible props without requiring another toggle.

### Range and room policy

Use a named, single-source provisional scan radius of **1,200 game units**. The value must not be duplicated in filters, audio, and logs. Runtime testing may tune it, but any change must be recorded with the tested route.

Apply a knowledge boundary in addition to raw distance:

- An interactable object must share at least one player room or be in a directly adjacent room and always pass a read-only background line-of-sight check.
- A door must share or connect the player's room set or an immediately adjacent room and pass a background-only line-of-sight test. Door geometry is excluded from that ray so the selected closed door does not block itself, while sight-blocking level geometry suppresses doors beyond it.
- Reject invalid room lists, props beyond the adjacency boundary, and props whose state says they are hidden, destroyed, disabled, or unavailable.

Do not use `PROPFLAG_ONTHISSCREENTHISTICK` as the scan's knowledge rule: the beacon must find things the blind player is not already aiming at. Conversely, do not treat mere allocation in the stage as knowledge; that would reveal distant or hidden content.

If no safe existing room-adjacency helper exists, add a small read-only adapter around the background room/portal query. Do not build a navigation graph in this milestone.

### Interactable-object eligibility

Refactor only the smallest useful predicate from the existing interaction code. Prefer a new read-only helper in `propobj.c/.h` which answers semantic eligibility without changing `g_InteractProp`, activating an object, playing audio, creating HUD text, or depending on the current render/facing state.

The predicate should require:

- prop type `PROPTYPE_OBJ` or `PROPTYPE_WEAPON` only when the underlying object is deliberately interactable rather than merely collectible;
- a live object pointer and healthy/non-destroyed state;
- no `OBJFLAG_CANNOT_ACTIVATE` or equivalent disabled state;
- Carrington Institute tag semantics from `propobjGetCiTagId`, or the established `OBJFLAG3_INTERACTABLE`/terminal semantics used by `objTestForInteract`;
- any object-specific state restriction that makes the action unavailable; and
- the milestone range/knowledge checks outside the semantic predicate.

Do not blindly copy all branches from `objTestForInteract`. Audit each accepted branch and explicitly log which rule admitted it. Thrown laptops, hoverbikes, grabbable props, lift-door special cases, and alarm controls should remain excluded unless they are demonstrably part of the scoped CI interactable-object route.

Do not manufacture user-facing names from model IDs. The category cue says “interactable object”; exact names can be added later when localization and semantic identity are reliable.

### Door eligibility and canonicalization

A door candidate must be a live `PROPTYPE_DOOR` with a live door object, valid rooms, and state that still represents a physical door. Do not require it to be on screen, centered, within normal Use range, or currently unlocked.

Door geometries can have sibling props. Select a canonical identity consistently, using the existing sibling linkage/door grouping rather than pointer ordering where possible. Emit one result and one sound for the group. Store enough member identity to keep following the logical door if one geometry becomes unavailable.

Record open fraction, mode, lock/unlock result if a pure query exists, sibling count, and rooms in diagnostics. These facts are for diagnosis; the two-category cue must not claim “open,” “closed,” or “unlocked” unless later work explicitly validates that speech contract.

## Beacon audio design

### Playback path

Add a dedicated accessibility prop-sound type/owner rather than borrowing a gameplay type. Use `psCreate` with the selected prop, the category sound ID, explicit volume/range values, and the new owner type so the feature can find and stop only its own channels.

Create one one-shot pulse at a time. Do not set the normal repeating flag merely to obtain cadence. The accessibility coordinator owns a timer and requests the next pulse after the prior cue's useful duration. This prevents an inaudible or invalid target from retaining a looping channel indefinitely.

Start with this tunable policy:

- one pulse immediately after selection;
- one pulse every **45 logical 60 Hz ticks** (0.75 seconds) while active;
- full reference level inside 200 units;
- useful attenuation through the 1,200-unit scan radius; and
- silence beyond a short fade margin no greater than 1,400 units.

Keep cadence and distances in named constants. Do not pulse while `lvupdate60 == 0`; use logical game time so pause does not accumulate a burst. If testing shows the samples overlap at 0.75 seconds, lengthen the cadence rather than starting concurrent copies.

### Channel lifecycle

At most one accessibility beacon channel may play at a time. The global scheduler stops the prior pulse and transfers its tracked slot before attaching the next cue, including when the category changes. Object and door targets are interleaved so their distinct cues never begin simultaneously.

Stop and clear the current channel when:

- the user toggles that category off;
- the scheduler advances to another target or category;
- the target becomes invalid;
- a menu opens, the game pauses, a cutscene begins, the player dies, or stage/player scope changes;
- accessibility or beacon configuration becomes disabled;
- the stage unloads or accessibility shuts down; or
- sound creation/update reports failure.

Never stop another system's channel. Never retain a raw prop pointer across stage teardown without first resetting the entire beacon state. Audio failure is nonfatal: keep the game playable, deactivate the beacon, and log the exact failure.

### Spatial truth and limitations

Attach the cue to the prop so existing room, distance, and pan calculations update as the player and prop move. Do not convert it to a non-positional UI sound after selection.

The current stereo system is not HRTF and should not be documented as full 3D localization. Acceptance should explicitly test left, right, ahead, behind, near, far, and different heights. A user may need to rotate to compare pan and resolve front from rear. If that is insufficient for the route, record it as evidence for a future supplemental clock-direction or pitch design; do not add that unplanned system during the initial implementation.

## Accessibility service integration

### Modules and ownership

Add a core module such as:

- `src/accessibility/accessibility_beacon.c`
- `src/include/accessibility/accessibility_beacon.h`

The module owns configuration state, command edge handling, snapshot storage, selected index, pulse schedule, and logging. It may call narrow read-only game helpers, but game prop/object code must not call the speech backend or contain accessibility policy.

Register the source explicitly in `CMakeLists.txt`, matching the existing accessibility source pattern.

Expose lifecycle functions with clear contracts, for example:

- `accessibilityBeaconInit` or reset as part of accessibility init;
- `accessibilityBeaconTick` once per logical gameplay tick;
- `accessibilityBeaconStop(reason)` for transitions/shutdown; and
- no public access to internal snapshot arrays.

Prefer one coordinator call from a stable point in `port/src/pdmain.c` after input and current player state are valid, as anticipated by the architecture hook ledger. If repository tracing proves `lv.c` is the only safe point, add one narrow call there and document why. Do not scatter hooks through per-prop tick or rendering code.

### Capacity and allocation

Use a bounded fixed-size result array for this prototype; do not allocate per frame. Pick a capacity comfortably above the scoped CI room count (for example 64), log truncation with the total eligible count, and preserve the nearest sorted results when full.

Store stable diagnostic identity in addition to the prop pointer: prop index if derivable from the prop pool, prop type, object/door pointer, model/tag, rooms, and door canonical identity. Validation must compare current type/object identity before dereferencing deeper state.

### Configuration

Add a registered Boolean setting named consistently with the existing service, preferably:

`Accessibility.InteractableBeacons=1`

This enables the commands but does **not** begin making sounds at startup. Both runtime category states always start inactive and require F5 and/or F6. The feature originally defaulted off during initial engineering, then was changed to default on before project-owner blind-user acceptance testing in accordance with the branch-wide acceptance policy. Record the effective value from the session-start log.

If the top-level accessibility switch is off, no commands, scan, channel, or beacon log spam should occur. Comprehensive normal session logging remains governed by the existing logging switch.

Do not add volume, radius, cadence, per-category, stage, or key settings yet. Milestone 6 will expose stable choices after this behavior has been tested.

### Speech

The spatial cue is the primary output. Do not require speech for pulse timing or localization.

On selection or cycling, the existing speech service may issue one concise interrupting label—`Interactable object` or `Door`—before the first pulse only if runtime testing shows that the samples alone are hard to learn. Treat that as a documented fallback, not the initial design. Do not announce internal IDs, exact coordinates, hidden lock state, or a guessed object name.

F6 continues to cancel speech while a menu is open; during unobscured CI gameplay it toggles the door beacon. F5 similarly retains menu repeat in menu context and toggles object beacons in gameplay. Speech-backend absence must not disable either sound beacon.

## Comprehensive logging contract

The project owner has explicitly requested comprehensive implementation logs; do not redact feature-relevant data. Use the existing structured accessibility log path and vocabulary where possible.

### Session/config evidence

Record:

- beacon module/schema version;
- enabled/default/effective configuration;
- stage and player-scope decision;
- scan radius, room policy, pulse cadence, volume/range parameters;
- exact category-to-sound mapping by symbolic name and numeric ID; and
- provisional action-to-key mapping.

### Every command

Record timestamp/tick, F5/F6 action and category, that category's previous state, both resulting category states, stage, player index, player prop identity, position, orientation, room list, menu/pause/cutscene/death state, and accepted/rejected reason.

### Every scan

For every active prop considered, record at minimum:

- prop pointer and pool index when available;
- prop type and raw flags;
- object/door pointer, model, CI tag, object flags/state, door mode/fraction/lock query, sibling identity, and room list when applicable;
- exact delta, 3D distance, horizontal bearing, relative bearing, and vertical offset;
- range, room adjacency, line-of-sight, health, activation, and canonicalization results;
- final category or exclusion reason; and
- final sorted index, selected state, and snapshot truncation.

Pointer values and raw internal identifiers are welcome in these diagnostics because they make lifetime and grouping bugs traceable.

### Every pulse and transition

Record selected snapshot index/identity/category, sound ID, requested position/prop, category-owned channel/owner/handle if available, volume and pan calculation/result, pulse due/actual tick, create result, stop reason, invalidation reason, replacement selection, empty state, and call duration.

Do not log an unchanged full prop audit every frame. Toggle-triggered scans record every candidate; twice-per-second automatic refreshes record scan summaries and retain/handoff/acquire/clear decisions without repeating every unchanged candidate. Between refreshes, log pulses, target validation changes, state transitions, and periodic health. This preserves comprehensive causal evidence without turning normal polling into redundant gigabytes.

## Implementation sequence

### Phase 1 — Read-only semantic extraction

1. Re-trace `propFindForInteract`, `objTestForInteract`, `doorTestForInteract`, CI tags, active-prop traversal, door sibling representation, and prop destruction/deactivation paths.
2. Document every branch chosen for the two milestone categories.
3. Add the smallest pure/read-only object and door helpers required by the scanner.
4. Add focused C assertions or a small harness where practical to prove that eligibility does not mutate `g_InteractProp`, door state, object state, HUD, or audio.

Exit: known CI terminals/laptop and doors classify correctly from synthetic or runtime state, while common decorations/pickups do not.

### Phase 2 — Snapshot and deterministic cycling

1. Add the beacon module and fixed-capacity result types.
2. Implement active-prop traversal, knowledge/range filters, door canonicalization, identity validation, and deterministic sorting.
3. Add context-gated F5/F6 per-category edge-triggered command handling.
4. Add complete scan/command logs before audio output.

Exit: logs show the expected two-category result set and stable order in CI; independent category toggles produce none/object/door/both states and do not affect game state.

### Phase 3 — Positioned pulses and lifecycle

1. Add a dedicated prop-sound owner/type and stop helper if the existing API cannot target one accessibility channel safely.
2. Attach `SFX_MENU_FOCUS` or `SFX_MENU_SUBFOCUS` to the selected canonical prop.
3. Implement logical-tick cadence and selected-target validation.
4. Stop on every transition listed above and on shutdown.
5. Verify only one accessibility channel is live and channel exhaustion/failure is nonfatal.

Exit: a sighted/runtime instrumentation pass confirms that cue pan/attenuation tracks the selected prop while the player turns and moves.

### Phase 4 — Build, runtime matrix, and blind acceptance

1. Build with the required MinGW64 environment.
2. Launch the executable only from the initialized MinGW64 environment.
3. Execute the scripted matrix below with full logging.
4. Fix crashes, false candidates, unstable ordering, misleading sound lifecycle, or intolerable cadence.
5. Hand the exact route and keys to the blind tester without live sighted steering.
6. Record pass/fail, observations, cue distinguishability, localization strategy, time/attempts, and any human assistance.

Exit: the acceptance criteria pass and the roadmap status is updated honestly.

## Verification matrix

### Build and disabled behavior

- Clean incremental Windows build using the documented commands.
- Top-level accessibility disabled: no scan, beacon sound, or beacon-specific behavior.
- `Accessibility.InteractableBeacons=0`: gameplay F5/F6 cause no scan or beacon sound.
- Logging disabled: feature still works without creating beacon logs.
- Speech/Tolk/NVDA unavailable: sound beacon still works and startup remains nonfatal.
- Accessibility-disabled play has no changed prop state, input behavior, or foreign audio-channel stops.

### Command/state behavior

- F5 from object-inactive selects the nearest eligible object; a second F5 stops only that beacon.
- F6 from door-inactive selects the nearest eligible door; a second F6 stops only that beacon.
- Exercise all four states: neither, object only, door only, and both. Toggling one category never changes the other's requested active state.
- With both active, object and door targets are interleaved, independently positioned, and never start simultaneously.
- An empty category scan gives one concise non-spatial failure indication only if already supported, leaves that requested category active but silent, and logs every exclusion; toggling it off and on retries.
- Rapid F5/F6 presses do not leak channels, dereference stale props, or leave more than one pulse per category active.
- Rescan after moving produces a fresh correctly ordered snapshot.

### Interactable objects

- Main CI laptop/computer or another known tagged terminal is admitted.
- At least one additional CI settings/training terminal is admitted if it uses the audited semantics.
- Decorative nearby objects are rejected.
- Ordinary collectible weapons/pickups are rejected by this milestone's category rule.
- Disabled, destroyed, moved-out-of-scope, or no-longer-actionable objects invalidate safely.
- The object cue is `SFX_MENU_FOCUS` and remains attached as the player turns and approaches.

### Doors

- One normal closed door is admitted.
- Open and moving states remain one logical target unless the physical door becomes unavailable.
- A locked door remains discoverable without being announced as unlocked.
- Multi-panel/sibling door geometry produces one result and one beacon.
- Destroyed, hidden, stage-disabled, or invalid door state is rejected or invalidated safely.
- The door cue is `SFX_MENU_SUBFOCUS` and never plays the actual door-open/close sample.

### Spatial and knowledge behavior

- Selected prop distinctly pans left and right when appropriate.
- Ahead/behind ambiguity is measured by turning, not assumed solved.
- Near/far attenuation changes usefully over the route.
- Vertical separation is measured and documented.
- A prop outside 1,200 units is excluded.
- A distant or nonadjacent-room prop is excluded even if allocated.
- A closed door at a room boundary remains discoverable when background line of sight is clear; its own door geometry is not included in the ray.
- An interactable object does not beacon through sight-blocking background geometry, regardless of its native interaction flags.

### Lifecycle and robustness

- Opening a menu, pausing, entering a cutscene, dying, restarting, changing stage, or quitting silences and resets the beacon.
- Returning to gameplay does not automatically resume an old target; a new command is required.
- Prop destruction/deactivation between pulses does not crash or play at stale coordinates.
- Failed sound-channel allocation is logged and nonfatal.
- A long enabled session does not accumulate channels; bounded refresh scans run only at the documented twice-per-second cadence.

### Blind-user acceptance script

Use a reproducible Carrington Institute training start and name the target before each attempt without describing its visual direction.

1. Tester starts in the known route state with headphones or a known stereo speaker arrangement.
2. Tester uses F5 and F6 independently, identifies the object and door cues, turns to localize each, and approaches it.
3. Tester finds and activates the designated laptop/terminal using ordinary game controls.
4. From a reset or documented next location, tester finds and reaches/uses the designated door.
5. Tester demonstrates object-only, door-only, both, and fully silent states, then stops and restores each category independently.

Record whether each task was completed without live sighted direction, number of category toggles/scans, wrong-target approaches, elapsed time, category confusions, front/rear confusions, and subjective cue fatigue. If assistance is required, state exactly what it was; do not mark the accessibility result complete until the route is repeatable unaided.

## Required build and launch procedure

Only building and launching the game require MinGW64. Normal inspection and file editing may use ordinary tools.

Build from the repository root in an initialized `C:\msys64\mingw64.exe` environment:

```sh
cmake -G"Unix Makefiles" -Bbuild .
cmake --build build -j4 -- -O
```

The executable is `build/pd.x86_64.exe`. Launch it only from the initialized MinGW64 environment; launching from a normal Windows command line can fail because dependent MinGW DLLs are not on the runtime path.

Do not add the ROM, generated executable, logs, save data, or copied build outputs to Git.

## Risks and mitigation

| Risk | Mitigation/evidence |
| --- | --- |
| Reusing immediate interaction code mutates global state or requires render focus | Extract pure semantic predicates; never call `propFindForInteract` or interaction action functions from a scan. |
| A scan reveals content through walls | Apply bounded room adjacency plus category-appropriate LOS policy and log every decision. |
| Closed doors block their own LOS test | Use the established background-only ray so walls occlude the door but door geometry itself does not. |
| Door panels appear multiple times | Canonicalize the sibling group and log all members. |
| Prop pointer becomes stale | Reset on stage transitions; retain identity fields; validate type/object identity before every pulse. |
| Existing sounds imply an action occurred | Use neutral focus/subfocus samples, never movement/switch/success sounds. |
| Category cues are too similar | Blind-test them early; record evidence and revise only the mapping if necessary. |
| Stereo pan cannot resolve front/rear | Require the player-turn triangulation test and document the limitation for later supplemental cues. |
| Pulse floods or masks gameplay | One channel per category, 0.75-second provisional cadence with a half-cadence category offset, immediate stop, no concurrent copies within a category, tune from evidence. |
| Feature consumes scarce sound channels | Dedicated owner, one globally transferred channel, one-shot lifecycle, allocation-failure handling, and long-session channel audit. |
| Provisional keys collide or exclude controller users | Keep scope explicit and replace them in Milestone 6's discoverable binding UI. |
| A lightweight implementer broadens the feature | Treat exactly two categories, CI-only scope, and non-goals as hard handoff constraints. |

## Explicit non-goals

- No new WAV, OGG, MP3, or ROM audio asset.
- No procedural oscillator, custom mixer, HRTF, binaural renderer, reverb redesign, or generalized audio engine.
- No automatic steering, camera snap, walking, aiming, activation, door opening, or route planning.
- No global radar or disclosure of distant, secret, hidden, cloaked, or inaccessible entities.
- No categories beyond interactable objects and doors.
- No generalized names for arbitrary props and no guessed localization.
- No campaign, multiplayer, Combat Simulator, or non-CI-training support.
- No final controller scheme, key rebinding, collision UI, or accessibility options menu; those are Milestone 6.
- No replacement for the normal Use interaction eligibility at close range.
- No claim that completing this prototype completes the broader Milestone 10 scanner or Milestone 11 navigation work.

## Completion checklist

- [x] Repository audit reconfirmed and any discrepancy documented.
- [x] Exactly two semantic categories implemented with pure/read-only eligibility.
- [x] Door siblings canonicalized and prop lifetimes validated in code; runtime cases remain below.
- [x] Context-gated F5 object and F6 door toggles implemented provisionally with independent category state.
- [x] `Accessibility.InteractableBeacons` registered, default enabled for acceptance testing, with runtime initially silent until F5/F6.
- [x] Interactable objects use positioned 880 Hz procedural chirps (superseding the accepted `SFX_MENU_FOCUS` prototype).
- [x] Doors use positioned 440 Hz procedural chirps (superseding the accepted `SFX_MENU_SUBFOCUS` prototype).
- [x] Global round-robin cadence, single-chirp retriggering, and all planned stop/reset paths implemented.
- [x] Comprehensive command, scan, candidate, ordering, pulse, and lifecycle logs implemented.
- [x] MinGW64 build passes using the required commands.
- [ ] Enabled, disabled, dependency-failure, empty-result, state-transition, and long-session runtime checks pass.
- [x] Blind tester distinguishes both categories and finds the CI laptop and office door without live sighted direction.
- [ ] Known spatial limitations and any tuned constants are recorded.
- [ ] Roadmap status updated to “engineering complete, accessibility validation pending” or “complete” based on actual evidence.

## Following milestone

After this work is accepted, Milestone 6 adds narrated, persistent accessibility settings and permanent collision-aware input actions, including the proven beacon controls. The broader interaction scanner remains Milestone 10 so it can build on real CI beacon evidence instead of assuming that every prop category behaves the same way.

## Implementation result (2026-07-19)

The engineering implementation is present and the default `ntsc-final` x86-64 MinGW64 build succeeded for the original sample-backed version. The core module is `src/accessibility/accessibility_beacon.c`; `port/src/pdmain.c` supplies one post-`lvTick` coordinator call and a pre-`lvStop` reset. No `propobj.c` or `propsnd.c` hook was required because existing read-only state and public spatial calculation APIs were sufficient. Blind-user testing confirmed the laptop and office-door beacons and reported that the prior choppiness was gone after channel hardening; a five-minute telemetry run showed bounded audio-channel use, zero allocation failures, and no sustained linear memory-growth pattern. Subsequent usability passes added twice-per-second automatic target refresh followed by a global multi-target round-robin scheduler. Current CI policy retains three targets per category with a 150-unit membership margin and a 300 ms minimum gap; the scheduler architecture keeps the cap and density policy replaceable for future enemy tracking. The later procedural revision removes beacon property-sound ownership entirely and publishes 440/880 Hz positioned chirps to the fixed-buffer accessibility mixer. The procedural object/door cues and the later three-chirp pickup category passed project-owner blind-user testing. Subsequent accepted refinements include knowledge-safe visibility filtering, door canonicalization, category toggle earcons, and state-preserving suspension outside live gameplay. Full campaign/Combat Simulator coverage and independent-user validation remain open.

Broader runtime and independent-user checklist items intentionally remain open. For future coverage passes, verify `Accessibility.InteractableBeacons=1` in both the effective configuration and session-start log, launch only through the MinGW64 environment, run the remaining verification matrix, and retain the accessibility session ID and relevant logs.
