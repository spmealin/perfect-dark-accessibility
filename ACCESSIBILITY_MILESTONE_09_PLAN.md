# Accessibility Milestone 9 plan — generic targeting feedback core and CI firing-range proof

## Status and handoff contract

This is the active implementation handoff for the first bounded slice of Roadmap Milestone 9. The implementation must create a reusable targeting-feedback core and prove it with the Carrington Institute firing range. It must not hard-code the audio state machine around `g_FrData`, `MODEL_TARGET`, or the CI stage.

Planning baseline: branch `accessibility`, commit `bb31d26f5c` (`accessibility: rotate beacons across nearby targets`).

The firing-range proof is engineering-complete only when the required MinGW64 build, runtime matrix, lifecycle checks, long-session audio checks, and source review pass. It is accessibility-complete only after the project owner performs blind-user acceptance testing and confirms that visible-target location and reticle alignment can be distinguished and used in the range.

Completing this proof does **not** complete all of Roadmap Milestone 9. Ordinary hostile/friendly characters, non-targetable characters and objects, cloaking, campaign occlusion rules, special sights, target speech/repeat, and multiple local players remain later adapters and acceptance work. Record the proof as “Milestone 9 firing-range slice complete; broader targeting coverage pending.”

If an audited symbol or frame-order fact differs from this document, preserve the user outcome and architectural boundaries, record the discrepancy, and make the smallest safe adjustment. Do not silently duplicate the game’s aiming math, add aim assistance, expose hidden targets, add external audio assets, or broaden the source beyond the firing range.

## User outcome

During a valid Carrington Institute firing-range exercise, with targeting feedback enabled by default for acceptance testing, the player can:

1. hear a distinct world-positioned pulse for every currently visible firing-range target;
2. distinguish separate visible targets through staggered round-robin pulses rather than simultaneous starts;
3. turn the weapon’s aiming point onto a target and immediately hear a distinct centered alignment cue layered over the positional target cue;
4. continue hearing a low-latency alignment cadence while the game still considers that target to be under the shot ray;
5. hear the alignment cadence stop immediately when the shot ray leaves the target;
6. destroy a target and automatically begin receiving cues for newly activated targets without toggling or rescanning; and
7. pause, open a menu, finish or leave the exercise, die, or change stage without stale target audio continuing.

The feature observes and communicates game state only. It must never rotate the player, move the reticle, alter auto-aim, select a target, change weapon spread, fire, reload, or guarantee a hit.

## Locked scope

### Implement now

- One generic, fixed-capacity targeting observation/state/audio core under `src/accessibility/`.
- One firing-range semantic source adapter, separate from that core.
- One narrow per-player observation hook after the game has finalized on-screen props and aimed-prop state for the rendered player.
- Positioned visible-target pulses for all eligible visible range targets.
- A centered aimed-at-target confirmation cadence.
- Configuration, lifecycle reset, audio ownership, comprehensive logging, build verification, runtime verification, and blind-user acceptance instructions.
- Data fields and source/category/relationship enums that can represent later combat targets without changing the core contract.

### Design now, do not activate now

- Generic source and relationship vocabulary for character, player, turret, vehicle, destructible object, and unknown future categories.
- Per-player state storage and player identity in every observation/log, while runtime output remains restricted to one local player.
- Optional localized target names, relationship, distance, and speech metadata in the observation contract. The range adapter leaves unsupported fields unknown/empty.
- A future combat-source adapter that can consume validated character/sight semantics.
- Policy profiles so dense combat can use a faster cadence than the firing-range proof without replacing the scheduler.

### Explicitly out of scope

- Campaign characters, enemies, allies, civilians, bots, multiplayer, Combat Simulator, or non-range CI props.
- Speech announcements or a repeat-target command in this slice.
- New permanent keyboard/controller bindings or options-menu UI; Milestone 6 owns discoverable settings and collision-free actions.
- Enemy radar, off-screen target discovery, nearby-character scanning, wall penetration, hidden/cloaked target disclosure, or any source not already presented by the current viewport.
- Hit, damage, bullseye, score, ammunition, reload, or exercise-goal announcements.
- External WAV/OGG assets, procedural synthesis, HRTF, aim automation, snap-to-target, aim magnetism, or camera movement.
- Refactoring the accepted interactable/door beacon scheduler unless a concrete shared-audio defect makes it necessary and the project owner approves the scope expansion.

## Terminology

- **Source adapter:** Read-only code that translates one gameplay system into a generic targeting observation. The first source is the CI firing range; later sources may include ordinary combat characters.
- **Candidate:** A target that the source says is semantically eligible and currently known to the player.
- **Visible candidate:** A candidate currently presented in the player’s viewport according to finalized engine render state.
- **Aimed candidate:** The visible candidate whose prop equals the engine-validated `currentplayer->lookingatprop.prop`.
- **Presence cue:** A world-positioned pulse attached to a visible candidate.
- **Alignment cue:** A centered, non-positional cue indicating that the weapon’s current query ray is over a valid candidate.
- **Identity:** A validated tuple that survives sorting and state comparisons without relying on a raw pointer alone.
- **Knowledge boundary:** The rule preventing output about entities that the game has not visually or semantically presented to the player.

## Confirmed repository facts

The implementation agent must recheck these before editing:

- `struct frdata` owns `targets[18]`; each `struct frtarget` contains `inuse`, `active`, `destroyed`, motion/script state, flags, and its world `prop` (`src/include/types.h`).
- `frSetTargetProps` resolves the 18 fixed target props from setup tags `0x05` through `0x1a` and initially marks them invisible (`src/game/training.c`).
- `frExecuteWeaponScript` selects targets for the exercise and sets the maximum concurrently active count.
- `frInitTargets` makes initial active targets visible; destruction in `frTick` hides the old target and activates the next one.
- `frTick` updates target movement, rotation, invincibility, destruction, and scripts before the player render pass.
- `g_FrIsValidWeapon` is set by `frBeginSession` and cleared by `frEndSession`. `frIsInTraining` also returns true during some completed/failed menu countdown states, so it is not sufficient by itself as the audio-active gate.
- `propsTickPlayer` clears and recomputes `PROPFLAG_ONTHISSCREENTHISTICK` for the current viewport. Object visibility requires, among other conditions, that `OBJFLAG2_INVISIBLE` is clear.
- `propsSort` builds `g_Vars.onscreenprops` from enabled props carrying `PROPFLAG_ONTHISSCREENTHISTICK`.
- `propFindAimingAt` uses the current weapon query ray and calls `shotCalculateHits`. That path tests background geometry and real prop hit geometry and returns the first intersected prop for a non-shooting query.
- `lvRender` writes that result to `currentplayer->lookingatprop`, rejects invalid/cloaked entities, and explicitly permits CI `MODEL_TARGET` objects.
- `sightTick` performs sight-specific tracking and uses `SFX_0007` as an existing target-acquisition sound when appropriate. It runs from `sightDraw`, reached through `playerRenderHud` and `bgunDrawSight`.
- The current top-level `accessibilityBeaconTick` runs after `lvTick` but before player rendering. That location sees previous-view render/aim state and is not the observation hook for this feature.
- `psCreate` provides prop-attached positional playback with explicit distance policy. The accepted beacon implementation demonstrates fixed storage, channel validation/reuse, stage-stop reset, telemetry, and a dedicated property-sound owner.
- Accessibility configuration defaults to enabled for project-owner acceptance testing, and the branch requires every newly implemented accessibility feature to do the same before handoff.

## Architecture

Keep four responsibilities separate:

```text
finalized game view/aim state
        |
        v
firing-range source adapter
  - semantic eligibility
  - knowledge/visibility boundary
  - stable identity and metadata
        |
        v
generic targeting core
  - observation diff
  - debounce and lifecycle
  - visible-set ordering
  - aimed-target state
        |
        v
generic cue policy/audio owner
  - one positioned presence lane
  - one centered alignment lane
  - cadence, channel validation, logging
```

The generic core must not include `game/training.h`, read `g_FrData`, compare `MODEL_TARGET`, or check `STAGE_CITRAINING`. Those belong only in the source adapter.

The source adapter must not own cadence timers, sound handles/channels, round-robin position, deduplication, or announcement policy. It builds one complete observation and submits it.

Gameplay files must not call `psCreate`, `sndStart`, Tolk, logging policy, or formatting code. They provide one narrow observation hook only.

## Generic observation contract

Define a bounded contract in `src/include/accessibility/accessibility_targeting.h`. Exact field names may follow project style, but preserve these semantics:

```c
enum accessibilitytargetsource {
	ACCESSIBILITY_TARGET_SOURCE_NONE,
	ACCESSIBILITY_TARGET_SOURCE_FIRING_RANGE,
	ACCESSIBILITY_TARGET_SOURCE_COMBAT,
};

enum accessibilitytargetprofile {
	ACCESSIBILITY_TARGET_PROFILE_NONE,
	ACCESSIBILITY_TARGET_PROFILE_FIRING_RANGE,
	ACCESSIBILITY_TARGET_PROFILE_COMBAT,
};

enum accessibilitytargetcategory {
	ACCESSIBILITY_TARGET_CATEGORY_UNKNOWN,
	ACCESSIBILITY_TARGET_CATEGORY_RANGE_TARGET,
	ACCESSIBILITY_TARGET_CATEGORY_CHARACTER,
	ACCESSIBILITY_TARGET_CATEGORY_PLAYER,
	ACCESSIBILITY_TARGET_CATEGORY_TURRET,
	ACCESSIBILITY_TARGET_CATEGORY_VEHICLE,
	ACCESSIBILITY_TARGET_CATEGORY_OBJECT,
};

enum accessibilitytargetrelationship {
	ACCESSIBILITY_TARGET_RELATIONSHIP_UNKNOWN,
	ACCESSIBILITY_TARGET_RELATIONSHIP_HOSTILE,
	ACCESSIBILITY_TARGET_RELATIONSHIP_FRIENDLY,
	ACCESSIBILITY_TARGET_RELATIONSHIP_NEUTRAL,
};

struct accessibilitytargetidentity {
	s32 playernum;
	s32 source;
	s32 sourceslot;
	s32 propnum;
	u8 proptype;
	uintptr_t objectidentity;
};

struct accessibilitytargetcandidate {
	struct accessibilitytargetidentity identity;
	struct prop *prop;              /* transient; validate before every use */
	s32 category;
	s32 relationship;
	struct coord position;
	f32 distance;
	f32 screenx;
	f32 screeny;
	f32 screenx1;
	f32 screenx2;
	f32 screeny1;
	f32 screeny2;
	u32 knowledgeflags;
	const char *localizedname;      /* optional and copied immediately */
};

struct accessibilitytargetobservation {
	s32 playernum;
	s32 source;
	s32 profile;
	s32 stage;
	s32 frame60;
	s32 inscope;
	s32 sighton;
	s32 targetindicatorvisible;
	s32 candidatecount;
	struct accessibilitytargetcandidate candidates[32];
	s32 hasaimedtarget;
	struct accessibilitytargetidentity aimedidentity;
};
```

Requirements:

- Capacity is fixed and named. Do not allocate while observing or cueing.
- `localizedname` is optional future metadata; if supplied, copy it into bounded owned storage during submission. Never retain callback-owned text.
- Identity comparisons use every stable applicable field. Never use a raw prop pointer as the only identity.
- `objectidentity` is diagnostic/validation identity, not user-facing output.
- Source adapters submit only known candidates. Do not submit hidden candidates with a flag and expect the core to suppress them.
- Candidate order supplied by a source is not presentation order. The core sorts deterministically.
- Cue timing and sound choices come from a named `accessibilitytargetprofile`, not stage/source checks scattered through the state machine. Define a compact policy structure containing presence sound, alignment sound, base-cycle ticks, minimum slot ticks, alignment-repeat ticks, distance parameters, debounce counts, and maximum scheduled candidates. The source selects a profile; the core validates it.
- Log capacity truncation and preserve the highest-priority candidates. For the range source, capacity 32 exceeds all 18 possible targets and truncation is a defect.
- Include player context now even though output is single-player-only, so a later multiple-player design does not require changing every event and state key.

## Firing-range source adapter

Create `src/accessibility/accessibility_targeting_game.c` as the gameplay-semantic adapter. Its public entry point should be narrowly named, for example:

```c
void accessibilityTargetingObserveCurrentPlayer(void);
```

### Scope gate

An observation is active only when all are true:

- top-level accessibility and `Accessibility.TargetingFeedback` are effective;
- `PLAYERCOUNT() == 1` for this proof;
- current stage is `STAGE_CITRAINING`;
- `g_FrIsValidWeapon` is true;
- current player, player prop, range data, prop pool, and view state are valid;
- the game is in ordinary first-person gameplay;
- the player is alive;
- no menu, pause, cutscene, endscreen, active menu overlay, or stage transition is suppressing gameplay output; and
- logical time is advancing.

If any gate fails, submit an out-of-scope observation so the core can stop stale audio and log the transition. Do not merely return while a cue may be active.

Do not use `frIsInTraining()` as the only gate because its result remains true during some result-menu countdowns.

### Candidate eligibility

Iterate all 18 `frGetData()->targets` entries. A visible candidate requires:

- `inuse` true;
- `active` true;
- `destroyed` false;
- non-null target prop and object;
- validated prop address inside the current prop pool, with a valid integral prop index;
- prop type `PROPTYPE_OBJ`;
- object model `MODEL_TARGET`;
- `PROPFLAG_ENABLED` set;
- `OBJFLAG2_INVISIBLE` clear;
- `PROPFLAG_ONTHISSCREENTHISTICK` set for the current player viewport;
- valid model/matrices and successful `modelGetScreenCoords`; and
- a projected rectangle intersecting the current viewport.

The projected-rectangle intersection is a defensive confirmation and supplies generic screen metadata. It must not replace the finalized prop on-screen flag.

Do not require the target to be stationary, facing the player, destructible in one hit, currently vulnerable, close to screen center, or within a new range limit. If the real renderer presents an active target, the presence cue represents it. Do not use `frIsTargetFacingPos`; it is laptop-gun targeting policy, not the player’s firing-ray truth.

### Aimed candidate

After building the visible set, compare the already-filtered `currentplayer->lookingatprop.prop` to candidate props. Mark exactly one matching candidate as aimed.

Do not call `propFindAimingAt` again. Do not compare the reticle point with the projected rectangle. Do not use `frCalculateHit`, mutate tracked props, or invoke any shooting path.

If `lookingatprop` names a range prop that did not pass the visible-candidate contract, log `aimed_candidate_rejected` with every failed condition and submit no aimed target. This protects the knowledge boundary and exposes timing mistakes.

### Screen metadata

For each candidate calculate and log:

- raw projected bounds;
- clipped bounds;
- normalized center relative to the current viewport, with horizontal and vertical values in `[-1, 1]`;
- world position and exact player distance;
- source slot and stable prop index; and
- whether the candidate equals `lookingatprop`.

The initial presence sound remains attached to the world prop so the established audio engine supplies pan and attenuation. Screen metadata exists for diagnostics and future view-relative cue designs; do not synthesize pan from it in this proof.

## Future combat adapter contract

Do not implement this adapter in the firing-range slice, but review the generic contract against this intended extension before freezing it.

A later ordinary-combat adapter should be able to:

1. build candidates from the finalized current-player on-screen prop list rather than a range table;
2. use existing character/player/object semantics to reject dead, disabled, cloaked, non-targetable, scripted-hidden, or otherwise unknown entities before submission;
3. use `sightCanTargetProp`, `sightIsPropFriendly`, team comparison, and sight/weapon-specific rules only where their existing contracts truthfully apply;
4. categorize a submitted candidate as character, player, turret, vehicle, or object and assign hostile/friendly/neutral/unknown relationship without changing cue-state logic;
5. map known entities to localized names through a separate semantic name resolver, leaving the name empty rather than guessing from a model ID;
6. submit the already-filtered `lookingatprop` match exactly as the range adapter does;
7. select a denser `ACCESSIBILITY_TARGET_PROFILE_COMBAT` while reusing identity diffing, debounce, round-robin, alignment state, audio ownership, lifecycle, and logging unchanged; and
8. omit entities that are off-screen or knowledge-protected even if an AI, mission script, or room list knows they exist.

The firing-range implementation passes the genericity review only if adding that adapter would require a new source file/profile and semantic resolvers, but no rewrite of the observation contract, visible-set state machine, aimed-target state machine, scheduler, channel owner, or lifecycle API.

## Observation hook and frame order

Add one call in the per-player normal-stage path only after all of these have occurred for that player:

1. `propsTickPlayer`;
2. `propsSort`;
3. `propFindAimingAt` and the `lookingatprop` validity filters;
4. special aim-tracking updates; and
5. `playerRenderHud`/`bgunDrawSight`/`sightTick` for that player.

The expected narrow location is immediately after `playerRenderHud` returns in `src/game/lv.c`, before menus and later overlays are rendered. Re-audit the exact control flow. If some ordinary first-person path bypasses that call, place the hook at the closest common post-sight point and document why.

Running after `sightTick` matters because the native sight may already play `SFX_0007` on acquisition. It also ensures the visible set, aimed prop, sight mode, and current player context all describe the same viewport.

Do not move the existing interactable beacon tick. Do not put targeting observation in the pre-render `port/src/pdmain.c` tick. Do not hook individual target ticks or collision functions.

Update the hook ledger in `ACCESSIBILITY_ARCHITECTURE.md` when implementing the call.

## Generic core state machine

Create `src/accessibility/accessibility_targeting.c`. It owns one state record per supported local-player slot even though this proof only emits for a single local player.

### Observation processing

For each submitted observation:

1. Validate the player index and observation capacity.
2. If out of scope, stop both cue lanes, clear visible/aimed state, and record the reason once per transition.
3. Copy candidates into owned fixed storage.
4. Validate and normalize each identity.
5. Sort deterministically by:
   - aimed candidate first for immediate scheduling only;
   - ascending absolute normalized horizontal screen offset;
   - ascending distance; and
   - source, source slot, and prop index as stable tie-breakers.
6. Diff the visible identity set against the prior observation.
7. Preserve round-robin position by identity when the set changes; do not reset to slot zero on every reorder.
8. Update aimed-target state independently from presence scheduling.
9. Produce cue decisions only after the whole observation is validated.

### Visibility stability

- A newly visible candidate normally becomes schedulable after two consecutive valid observations. This filters single-frame edge flicker.
- A candidate that is already aimed bypasses the entry debounce and becomes schedulable immediately.
- Remove a presence candidate after two consecutive missing/invalid observations.
- During a missing-frame grace period, do not start a new pulse on the uncertain candidate.
- All debounce counts are named constants and logged.

The source still observes every rendered frame. Debounce affects cue membership, not logging or aimed-ray truth.

### Aimed-target stability

- Acquisition of a valid aimed identity is immediate.
- A target change is a loss of the old identity and acquisition of the new one in the same observation.
- Suppress alignment playback immediately on the first observation with no aimed candidate.
- Delay only the diagnostic/spoken concept of stable loss for two observations to avoid future speech chatter. This slice has no loss speech.
- Reacquiring within that grace resumes without generating multiple stacked acquisition sounds.
- Never continue the alignment cadence based only on a stale pointer or prior target.

## Audio design

Use two deliberately separate lanes.

### Presence lane: positioned and serialized

- Initial candidate sound: `SFX_MENU_SELECT`.
- Playback: `psCreate` attached to the candidate prop.
- Ownership: add `PSTYPE_ACCESSIBILITY_TARGETING`, distinct from `PSTYPE_ACCESSIBILITY_BEACON` and gameplay owners.
- Concurrency: at most one targeting-presence channel may play at once.
- Before every pulse, validate the identity/prop/object again, stop or reclaim only the exact free-or-targeting-owned prior channel, then attach the next pulse.
- Do not create one persistent channel per visible target.
- Do not use the repeating property-sound flag. The core owns cadence and starts one-shots.

Starting cadence policy for the range profile:

```text
base full-cycle interval: 36 logical 60 Hz ticks
slot interval: clamp(base interval / visible count, 6, 36) ticks
one target: every 36 ticks (0.60 seconds)
two targets: alternating every 18 ticks
three targets: round-robin every 12 ticks
four targets: round-robin every 9 ticks
five or six targets: every 7 or 6 ticks
seven or more: every 6 ticks, preserving round-robin order
```

This intentionally scales toward the user’s stated future combat need: six to ten targets should refresh rapidly and may sound busy. The minimum is provisional. If the chosen sample is longer than a slot, stop it cleanly before transferring the channel rather than allowing simultaneous target starts. Record tuning changes and why.

Start with the accepted beacon distance profile unless range listening shows that target scale needs different values: full level inside 200 units, useful attenuation through 1,200 units, silent by 1,400 units. Keep these values in a targeting-specific policy structure; do not reference beacon-private constants.

`SFX_MENU_SELECT` is selected because it is short, already packaged, distinct from the object/door `SFX_MENU_FOCUS` and `SFX_MENU_SUBFOCUS` pulses, and does not falsely claim a hit, destruction, conveyor stop, or score. It remains provisional until audible and blind-user testing.

Do not use `SFX_FR_CONVEYER`, `SFX_FR_CONVEYER_STOP`, range alarm/light sounds, gunfire, hit, success, or failure sounds as presence cues because they already communicate real range events.

### Alignment lane: centered overlay

- Initial sound: `SFX_0007`, the sound already used by `sightTick` for target acquisition.
- Playback: centered, non-positional one-shot through the normal sound API.
- It may overlap the presence lane; that composite is the intended “target location plus aligned shot” result.
- It must not create multiple simultaneous copies of itself.
- Play immediately on acquisition when the native sight did not already provide the same acquisition sound.
- While the same aimed identity remains valid, repeat every 12 logical ticks (5 Hz) as the initial hold-confirmation cadence.
- Stop scheduling immediately on aim loss, scope loss, pause, or invalidation.

Because `sightTick` may already emit `SFX_0007`, the observer runs after it. Suppress the accessibility acquisition copy when the active sight/aim path is expected to have emitted the native sound; log `native_acquisition_used=1`. The accessibility hold cadence begins after its normal interval. When the always-visible target indicator is present without full aim mode and native `sightTick` did not emit, play the accessibility acquisition copy immediately.

Do not modify or remove the native `sightTick` sound in this slice. If runtime evidence shows that native-emission detection cannot be truthful, use a different centered existing sample for the accessibility lane rather than double-playing `SFX_0007`; record the replacement.

### Interaction with existing beacons

The targeting presence lane and the accepted interactable/door beacon lane have separate owners. Do not let either stop the other’s channel. For the initial range acceptance route, turn off F5/F6 world beacons so the two targeting layers can be judged in isolation, then test with them enabled to document masking or collision.

Do not refactor both systems into a shared global cue arbiter in this slice. Record overlap evidence as input to a later centralized accessibility-audio policy. The alignment lane is intentionally allowed to overlay the targeting presence lane.

## Lifecycle and memory safety

Add `accessibilityTargetingReset(const char *reason)` and call it from:

- accessibility initialization before output can begin;
- accessibility shutdown before audio teardown;
- the existing stage-stop location immediately before `lvStop`;
- any fatal internal validation failure that leaves channel ownership uncertain; and
- the per-player observer when scope transitions from active to inactive.

Reset must:

- stop only exact channels/handles that are still accessibility-targeting-owned;
- clear transient prop pointers before stage memory is released;
- clear candidate arrays, counters, schedule cursor, aimed identity, debounce state, and timers;
- tolerate repeated calls;
- log the prior state and reason when logging is available; and
- never allocate or dereference an unvalidated stale prop.

Use fixed arrays only. No per-frame heap allocation, no duplicated growing logs in memory, no one-handle-per-target design, and no retained model matrix pointers. Logger scratch allocation is permitted only through the existing logger’s paired lifetime.

Add telemetry comparable to the hardened beacon implementation: process working set/private bytes on Windows where already supported, sound counts, property-sound channels in use/stopped, targeting-owned channel count, presence pulse count, alignment pulse count, visible count, aimed state, and reset count. Periodic telemetry should be bounded and should not require a Windows dependency in the platform-independent core.

## Configuration and defaults

Register:

```text
Accessibility.TargetingFeedback=1
```

It is subordinate to `Accessibility.Enabled`. It defaults to one because every implemented feature must be enabled for project-owner blind-user acceptance testing. Existing `pd.ini` values override the compiled default.

One setting controls both range presence and alignment cues for this slice. Keep internal booleans for the two lanes so a future settings UI can expose them without changing the state machine, but do not add undocumented keys preemptively.

The feature activates automatically only during a valid firing-range exercise and is silent elsewhere. No provisional key is required. Do not reuse F5/F6, which already own menu repeat/cancel and gameplay object/door beacons. Do not add F7/F8 without a separate input-collision decision.

Before acceptance handoff, inspect the actual `build/pd.ini` or effective saved configuration and the session-start log. Confirm top-level accessibility, logging, speech/backend as applicable, interactable beacons, and targeting feedback effective values. Targeting feedback must be effective even though F5/F6 beacon categories may remain toggled off for the isolated audio test.

## Comprehensive logging contract

The project owner permits comprehensive local diagnostic logging. Log all feature-relevant state; performance, not privacy minimization, is the reason to aggregate unchanged frames.

### Session and configuration

Record:

- planning/implementation commit and build configuration where available;
- configured/effective targeting-feedback state;
- selected source/profile and all cadence, debounce, distance, capacity, and sound constants;
- player count and the single-player rejection policy; and
- feature init/reset/shutdown state.

### Observation

For every dedicated high-detail range test, record:

- frame/tick, player index, stage, room list, camera/view dimensions, sight type, `gunsightoff`, target-indicator option, pause/menu/cutscene/death state, and every scope gate;
- all 18 range slots, including raw `inuse`, `active`, `destroyed`, target flags, motion/rotation state, invisibility/enabled/on-screen flags, prop/object/model identity, prop index, rooms, and position;
- every inclusion/exclusion reason;
- projected raw/clipped bounds, normalized center, distance, and candidate order;
- raw and filtered `lookingatprop`, matching candidate identity, and rejection reason when it does not match the visible set; and
- observation duration and capacity counts.

Do not log ROM script bytes or extracted copyrighted data.

### Diff and scheduling

Record:

- visible enter/confirm/missing/remove transitions;
- prior/new identity sets and deterministic order;
- schedule preservation, cursor, slot interval, next pulse tick, and skipped invalid entries;
- aimed acquire/change/first-miss/stable-loss/reacquire transitions;
- native acquisition expected/used/suppressed decisions; and
- every policy suppression reason.

### Audio lifecycle

Record:

- lane, sound ID, identity, prop/channel/handle, previous channel, ownership before/after, reuse, stop reason, volume/pan/ranges, creation result, and call duration;
- whether a presence and alignment pulse overlapped intentionally;
- channel-allocation failure or evidence that gameplay reused a prior slot; and
- periodic sound/memory telemetry and deltas from baseline.

Unchanged observations may be summarized periodically after the initial high-detail proof, but every state change, pulse, stop, failure, and lifecycle transition must remain explicit. Logging failure must never block rendering, aiming, firing, or audio cleanup.

## Expected files

Create:

```text
src/accessibility/accessibility_targeting.c
src/accessibility/accessibility_targeting_game.c
src/include/accessibility/accessibility_targeting.h
```

Modify:

```text
CMakeLists.txt
src/accessibility/accessibility.c
src/include/accessibility/accessibility.h
src/include/constants.h
src/game/lv.c                         # one post-sight observation hook
port/src/pdmain.c                     # stage-stop reset beside beacon reset
ACCESSIBILITY.md
ACCESSIBILITY_ARCHITECTURE.md
ACCESSIBILITY_ROADMAP.md
ACCESSIBILITY_TESTING.md
ACCESSIBILITY_MILESTONE_09_PLAN.md
```

Do not modify `training.c`, `prop.c`, `propobj.c`, `sight.c`, `bondgun.c`, or `propsnd.c` unless tracing proves a missing pure semantic contract. Existing public functions and state are sufficient for the planned adapter. If one of these established files must change, add the smallest read-only helper, document why direct observation was unsafe, and update the hook ledger.

Do not modify Tolk, controller DLLs, generated files, ROM/extracted content, `build/`, or vendored code.

## Implementation sequence

### Step 1 — Preflight and baseline

1. Read `AGENTS.md`, all five root accessibility documents, and this plan completely.
2. Confirm branch `accessibility`, planning baseline, and expected dirty documentation only.
3. Re-run the symbol/frame-order audit named above.
4. Build the unchanged baseline through the required MinGW64 environment:

   ```sh
   cmake -G"Unix Makefiles" -Bbuild .
   cmake --build build -j4 -- -O
   ```

5. Record executable path, size, commit, configuration, warnings, and Git status.

### Step 2 — Add the generic data contract and pure state logic

1. Add enums, identity, candidate, observation, lifecycle, and submission contracts.
2. Implement identity validation/comparison, deterministic sorting, fixed candidate copies, visible-set diff, debounce, schedule preservation, and aimed-state transitions without audio.
3. Keep all range symbols out of the core file.
4. Add compile-time/static capacity checks where project style permits.
5. Add focused tests or an isolated harness for pure identity/diff/scheduler functions if feasible without exposing internals in production headers.

### Step 3 — Implement the firing-range adapter

1. Add the strict scope gate and explicit out-of-scope submission.
2. Add safe prop-index validation patterned after the beacon implementation.
3. Traverse the 18 range slots and log every decision.
4. Add screen projection and viewport-intersection validation.
5. Match the already-filtered `lookingatprop` to the completed candidate set.
6. Submit one complete observation; do not emit audio from the adapter.

### Step 4 — Add the post-sight hook

1. Add exactly one normal-game call after finalized sight processing for the current player.
2. Verify it runs once per rendered player viewport and never before `propsSort`/aim selection.
3. Verify menu/pause/third-person/invalid paths submit or cause a clearing observation rather than leaving stale state.
4. Update the architecture hook ledger with input, timing, owner, and failure behavior.

### Step 5 — Add presence audio

1. Add the dedicated property-sound owner.
2. Implement exact channel ownership validation and one-channel transfer.
3. Attach `SFX_MENU_SELECT` to scheduled props using the range profile.
4. Revalidate identity immediately before `psCreate`.
5. Stop/clear safely on membership changes, scope loss, failure, and reset.
6. Confirm no two targeting presence pulses start together.

### Step 6 — Add alignment audio

1. Implement acquisition/change/loss state independent of presence scheduling.
2. Integrate native `SFX_0007` acquisition suppression after `sightTick`.
3. Add the 12-tick centered hold cadence without simultaneous copies.
4. Verify aim loss suppresses the next pulse immediately.
5. Log intentional overlap with the presence lane.

### Step 7 — Add configuration, lifecycle, and telemetry

1. Register the default-on setting and effective query.
2. Add init/shutdown/stage-stop resets.
3. Add bounded detailed logs and periodic telemetry.
4. Verify disabled paths do no candidate projection, sorting, or audio work beyond the minimal stop/reset needed for a state transition.

### Step 8 — Documentation and build

1. Document the setting, automatic range scope, two sounds, cue meaning, known stereo limitations, and lack of aim automation.
2. Record the new hooks and source/core boundary.
3. Add a pending targeting section to the testing record without claiming runtime evidence.
4. Configure and build only through MinGW64 using the baseline commands.
5. Resolve every new warning in targeting files. Do not treat unrelated existing warnings as new failures.

### Step 9 — Runtime smoke and truth table

Launch `build/pd.x86_64.exe` only from an initialized MinGW64 environment. Never launch it directly from PowerShell or Command Prompt.

Run at least:

| Accessibility | Targeting setting | Valid range session | Expected |
| ---: | ---: | ---: | --- |
| 0 | any | yes/no | No targeting observation/audio; safe reset only |
| 1 | 0 | yes | No candidate work or targeting audio |
| 1 | 1 | no | Silent and cleared outside the exercise |
| 1 | 1 | yes | Presence and alignment cues active |
| 1 | 1 | paused/menu | Both lanes stop and remain silent |
| 1 | 1 | exercise ends/stage changes | Exact cleanup before target memory teardown |

Parse every produced accessibility JSONL log. Confirm no runtime file becomes tracked.

### Step 10 — Firing-range behavior matrix

Test at minimum:

- Falcon 2 bronze with one stationary target;
- target entering/leaving each viewport edge;
- target behind the player versus merely off-center;
- a target geometrically occluded by range/background structure where reproducible;
- moving target, stopping target, rotating target, and target viewed edge-on;
- multiple concurrently active targets, including the exercise with the greatest active count available;
- aim entering, holding, leaving, rapidly crossing, and switching directly between two targets;
- aiming at the target edge where the projected box contains the reticle but real collision does not, confirming no false alignment cue;
- target destruction and automatic activation of its replacement;
- pre-start countdown, first shot, failure, completion, result menu, resume/exit, death, pause, and leaving the room;
- always-show-target enabled and disabled, full aim held and released;
- representative default, zoom, rocket/follow-lock, CMP150, Farsight, Reaper/spread, projectile, explosive, and melee exercises where available;
- F5/F6 beacons off, then object only, door only, and both, recording masking without cross-owner stops; and
- repeated exercise start/end cycles without restarting the process.

For each case compare cue/log output with semantic game state, not only visual appearance.

### Step 11 — Performance and leak regression

Run a continuous range session or repeated-session sequence long enough to expose progressive slowdown; target at least 20 minutes if practical. Record:

- average/worst observation duration;
- candidate count and sort work;
- presence/alignment pulses per minute;
- sound channels allocated, reused, stopped, and still owned;
- `g_SndNumPlaying` and stopped-channel counts;
- process working set/private bytes over time;
- log bytes/events per minute;
- frame-time/FPS with accessibility disabled, targeting enabled without logging, and targeting plus full logging; and
- state after at least ten exercise start/end cycles.

Pass requirements:

- fixed memory use in the targeting modules;
- no upward targeting-owned channel/handle trend;
- no stale audio after scope/reset;
- no per-frame heap allocation;
- no progressive frame/video degradation attributable to the feature;
- no mutation of target, sight, tracked-prop, weapon, or player control state; and
- accessibility-disabled behavior equivalent to baseline.

### Step 12 — Blind-user acceptance

Before handoff, ensure all accessibility features, comprehensive logging, required speech backend, and `Accessibility.TargetingFeedback` default/effective on. Tell the tester how to turn F5/F6 beacons off so the targeting test begins in isolation.

Ask the project owner to complete these tasks without sighted aiming instructions:

1. identify whether zero, one, or multiple targets are currently visible through the cue pattern;
2. turn toward and locate a designated target using its positioned pulse;
3. move the aiming point onto the target and hold it there using the alignment overlay;
4. deliberately move off and reacquire it several times;
5. destroy it and acquire the newly activated target without toggling a feature;
6. repeat with at least two simultaneous targets and one moving target; and
7. report distinction, localization, latency, masking, fatigue, false positives, false negatives, and preferred cadence/volume.

Record exact exercise/weapon/difficulty, settings, session log ID, task outcome, tuning changes, and remaining barriers. A compile or sighted runtime check cannot substitute for this evidence.

## Focused automated/pure test matrix

Where practical, cover the generic core without running the game:

- zero, one, 18, and capacity-count candidates;
- duplicate identities and conflicting pointers;
- invalid player/source/category/relationship values;
- deterministic sorting and tie-breaks;
- visible entry debounce, aimed-entry bypass, missing grace, and removal;
- schedule cursor preservation across reorder/add/remove;
- slot calculation for 1 through 32 candidates;
- direct target-to-target aim changes;
- first-miss suppression and grace-period reacquire;
- scope loss and repeated reset;
- frame/tick wrap-safe comparisons;
- channel creation failure and stale/reused channel ownership;
- source-supplied text copied/empty/overlong/invalid where future metadata is enabled; and
- feature disabled before, during, and after active state.

Do not create a production debug command that mutates firing-range state solely to make tests easier.

## Review checklist

### Architecture

- [ ] Generic core contains no firing-range stage/model/data checks.
- [ ] Range adapter owns semantic eligibility and submits a complete snapshot.
- [ ] Exactly one post-sight gameplay observation hook exists.
- [ ] No accessibility code recomputes the shot ray or changes `lookingatprop`/tracked props.
- [ ] Player/source/category/relationship are part of the contract from the start.
- [ ] Fixed storage and stable validated identity are used throughout.
- [ ] Architecture hook ledger is updated.

### Knowledge and gameplay safety

- [ ] Only active, visible, enabled range targets become candidates.
- [ ] Aimed output requires membership in that same known candidate set.
- [ ] Hidden/inactive/destroyed/off-screen targets never produce cues.
- [ ] No aim, movement, firing, auto-aim, camera, score, damage, or target-script state is mutated.
- [ ] Projected rectangles are diagnostic/visibility confirmation, not alternate aiming logic.
- [ ] Feature-off path leaves gameplay unchanged.

### Audio and lifecycle

- [ ] Presence targets are serialized and round-robin.
- [ ] Alignment may overlay presence but never stacks copies of itself.
- [ ] Native `SFX_0007` acquisition is not doubled.
- [ ] Only targeting-owned channels/handles are stopped or reused.
- [ ] Reset is idempotent and runs before stage audio/prop teardown.
- [ ] Pause/menu/death/end/stage transitions leave no stale sound.
- [ ] Repeated sessions show no channel or memory growth.

### Output and tuning

- [ ] Presence and alignment sounds are clearly distinguishable.
- [ ] One-target cadence is calm enough for sustained practice.
- [ ] Multi-target cadence refreshes every target quickly enough.
- [ ] Aim acquisition and loss feel immediate.
- [ ] Existing range sounds are not falsely reused.
- [ ] Interactable/door beacon masking is documented.

### Evidence

- [ ] Baseline and final MinGW64 builds pass.
- [ ] Executable is launched only from MinGW64.
- [ ] Runtime truth table and behavior matrix are recorded.
- [ ] Logs parse and contain all decision/audio/lifecycle fields.
- [ ] Long-session performance and ownership data are recorded.
- [ ] Accessibility-disabled regression passes.
- [ ] Blind-user acceptance is recorded or explicitly pending.
- [ ] Broader Roadmap Milestone 9 remains pending unless separately implemented and tested.

## Stop conditions for the implementation agent

Stop and report rather than guessing if:

- finalized per-player aim state cannot be observed after sight processing without a materially invasive hook;
- `PROPFLAG_ONTHISSCREENTHISTICK` is demonstrably from the wrong player/frame at the proposed hook;
- the range target prop can be reused or freed before the planned validation catches it;
- no existing sound can be made distinguishable without changing the audio asset/mixer scope;
- reliable native `SFX_0007` duplicate suppression requires changing sight behavior;
- a required change would mutate shot/target selection or reveal non-visible targets; or
- the new system causes recurring channel growth, slowdown, or stale audio that cannot be fixed within the owned modules.

## Handoff report format

Append an execution result to this file containing:

```text
Implementation commit/dirty patch:
Branch and planning baseline:
Files added/modified:
Established game hooks added and why:
Any deviation from the locked design and why:

Generic-core/source-separation review:
Frame-order verification:
Knowledge-boundary verification:
Sound IDs and any tuning changes:
Configuration/effective defaults:
Build configuration and commands:
Executable/dependency inspection:
Pure/automated tests:
Runtime truth-table result:
Firing-range behavior matrix:
Special-weapon/sight result:
Audio ownership/lifecycle result:
Long-session performance/memory result:
Accessibility-disabled regression:
Blind-user acceptance evidence:
Relevant accessibility log session IDs:
Remaining Roadmap Milestone 9 work:
Known regressions/limitations:
Git status and untracked runtime data check:
```

Do not mark the whole roadmap milestone complete in this report. After the firing-range slice is accepted, move this plan into `milestones/` as the executed record only when the project owner asks to close or archive the slice.

## Execution result — 2026-07-20

Implementation commit/dirty patch: uncommitted working-tree implementation on `accessibility`.

Branch and planning baseline: `accessibility` from `bb31d26f5c` (`accessibility: rotate beacons across nearby targets`).

Files added: `src/include/accessibility/accessibility_targeting.h`, `src/accessibility/accessibility_targeting.c`, and `src/accessibility/accessibility_targeting_game.c`.

Established files modified: `CMakeLists.txt`, `src/accessibility/accessibility.c`, `src/include/accessibility/accessibility.h`, `src/include/constants.h`, `src/game/lv.c`, `port/src/pdmain.c`, and the accessibility roadmap/architecture/user/testing documents.

Established game hooks added and why: `lvRender` first captures projected target bounds after aim/tracked-prop calculation and before PC prop rendering converts model matrices in place, then calls the firing-range observer after all player sight/HUD work so final native alignment state is combined with that same-frame cache. `pdmain.c` resets targeting before `lvStop` so no target prop or audio owner survives stage teardown.

Design deviations: unchanged candidate and observation records are change-triggered plus periodically repeated rather than written on every rendered frame. This preserves every state transition and a complete once-per-second audit while avoiding synchronous JSONL flushes for 18 unchanged slots at 60 Hz. The core currently defines only the firing-range policy; the combat profile enum/contract is reserved but deliberately rejected until a truthful combat adapter and sounds are designed.

Generic-core/source-separation review: the core includes no training header, CI stage constant, `frdata`, `frtarget`, or `MODEL_TARGET`. It owns fixed per-player state, profiles, debounce, identity ordering, scheduling, audio ownership, telemetry, and reset. The game adapter alone owns all firing-range semantics. A combat adapter can submit the existing identity/category/relationship/position/projection/name contract and add a policy without rewriting the state machine.

Frame-order verification: source trace confirms `lookingatprop` is selected/filtered while float model matrices remain valid; fixed-size projected bounds are captured there before `bgRender`; `sightTick` then runs inside the HUD/sight path; and observation submission runs at the end of `lvRender`. It does not call `propFindAimingAt` or any hit-test routine.

Knowledge-boundary verification: the source submits only in-use, active, undestroyed, enabled, non-invisible `MODEL_TARGET` objects whose prop was rendered this tick, whose same-frame model projection succeeds with finite coordinates, and whose bounds intersect the current viewport. Aim output is accepted only when the final `lookingatprop` identity is already in that submitted set. Rejected aimed props are logged.

Sound IDs and tuning: presence uses positioned `SFX_MENU_SELECT` on one reusable `PSTYPE_ACCESSIBILITY_TARGETING` channel with 200/1200/1400 distance points. The base cycle is 36 ticks divided by eligible count with a six-tick minimum. Alignment uses centered `SFX_0007` every 12 ticks. Expected native acquisition playback suppresses the immediate accessibility copy; the accessibility hold cadence starts one interval later.

Configuration/effective defaults: `Accessibility.TargetingFeedback=1`, subordinate to `Accessibility.Enabled`, with no new key binding. All accessibility features remain compiled on by default for owner acceptance testing.

Build configuration and commands: unchanged baseline and final dirty tree both built successfully for default `ntsc-final`, `x86_64-windows`, using MinGW64, Unix Makefiles, and `cmake --build build -j4 -- -O`. The final executable is `build/pd.x86_64.exe`. The final changed-source rebuild emitted no warning from either targeting source.

Executable/dependency inspection: link completed and retained the existing Tolk runtime target. No external target audio asset or new DLL was introduced.

Pure/automated tests: compile/link and source-boundary audits only; no standalone unit harness exists for the C state machine in this patch.

Runtime truth-table result: pending interactive firing-range testing.

Firing-range behavior matrix: pending interactive testing.

Special-weapon/sight result: native-acquisition suppression is implemented from final sight tracking state; weapon-by-weapon runtime evidence is pending.

Audio ownership/lifecycle result: source audit confirms separate beacon/targeting property-sound owner types, one reusable presence slot, one self-clearing direct-sound handle, idempotent scope/lifecycle reset, and stage-stop reset before teardown. Runtime confirmation is pending.

Long-session performance/memory result: fixed arrays only and no targeting heap allocation. The first repeated-session log showed stable private memory and no persistent targeting-owned channels, but exposed severe target/audio churn caused by post-render matrix projection. Projection now occurs before matrix conversion, non-finite results are rejected, and the fixed cache is cleared after same-frame consumption. A corrected-build soak is pending.

Accessibility-disabled regression: code paths gate on the effective setting and top-level accessibility state; runtime comparison is pending.

Blind-user acceptance evidence: pending project-owner testing. Do not describe the firing-range proof or Roadmap Milestone 9 as accessibility-complete.

Relevant accessibility log session IDs: none; the executable was not launched during implementation.

Remaining Roadmap Milestone 9 work: runtime tuning/acceptance for this proof, then ordinary hostile/friendly/non-targetable character semantics, cloaking/occlusion, special sights, speech/repeat, campaign behavior, and multiplayer.

Known limitations: only the CI firing-range source/profile emits; output is intentionally absent outside a valid exercise. Projection and native acquisition assumptions need real weapon/sight testing. Existing game/build warnings remain unrelated.

Git status and runtime data: implementation and documentation are uncommitted; no runtime log/save/config artifacts were intentionally created.

### Stability correction — 2026-07-20

Blind-user testing across repeated range exercises exposed choppy video and unreliable spoken feedback. Session `1784563320` showed stable private memory near 653.9 MB and targeting-owned channel counts returning to zero, so it did not support a heap or persistent-channel leak. It did show 879 presence pulses, 807 stops, and non-finite projected bounds on 509 pulses. Source tracing found that the single end-of-`lvRender` adapter call projected models after the PC translucent prop pass had converted their float matrices in place with `mtxF2LBulk`.

The corrected adapter is two-phase. `accessibilityTargetingCaptureGame` caches only fixed-size projection values after aiming/tracked-prop calculation and before `bgRender`; `accessibilityTargetingObserveGame` still runs after sight/HUD work and combines the cache with final aim/native-alignment state. The cache is identity-, frame-, and player-validated, rejects every non-finite coordinate, and is cleared immediately after consumption. No model matrix pointer or per-frame heap allocation is retained. The MinGW64 `ntsc-final` build completed successfully with `cmake --build build -j4 -- -O`; repeated-range runtime soak and blind-user confirmation remain pending.

### Shootability-state extension — 2026-07-20

Range targets can remain visible and under `lookingatprop` while their backs face the player, but the range does not consider that a positive shooting opportunity. The generic candidate contract now carries a shootability state independently from visibility. The firing-range adapter derives `shootable` versus `facing_away` from the established `frIsTargetFacingPos` gameplay helper. A facing-away target remains eligible for positioned presence rotation, while the centered alignment lane stops immediately and reacquires when the same target becomes shootable again. The core logs the state and reason on candidate, observation, acquisition, and loss transitions. This extension does not add a ray cast, copy the range's angle constants, alter target motion/damage, or make shootability a universal assumption for future adapters.

The same acceptance-test preparation corrected two adjacent menu barriers. Selectable narration now falls back to visible right-side text when its left label is empty, restoring the pre-session `OK` and `Cancel` captions. The custom-rendered result scoring item now provides a localized dialog summary containing every visible session/scoring statistic; it is read on dialog entry and reconstructed by F5 repeat. The combined MinGW64 build passed, and blind-user confirmation remains pending.
