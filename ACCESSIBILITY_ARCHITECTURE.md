# Accessibility architecture notes

This document records repository evidence and a proposed architecture. It deliberately separates what exists from what has not yet been implemented.

Status labels:

- **Confirmed** — observed in the current checkout; the initial repository trace used commit `514bf7aff`.
- **Proposed** — a design direction, not present in source.
- **Question** — requires a focused experiment or user decision.

## Repository map

| Area | Confirmed role | Accessibility relevance |
| --- | --- | --- |
| `src/game/` | Reimplemented game systems | Owns menus, HUD messages, objectives, player state, targeting, inventory, and world semantics. |
| `src/accessibility/` | Platform-independent accessibility core | Milestone 2 implements lifecycle coordination and JSONL diagnostic logging here. |
| `src/lib/` | Lower-level game/runtime systems | Owns audio manager, sound, scheduler-facing and N64-compatible facilities. |
| `src/include/` | Game and shared headers | Defines `struct player`, menu structures, platform macros, and game APIs. |
| `port/src/` | Native PC entry point and services | Owns startup, configuration, filesystem, SDL input/video/audio, ROM loading, and logging. |
| `port/include/` | Port service headers | Candidate home for backend and native accessibility interfaces. |
| `src/assets/<ROMID>/` | Version-specific source asset descriptions | Do not use as an accessibility implementation directory; content differs by ROM version. |
| `build/src/generated/<ROMID>/` | Generated build output | Never edit or commit. |
| `dist/` | Packaging metadata and platform assets | Only relevant when an accessibility runtime dependency is deliberately packaged. |
| `tools/` | Asset/build utilities and recomp submodule | Not a first-phase hook location. |

`CMakeLists.txt` recursively includes C and C++ sources under `src/game` and `port`, then generates asset headers for the selected `ROMID`. The explicit `SRC_ACCESSIBILITY` list now owns the coordinator, announcements, menu semantics, beacons, targeting core/source adapter, logging, and speech core. Platform backend selection compiles exactly one native or null speech implementation.

The project builds several ROM configurations. `ROMID` defaults to `ntsc-final`; the README also documents `ntsc-1.0`, `jpn-final`, and `pal-final`. Accessibility logic must not depend on one version's generated addresses or extracted content.

The confirmed Windows baseline commands are:

```sh
cmake -G"Unix Makefiles" -Bbuild .
cmake --build build -j4 -- -O
```

For the default configuration, the resulting executable is `build/pd.x86_64.exe`.

## Confirmed semantic boundaries

### Process lifecycle and configuration

`port/src/main.c:main` initializes arguments, crash handling, system services, filesystem, configuration, video, input, audio, and ROM data before `mainProc`. `cleanup` saves input binds and `pd.ini`, then shuts down video and crash handling. This is the narrow lifecycle boundary for an accessibility service.

`port/src/config.c` exposes typed registration functions. `PD_CONSTRUCTOR static gameConfigInit` in `port/src/main.c` registers `Game.*` values before `configInit` reads `CONFIG_PATH`, which is `$S/pd.ini`. Accessibility settings can use the same registration mechanism and exist before a game profile is chosen.

`port/src/system.c:sysLogPrintf` writes the general engine log. File logging is enabled by `--log` and uses `pd.log`; that stream is not an appropriate accessibility playtest schema.

### Main tick

`port/src/pdmain.c:mainTick` calls `lvTick`, then calls `lvTickPlayer` once for each active player before rendering. This provides a potential once-per-frame observation point, but per-player state must carry a stable player number. Accessibility work should not depend on render calls.

### Menus

Relevant code is concentrated in `src/game/menu.c`, `src/game/menuitem.c`, `src/game/mainmenu.c`, `src/game/activemenu.c`, `src/game/trainingmenus.c`, and `port/src/optionsmenu.c`.

Confirmed flow:

1. `menuOpenDialog` initializes a `struct menudialog` and assigns `focuseditem` using `dialogFindFirstItem`, with a later `MENUOP_CHECKPREFOCUSED` pass.
2. `dialogTick` processes a dialog and calls the central `dialogChangeItemFocus` for normal navigation.
3. `dialogChangeItemFocus` covers directional and PC mouse focus and sends `MENUOP_FOCUS` to the selected handler.
4. `dialogTick` compares the previous and current `focuseditem` and plays `MENUSOUND_FOCUS` on change.
5. `menuResolveText` resolves a language ID, literal, or dynamic callback. `menuResolveDialogTitle` resolves dialog titles.
6. `menuitemTick` dispatches behaviour by item type. Handlers use operations including `MENUOP_GETOPTIONTEXT`, `MENUOP_GETSELECTEDINDEX`, `MENUOP_GETSLIDER`, `MENUOP_GETSLIDERLABEL`, `MENUOP_GET`, `MENUOP_SET`, and `MENUOP_LISTITEMFOCUS`.

This means focus identity is centralized but accessible names and values are type-dependent. Milestone 4 observes the final active state once after `menuProcessInput`, resolves every currently used focusable control family by type, and logs unsupported future types rather than guessing. Custom-rendered rows expose text through one read-only generic semantic operation instead of screen-specific narration hooks.

`src/game/mainmenu.c` defines the main menu and many dynamic handlers. A generic menu hook should narrate those definitions without adding calls to every individual menu.

### HUD messages, subtitles, and dialogue

`src/game/hudmsg.c:hudmsgCreateFromArgs` is the common sink used by the public HUD-message wrappers. It applies subtitle preferences, alive checks, and duplicate suppression before allocating and filling a `struct hudmessage`. A hook after successful queue admission can announce only messages the HUD accepted.

`hudmsgCreateAsSubtitle` checks in-game/cutscene subtitle options, may split long text according to audio duration, and eventually calls the common sink. `src/game/chraicommands.c:aiSpeak` plays prop audio and sends eligible text to `hudmsgCreateAsSubtitle`. The accessibility queue must account for subtitle chunks and audio channel context to avoid repeats.

### Briefings and objectives

`src/game/setup.c:setupLoadBriefing` loads the language bank and fills `g_Briefing.objectivenames` and `briefingtextnum`. `src/game/menuitem.c:menuitemScrollableGetText` returns the localized briefing body for `DESCRIPTION_BRIEFING`. Pre/post-mission briefing and objective menus are defined in `src/game/mainmenu.c`.

`src/game/objectives.c` exposes `objectiveGetCount`, `objectiveGetText`, `objectiveGetDifficultyBits`, and `objectiveCheck`. `objectivesCheckAll` detects a changed `g_ObjectiveStatuses` entry and creates the existing completed/incomplete/failed HUD message. The changed-status branch is the semantic objective event boundary.

### Player status, weapons, and damage

`struct player` in `src/include/types.h` owns health, displayed health/shield, hands, gun control, ammunition, inventory, current target candidates, pose, and room context.

`src/game/player.c` provides `playerGetHealthFrac`, `playerGetShieldFrac`, `playerTickDamageAndHealth`, and display triggers. `src/game/bondgun.c` provides `bgunGetWeaponNum`, `bgunGetName`, `bgunGetShortName`, `bgunGetAmmoTypeForWeapon`, `bgunGetAmmoQtyForWeapon`, and `bgunGetAmmoCapacityForWeapon`.

Actual player gun damage is applied in `src/game/chraction.c:chrDamage`, where the victim player becomes current, health is reduced, death is checked, feedback is triggered, and the previous player is restored. Other health changes exist, including scripted/training assignments and health stealing in `player.c`; therefore `chrDamage` alone is not a complete status source. A sampled status snapshot plus threshold/change detection is safer for the first status query. A later directional damage event may justify a narrow `chrDamage` hook.

### Targeting and world state

`src/game/lv.c` obtains the prop under aim with `propFindAimingAt`, filters some invalid/cloaked cases, and updates `currentplayer->lookingatprop` and tracked props. `src/game/sight.c:sightTick` manages sight-specific target tracking and includes `sightCanTargetProp` and `sightIsPropFriendly`.

Targeting uses a two-phase observation. It captures fixed-size projected bounds immediately after aim and tracked-prop calculation, before PC prop rendering converts model matrices in place, then consumes and clears that cache after sight/HUD rendering has finalized native alignment state. The firing-range source admits active, undestroyed `MODEL_TARGET` props; the combat source admits ordinary onscreen hostile character props using the semantic filters described below. Both require successful finite projection and viewport intersection, and match `lookingatprop` only against the admitted set. Ordinary combat naming, non-character threats, multiplayer output, and special-sight behavior still need investigation.

The R-Tracker is a separate semantic radar rather than an extension of combat targeting. `radarGetRTrackedType` is the single eligibility boundary used by both the native renderer and the accessibility adapter: yellow/blue object flags, the blue-marker cheat gate, and tracked-character life/cloak state remain native policy. The adapter scans active props only while the native device is active, assigns stable identities to ten fixed oscillator voices, and communicates category, bearing, front/rear, horizontal distance, and relative height. It intentionally preserves the visual radar's lack of line-of-sight, room, and render restrictions.

The mutually exclusive IR and X-Ray Scanners reuse those ten voices for viewport-bound sources. `objIsHighlightedByInfrared` is shared with `objRender`, so IR admits only conditional-scenery and explicit-infrared objects that receive the special white highlight. X-Ray has no target flag: `objGetXrayHighlightDistance` shares the renderer's eraser-origin/radius calculation, and the adapter retains the nearest ten rendered object/door/weapon props. Single-player `PROPFLAG_ONANYSCREENPREVTICK` supplies stable evidence from the preceding rendered frame after `lvTick` clears the current-screen bit. This creates one frame of deliberate latency while avoiding another projection pass, render-hook state, or off-screen disclosure. Device-bit gating excludes the Farsight's separate X-Ray vision mode.

### Input, audio, native platform, and repository boundaries

`port/src/input.c` and `port/include/input.h` implement SDL keyboard, mouse, and controller input, binding persistence, and direct key/button queries. Existing bindings primarily represent emulated game controls. Accessibility commands such as repeat, cancel, status, and scan need a collision-free action design rather than scattered hard-coded keys.

The view-orientation recovery action uses the existing extended-control binding system rather than a direct key poll. The previously unused `CK_1000` control bit is exposed and persisted as `RESET_VIEW`, with End and controller right-stick click as the PC defaults. Using a new persisted name prevents legacy `CK_1000=NONE` entries from suppressing the new defaults in existing configuration files. `bmoveProcessInput` consumes its pressed edge after normal vertical-look processing, sets `vv_verta` to zero, and clears vertical speed and automatic-centering state while leaving `vv_theta` unchanged. Because it is a normal persisted binding, players can rebind or remove it through the extended controls menu.

Native audio output is initialized in `port/src/audio.c`; the game-facing chain includes `src/lib/audiomgr.c`, `src/lib/snd.c`, `src/game/propsnd.c`, and `src/game/music.c`. `src/game/chraicommands.c:aiSpeak` and prop-sound functions associate dialogue audio with subtitle text. Speech should remain a separate service so it does not enter the game sound mixer accidentally; later earcons may deliberately use an appropriate audio interface after volume and channel behaviour are tested.

`src/include/platform.h` defines `PLATFORM_WIN32`, POSIX platform variants, architecture/endian macros, and `PD_CONSTRUCTOR`. Windows-specific implementation is spread across guarded code in port services such as `port/src/system.c`, `port/src/fs.c`, and `port/src/crash.c`, with the SDL video backend under `port/fast3d`. `dist/windows/icon.rc` is packaging metadata, not a speech integration point.

`port/fast3d` carries its own license and is effectively a third-party rendering subsystem. `port/include/external/minimp3.h` is vendored, and `tools/recomp` is a Git submodule. Accessibility work should avoid all three. The ignored `build/` directory, ignored `src/generated/`, ignored `extracted/` content, and region-specific generated asset outputs are not sources to edit or commit.

Version conditionals such as `VERSION`, `PAL`, and `PLATFORM_N64` occur in relevant game files, including menu, HUD, and subtitle paths. Canonical asset descriptions under `src/assets/<ROMID>/` also differ by ROM. New hooks must compile on each supported branch of those conditionals and must use normal localization/state APIs rather than addresses or data from one ROM configuration.

## Accessibility module layout

Milestones 2 and 3 implement:

```text
src/accessibility/
  accessibility.c          lifecycle and feature coordinator
  accessibility_cane.c     live seven-angle movement-collision orientation cue
  accessibility_log.c      comprehensive structured development log
  accessibility_observer.c active player/CamSpy perspective and collision-pose adapter
  accessibility_speech.c   speech lifecycle and UTF-8 output boundary
  accessibility_tracker.c  native R-Tracker semantic adapter and fixed-slot state
  accessibility_targeting.c generic fixed-capacity target state and owned audio lanes
  accessibility_targeting_game.c firing-range and hostile-character semantic source adapter
src/include/accessibility/
  accessibility.h
  accessibility_cane.h
  accessibility_log.h
  accessibility_observer.h
  accessibility_speech.h
  accessibility_speech_backend.h
  accessibility_tracker.h
port/src/accessibility/
  speech_null.c             unavailable backend for non-Windows targets
  speech_tolk.c             dynamically loaded Windows Tolk backend
```

Later milestones propose:

```text
src/accessibility/
  accessibility_events.c   normalized event creation
  accessibility_menu.c     menu semantic adapters
  accessibility_status.c   queryable player snapshots
  accessibility_world.c    target/scanner/navigation experiments
src/include/accessibility/
  accessibility_events.h
```

An implementation may use fewer files initially. The dependency direction should remain:

```text
game semantic hook/query
        -> accessibility event core
        -> queue and logging policy
        -> speech backend interface
        -> Windows technology or null backend
```

The game must not depend on a native speech implementation. Logging should observe normalized input and queue outcomes, not intercept backend internals as its only evidence source.

## Core contracts

### Lifecycle

Milestone 2 implements:

```c
void accessibilityInit(void);
void accessibilityShutdown(void);
s32 accessibilityIsEnabled(void);
```

`port/src/main.c` calls initialization after `configInit` and shutdown before configuration/video/crash cleanup. Both calls are idempotent. Disabled operation is a no-op. A future queue or state observer may add `accessibilityTick`; Milestone 2 deliberately does not modify `port/src/pdmain.c`.

### Speech backend

Milestone 3 implements this main-thread-only UTF-8 boundary:

```c
s32 accessibilitySpeechInit(void);
void accessibilitySpeechShutdown(void);
s32 accessibilitySpeechIsAvailable(void);
const char *accessibilitySpeechGetBackendName(void);
s32 accessibilitySpeechOutput(const char *utf8, s32 interrupt);
s32 accessibilitySpeechCancel(void);
```

The core owns lifecycle and request logging. The backend owns native initialization, strict UTF-8/UTF-16 conversion, cancellation, and technology-specific error reporting. Windows dynamically loads a separately built `Tolk.dll` from the executable directory and uses Tolk's default screen-reader-only policy; SAPI fallback is not enabled. Non-Windows builds select the null backend. The proof calls Tolk only during startup, explicit test/harness requests, cancellation, and shutdown—never from a frame tick.

### Events and queue

A normalized event should carry only fields needed for policy and diagnosis:

- category and event kind;
- player/context ID where applicable;
- priority: critical, high, normal, or low;
- resolved UTF-8 announcement text, or a semantic payload formatted by the owning adapter;
- stable deduplication key and optional replacement group;
- creation time, expiry time, and interrupt policy;
- source subsystem tag, never a raw pointer.

Suggested behaviour:

- **Critical:** death or immediately blocking failure; interrupts lower output.
- **High:** objective failure/change or severe status threshold; replaces stale events in its group.
- **Normal:** focus, selected value, weapon change, accepted HUD message.
- **Low:** exploratory scanner detail and optional hints; expires quickly.
- Repeated focus on the same semantic item is suppressed unless the user invokes repeat.
- Rapid slider/list changes replace an earlier value from the same control.
- A dialog change clears stale focus/value announcements before announcing the new context.
- User cancel clears speech but does not mutate game state.

Exact priority conflicts must be tuned with blind testers and recorded in tests.

### Configuration

Milestones 2 and 3 implement these `pd.ini` keys. They now default to on for blind-user acceptance testing:

```ini
Accessibility.Enabled=1
Accessibility.LoggingEnabled=1
Accessibility.SpeechEnabled=1
```

Implemented feature keys are also enabled by default for acceptance testing:

```ini
Accessibility.MenuNarration=1
Accessibility.HudMessages=1
Accessibility.EnvironmentalHazards=1
Accessibility.InteractableBeacons=1
Accessibility.IRScannerAudio=1
Accessibility.NonHostileBeacons=1
Accessibility.TargetingFeedback=1
Accessibility.WeaponFunctionCues=1
Accessibility.XRayScannerAudio=1
Accessibility.VirtualCaneMode=1
Accessibility.RTrackerAudio=1
```

Navigation and hostile-cue tuning is constructor-registered as bounded floats:

```ini
Accessibility.VirtualCaneReach=900
Accessibility.VirtualCaneFullVolumeDistance=112.5
Accessibility.VirtualCaneFadeDistance=750
Accessibility.VirtualCaneMaximumAudibleDistance=975
Accessibility.EnemyFullVolumeDistance=600
Accessibility.EnemyFadeDistance=3500
Accessibility.EnemyMaximumDistance=4000
Accessibility.EnemyVolume=0.14
```

Later features may add:

```ini
Accessibility.ObjectiveNarration=1
Accessibility.StatusNarration=1
Accessibility.Verbosity=1
```

The implemented keys are constructor-registered bounded integers or floats in the existing config registry. Cross-field runtime validation enforces ordered attenuation thresholds, keeps the cane audible through its configured reach, rejects non-finite values, and caps enemy gain at 0.25. Effective values are sampled at startup and recorded in the session log; editing `pd.ini` requires a restart. Accessibility settings belong in `pd.ini`, not only in a selected Perfect Dark profile, because startup menus need them. Later key names/ranges remain provisional, and a later in-game settings page should use the same values.

### Playtest logging

Milestone 2 writes `$S/accessibility.log` only when both accessibility and logging are explicitly enabled. It truncates the prior session, writes synchronous/flushed JSON Lines, and records schema, sequence, session, monotonic microseconds, complete build metadata, category, event, and a detailed message. Open/write/flush/close failures disable the logger nonfatally. The current logger is main-thread-only and records lifecycle events; later hooks will add feature context and queue decisions.

The project owner has prioritized diagnostic completeness over privacy minimization during development. The logger may include resolved text, player/profile names, paths, command arguments, precise coordinates, input history, native handles, pointers, and any other feature-relevant state. Do not add redaction or field filtering. The log defaults to enabled for blind-user acceptance testing, remains locally configurable, is ignored by Git, and is never uploaded automatically. Never include ROM contents, extracted copyrighted assets, passwords, authentication tokens, or unrelated operating-system secrets. Size limits, rotation, and public-distribution privacy policy are deferred until actual logging volume is measured.

## Feature architecture

### Menu narration

Milestone 4 uses one post-`menuProcessInput` observation for each menu slot, while the menu slot and current-player context are still valid. Comparing owned snapshots captures final initial focus, keyboard/controller/mouse focus, sibling swipes, push/pop, disabled-item correction, and value/subfocus changes without publishing intermediate transitions.

The menu adapter derives label, role, current value, availability, position/count, and internal subfocus for selectable actions, checkboxes, sliders, dropdowns, standard/custom lists, keyboards, scrollables, carousels, rankings, and player-stat tables. A selectable normally uses its left text as its label, but falls back to its visible right text when the left text is empty. Dynamic callback results are copied immediately. Long scrollable values and dialog summaries have an 8,192-byte normalized-text budget, matching the menu renderer's 8,000-byte briefing source; the composed announcement has a 12,288-byte budget. Presentation-only labels, objectives panels, separators, models, meters, marquees, controller diagrams, and color swatches do not create synthetic focus announcements unless their owning dialog explicitly publishes the equivalent semantic summary.

Custom-rendered list rows, controls with render-only values, and dialogs with important non-focusable content implement one read-only `MENUOP_GETACCESSIBILITYTEXT` operation. Option/control providers expose focused semantics. A normal list whose spoken option needs richer text than its visual row explicitly sets `MENUITEMFLAG_ACCESSIBILITYOPTION`; the core then prefers its `MENUACCESSIBILITYPART_OPTION` provider while leaving visual option text unchanged. A `MENUACCESSIBILITYPART_SUMMARY` provider can expose a localized dialog summary from an item explicitly marked `MENUITEMFLAG_ACCESSIBILITYSUMMARY`. A marked `MENUITEMTYPE_LABEL` without a provider instead contributes its normally resolved visible label text, supporting simple confirmation prompts without a dialog-specific callback. Explicit opt-in is required because some menu definitions store dialog pointers, rather than callable handlers, in the same union field. The accessibility core finds marked providers or labels generically and never identifies a specific dialog to drive narration. Unknown/future focusable types are logged once per state change and never receive invented semantics.

Dialog/focus/value output uses one replaceable menu announcement group: newer state interrupts stale state, unchanged frames are silent, repeat bypasses deduplication, and cancel does not change menu state. The dialog title and optional semantic summary are included only on entry or return to that dialog; focus and value changes within it speak only the current control. F5 reconstructs the title, summary, and current focus for an explicit repeat. Sliders are reported as rounded percentages of their configured maximum. The shared mission-objectives provider supplies the same difficulty-filtered localized names and numbering rendered on pre-mission, pause, endscreen, retry, and next-mission objective pages. It omits state where the renderer does not display one and otherwise appends the renderer's localized Complete, Incomplete, or Failed state. Mission endscreens provide a second semantic summary containing every currently visible status, timing, unlock, weapon, and shooting-stat field; rows hidden to make room for a new-cheat announcement remain absent from speech. The pause Inventory list and firing-range `Weapons Available` list share rich-option formatting for the visible item name, manufacturer, primary/secondary functions, and marquee description; the pause path retains CamSpy variants, mission-specific necklace credentials, and device checkbox state from the same semantic sources as the visual controls. Pause Abort labels explicitly opt into the same static-summary path as the PC exit prompt. The firing-range training-information handler uses the summary contract for its localized weapon name, challenge values, and description. The firing-range scoring model uses the same contract for completion/failure reason, score, targets destroyed, difficulty, elapsed time, weapon, accuracy, per-zone hit counts and points, and hit total. The device-training details dialog also uses this contract to announce its localized description, including the Data Uplink information screen, even though the visual text panel is not focusable. Milestone 4 speaks only menu slot zero while observing/logging every slot. Automatic objective state-change events and final configurable actions remain later work.

The firing-range custom weapon list exposes the same saved proficiency score used to render its three stars. Its accessibility option text maps score values zero through three to no completion suffix, bronze completed, bronze and silver completed, or all three completed, using the existing localized difficulty and completion strings.

The holo-training details dialog follows the device-training summary pattern: its focused `OK`/`Resume` item supplies the localized `htGetDescription()` text already rendered by `DESCRIPTION_HOLOTRAINING`. This exposes the static non-focusable panel on dialog entry and explicit repeat without adding screen-specific speech calls or duplicating text.

### HUD and objective narration

Publish an accepted-HUD event after `hudmsgCreateFromArgs` commits a message. Carry type, flags, player, and audio channel so policy can distinguish subtitles from pickups and system notices. Publish objective index and new state from `objectivesCheckAll`; the objective adapter can resolve localized objective text separately. Objective state events take precedence over a redundant generic HUD version.

### Status queries

Capture a per-player snapshot at a stable logical tick: health fraction, shield fraction, equipped weapons/functions, relevant loaded/reserve ammunition, death/pause state, and stage context. A user command formats this snapshot on demand. Automatic threshold announcements compare semantic snapshots and use hysteresis to avoid chatter.

### Virtual cane

The virtual-cane core runs once after `lvTick` on the main game thread. It accepts F4 only in ordinary single-player walking gameplay, cycles a constructor-registered `0..2` mode, and schedules seven live camera-relative angles without resetting for movement. The slow and fast schedules use region-correct logical ticks, include an end pause, skip overdue work after a hitch, and enforce no more than one collision query per tick.

A shared observer adapter selects the prop and pose that own the currently rendered first-person perspective. It returns Joanna's position, rooms, camera vector, and walking bbox normally, or the active CamSpy prop, rooms, camera vector, and the CamSpy movement collision dimensions while `CAMERAMODE_EYESPY` is effective. The cane permits that remote-camera mode even though Joanna is not in ordinary walking movement. An observer identity change clears the old partial sweep before beginning from the new perspective.

Each scheduled sample copies the current player movement bbox and follows the walking system's room traversal plus `cdExamCylMove06`/`cdExamCylMove02` ordering. The collision mask is background, objects, doors, and path blockers when normal Bond collision is enabled, otherwise background only; characters and players are excluded. Collision APIs publish through shared global scratch state, so the adapter immediately copies a swept hit's full position/geometry record or derives the destination-overlap fallback from its returned obstacle edge before computing distance, volume, and pan. The query never runs on the audio thread and does not call the state-mutating `bwalkCalculateNewPosition` wrapper.

Seven fixed procedural-mixer slots own one 330 Hz, 35 ms chirp each. Atomic sequences transfer frequency, normalized volume, and pan to audio-owned phase/envelope storage; stop clears all slots. The path allocates no native game sound channels. One preallocated text buffer aggregates all seven sample records and uses the logger's preformatted event API, avoiding a periodic formatting allocation while retaining the logger's existing synchronous flush behavior. Advanced diagnostics expose collision timing/counters and mixer request/active masks.

### Target and scanner

The environmental-hazard adapter traverses active props after `lvTick` and recognizes the engine's semantic damaging laser-door type. It derives a beam centerline from the live door model bounding box and rotation, computes distance and facing against the closest point on that segment, and requires a background-only line-of-sight ray. One retained nearest candidate drives a 220 Hz source along the segment and back. This is independent of setup tags and can apply to matching laser barriers in other stages without disclosing inactive, distant, occluded, or rearward hazards.

The targeting core accepts bounded observations containing source/profile, stable identity, category, relationship, a generic shootability state, position/distance, projected bounds, optional localized name, optional normalized aim quality/raw aim distance, and one aimed identity. It owns per-player fixed arrays, two-frame visible/removal debounce, identity-preserving sort/round-robin state, one `PSTYPE_ACCESSIBILITY_TARGETING` presence channel, and one centered procedural alignment lane. Visibility and shootability are deliberately independent: an admitted target remains in the positioned presence rotation when temporarily unshootable, while centered alignment output requires the aimed candidate to be explicitly shootable. The firing-range adapter derives that state from the existing `frIsTargetFacingPos` rule, so a back-facing target immediately stops alignment and reacquires it after rotating toward the player without duplicating range angle thresholds. The firing-range profile uses `SFX_MENU_SELECT`, a 36-tick base cycle divided among visible targets with a six-tick minimum. While aligned, it maps normalized aim quality to a continuous 660–1320 Hz centered sine tone.

The same adapter supplies a separate generic combat profile from the engine's bounded onscreen-prop list. It admits active, enabled, combat-capable hostile character/player props whose rendered model bounds intersect the viewport and whose body midpoint has shooting-blocker line of sight from the camera. Existing team comparison, friendly classification, cloak/IR, hidden, untargetable, dying/dead, and knockout action state provide semantic filtering; no stage, holo-training, or character-script identifier participates. The finalized attack-query prop authoritatively retains an aimed, partially visible hostile when a midpoint ray alone would reject it.

A third, alignment-only device profile handles confirmed special-item target contracts. A fixed registry carries stage, weapon, target tag, and scope, permits multiple rows for one item, and maps CI exercise rules to the exact object tags used by their AI success scripts (`WEAPON_DATAUPLINK` to `0x30`, `WEAPON_ECMMINE` to `0x32`, and `WEAPON_DOORDECODER` to `0x35`). These point-target devices compare against the raw same-frame non-random attack-query prop because ordinary `lookingatprop` filtering can discard their valid objects. The CamSpy path instead traverses the engine's active `criteria_holograph` list, so it generalizes to CI training and campaign photo objectives without a stage or model table. During the pre-render capture phase it copies only fixed-capacity semantic/projection values and applies the same incomplete-status, health, rendered/front-facing, 400-unit horizontal range, and fully-inside-viewport conditions used by `objectiveCheckHolograph`. Because a successful photograph is defined by framing rather than a center ray, any surviving criterion drives alignment; the closest screen-center identity wins only to keep multiple valid criteria stable. The core suppresses both round-robin and combat presence for this profile, leaving only the centered 660 Hz alignment lane. This is a semantic target contract, not a model/interactable heuristic; unrelated scenery receives no fabricated target, and ECM feedback still does not predict projectile trajectory or promise a valid landing.

Combat presence has ten preallocated mixer voices rather than the firing-range property-sound round robin. Stable target identities retain slots while eligible; each voice generates a 440 Hz spatial cue whose mode, period, and duration derive from target distance and the semantic unarmed melee range. The adapter reads `weaponfunc_melee.range` from the unarmed primary function (`X=60` in the current data) rather than duplicating the gameplay constant. It supplies a near-surface cue distance derived from camera-to-body-midpoint distance minus character radius. At `>=5X`, period/duration are 750/180 ms; the values interpolate linearly to 200/50 ms at `X`; at `<=X`, the voice becomes continuous. The audio bridge has a separate immediate-trigger sequence so every accumulated 40 ms period reduction advances the next chirp rather than inheriting latency from the previous longer cycle. Entry to continuous mode remains exactly `X`; after entry, an exit-only `1.1X` threshold absorbs collision/animation jitter without prematurely claiming melee range. A 10 ms gain ramp removes clicks at continuous-mode transitions. This does not claim a guaranteed hit because the actual melee ray, aim, target geometry, occlusion, and attack animation remain authoritative.

Deterministic phase offsets prevent simultaneous starts, and live prop-sound attenuation/pan/cadence updates each logical observation. Dying, dead, knockout-fall, and knocked-out characters release their slot immediately. Empty slots are filled in the core's existing screen-center/distance order; additional candidates wait for a release instead of displacing an audible identity every time ordering changes. Direct alignment remains a separate fixed 660 Hz tone even while unarmed. Profile transitions clear identities and all owned targeting lanes. Semantic high-value-zone quality such as head proximity remains future adapter work. All candidate, projection, slot, oscillator, and mix storage is fixed-capacity with no runtime allocation.

The firing-range aim-quality source reuses the exact hit coordinate calculated by the existing non-shooting `FINDPROPCONTEXT_QUERY` path that selects `lookingatprop`. A narrow optional-output wrapper exposes that coordinate without changing ordinary callers, firing, randomness, collision order, or weapon spread. The adapter retains the raw same-frame prop/coordinate for semantic device comparison and separately retains the coordinate tied to the final filtered aimed prop for firing-range quality. It computes the same Euclidean target-center distance used by `frCalculateHit`, then normalizes continuously over a 75-unit outer scoring radius. Projected rectangles remain visibility diagnostics and never determine fine aim. The PC backend generates the sine oscillator after the normal game mix and before SDL queueing; it preserves phase, interpolates frequency, applies a 10 ms gain ramp, mixes equally into both channels, and uses a fixed staging buffer with no runtime allocation or game sound handle.

The combat profile has its own configuration-backed attenuation policy rather than inheriting the short firing-range/device envelope. Defaults keep visible eligible hostiles full-distance audible through 600 world units, fade through 3,500, and become silent at 4,000. The backend applies a default 0.14 combat oscillator gain after semantic distance attenuation. These values do not broaden eligibility: native hostile relationship, living/combat-capable state, viewport presence, and shooting-blocker line of sight remain mandatory.

A scanner is a separate user-enabled query over nearby eligible props; it must not reuse render visibility as its entire semantic model or reveal hidden mission information. Milestone 5 first proved this boundary in Carrington Institute training; F5/F6/F8 now use the same bounded adapter in every mission and one-local-player Combat Simulator match. Each enabled category refreshes a bounded snapshot twice per second and retains up to three nearby targets using a 150-unit membership margin; interactables, doors, pickups, and non-hostile people are independently controlled by F5, F6, F8, and F7. A dedicated centered confirmation lane reports the resulting category state after those gameplay commands: 880-to-1320 Hz means enabled and 880-to-440 Hz means disabled, using two 35 ms beeps separated by 25 ms. It does not reuse the positioned beacon lane or alter the F4 virtual-cane command. Menu, pause, cutscene, death, unsupported-player, and temporary observer loss are output-suppression states rather than implicit toggle commands: the adapter stops the chirp, clears target/schedule snapshots, preserves the four category selections, and performs a fresh scan without an earcon when eligible gameplay resumes. Stage teardown, feature disablement, and explicit F5–F8 commands remain state-changing boundaries. Interactable-object eligibility shares `objIsPotentiallyInteractable` with `objTestForInteract`, covering CI-tagged objects, alarms, thrown laptops, Hacker Central terminals, explicit interactables, lift controls, and movement-state-eligible vehicles/grabbable props. Scanner policy additionally requires healthy, active, non-invisible state without `OBJFLAG_CANNOT_ACTIVATE`; `OBJFLAG_DEACTIVATED` is not an interaction exclusion. Door eligibility additionally requires current-view rendering both when scanning and immediately before playback. Sibling door leaves share the lowest prop number as one stable canonical identity, and one rendered leaf is selected as the positioned source, preventing a paired doorway from consuming two result or schedule slots. The character category is stage-agnostic in one-local-player sessions and admits living, perceptible friendly, neutral, or native blue-sight protected `PROPTYPE_CHR` props using engine relationship and life/visibility state; a camera-to-body-midpoint shooting-blocker ray prevents through-wall disclosure. Protected characters use the people beacon and are admitted as aim-only combat candidates, allowing the alignment tone without assigning a hostile presence oscillator. Ordinary hostiles enter the existing combat slots while ordinary friendly/team characters remain excluded from combat feedback. Doors and non-hostile people consume the shared active-observer pose, so their origin, rooms, range, bearing, and visibility move to the CamSpy while its camera is active and return to Joanna with the visible perspective. Remote people additionally require `PROPFLAG_ONTHISSCREENTHISTICK`, preventing characters that are room-connected but absent from the CamSpy viewport from sounding; the ordinary player-centered scanner retains its wider spatial-awareness policy. The CamSpy prop is never treated as a person candidate. Body-actionable interactable and pickup categories pause during remote viewing. An observer change immediately stops the old chirp and rebuilds the fixed schedule without changing category toggle state. A global round-robin scheduler interleaves all enabled categories and retriggers one procedural chirp voice, guaranteeing that two beacons never start together. Doors use one 440 Hz chirp, non-hostile people use two rapid 440 Hz chirps, interactable objects use one 880 Hz chirp, and pickups use three quick 880 Hz chirps. The fixed scan holds 64 results and at most three retained identities per category. Split-screen and cooperative output composition remain unsupported.

### Navigation

Navigation is intentionally an experiment. Start with player position/orientation, rooms, a small set of known landmarks, and route-deviation logging in one training environment. Evaluate spoken clock directions versus earcons, metric versus qualitative distance, cue cadence, door/elevator transitions, and recovery after leaving a route. Do not generalize to all stages until route data and blind task completion support it.

## Upstream hook ledger

This table records implemented and anticipated changes to established files so future diffs remain deliberate.

| Established file | Proposed narrow hook or reason | Semantic payload | Why polling alone may be insufficient | Status |
| --- | --- | --- | --- | --- |
| `CMakeLists.txt` | Register core sources and select exactly one native/null speech backend | Build platform/configuration only | `src/accessibility` is outside the game glob and platform backends must not compile together | Implemented through the R-Tracker audio slice |
| `port/src/main.c` | Initialize after `configInit`; shut down in `cleanup` | Lifecycle and logger availability | First UI may occur before a later tick; resources need ordered shutdown | Implemented in Milestone 2 with two calls |
| `port/src/pdmain.c` | Call the compile-time-optional performance observer, call accessibility gameplay ticks immediately after `lvTick`, and reset owned audio before `lvStop` | Timing, input, stage/player context, safe main-thread collision queries, and teardown | Gameplay cues need settled semantic state, cane collision scratch state must be copied on the main thread, and owned sounds must stop before stage memory is disabled | Beacon, virtual-cane, laser-hazard, and R-Tracker ticks run after `lvTick`; cane, targeting, hazard, and R-Tracker stage-stop resets protect owned voices and stage identities |
| `port/src/audio.c` | Mix procedural accessibility voices into each completed stereo buffer before SDL queueing | Centered targeting tone, single/patterned beacon and cane chirps, toggle and weapon-function patterns, positioned environmental-hazard tone state, and concurrent R-Tracker markers | Clean responsive carriers cannot be made from game samples with finite duration or baked-in modulation | Independent fixed voices share one staging buffer: centered fine aim, one/two/three-pulse beacon patterns, a rising/falling toggle-confirmation lane, seven cane slots, a one/two-beep weapon-function lane, continuous hazards, combat slots, and ten R-Tracker slots; none allocate at runtime |
| `src/game/menutick.c` | Observe the final active dialog/focus once immediately after `menuProcessInput` | Menu slot/player/root/depth and current menu/dialog state | Captures all focus paths after item state settles without hooks in every transition | Implemented in Milestone 4 with one call |
| `src/game/activemenutick.c` | Observe the settled active-menu screen and highlighted slot once after all sampled input is processed | Primary-player active-menu mode, screen index, slot index, and the localized `amGetSlotDetails` label | Weapon/device selection is a gameplay radial rather than a normal `struct menu`; observing after input avoids duplicate speech from intermediate controller samples | Weapon/device screen narration implemented; function and bot-order screens remain deferred |
| `src/game/menu.c` | Expose a read-only focused-item runtime-data lookup | Dialog/item to existing row/block data | Accessibility must not duplicate private row/block mapping | Implemented in Milestone 4 as `menuGetItemData` |
| `src/game/menuitem.c` | Expose type-owned ranking/player-stats summaries only if existing APIs cannot be queried safely by the adapter | Current semantic row/stat labels and values | Compound presentation state is assembled inside type-specific render paths | Audit found no hook necessary; generic scroll/selection summaries are used |
| `src/game/activemenu.c`, `endscreen.c`, `filemgr.c`, `mainmenu.c`, `trainingmenus.c`, and `mplayer/setup.c` | Answer one read-only `MENUOP_GETACCESSIBILITYTEXT` query for focusable custom-rendered rows, carousels, and optional dialog summaries; mark a simple visible label when it is itself the summary | Caller-owned UTF-8 buffer, requested part/index, or the label's normally resolved text | Render callbacks and non-focusable panels otherwise expose pixels/borrowed scratch text, not stable semantics | Implemented in Milestone 4; `trainingmenus.c` supplies firing-range weapon-information, post-session scoring, visible proficiency-star completion, device-training information, and holo-training description summaries; `mainmenu.c` supplies the shared mission-objective provider, rich pause-inventory descriptions, and the marked PC exit prompt; `endscreen.c` supplies mission-result panels and opts every endscreen/retry objective page into the shared provider; reused handlers in `fmb.c` require no duplicate hook |
| `src/game/hudmsg.c` | Publish after a message passes suppression and is queued | Resolved text, type, flags, player, audio channel, message ID | Polling the HUD array loses admission order and reason | Implemented for generic HUD-message narration; types 6 and 11 are logged but explicitly excluded as subtitles |
| `src/game/objectives.c` | Publish inside the changed-status branch of `objectivesCheckAll` | Objective index, previous/new state | The existing HUD text can duplicate or omit useful objective identity | Proposed |
| `src/game/chraction.c` | Optional later directional damage event after actual player damage | Victim player, magnitude band, direction/source category | Snapshot detects loss but not source/direction | Question; not needed for first status query |
| `src/game/lv.c` | Capture projected target bounds, onscreen hostile-character props, and the existing query-ray hit coordinate before prop rendering, then observe after player sight/HUD rendering | Finite projected bounds, final filtered `lookingatprop`, exact query hit, native sight state, player viewport, and rendered character membership | PC prop rendering converts float model matrices in place before sight/HUD state is final, so one hook cannot safely obtain both states | Two narrow calls in `lvRender` support both firing-range and generic hostile-character profiles; query hits are accepted only when the prop survives the profile's final semantic filtering |
| `src/game/bondgun.c` | Observe effective weapon function after gameplay weapon processing | Player, stage, equipped weapon, and the final `bgunIsUsingSecondaryFunction()` value | The visual state includes persistent configuration and temporary inversion; observing the input alone would announce failed commands or misclassify special functions | One end-of-tick semantic observation; initial/stage/weapon changes establish silent baselines and only same-weapon state transitions cue |
| `src/game/prop.c` and `src/include/game/prop.h` | Offer an optional hit-coordinate result from the existing non-shooting aim query | Selected query prop and its already-calculated collision point | Fine aim cannot truthfully use projected bounds, and repeating the collision query would duplicate expensive work | `propFindAimingAtWithHit` wraps the unchanged query path; ordinary callers and shot behavior remain unchanged |
| `src/game/sight.c` | Expose sight-validity/friendliness helpers to adapter | Eligibility and relationship | Avoid duplicating sight rules | Question; prefer existing public APIs if sufficient |
| `src/game/radar.c`, `src/include/game/radar.h` | Expose one read-only R-Tracker marker classification and make the native renderer consume it | None/yellow/blue/character category for an active prop | A second copy of cheat, cloak, death, and flag rules could drift from the visual radar and disclose different targets | Implemented for the nonvisual R-Tracker slice; rendering output is otherwise unchanged |
| `src/game/propobj.c`, `src/include/game/propobj.h` | Expose pure IR/X-Ray renderer queries and the native potential-interaction predicate | Conditional-scenery/infrared highlight state, X-Ray range, and broad object interaction semantics before range/facing checks | Copied flag, movement-state, or eraser math could drift and announce a different object set | `objIsHighlightedByInfrared`, `objGetXrayHighlightDistance`, and `objIsPotentiallyInteractable` are shared with their native consumers |
| `src/game/propsnd.c` | Reuse public read-only distance-volume and pan calculations for procedural spatial cues | World position, distance, range, volume, and pan | Procedural cues should retain the tested spatial behavior without allocating or stopping gameplay channels | No hook needed; beacon and hazard cores call `psCalculateVolumeFromDistance` and `psCalculatePan` |
| `src/include/constants.h` | Reserve `PSTYPE_ACCESSIBILITY_TARGETING` | Prop-sound ownership for target-presence cues only | Targeting must stop/reuse only its own positioned sample, never gameplay sounds | Targeting owner added in the Milestone 9 firing-range slice; the obsolete beacon owner was removed when beacons moved to the procedural chirp lane |
| `port/include/input.h` | Use provisional context-sensitive PC F4/F5/F6/F7/F8 accessibility keys and expose both Alt modifier bits | Development-only action identifiers | Gameplay uses F4 for virtual-cane mode, F5/F6/F8 for interactable/door/pickup scanners, and F7 for non-hostile people; menus retain their F5/F6 contexts; Alt+F4 must not change cane state | F7 was previously absent from the virtual-key enum and unbound in the default PC input path; replacement by Milestone 6 actions/settings remains required |
| `port/src/input.c` | Add configurable accessibility actions or a dispatch boundary | Repeat, status, beacon/scan, cancel, navigation commands | Current binding model represents game controls, not a separate action set | Proposed for Milestone 6; provisional keys require no binding-model change |
| `port/src/input.c`, `port/src/optionsmenu.c`, `src/include/constants.h`, `src/game/bondmove.c` | Add a configurable reset-view gameplay action using the unused extended control bit | Pressed edge from End, R3, or a player-selected replacement binding | Gives a deterministic horizontal-orientation recovery command without changing yaw or bypassing the binding system | Implemented as `CK_1000`/`BUTTON_RESET_VIEW`; PC defaults are End and right-stick click |
| `port/src/optionsmenu.c` | Add an accessibility settings entry/dialog | Existing registered values, including proven beacon actions | Users need discoverable control without editing `pd.ini` | Proposed for Milestone 6 after beacon behavior is tested |

`src/game/player.c` is a confirmed future semantic source but does not need Milestone 4 hooks. `src/game/mainmenu.c` has one read-only semantic-provider case for its custom-rendered mission list; it contains no speech policy.

## Known uncertainties and required experiments

1. **Windows speech technology:** Milestone 3 implements pinned Tolk commit `e5149f0cb6ef9b941673017e0e7b7c409e485fbe` as a dynamically loaded shared library. NVDA 2026.1 runtime requests, Unicode conversion, cancellation, missing-dependency behavior, and clean unload passed; other readers and future compatibility remain unverified.
2. **Threading:** determine whether native speech can be pumped without blocking and which calls must occur on the main thread or a COM-initialized worker.
3. **Menu semantics:** Milestone 4 implements the stable post-input observer and current type/provider matrix. Compilation and lifecycle smoke evidence exist; callback lifetime, localization variants, compound-control usefulness, full input parity, first-focus timing, and the complete scripted interaction matrix still need runtime and blind-user verification.
4. **Localization:** test all supported ROM configurations for string resolution and region-specific control codes; determine how new accessibility-only strings will be translated.
5. **HUD duplication:** correlate subtitle splitting, HUD duplicate suppression, objective-generated HUD messages, audio channels, and cutscene transitions.
6. **Multiple players:** define which local player's focus/status owns speech and how simultaneous events are identified or suppressed.
7. **Input:** determine a collision-free binding model and whether accessibility commands work while paused, in menus, and during gameplay.
8. **Status sampling:** choose a stable tick placement and verify health, shield, weapon, ammo, death, stage changes, scripted changes, and pause behaviour.
9. **Target truth:** define stable semantic identities and ensure target/scanner output does not reveal cloaked, occluded, scripted, or otherwise unknown entities.
10. **Navigation model:** inspect room/portal/pad data and prototype one training path; do not assume source geometry yields a usable route graph.
11. **Log storage:** verify `$S` resolution in portable and installed Windows modes; measure real volume and write cost before choosing buffering, size limits, or session rollover.
12. **Shutdown/crash:** verify cancellation and cleanup during normal exit, initialization failure, window close, and crash-handler interaction without making speech a crash dependency.
