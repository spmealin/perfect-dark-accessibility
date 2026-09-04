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

For the default configuration, the resulting executable is
`build/pd.x86_64.exe`. MinGW Windows builds copy `libwinpthread-1.dll`, the
architecture-matching shared GCC runtime, `SDL2.dll`, and `zlib1.dll` from the
active compiler's binary directory into `build/` with `copy_if_different`.
Together with the existing Tolk/controller runtime target, this makes the
developer output directly launchable from Windows Explorer without relying on
the MinGW shell's `PATH`. Missing runtime inputs fail configuration with their
resolved path rather than producing an incomplete output silently.

The MinGW-only `pd_zip` target stages those runtime files, the executable,
license notices, a newly generated package configuration, and a ROM-placement
notice before creating `build/dist/pd.zip`. The PowerShell wrapper
`tools/build_windows_dist.ps1` performs both configuration and this target
through an initialized MinGW64 bash. `PD_PACKAGE_ACCESSIBILITY` defaults to
`OFF`, producing a package with top-level accessibility, logging, and speech
disabled; it does not change the developer's `build/pd.ini`. ROMs, extracted
assets, saves, logs, diagnostics, and personal configuration are never package
inputs.

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

`src/game/hudmsg.c:hudmsgCreateFromArgs` is the common sink used by the public HUD-message wrappers. After the native subtitle-option and alive checks, a semantic hook publishes each message before the visual queue's duplicate and capacity policies. This preserves every back-to-back pickup event for speech without changing which messages the sighted HUD allocates or renders. The accessibility adapter still excludes in-game and cutscene subtitle types.

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

Targeting uses a two-phase observation. It captures fixed-size projected bounds immediately after aim and tracked-prop calculation, before PC prop rendering converts model matrices in place, then consumes and clears that cache after sight/HUD rendering has finalized native alignment state. The firing-range source admits active, undestroyed `MODEL_TARGET` props; the combat source admits ordinary onscreen hostile character props, hostile `OBJTYPE_AUTOGUN` props, active `OBJTYPE_CCTV` security cameras, and aim-only object candidates when the retained attack ray provides the required semantic hit. Breakable obstructions share one accessibility predicate with the cane: an active, enabled, non-hidden, healthy, mortal object carrying `OBJFLAG_PATHBLOCKER`; the targeting adapter additionally checks the active attack family against native gunfire/explosion immunity flags. Destructible loot containers use native parent/child ownership. The same exact attack ray also admits a generic destroyable-object fallback for active, enabled, visible, healthy, mortal object props that the selected attack can damage. More-specific path-blocker and loot-container categories take precedence, while the fallback uses no stage or model allowlist and creates no presence cue or enlarged target area. Other ordinary objects may also be admitted when the finalized `lookingatprop` is the same retained exact query hit and the native sight system has kept its `OBJFLAG3_REACTTOSIGHT`/`sightIsReactiveToProp` result, directly mirroring the visible blue/red reactive reticle and rejecting destroyed objects without stage or model identifiers. Autoguns reuse native health, deactivation, ammunition, malfunction, and target-team state, so disabled/destroyed/non-threatening turrets and the player's deployed Laptop Gun are excluded from hostile presence without stage identifiers; an otherwise healthy mortal object may still truthfully receive only the interrupted aim lock when directly hit by the query. Cameras reuse native health, deactivation, and camera-disabled state. Their presence visibility samples five bounded points inside the finite projected rectangle, requires each attempted point to hit actual model polygons, and applies semantic visual line of sight to the resulting world surface; this admits a genuinely exposed portion over cover without treating the coarse rectangle as visible geometry. Characters match the finalized native target, while object categories use the retained non-random attack-query hit for direct crosshair alignment. Ordinary combat naming, multiplayer output, and special-sight behavior still need investigation.

The R-Tracker is a separate semantic radar rather than an extension of combat targeting. `radarGetRTrackedType` is the single eligibility boundary used by both the native renderer and the accessibility adapter: yellow/blue object flags, the blue-marker cheat gate, and tracked-character life/cloak state remain native policy. The adapter scans active props only while the native device is active, assigns stable identities to ten fixed oscillator voices, and communicates category, bearing, front/rear, horizontal distance, and relative height. It intentionally preserves the visual radar's lack of line-of-sight, room, and render restrictions.

The Combat Simulator audio radar observes the final marker stream inside `radarRender` rather than reconstructing radar eligibility from world props. `accessibilityCombatRadarCaptureBegin`, `CaptureDot`, and `CaptureEnd` copy a bounded semantic snapshot while the native renderer applies its existing options, team, cloak, death, and scenario callbacks. The main-thread adapter then schedules an F3 clockwise snapshot and Shift+F3-controlled enemy contact transitions from copied identities, categories, coordinates, native colors, and vertical state. Sixteen snapshot slots, sixteen contact records, a 48-entry priority queue, and one procedural mixer voice are fixed storage. Stage and scope transitions discard queued events and stage identities before their owners can become invalid. The adapter deliberately inherits native radar information semantics, including markers outside the viewport or behind walls, and supports only one-local-player normal Combat Simulator gameplay.

King of the Hill adds a separate semantic objective beacon without changing scenario code. The adapter polls the scenario's floor-adjusted `g_ScenarioData.koh.hillpos` after `lvTick`; the scenario derives this position and its occupancy room from the same selected hill pad, and uses the position for the radar dot. A bounded line-of-sight and marker-range policy supplies the stronger local beacon. The Combat Simulator capture additionally records whether the null-prop King of the Hill marker was actually passed to `radarDrawDot`; only that final native-render evidence enables the quieter unrestricted radar guide. One dedicated fixed mixer voice reproduces the player-marker dual sweep with a one-second single identity chirp, preserving all four player-authored marker slots.

The IR Scanner continues to reuse the R-Tracker's ten voices for its viewport-bound special highlights. `objIsHighlightedByInfrared` is shared with `objRender`, so IR admits only conditional-scenery and explicit-infrared objects that receive the special white highlight. X-Ray does not own a generic audio lane because its renderer recolors every nearby prop without semantic distinction. Instead, `accessibilityVisibilityIsXrayExposed` is a shared alternate-visibility predicate used by the beacon and combat-target adapters. It combines the uninhibited X-Ray Scanner device bit, native vision mode, `PROPFLAG_ONANYSCREENPREVTICK`, and `objGetXrayHighlightDistance`; qualifying props retain their normal semantic category and sound while bypassing only ordinary room-neighbour and physical line-of-sight gates. Existing actionability, collectability, relationship, health, viewport, range, capacity, and lifecycle policies remain authoritative. The preceding-frame flag introduces one frame of deliberate latency while avoiding another projection pass or off-screen disclosure, and device-bit gating excludes the Farsight's separate X-Ray vision mode.

### Input, audio, native platform, and repository boundaries

`port/src/input.c` and `port/include/input.h` implement SDL keyboard, mouse, and controller input, binding persistence, and direct key/button queries. Existing bindings primarily represent emulated game controls. Accessibility commands such as repeat, cancel, status, and scan need a collision-free action design rather than scattered hard-coded keys.

The view-orientation recovery action uses the existing extended-control binding system rather than a direct key poll. The previously unused `CK_1000` control bit is exposed and persisted as `RESET_VIEW`, with End and controller right-stick click as the PC defaults. Using a new persisted name prevents legacy `CK_1000=NONE` entries from suppressing the new defaults in existing configuration files. `bmoveProcessInput` consumes its pressed edge after normal vertical-look processing, sets `vv_verta` to zero, and clears vertical speed and automatic-centering state while leaving `vv_theta` unchanged. Because it is a normal persisted binding, players can rebind or remove it through the extended controls menu.

Native audio output is initialized in `port/src/audio.c`; the game-facing chain includes `src/lib/audiomgr.c`, `src/lib/snd.c`, `src/game/propsnd.c`, and `src/game/music.c`. `src/game/chraicommands.c:aiSpeak` and prop-sound functions associate dialogue audio with subtitle text. Speech should remain a separate service so it does not enter the game sound mixer accidentally; later earcons may deliberately use an appropriate audio interface after volume and channel behaviour are tested.

`src/include/platform.h` defines `PLATFORM_WIN32`, POSIX platform variants, architecture/endian macros, and `PD_CONSTRUCTOR`. Windows-specific implementation is spread across guarded code in port services such as `port/src/system.c`, `port/src/fs.c`, and `port/src/crash.c`, with the SDL video backend under `port/fast3d`. `dist/windows/icon.rc` is packaging metadata, not a speech integration point.

`port/fast3d` carries its own license and is effectively a third-party rendering subsystem. `port/include/external/minimp3.h` is vendored, and `tools/recomp` is a Git submodule. Accessibility work should avoid all three. The ignored `build/` directory, ignored `src/generated/`, ignored `extracted/` content, and region-specific generated asset outputs are not sources to edit or commit.

Version conditionals such as `VERSION`, `PAL`, and `PLATFORM_N64` occur in relevant game files, including menu, HUD, and subtitle paths. Canonical asset descriptions under `src/assets/<ROMID>/` also differ by ROM. New hooks must compile on each supported branch of those conditionals and must use normal localization/state APIs rather than addresses or data from one ROM configuration.

## Accessibility module layout

The implemented accessibility-owned modules are:

```text
src/accessibility/
  accessibility.c                lifecycle, configuration, and feature gates
  accessibility_announcement.c   centralized speech request groups and retained menu text
  accessibility_beacon.c         object, door, pickup, and non-hostile-character scanners
  accessibility_cane.c           live nine-angle movement/terrain orientation cue
  accessibility_compass.c        cardinal-crossing speech and numbered click policy
  accessibility_hazard.c         damaging-laser semantic adapter and sweep policy
  accessibility_hill.c           King of the Hill center and native-radar guide policy
  accessibility_hud.c            semantic HUD-message events and direct respawn overlay adapter
  accessibility_incident.c       Shift+F2 bounded history and semantic state capture
  accessibility_log.c            buffered structured development log
  accessibility_landmark.c       authored semantic landmark registry and playback
  accessibility_marker.c         four player-authored landmark slots and LOS policy
  accessibility_menu.c           menu semantic snapshots, formatting, and repeat/cancel
  accessibility_observer.c       active player/CamSpy perspective and collision-pose adapter
  accessibility_path_blocker.c   shared breakable-route-obstruction semantics
  accessibility_performance.c    compile-time-optional bounded graphics diagnostics
  accessibility_speech.c         speech lifecycle and UTF-8 output boundary
  accessibility_stance.c         effective player-stance observation and cue policy
  accessibility_targeting.c      generic fixed-capacity target policy and audio state
  accessibility_targeting_game.c range/combat/security/device semantic source adapters
  accessibility_tracker.c        R-Tracker and IR/X-Ray semantic adapters and fixed slots
  accessibility_visibility.c     shared semantic visual-line-of-sight policy
  accessibility_weapon.c         weapon-change speech and function-state earcons
src/include/accessibility/
  *.h                            narrow contracts corresponding to the modules above
port/src/accessibility/
  accessibility_tone.c           fixed procedural mixer and atomic command bridge
  speech_null.c                   unavailable backend for non-Windows targets
  speech_tolk.c                   dynamically loaded Windows Tolk backend
port/include/accessibility/
  accessibility_tone.h           platform mixer command/diagnostic contract
```

Likely later modules remain:

```text
src/accessibility/
  accessibility_events.c         normalized prioritized event queue, if evidence requires it
  accessibility_status.c         queryable player snapshots
  accessibility_navigation.c     route/goal guidance, separate from the virtual cane
src/include/accessibility/
  accessibility_events.h
  accessibility_status.h
  accessibility_navigation.h
```

An implementation may use fewer files initially. The dependency direction should remain:

```text
game semantic hook/query
        -> accessibility semantic adapter/core
        -> announcement and logging policy
        -> speech backend interface
        -> Windows technology or null backend
```

The game must not depend on a native speech implementation. Feature adapters
must publish speech through `accessibility_announcement.c`, not call a platform
backend or Tolk directly. Lifecycle diagnostics may call the core speech
contract for its explicit backend test. Logging should observe normalized input
and announcement outcomes, not intercept backend internals as its only evidence
source.

## Core contracts

### Lifecycle

Milestone 2 implements:

```c
void accessibilityInit(void);
void accessibilityShutdown(void);
s32 accessibilityIsEnabled(void);
```

`port/src/main.c` calls initialization after `configInit` and shutdown before
configuration/video/crash cleanup. Both calls are idempotent. Disabled operation
is a no-op except when the compile-time graphics diagnostics explicitly request
an accessibility-disabled control log. `port/src/pdmain.c` owns the settled
post-`lvTick` feature coordinator calls and pre-`lvStop` audio/state resets.

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

The core owns lifecycle and request logging. The backend owns native
initialization, strict UTF-8/UTF-16 conversion, cancellation, and
technology-specific error reporting. Windows dynamically loads a separately
built `Tolk.dll` from the executable directory and uses Tolk's default
screen-reader-only policy; SAPI fallback is not enabled. Non-Windows builds
select the null backend. Tolk output is asynchronous internally, but its
non-thread-safe API is invoked on the main thread and every call is timed.
Initialization/detection occurs only at startup; no device enumeration or
detection polling occurs in a frame tick. A worker-backed speech queue remains
an experiment only after Tolk thread/COM ownership is proven.

### Announcements and future queue

The current announcement coordinator owns the replaceable menu group, normal HUD,
weapon-change, direct-rendered weapon-function, and respawn-countdown output,
generic feature status output, cancellation, elapsed
backend timing, and a fixed 12,288-byte retained menu-repeat buffer. Feature
adapters do not allocate retained speech text and do not call the platform
backend. Tolk itself consumes or queues UTF-16 text during its asynchronous
output call.

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
- **Normal:** focus, selected value, weapon change or function label, semantic HUD message.
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
Accessibility.PlayerStatus=1
Accessibility.EnvironmentalHazards=1
Accessibility.InteractableBeacons=1
Accessibility.IRScannerAudio=1
Accessibility.NonHostileBeacons=1
Accessibility.TargetingFeedback=1
Accessibility.TargetingVolume=0.15
Accessibility.WeaponChangeAnnouncements=1
Accessibility.WeaponFunctionCues=1
Accessibility.StanceCues=1
Accessibility.XRayScannerAudio=1
Accessibility.VirtualCaneMode=1
Accessibility.RTrackerAudio=1
Accessibility.AudibleMarkers=1
Accessibility.AuthoredLandmarks=1
```

Navigation and hostile-cue tuning is constructor-registered as bounded floats:

```ini
Accessibility.VirtualCaneReach=900
Accessibility.VirtualCaneFullVolumeDistance=112.5
Accessibility.VirtualCaneFadeDistance=750
Accessibility.VirtualCaneMaximumAudibleDistance=975
Accessibility.VirtualCaneNearFrequency=600
Accessibility.VirtualCaneFarFrequency=300
Accessibility.VirtualCaneTerrainReach=450
Accessibility.VirtualCaneTerrainHeightThreshold=12
Accessibility.VirtualCaneDropHeightThreshold=80
Accessibility.VirtualCaneVolume=0.184
Accessibility.EnemyFullVolumeDistance=600
Accessibility.EnemyFadeDistance=3500
Accessibility.EnemyMaximumDistance=4000
Accessibility.EnemyVolume=0.25
Accessibility.EnemyFrequency=900
Accessibility.MarkerRange=1200
Accessibility.MarkerVolume=1.0
```

Later features may add:

```ini
Accessibility.ObjectiveNarration=1
Accessibility.Verbosity=1
```

The implemented keys are constructor-registered bounded integers or floats in the existing config registry. Cross-field runtime validation enforces ordered attenuation thresholds, keeps the cane audible through its configured reach, normalizes the cane pitch endpoints so near is not lower than far, rejects non-finite values, and caps cane and enemy gain at 0.4. Cane hit distance maps logarithmically from the configured near frequency at contact to the far frequency at maximum reach. Walkable-floor samples ahead of the active observer resolve portal rooms along a body-height horizontal trace, then independently raise the collision query point above the current ground. This avoids the previous upward-sloping room trace that could omit a lower stair room until the player approached its portal. The first elevation change above the configured threshold is selected and its sign is encoded with an upward or downward logarithmic contour centered on that distance pitch. The result passes through the existing per-slot oscillator command without allocation or an additional voice. Effective values are sampled at startup and recorded in the session log; editing `pd.ini` requires a restart. Accessibility settings belong in `pd.ini`, not only in a selected Perfect Dark profile, because startup menus need them. Later key names/ranges remain provisional, and a later in-game settings page should use the same values.

### Playtest logging

Normal builds write `$S/accessibility.log` only when both accessibility and
logging are explicitly enabled; diagnostic builds may also open it for the
explicit accessibility-disabled graphics control. The logger truncates the
prior session and records JSON Lines containing schema, sequence, session,
monotonic microseconds, complete build metadata, category, event, and a detailed
message. It is main-thread-only. A fixed 64 KiB stdio buffer and fixed 4 KiB
format buffer avoid allocation and disk flushes for ordinary events; oversized
messages allocate exact temporary storage so diagnostic detail is not
truncated. The first record after each one-second interval flushes the batch;
lifecycle events and shutdown also flush. Open/write/flush/close failures
disable the logger nonfatally.

`accessibility_incident.c` maintains a fixed 60-entry ring sampled every 15 logical ticks, providing approximately 15 seconds of pre-capture observer, presentation, and input history without per-sample allocation or log traffic. Shift+F2 writes the ring followed by current player, objective, inventory, beacon scheduler, nearby-prop, and view-blocker state through the existing buffered logger. The beacon module additionally selects at most 16 scanner-supported props within 1,200 units and a forward dot product of at least 0.75, ordered by alignment and distance. It reevaluates each through the same private semantic, range, room, and LOS helpers into local result storage, records the exact rejection boundary, and never replaces the live scanner snapshot, schedule, identities, or counters. All semantic collection and collision work stays on the main thread after `lvTick`; the audio callback is not involved. The active-observer adapter makes the capture follow Joanna or the CamSpy consistently with other navigation features. Capture identifiers are process-local and are correlated with the logger's session field. State resets at stage teardown while the capture sequence remains session-local.

The project owner has prioritized diagnostic completeness over privacy minimization during development. The logger may include resolved text, player/profile names, paths, command arguments, precise coordinates, input history, native handles, pointers, and any other feature-relevant state. Do not add redaction or field filtering. The log defaults to enabled for blind-user acceptance testing, remains locally configurable, is ignored by Git, and is never uploaded automatically. Never include ROM contents, extracted copyrighted assets, passwords, authentication tokens, or unrelated operating-system secrets. Size limits, rotation, and public-distribution privacy policy are deferred until actual logging volume is measured.

## Feature architecture

### Menu narration

Milestone 4 uses one post-`menuProcessInput` observation for each menu slot, while the menu slot and current-player context are still valid. Comparing owned snapshots captures final initial focus, keyboard/controller/mouse focus, sibling swipes, push/pop, disabled-item correction, and value/subfocus changes without publishing intermediate transitions.

The menu adapter derives label, role, current value, availability, position/count, and internal subfocus for selectable actions, checkboxes, sliders, dropdowns, standard/custom lists, keyboards, scrollables, carousels, rankings, and player-stat tables. A selectable uses its left text as its label and also announces the dynamic right-side text rendered by `menuitemSelectableRender`; when the left text is empty, that right-side text becomes the label. This exposes generated names and other secondary values without screen-specific hooks. Dynamic callback results are copied immediately. Long scrollable values and dialog summaries have an 8,192-byte normalized-text budget, matching the menu renderer's 8,000-byte briefing source; the composed announcement has a 12,288-byte budget. Presentation-only labels, objectives panels, separators, models, meters, marquees, controller diagrams, and color swatches do not create synthetic focus announcements unless their owning dialog explicitly publishes the equivalent semantic summary.

Custom-rendered list rows, controls with render-only values, and dialogs with important non-focusable content implement one read-only `MENUOP_GETACCESSIBILITYTEXT` operation. Option/control providers expose focused semantics. A normal list whose spoken option needs richer text than its visual row explicitly sets `MENUITEMFLAG_ACCESSIBILITYOPTION`; the core then prefers its `MENUACCESSIBILITYPART_OPTION` provider while leaving visual option text unchanged. A `MENUACCESSIBILITYPART_SUMMARY` provider can expose a localized dialog summary from an item explicitly marked `MENUITEMFLAG_ACCESSIBILITYSUMMARY`. A marked `MENUITEMTYPE_LABEL` without a provider instead contributes its normally resolved visible label text, supporting simple confirmation prompts without a dialog-specific callback. Explicit opt-in is required because some menu definitions store dialog pointers, rather than callable handlers, in the same union field. The accessibility core finds marked providers or labels generically and never identifies a specific dialog to drive narration. Combat Simulator challenge dialogs use this generic opt-in on their non-focusable `DESCRIPTION_MPCONFIG` and `DESCRIPTION_MPCHALLENGE` panels; their providers return the same localized `menuitemScrollableGetText` value used by rendering. Unknown/future focusable types are logged once per state change and never receive invented semantics.

Dialog/focus/value output uses one replaceable menu announcement group: newer state interrupts stale state, unchanged frames are silent, repeat bypasses deduplication, and cancel does not change menu state. The dialog title and optional semantic summary are included only on entry or return to that dialog; focus and value changes within it speak only the current control. F5 reconstructs the title, summary, and current focus for an explicit repeat. A slider first requests `MENUOP_GETSLIDERLABEL` after obtaining its current value, matching the semantic label used by rendering; only a slider without a nonempty formatted label falls back to a rounded percentage of its configured maximum. The shared mission-objectives provider supplies the same difficulty-filtered localized names and numbering rendered on pre-mission, pause, endscreen, retry, and next-mission objective pages. It omits state where the renderer does not display one and otherwise appends the renderer's localized Complete, Incomplete, or Failed state. Mission endscreens provide a second semantic summary containing every currently visible status, timing, unlock, weapon, and shooting-stat field; rows hidden to make room for a new-cheat announcement remain absent from speech. The pause Inventory list and firing-range `Weapons Available` list share rich-option formatting for the visible item name, manufacturer, primary and secondary functions, complete marquee description, and active device state; the pause path retains CamSpy variants, mission-specific necklace credentials, and device checkbox state from the same semantic sources as the visual controls. Pause Abort labels explicitly opt into the same static-summary path as the PC exit prompt. The firing-range training-information handler uses the summary contract for its localized weapon name, challenge values, and description. The firing-range scoring model uses the same contract for completion/failure reason, score, targets destroyed, difficulty, elapsed time, weapon, accuracy, per-zone hit counts and points, and hit total. The device-training details dialog also uses this contract to announce its localized description, including the Data Uplink information screen, even though the visual text panel is not focusable. Milestone 4 speaks only menu slot zero while observing/logging every slot. Automatic objective state-change events and final configurable actions remain later work.

The firing-range custom weapon list exposes the same saved proficiency score used to render its three stars. Its accessibility option text maps score values zero through three to no completion suffix, bronze completed, bronze and silver completed, or all three completed, using the existing localized difficulty and completion strings.

The holo-training details dialog follows the device-training summary pattern: its focused `OK`/`Resume` item supplies the localized `htGetDescription()` text already rendered by `DESCRIPTION_HOLOTRAINING`. This exposes the static non-focusable panel on dialog entry and explicit repeat without adding screen-specific speech calls or duplicating text.

### HUD and objective narration

Publish an accepted-HUD event after `hudmsgCreateFromArgs` commits a message. Carry type, flags, player, and audio channel so policy can distinguish subtitles from pickups and system notices. Publish objective index and new state from `objectivesCheckAll`; the objective adapter can resolve localized objective text separately. Objective state events take precedence over a redundant generic HUD version.

### Status queries

The status adapter runs after `lvTick`, accepts F1 only in unobscured
one-local-player gameplay, and formats caller-owned fixed buffers from semantic
engine state. F1 reports health, nonzero shield, and—during a normal Combat
Simulator match—team identity, scenario state, player score/rank, applicable
limit, and match time. Shift+F1 uses the native team score/ranking calculations
and is silently ignored unless teams are enabled. Alt/Control-modified F1 is
reserved for other software and ignored.

Scenario fields follow the native HUD/radar visibility contract. A Briefcase
hold countdown, Hacker Central download percentage, Pop a Cap survival
countdown, or King of the Hill point countdown is only exposed to the player
whose HUD shows it. Team queries may name public carriers/control state but
never reveal another player's private countdown or progress; Capture the Case
carrier information additionally requires Show on Radar. The adapter
deliberately omits device telemetry, ammunition, and automatic timer warnings.
The common HUD path already speaks the native one-minute warning, and the game
retains its last-ten-seconds alarm.

`lv.c` exposes read-only match-limit getters so the adapter does not own or
duplicate multiplayer limit state. Output uses the existing replaceable status
announcement group and records the resolved report or suppression reason in
the accessibility log. New compact English connective labels are isolated in
`accessibility_status.c`; they require localization before upstream release.
Automatic health thresholds remain future work and should compare semantic
snapshots with hysteresis.

### Virtual cane

The virtual-cane core runs once after `lvTick` on the main game thread. It accepts F4 in ordinary single-player walking, grabbed-object, or hoverbike movement, cycles a constructor-registered `0..2` mode, and schedules nine live angles from -60 through +60 degrees without resetting for movement. Walking/grab/CamSpy fans remain camera-relative. A mounted fan is centered on current bike velocity, falling back to bike heading at negligible speed, while playback remains camera-relative. Moving a grabbed crate retains the ordinary collision sweep. Each grab query temporarily disables exactly the current player's native `grabbedprop` perimeter; each vehicle query instead disables rider and mounted-bike perimeters and uses the native bike cylinder. Walking-only terrain traversal, crouch-passage, and ladder classifiers stay disabled until normal walking resumes. The slow and fast schedules use region-correct logical ticks, include an end pause, skip overdue work after a hitch, and enforce no more than one collision query per tick.

A shared observer adapter selects the prop and pose that own the currently rendered first-person perspective. It returns Joanna's position, rooms, camera vector, and walking bbox normally, or the active CamSpy prop, rooms, camera vector, and the CamSpy movement collision dimensions while `CAMERAMODE_EYESPY` is effective. A companion read-only vehicle snapshot exposes the mounted hoverbike prop, native bbox/ground, heading, velocity, travel direction, speed, turn speed, and mode without replacing the listener camera. The cane permits both contexts even though Joanna is not in ordinary walking movement. An observer or mounted-vehicle identity change clears the old partial sweep before beginning from the new profile.

Each scheduled sample copies the current player movement bbox and follows the walking system's room traversal plus `cdExamCylMove06`/`cdExamCylMove02` ordering. The collision mask is background, objects, doors, and path blockers when normal Bond collision is enabled, otherwise background only; characters and players are excluded. Collision APIs publish through shared global scratch state, so the adapter immediately copies a swept hit's full position/geometry record or derives the destination-overlap fallback from its returned obstacle edge before computing distance, volume, and pan. If that returned prop satisfies the shared breakable-path-blocker predicate, the sample records that semantic classification; it does not search beyond the movement collision or infer from a model number. The query never runs on the audio thread and does not call the state-mutating `bwalkCalculateNewPosition` wrapper.

Ten fixed procedural-mixer slots own nine cane-angle chirps and one context slot shared by mutually exclusive walking-grade and hoverbike blocked-movement cues. Atomic sequences transfer start/end frequency, duration, pattern, configured master gain, distance attenuation, and pan to audio-owned phase/envelope storage; stop clears all slots. A wall uses a steady distance-pitched 35 ms chirp, a breakable path blocker uses a 90 ms falling octave that resolves to the same distance pitch, and rising or falling terrain uses a 140 ms logarithmic contour centered on that pitch. A movement collision carrying `GEOFLAG_LADDER` or `GEOFLAG_LADDER_PLAYERONLY` uses a 165 ms fixed pattern of three ascending rung chirps, preserving distance pitch, attenuation, and the collision point's pan while distinguishing engine-climbable geometry without stage-specific data. Four front-loaded floor samples look for support before the nearest movement collision. A missing floor or downward change beyond the configurable 80-unit default brackets a large drop; five bounded refinements require at least two unsafe results and position a 260 ms steep falling contour at the estimated supported/unsupported transition. The nearer edge ordinarily supersedes a wall beyond it, but a wall or railing before the edge remains authoritative. A blocker less than one live player collision radius beyond a refined edge also supersedes it because the intervening gap cannot accept the player's collision envelope; this prevents lower floor behind or beneath a wall from being announced as a reachable ledge. Vehicle reach is the larger of configured reach or one second of current travel plus radius, capped at three times configured reach; vehicle terrain probes cap at 1,200 units and retain only large-drop results. A local walking observer whose current-height cylinder is blocked receives one conditional full-squat-height sweep just beyond the collision. Terrain up to one player radius before the barrier does not suppress this test; if the squat sweep clears, the co-located crouch passage supersedes ordinary terrain. Its 135 ms fixed mixer pattern contains two 55 ms chirps separated by 25 ms, with the second carrier 1.5 times lower than the distance-pitched first carrier. Large drops, CamSpy, hoverbikes, and an already fully squatted envelope skip this classification. A rejected native bike movement attempt emits a camera-relative 220-to-140 Hz, 90 ms cue from the context slot no more than twice per second. All use the bounded `Accessibility.VirtualCaneVolume` master gain, which defaults to 0.184. The F4 command also publishes the selected mode through the centered toggle-confirmation lane: Slow rises in two beeps, Fast rises with two high beeps after the base, and Off falls in two. The path allocates no native game sound channels or dynamic memory. One preallocated text buffer aggregates all nine sample records and uses the logger's preformatted event API, avoiding periodic formatting allocation; the logger batches ordinary disk flushes. Advanced diagnostics expose active profile, vehicle speed/reach/self-exclusion, floor-query/refinement records, drop/barrier gap suppression, current surface plane and grade, per-ray continuation decisions, terrain/crouch/ladder classification, tone pattern, standing and crouch collision results, collision timing/counters, and mixer request/active masks.

An upward-terrain candidate also receives at most four read-only `cdTestVolume` clearance checks at its existing floor samples. Each check shifts the current player cylinder to the sampled floor while preserving the live `playerGetBbox` radius, lower step margin, and animated upper extent; it therefore evaluates stand, duck, squat, and intermediate stance transitions without synthesizing a posture. The player perimeter and slope mode are restored immediately after the bounded query. A clear rise cedes priority to any farther movement collision. Without a farther collision, it cedes to silence only if two samples establish a stable plateau whose beginning lies within 25 percent of terrain reach.

For a local walking observer, one read-only native floor query at sweep start captures a point and polygon normal defining an absolute world-space plane. Each ray compares its four existing floor samples with the world height predicted by that plane, avoiding dependence on a player ground height that may change during the sweep; only a match within the configured terrain-height tolerance can classify a safe continuation. Upward continuations must also pass the live-stance traversal checks. A safe continuation cedes to a farther wall or silence, while step-flagged geometry, plane deviations, starts, ends, stairs, blocked rises, and drops keep the established ray semantics. Once all nine ray slots finish, a second read-only floor query refreshes the current plane and camera-forward grade before one centered 140 ms grade contour plays in the existing end pause through the context slot. A 3.5-percent enter and 2.5-percent exit threshold provide hysteresis. It communicates current ascent/descent without masking spatial geometry ahead or replaying a stale start-of-sweep direction. The classifier adds no allocation, adds two floor queries per complete sweep rather than per ray, and remains disabled for CamSpy, grabbed-object, and mounted movement. Aggregate diagnostics record both surface points/normals, the forward and ray grades, maximum plane residual, safety/suppression decision, and grade-context emission. Collision-query helpers immediately copy the engine's global scratch result, while a bounded guard owns every temporary player/prop perimeter and slope-mode change and restores them through one path. The guard records only perimeters that were enabled when acquired, so cleanup cannot accidentally enable collision that another engine operation had already disabled.

### Accessible compass

The compass is independent of virtual-cane policy even though the two provisional commands share F4. Plain F4 reaches only the cane; Shift+F4 reaches only the compass; Ctrl and either Alt modifier reject both. Both policies keep their own physical F4 edge state because the legacy `inputKeyJustPressed` helper consumes its edge when first queried; modifier filtering then selects exactly one action from that shared press. Its registered mode persists as `0=off`, `1=speech and sonification`, or `2=sonification only`. The core samples the shared active observer's camera look vector after `lvTick`, projects it horizontally, and follows the native `atan2(-look.x, look.z)` view-angle convention, mapping world `+Z/-X/-Z/+X` to North/East/South/West. It unwraps the shortest per-tick angular displacement across zero degrees and reports an axis only when rotation crosses its exact multiple of 90 degrees. Six-degree same-axis hysteresis prevents controller or mouse jitter from chattering, and an observer identity change establishes a silent new baseline.

Each crossing sends one atomic command to a dedicated centered procedural voice: North has one 30 ms 600 Hz click, East two, South three, and West four, separated by 65 ms. A short harmonic component and decaying envelope distinguish the cue from pure scanner sine chirps. Speech mode publishes the matching isolated cardinal label through the announcement boundary with replacement semantics at the same main-thread event; sound-only mode makes no ongoing speech request. Mode changes use one-time spoken confirmations in every mode so a screen-reader user can identify the resulting selection. The audio path is fixed-storage and allocation-free. Menu, pause, cutscene, death, unsupported-player, stage-stop, observer-change, and shutdown scopes stop a partial pattern while retaining the configured mode.

### Target and scanner

The shared accessibility relationship adapter applies native blue-sight protection first, classifies `TEAM_NONCOMBAT` as neutral because native AI acquisition explicitly excludes that team, then applies native friendly and generic enemy comparisons. Targeting, Combat Simulator radar classification, and the F7 people scanner consume this one result, so perceptible non-combatants use the neutral-character drone rather than a hostile cue and script-driven team changes reclassify without stage or model allowlists. Cloak/IR, hidden, untargetable, life/action, render, and line-of-sight state remain separate eligibility filters.

The F7 scanner retains relationship in each fixed result and drone slot. All eligible non-hostile people share a steady 440 Hz fundamental and 550 Hz major third. Neutral and protected relationships keep both components continuous; friendly relationships gate only the upper component on for one second and off for one second with ten-millisecond mixer-side edges. A relationship change restarts that slot's modulation on the next bounded refresh. This adds one atomic semantic flag and one fixed sample counter per existing voice, with no allocation or additional mixer channel.

The environmental-hazard adapter traverses active props after `lvTick` and recognizes the engine's semantic damaging laser-door type. It derives a beam centerline from the live door model bounding box and rotation, computes distance and facing against the closest point on that segment, and requires a background-only line-of-sight ray. One retained nearest candidate drives a 220 Hz source along the segment and back. This is independent of setup tags and can apply to matching laser barriers in other stages without disclosing inactive, distant, occluded, or rearward hazards.

The targeting core accepts bounded observations containing source/profile, stable identity, category, relationship, a generic shootability state, position/distance, projected bounds, optional localized name, optional normalized aim quality/raw aim distance, an obstruction classification, and one aimed identity. It owns per-player fixed arrays, two-frame visible/removal debounce, identity-preserving sort/round-robin state, one dedicated procedural firing-range presence lane, and one centered procedural alignment lane. Visibility and shootability are deliberately independent: an admitted target remains in the positioned presence rotation when temporarily unshootable, while centered alignment output requires the aimed candidate to be explicitly shootable. The alignment lane derives an interrupted semantic flag for `ACCESSIBILITY_TARGETING_RELATIONSHIP_PROTECTED`, `ACCESSIBILITY_TARGETING_CATEGORY_BREAKABLE_PATH_BLOCKER`, and `ACCESSIBILITY_TARGETING_CATEGORY_LOOT_CONTAINER`. The port audio layer renders those locks as 90 ms of sound followed by an exact 10 ms silent gap in each 100 ms cycle, with two-millisecond edges inside the sounding portion to avoid clicks. A separate penetrable-obstruction flag applies a 12 Hz amplitude tremolo between 55 and 100 percent gain without modifying the carrier frequency, preserving character body-region pitch. Both pattern flags can coexist. All other admitted categories retain the solid lock. The firing-range adapter derives shootability from the existing `frIsTargetFacingPos` rule, so a back-facing target immediately stops alignment and reacquires it after rotating toward the player without duplicating range angle thresholds. The firing-range profile uses a 100 ms one-shot with the configured enemy carrier, 78/22 fundamental/harmonic blend, and master gain; a 36-tick base cycle is divided among visible targets with a six-tick minimum. It does not allocate a game sound channel or occupy one of the ten combat slots. While aligned, it maps normalized aim quality to a solid 660–1320 Hz centered sine tone.

The same adapter supplies a separate generic combat profile from the engine's bounded onscreen-prop list. It admits active, enabled, combat-capable hostile character/player props, hostile autoguns, and active security cameras whose rendered model bounds intersect the viewport and whose semantic aim point has visual line of sight from the camera. Existing team comparison, friendly classification, cloak/IR, hidden, untargetable, dying/dead, and knockout action state provide character filtering; native health, disabled, deactivated, ammunition, malfunction, and team-mask state provide object filtering. Character visibility is a fixed five-point, allocation-free, early-exit probe: center, upper, lower, and camera-relative left/right upper-body positions. This recognizes meaningful partial exposure without treating the renderer's coarse onscreen flag as proof through walls, and records the accepting sample plus query count for performance review. Autoguns and security cameras retain one center ray. The shared visibility adapter tests background, doors, ordinary objects, and path blockers with sight-blocking geometry, but adds `CDTYPE_AIOPAQUE`, so props marked `OBJFLAG_AISEETHROUGH` do not conceal a visually observable target. No stage, holo-training, model, or script identifier participates. Characters normally use the finalized native `lookingatprop` selection. Object targets use the separately retained, same-frame non-random query-ray prop for direct alignment. The query now exposes an optional second hit from the already-populated ordered shot list only when the first hit is native non-bulletproof glass that does not slow the bullet. The glass predicate covers standalone glass objects and only `MODELPART_WINDOWEDDOOR_0003` on `g_SkelWindowedDoor`; opaque door geometry, invincible glass, and bulletproof glass remain blockers. This permits an exact combat lock on the real character/object hit behind the pane without an additional collision query. Device and firing-range profiles ignore the optional penetrated hit. When that query hits no prop, autoguns alone have a bounded three-logical-pixel fallback around their finite projected model rectangle. Selection minimizes distance to the rectangle, then its center and stable prop number. It reuses the already captured active/hostile/rendered/viewport/center-line-of-sight state, performs no extra world query or allocation, and is suppressed whenever any foreground prop owns the exact ray. The fallback is feedback tolerance rather than a claim that randomized gunfire will hit.

When the active right-hand function carries `FUNCFLAG_THREATDETECTOR`, the adapter copies the exact four-entry `player->trackedprops` list maintained by `lvFindThreats` for the sight's visible threat boxes into a bounded observation side channel. It deliberately does not duplicate the threat rules: native code decides whether an autogun, Skedar shuttle, dangerous grenade/mine, secondary-function Dragon, or one-hit explosive firing-range target occupies a slot. Accessibility validates the stable prop, active/rendered state, and native screen bounds. In the combat profile it also prepends those entries as hostile candidates and uses the retained non-random attack-query prop for direct alignment. Prop-identity deduplication prevents an autogun already present in the generic projection set from consuming two candidates or voices. Native threat entries are ordered before generic candidates so fixed candidate capacity cannot hide information already shown by the detector; the native four-slot visual capacity remains authoritative. In the firing-range profile, ordinary target presence and scoring aim remain unchanged while the side channel identifies which subset the detector has newly boxed.

The targeting core owns fixed eight-entry known and pending-alert arrays for the native threat side channel. A newly observed identity is queued once; identities remain known through two missing observations to absorb transient native-slot loss. Pending identities are revalidated against the current native list before playback and dropped if no longer visible. One dedicated mixer lane serializes simultaneous additions at 12-tick intervals as positioned 120 ms 1,000-to-2,000 Hz upward sweeps using the combat harmonic waveform, configurable enemy range curve, and enemy master gain. The core also compares the final valid aimed candidate's prop identity with the current native threat set across both combat and firing-range profiles. A match immediately triggers the same sweep and repeats it every 15 ticks while exact shootable aim remains; the ordinary alignment lane continues independently beneath it. A just-started new-contact sweep satisfies the first aimed pulse without restarting the oscillator. Aim loss stops an aim-owned sweep immediately, while a separate new-contact sweep is allowed to finish. Scope loss, detector deactivation, profile reset, and shutdown clear both state and audio. This event/aim lane is independent of recurring range presence, combat slots, scanner chirps, and centered alignment, uses atomic commands plus fixed mixer state, and allocates no memory or native sound channel.

A third, alignment-only device profile handles confirmed special-item target contracts. A fixed registry carries stage, weapon, target tag, expected prop type, minimum difficulty, and scope and permits multiple rows for one item. Its campaign audit covers the three CI exercises plus every setup-script object that is explicitly paired with an equipped or thrown mission device: both Defection ECM hubs and its Uplink PC; Investigation's Uplink PC; Infiltration's Comms Rider antenna and Explosives terminal; Rescue's three Uplink terminals; Escape's Auto-Surgeon hoverbed on Special Agent and above; G5's Door Decoder keypad; Chicago's two Remote Mine door surfaces, Reprogrammer hovercab, and Tracer Bug limousine; Defense's Uplink shuttle; and the three Skedar Ruins Target Amplifier pillars. `documentation/ACCESSIBILITY_SPECIAL_DEVICE_TARGET_AUDIT.md` records the exact tags, native checks, inventory-only exclusions, and maintenance rule. Door targets explicitly require `PROPTYPE_DOOR`; all other current rows require `PROPTYPE_OBJ`, preventing a reused tag from silently admitting a different prop class. Campaign rows use equipped-weapon and minimum-difficulty scope rather than training-session scope. These point-target devices compare against the raw same-frame non-random attack-query prop because ordinary `lookingatprop` filtering can discard their valid objects. The CamSpy path instead traverses the engine's active `criteria_holograph` list, so it generalizes to CI training and campaign photo objectives without a stage or model table. During the pre-render capture phase it copies only fixed-capacity semantic/projection values and applies the same incomplete-status, health, rendered/front-facing, 400-unit horizontal range, and fully-inside-viewport conditions used by `objectiveCheckHolograph`. Because a successful photograph is defined by framing rather than a center ray, any surviving criterion drives alignment; the closest screen-center identity wins only to keep multiple valid criteria stable. The core suppresses both round-robin and combat presence for this profile, leaving only the centered 660 Hz alignment lane. This is a semantic target contract, not a model/interactable heuristic; unrelated scenery receives no fabricated target, and placement feedback still does not predict trajectory or promise success.

Combat presence has ten preallocated mixer voices rather than the firing range's single procedural round-robin lane. Stable target identities retain slots while eligible. Character and autogun voices generate a configurable 900 Hz spatial carrier mixed from 78 percent fundamental and 22 percent second harmonic. Horizontal bearing remains native stereo pan; an experimental vertical channel converts the projected target midpoint and `player->crosspos` through the camera's screen-to-direction transform, subtracts their vertical angles, normalizes the result across the port math library's zero-to-tau `atan2f` seam, then maps that signed aim error onto carrier pitch with a five-degree aligned dead zone, an exponential two-thirds-to-five-thirds multiplier over clamped minus/plus 45-degree errors, and per-observation 0.35 smoothing. The adapter captures that complete calculation beside the targeting query and before rendering mutates or advances camera/model state; the audio policy consumes the immutable angle instead of recomputing it later. `player->crosspos` is the same aim point used by `propFindAimingAtWithHit` through `bgunCalculatePlayerShotSpread` in query mode, so player position, floor elevation, and a potentially different camera-center vector cannot bias the cue. Diagnostics preserve the aim coordinate, projected midpoint, normalized-but-unclamped angle, and clamped audio angle. A per-slot atomic contour flag makes the first half of each noncontinuous enemy chirp use the normal carrier and the second half use the smoothed elevation carrier. Alignment therefore collapses into one uniform beep, while a mismatched second half communicates which vertical correction is required. This midpoint change preserves oscillator phase and needs no additional voice or audio-thread allocation. The close-range continuous mode cannot encode two temporal halves, so it retains the smoothed elevation carrier and returns to the normal carrier at alignment. Their mode, period, and duration derive from target distance and the semantic unarmed melee range. The adapter reads `weaponfunc_melee.range` from the unarmed primary function (`X=60` in the current data) rather than duplicating the gameplay constant. It supplies a near-surface cue distance derived from camera-to-body-midpoint distance minus character radius. At `>=5X`, period/duration are 500/180 ms; the values interpolate linearly to 200/50 ms at `X`; at `<=X`, the voice becomes continuous. The audio bridge has a separate immediate-trigger sequence so every accumulated 40 ms period reduction advances the next chirp rather than inheriting latency from the previous longer cycle. Entry to continuous mode remains exactly `X`; after entry, an exit-only `1.1X` threshold absorbs collision/animation jitter without prematurely claiming melee range. A 10 ms gain ramp removes clicks at continuous-mode transitions. Security-camera voices use the same allocation-free harmonic oscillator and spatial controls but leave the contour flag clear, retaining their independent linear 1,600-to-1,000 Hz sweep over 140 ms every 500 ms instead of presenting melee-distance or elevation semantics. This does not claim a guaranteed hit because the actual attack ray, aim, target geometry, occlusion, and attack animation remain authoritative.

Deterministic phase offsets prevent simultaneous starts, and live prop-sound attenuation/pan/cadence updates each logical observation. Dying, dead, knockout-fall, and knocked-out characters release their slot immediately; threat-detector objects release when the native slot disappears or the detector function becomes inactive. Empty slots are filled in the core's existing screen-center/distance order; additional candidates wait for a release instead of displacing an audible identity every time ordering changes. Direct alignment remains a separate centered tone, but the core admits it only when the active profile reports the native target indicator visible. The game adapter mirrors `sightDraw`/`sightDrawTarget`: it requires a non-melee sight and the native Sight On Screen option, then accepts either the Always Show Target marker outside fine aim or a sight type that renders the marker while fine aim is active. A hidden indicator immediately clears alignment identity, grace, and threat-aim repetition while leaving semantic candidates available to positioned presence and distance feedback; this prevents the unarmed camera-center melee query from exposing an invisible reticle without suppressing ordinary weapon markers such as the Laptop Gun. Character locks classify the already-selected model hit part into head, arm, or standard regions and apply 825, 528, or 660 Hz respectively; non-character locks retain 660 Hz, and the firing range retains its independent scoring-quality curve. Combat characters alone receive a two-observation release debounce while the prior identity remains an otherwise valid visible candidate; target changes, a hidden indicator, and semantic invalidation remain immediate. Object/device/range alignment remains exact-ray except for the bounded turret tolerance described above. Rate-limited observations record `indicator_visible`, the derived `alignment_permitted` gate, and `target_indicator_hidden` suppression reason; combat scope diagnostics additionally record the current weapon/function, threat-detector state, effective native X/Y auto-aim enablement, both native auto-aim prop identities, raw query identity/hit part/region, tolerant turret identity, distance from its projected bounds, configured tolerance, and final alignment source. Profile transitions clear identities and all owned targeting lanes. All candidate, projection, slot, oscillator, and mix storage is fixed-capacity with no runtime allocation.

The firing-range aim-quality source reuses the exact hit coordinate calculated by the existing non-shooting `FINDPROPCONTEXT_QUERY` path that selects `lookingatprop`. A narrow optional-output wrapper exposes that coordinate and the already-selected model hit part without changing ordinary callers, firing, randomness, collision order, or weapon spread. The adapter retains the raw same-frame prop/coordinate/hit-part tuple for semantic device comparison and character body-region feedback, and separately retains the coordinate tied to the final filtered aimed prop for firing-range quality. It computes the same Euclidean target-center distance used by `frCalculateHit`, then normalizes continuously over a 75-unit outer scoring radius. Projected rectangles remain visibility diagnostics and never determine fine aim. The PC backend generates the sine oscillator after the normal game mix and before SDL queueing; it preserves phase, interpolates frequency, applies a 10 ms gain ramp, mixes equally into both channels, and uses a fixed staging buffer with no runtime allocation or game sound handle. Its centered alignment gain comes from bounded `Accessibility.TargetingVolume`, default `0.15`; positioned combat presence retains the separate `Accessibility.EnemyVolume` control.

The combat profile has its own configuration-backed attenuation policy rather than inheriting the short firing-range/device envelope. Defaults keep visible eligible hostiles full-distance audible through 4,000 world units, fade through 5,500, and become silent at 6,000. The game adapter also publishes the live gameplay FOV, configured default FOV, and a normalized zoom blend that reaches one after a 25-percent narrowing. Core policy linearly blends the ordinary curve into configurable 6,000/9,000/12,000 scoped distances. This uses camera state rather than weapon identifiers, follows custom FOV scaling, and changes continuously through zoom transitions. An accessibility-local bounded attenuation helper reproduces the established curve shape while allowing scoped feedback beyond `psCalculateVolumeFromDistance`'s native 6,000-unit clamp; it does not alter native game audio. The backend applies a default 0.25 combat oscillator gain after semantic distance attenuation and accepts a 100–4,000 Hz configurable carrier. These values do not broaden eligibility: native hostile relationship, living/combat-capable state, narrowed-viewport presence, and semantic visual line of sight remain mandatory. Rate-limited observation logs include both FOVs, zoom blend, profile name, and effective curve.

Manual-aim precision guidance is a two-pass presentation mode over that same eligible combat set, not a replacement for native targeting. The adapter activates it from the game's semantic sight state while any weapon is manually aimed, rather than from a weapon-name or zoom test. During the existing pre-render projection capture its cheap first pass walks each eligible character, hostile-autogun, and targetable-CCTV model's current node tree and projects collision-box centers through their live matrices. Characters exclude gun/hat boxes and retain at most one nearest-crosshair anchor in each of five fixed semantic groups: head, torso, arm, lower body, and other; fixed objects retain their nearest general collision-box anchor. The character visibility probe supplies a bounded regional preference. `chrCalculateAutoAim` is the character fallback when no model collision anchor is available; fixed objects fall back to the center of their already-validated finite projected model bounds.

The detailed pass is gated by a 12-logical-pixel expansion of the projected model rectangle and runs for at most one target per frame, preferring the core's retained precision identity. It projects five fixed interior samples from the preferred collision box, tests them nearest-first with the same coarse model-box and detailed polygon routines used by native shot processing, and confirms the returned surface point with the shared semantic visibility query. It proceeds through the next-nearest retained boxes until a sample validates or a hard 15-query budget is exhausted. An existing exact lock suppresses the pass. The validated geometry point replaces the coarse anchor only for presentation; no result is written into native sight, auto-aim, shot, or damage state. Core policy still chooses the nearest screen-space target with a fixed identity switch margin, then overrides only that target's existing combat-slot cadence and pan. Exact lock remains wholly dependent on the established native/raw aim identity or the existing turret-only three-pixel tolerance.

The path uses fixed stack arrays and existing model state, performs no allocation, and adds no audio voice. Compile-time performance diagnostics retain cumulative refinement, model-query, hit, miss, budget-exhaustion, total-microsecond, and maximum-microsecond counters. `performance/targeting_window` publishes their one-second deltas beside render FPS, game-tick rate, and maximum frame gap; rate-limited `targeting/precision_refinement` and detailed `combat_candidate` records expose per-refinement query count, result, selected surface, budget state, and elapsed time. This makes both average cost and rare query spikes attributable without logging from the audio callback.

The exact raw attack query may also admit an aim-only loot container. Eligibility is data-driven: the outer prop must be an active, healthy, mortal object and must directly own a non-hidden, collectable child whose setup carries `OBJFLAG_INSIDEANOTHEROBJ`. Native pickup types supply the child semantic; hats and escape steps are excluded because they are not container loot. Weapon-function type and the object's gunfire/explosion immunity flags determine whether the current attack can damage it. This adds no presence beacon, projected-bounds tolerance, model-name test, or stage-specific list. The resulting `ACCESSIBILITY_TARGETING_CATEGORY_LOOT_CONTAINER` uses the interrupted non-hostile lock and logs the outer and child identities through `targeting/loot_container_aim`.

If neither the path-blocker nor loot-container predicate applies, that same raw
query may admit `ACCESSIBILITY_TARGETING_CATEGORY_DESTROYABLE_OBJECT`. The
fallback requires a live prop/object identity, active and enabled prop state,
non-invisible and non-deleting object state, native healthy/mortal semantics,
and compatibility between the selected attack family and the object's native
gunfire/explosion immunity flags. It therefore covers destructible cover such
as Pelagic II's ordinary crates without naming the stage or model. It remains
an exact-hit, aim-only interrupted lock and logs through
`targeting/destroyable_object_aim`; it never implies an enemy behind the object
or discloses the object away from the crosshair.

A scanner is a separate user-enabled query over nearby eligible props; it must not reuse render visibility as its entire semantic model or reveal hidden mission information. Milestone 5 first proved this boundary in Carrington Institute training; F5/F6/F8 now use the same bounded adapter in every mission and one-local-player Combat Simulator match. Each enabled category refreshes a bounded snapshot twice per second and retains up to three nearby targets using a 150-unit membership margin; interactables, doors, pickups, and non-hostile people are independently controlled by F5, F6, F8, and F7. A dedicated centered confirmation lane reports the resulting category state after those gameplay commands: 880-to-1320 Hz means enabled and 880-to-440 Hz means disabled, using two 35 ms beeps separated by 25 ms. It does not reuse a positioned lane or alter the F4 virtual-cane command. Menu, pause, cutscene, death, unsupported-player, temporary observer loss, and stage teardown are output-suppression states rather than implicit toggle commands: the adapter stops owned voices, clears target/schedule snapshots and stage-owned identities, preserves the four session-sticky category selections, and performs a fresh scan without an earcon when eligible gameplay resumes. Initialization, shutdown, feature disablement, and explicit F5–F8 commands remain state-changing boundaries. Interactable-object eligibility shares `objIsPotentiallyInteractable` with `objTestForInteract`, covering CI-tagged objects, alarms, thrown laptops, Hacker Central terminals, explicit interactables, lift controls, and movement-state-eligible vehicles/grabbable props. Scanner policy additionally requires healthy, active, non-invisible state without `OBJFLAG_CANNOT_ACTIVATE`; `OBJFLAG_DEACTIVATED` is not an interaction exclusion. Mission navigation landmarks do not bypass this policy or enter F5 results; they use the separate authored-landmark registry described below. Range and room relation are prefilters, not visibility evidence. Discovery and pre-pulse validation share the combat profile's semantic visual ray: background, doors, ordinary objects, and path blockers can occlude, while `CDTYPE_AIOPAQUE` excludes props carrying `OBJFLAG_AISEETHROUGH`. The queried object temporarily disables its own perimeter around the synchronous main-thread ray and restores the prior enabled state, preventing self-collision without ignoring intervening opaque props. For interactables, the origin is the fast path; if it is embedded in supporting world geometry, an allocation-free fallback derives the closest model-box face and tests its center plus four conservative inset points, pulling endpoints 0.25 world units toward the camera to avoid boundary ambiguity. When all six ordinary rays fail, only an object rendered on-screen and lacking native `OBJFLAG2_INTERACTCHECKLOS` may retry the same five face points with an additional camera pull. The generic allowance is eight world units. A monitor carrying native `OBJFLAG_MONITOR_RENDERPOSTBG` placement semantics receives 24 units because the Investigation terminal pad data places its rendered plane about 14 units inside its mounting geometry. Both paths preserve a passing collision ray; render state alone never admits the target. At most eleven rays are issued with early exit. Detailed scan results use samples `0` for the origin, `1`–`5` for ordinary surface points, and `6`–`10` for the bounded embedded retry. Door eligibility additionally requires current-view rendering both when scanning and immediately before playback. Sibling door leaves share the lowest prop number as one stable canonical identity, and one rendered leaf is selected as the positioned source, preventing a paired doorway from consuming two result or schedule slots. The door visibility query temporarily disables the collision perimeter of every leaf in that canonical sibling group, preserving each leaf's prior disabled state, then applies the same origin and five nearest-face probes without the interactable-only embedded retry. This prevents a second leaf or an anchor embedded in the frame from rejecting its own visible doorway while unrelated opaque geometry remains authoritative. The character category is stage-agnostic in one-local-player sessions and admits living, perceptible friendly, neutral, or native blue-sight protected `PROPTYPE_CHR` props using engine relationship and life/visibility state; a camera-to-body-midpoint visual ray prevents through-wall disclosure while permitting engine-marked transparent props. Protected characters use the people beacon and are admitted as aim-only combat candidates, allowing the alignment tone without assigning a hostile presence oscillator. Ordinary hostiles enter the existing combat slots while ordinary friendly/team characters remain excluded from combat feedback. Doors and non-hostile people consume the shared active-observer pose, so their origin, rooms, range, bearing, and visibility move to the CamSpy while its camera is active and return to Joanna with the visible perspective. Remote people additionally require `PROPFLAG_ONTHISSCREENTHISTICK`, preventing characters that are room-connected but absent from the CamSpy viewport from sounding; the ordinary player-centered scanner retains its wider spatial-awareness policy. The CamSpy prop is never treated as a person candidate. Body-actionable interactable and pickup categories pause during remote viewing. An observer change immediately stops the old voices and rebuilds fixed state without changing category toggle state. A global round-robin scheduler interleaves door, interactable, and pickup categories through one procedural chirp voice, guaranteeing that two of those beacons never start together. Doors use one 440 Hz chirp, interactable objects use one 880 Hz chirp, and pickups use three quick 880 Hz chirps. Non-hostile people instead use three dedicated continuous voices so every retained person remains simultaneously spatially trackable: each voice mixes a 440 Hz sine at 72 percent with a 550 Hz major-third sine at 28 percent, applies twice the ordinary distance gain through a 0.10 master level, and ramps gain/pan over 10 ms. Stable identity retention and the same 150-unit replacement margin prevent unnecessary phase restarts. This static chord is distinct from the markers' opposed moving 300–600 Hz oscillators and 800 Hz identity chirps. The fixed scan holds 64 results and at most three retained identities per category; all mixer storage is preallocated and audio-thread work remains bounded. Split-screen and cooperative output composition remain unsupported.

Interactable and door visibility now match the combat adapter's viewport semantics rather than treating a clear spatial ray or the engine's coarse render eligibility as sufficient evidence. PC prop rendering converts model matrices in place, while the beacon tick runs before the next render; calling `modelGetScreenCoords` from that tick can therefore reject a genuinely visible prop. A fixed 256-entry cache captures `objIsPotentiallyInteractable` object/weapon projections and all door projections beside the established targeting capture, before prop rendering mutates those matrices. Discovery and pre-pulse validation verify prop number, prop pointer, object pointer, stage, player, and a maximum two-tick age, then require the captured `PROPFLAG_ONTHISSCREENTHISTICK`, successful finite projection, and rectangle intersection with the captured active scaled viewport. Overflow is bounded and exposed diagnostically. This is deliberately stricter than the pickup awareness policy: a nearby computer or door behind or vertically outside the camera may have clear semantic line of sight but cannot sound until some part of its rendered model enters the screen. For sibling doors, any viewport-intersecting leaf may carry the one canonical cue, and validation transfers the retained source between visible leaves without duplicating it. The existing semantic collision ray remains independently mandatory, so projection cannot reveal a prop through opaque geometry.

The dedicated-door audio revision supersedes the shared-door scheduling policy above. Interactables and pickups remain on the serialized procedural chirp lane; up to three canonical visible doors occupy independent preallocated 440 Hz voices governed by one shared 375 ms clock. Fixed windows begin 125 ms apart, each chirp lasts 100 ms, and a newly assigned voice waits if its current window has already begun. Thus each door retains independent pan and gain without simultaneous or partial chirps, while dense computer or pickup sets cannot delay, replace, or attenuate it. Door scanning extends to 3,000 world units, with proportional 416.7/2,666.7/3,166.7-unit full/fade/silent attenuation thresholds, and its backend master level is 0.24, 50 percent above the former shared chirp lane. Visibility and line of sight remain tied to an actually rendered leaf, but distance and stereo placement use the stable closed-position doorway center: `door->startpos` for one leaf and the mean sibling `startpos` for a multipart doorway. Opening, swinging, or transferring the retained visible leaf therefore cannot pull the cue toward one edge. The established rendered-view, sibling canonicalization, semantic line-of-sight, bounded retention, suppression, and fixed-storage rules otherwise remain unchanged.

Scanner selection and scanner output have separate lifetimes. Four registered configuration integers own the F5 interactable, F6 door, F8 pickup, and F7 non-hostile selections; the gameplay command updates the corresponding value, and the port's existing orderly-exit `configSave` writes it to `pd.ini` after accessibility shutdown. Initialization restores category selection from those values, subject to the existing interactable- or non-hostile-backend master switch. Stage changes, menu/pause suppression, perspective changes, feature scope loss, and shutdown may clear targets, schedules, oscillator state, and runtime active flags, but do not overwrite the registered selections. No target identity, position, or audio state persists across stages or processes.

Pickup eligibility normally follows `objTestForPickup` object types and flags. The current player's visible, inactive, deployed CamSpy is the single character-prop exception because native retrieval is handled separately in `bondeyespy.c`. It is eligible only after control returns to Joanna; active, held, hidden, destroyed, and unrelated character props remain excluded. Each fixed result records whether its stable entity pointer belongs to a character or object so validation cannot reinterpret the CamSpy identity if a prop slot is reused.

### Navigation

The player-authored marker module owns four fixed coordinate/room snapshots.
F9 through F12 place or move a slot at the active accessibility observer's
camera; Shift removes it. Each logical gameplay tick calculates 3D distance and
runs at most four bounded portal-aware collision rays against background and
doors. A blocked ray publishes zero gain, while an off-screen clear ray remains
eligible. The observer adapter makes the same policy follow Joanna or an
active CamSpy without storing live prop pointers in marker state.

The port tone backend reserves four marker voices. Each renders two normalized,
opposed triangular frequency sweeps plus spatial gain and pan. A shared
allocation-free identity scheduler serializes one-to-four 800 Hz chirps with
75 ms gaps and a minimum 500 ms start interval; removal appends a centered
400 Hz chirp.
Gameplay publishes only atomic enabled/gain/pan/restart values. Menus and other
temporary presentation states mute voices but retain core slots; stage-stop,
disable, and shutdown reset them.

The authored-landmark module is a separate fixed registry of stage, setup tag,
expected prop type, optional objective index, and semantic name. Up to four entries in the current stage
receive their own preallocated versions of the same opposed 300–600 Hz base
voice, but authored slots never enter the player-marker identity scheduler and
therefore never chirp. Each tick validates the tagged object's identity, active
and healthy state, configured marker range, and the shared visual line-of-sight
ray from the active Joanna/CamSpy observer. Menus and other temporary scope
loss mute voices; destruction, stage change, feature disable, and shutdown
discard them. Registry entries cover Rescue's tagged intact silver-X wall and
Air Base's suitcase-deposit conveyor. The conveyor is gated to its applicable,
incomplete objective. Registry ownership excludes it from F5 independently of
landmark output state, preventing one prop from communicating conflicting
mission-landmark and ordinary-interaction semantics. Authored landmarks do not
consume scanner capacity or create a target lock.

This landmark slice remains an experiment. Route guidance, automatic
breadcrumbs, objective selection, and route-deviation policy are separate and
must not infer destinations from marker state. Do not generalize a future route
model to all stages until blind task completion supports it.

## Upstream hook ledger

This table records implemented and anticipated changes to established files so future diffs remain deliberate.

| Established file | Proposed narrow hook or reason | Semantic payload | Why polling alone may be insufficient | Status |
| --- | --- | --- | --- | --- |
| `CMakeLists.txt` | Register core sources, including shared world semantics, select exactly one native/null speech backend, copy Windows runtimes, and define the clean Windows ZIP target | Build platform/configuration only | `src/accessibility` is outside the game glob; platform backends must not compile together; package staging must never admit ROM, save, log, or personal configuration paths | Implemented through the Windows redistributable target |
| `port/src/main.c` | Initialize after `configInit`; shut down in `cleanup` | Lifecycle and logger availability | First UI may occur before a later tick; resources need ordered shutdown | Implemented in Milestone 2 with two calls |
| `port/src/system.c`, `port/include/system.h` | Expose monotonic microsecond timing and read-only process-memory totals | Timing plus Windows working-set/private-byte values when available | Backend-call latency, bounded scan cost, and suspected long-session growth need a shared platform boundary rather than feature-specific native APIs | Implemented; non-Windows memory queries return unavailable while timing remains portable |
| `port/src/pdmain.c` | Call the compile-time-optional performance observer, call accessibility gameplay ticks immediately after `lvTick`, and reset owned state/audio before `lvStop` | Timing, input, stage/player context, bounded incident history, safe main-thread collision queries, settled effective player stance, compass crossings, status-command state, and teardown | Gameplay cues, on-demand status, and diagnostic capture need settled semantic state; cane, compass, player-marker, authored-landmark, hill, stance, and incident work must remain on the main thread; owned sounds and input edges must reset before stage memory is disabled | Beacon, virtual-cane, accessible-compass, laser-hazard, player-marker, authored-landmark, R-Tracker, Combat Simulator radar, King of the Hill, effective-stance, F1 status, and Shift+F2 incident ticks run after `lvTick`; stage-stop resets protect owned voices, identities, history, and shortcut edges |
| `port/src/video.c`, `port/include/video.h`, and diagnostic-only boundaries in `port/fast3d/gfx_pc.cpp`, `gfx_sdl2.cpp`, `gfx_opengl.cpp`, and their headers | Under `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS`, expose fixed per-frame timings and startup graphics metadata without changing rendering | SDL event/dimension time, framebuffer setup/resolve, display-list translation, limiter, swap, finish, window/GL identity | Aggregate main-loop cadence cannot distinguish game work from a blocked OpenGL present; the rare fault must be captured in its first reproduction | Compile-time optional; ordinary builds contain no timing path. Fast3D changes contain only timers/read-only getters and no accessibility policy or logging |
| `port/src/audio.c` | Mix procedural accessibility voices into each completed stereo buffer before SDL queueing | Centered targeting tone, firing-range presence pulse, threat-detector new-contact sweep, single/patterned beacon and cane chirps, compass clicks, continuous friendly-character chords, marker sweeps/identities, toggle, stance, and weapon-function patterns, positioned environmental-hazard tone state, hostile/security-camera combat cues, concurrent R-Tracker markers, serialized Combat Simulator radar events, and the King of the Hill guide | Clean responsive carriers cannot be made from game samples with finite duration or baked-in modulation | Independent fixed voices share one staging buffer: centered fine aim, one harmonic firing-range round-robin lane, one serialized rising threat-contact lane, one/three-pulse object and pickup beacon patterns, three repeating door-chirp slots, three two-oscillator friendly drone slots, a one/two/three-beep toggle, cane-mode, and stance lane, ten cane slots (nine ray voices plus one shared context voice), one dedicated one-to-four-click compass lane, four two-oscillator marker slots with one serialized identity lane, one dedicated hill dual-sweep/identity lane whose chirp cadence reports native scoring progress, a one/two-beep weapon-function lane, continuous hazards, ten combat slots with fixed or swept frequency, ten R-Tracker slots, and one Combat Simulator radar lane; none allocate at runtime |
| `src/game/menutick.c` | Observe the final active dialog/focus once immediately after `menuProcessInput` | Menu slot/player/root/depth and current menu/dialog state | Captures all focus paths after item state settles without hooks in every transition | Implemented in Milestone 4 with one call |
| `src/game/activemenutick.c` | Observe the settled active-menu screen and highlighted slot once after all sampled input is processed | Primary-player active-menu mode, screen index, slot index, and the localized `amGetSlotDetails` label | Weapon/device selection is a gameplay radial rather than a normal `struct menu`; observing after input avoids duplicate speech from intermediate controller samples | Weapon/device screen narration implemented; function and bot-order screens remain deferred |
| `src/game/menu.c`, `src/include/game/menu.h` | Expose a read-only focused-item runtime-data lookup | Dialog/item to existing row/block data | Accessibility must not duplicate private row/block mapping | Implemented in Milestone 4 as `menuGetItemData` |
| `src/include/types.h`, `src/include/game/menuitem.h` | Define the generic caller-owned accessibility query payload and expose the existing keyboard layout read-only | Requested semantic part/index/buffer plus keyboard rows | Custom handlers need one shared operation contract, and keyboard narration must not duplicate the renderer's private key table | Implemented for `MENUOP_GETACCESSIBILITYTEXT`; no speech policy or accessibility-owned state lives in these headers |
| `src/game/menuitem.c` | Expose type-owned ranking/player-stats summaries only if existing APIs cannot be queried safely by the adapter | Current semantic row/stat labels and values | Compound presentation state is assembled inside type-specific render paths | Audit found no hook necessary; `mplayer/ingame.c` providers query the same ranking and player-stat records used by these renderers |
| `src/game/activemenu.c`, `endscreen.c`, `filemgr.c`, `mainmenu.c`, `trainingmenus.c`, `mplayer/setup.c`, `mplayer/ingame.c`, `fmb.c`, `src/include/game/mainmenu.h`, and `src/include/game/mplayer/setup.h` | Answer one read-only `MENUOP_GETACCESSIBILITYTEXT` query for focusable custom-rendered rows, carousels, and optional dialog summaries; mark a simple visible label when it is itself the summary; declare shared providers where menu definitions cross translation units | Caller-owned UTF-8 buffer, requested part/index, or the label's normally resolved text | Render callbacks and non-focusable panels otherwise expose pixels/borrowed scratch text, not stable semantics | Implemented in Milestone 4; `trainingmenus.c` supplies firing-range weapon-information, post-session scoring, visible proficiency-star completion, device-training information, holo-training description summaries, and Hangar Information's custom-rendered localized subheadings; `mainmenu.c` supplies the shared mission-objective provider, rich pause-inventory descriptions, mission difficulty-completion stars, and the marked PC exit prompt; `endscreen.c` supplies mission-result panels and opts every endscreen/retry objective page into the shared provider; `mplayer/setup.c` supplies Combat Simulator challenge descriptions and per-player-count completion stars while preserving the current-challenge hidden-state handler; `mplayer/ingame.c` marks the post-session Save Player question and publishes the complete Game Over, ranking, and player-stat controls; `fmb.c` opts the 4 MB challenge confirmation into that shared provider |
| `src/game/hudmsg.c` | Publish after a message passes suppression and is queued | Resolved text, type, flags, player, audio channel, message ID | Polling the HUD array loses admission order and reason | Implemented for generic HUD-message narration; types 6 and 11 are logged but explicitly excluded as subtitles |
| `src/game/mplayer/mplayer.c` | Publish the direct-rendered post-death overlay once per rendered state | Localized prompt, displayed positive countdown integer, visibility, and player | The modal text bypasses the common HUD queue; polling only `deadtimer` would speak while the overlay is suppressed by death animation, pause, co-op/anti rules, cutscene, or match end | Implemented with fixed per-player deduplication; the prompt is spoken on overlay entry and each changed displayed integer once |
| `src/game/objectives.c` | Publish inside the changed-status branch of `objectivesCheckAll` | Objective index, previous/new state | The existing HUD text can duplicate or omit useful objective identity | Proposed |
| `src/game/chraction.c` | Optional later directional damage event after actual player damage | Victim player, magnitude band, direction/source category | Snapshot detects loss but not source/direction | Question; not needed for first status query |
| `src/game/lv.c` | Capture projected target bounds, scoped character collision-region anchors and bounded geometry validation, deliberately interactable-object bounds, onscreen hostile-character props, and the existing query-ray hit coordinate/body part before prop rendering; observe after player sight/HUD rendering; expose read-only multiplayer limit getters | Finite projected bounds, hit-region centers and validated surface guidance, final filtered `lookingatprop`, exact query hit and model hit part, native sight/X-Ray state, player viewport, rendered interactable/character membership, and time/player/team score limits | PC prop rendering converts float model matrices in place before sight/HUD state is final, so post-render beacon ticks cannot safely project terminals or sniper anchors and one targeting hook cannot obtain both targeting states; status queries must not duplicate private limit globals | Adjacent bounded beacon and targeting capture calls plus the later targeting observation support viewport-true scanners, firing-range, generic hostile-character, and FarSight X-Ray combat profiles; scoped guidance retains five fixed collision-region anchors and validates at most one nearby target with 15 allocation-free geometry queries. Query results survive final semantic filtering. Three pure getters supply the F1 status adapter with authoritative limits |
| `src/game/bondgun.c` | Mark forward/back weapon-cycle requests and observe settled weapon/function state after gameplay weapon processing | Requested weapon, player, stage, equipped weapon, final `bgunIsUsingSecondaryFunction()` value, and the localized function label when the native Show Gun Function option makes it visible | Speech must follow a successful semantic switch rather than input alone; the function label is rendered directly rather than admitted to the HUD-message queue, and the visual state also includes persistent configuration and temporary inversion | Two request markers reuse the existing cycle functions; one end-of-tick observation resolves pending weapon speech and publishes visible function-label changes, while initial/stage and weapon changes establish silent baselines |
| `src/game/bondbike.c` | Publish the settled result of each native hoverbike movement attempt to the bounded incident recorder and vehicle-cane feedback adapter | Requested displacement/turn, collision result, and immediate obstacle identity | Shift+F2 polling after the tick cannot recover the collision subsystem's transient obstacle result or distinguish a blocked command from an idle vehicle; a timely blocked cue must follow the actual rejected command | Implemented as two allocation-free semantic calls after native push/retry handling; no movement state or collision decision is changed |
| `src/game/prop.c` and `src/include/game/prop.h` | Offer optional hit-coordinate/model-hit-part results and the next actual hit behind native penetrable glass from the existing non-shooting aim query | Selected query prop, its already-calculated collision point/body part, native per-hit bullet-slowing/bulletproof state, glass object type, and windowed-door display-list node | Fine aim cannot truthfully use projected bounds, and repeating the collision query would duplicate expensive work; a whole windowed door cannot be treated as transparent because its frame remains opaque | `propFindAimingAtWithHit` preserves the ordinary query path; `propFindAimingAtWithPenetrableHit` additionally reports the first hit beyond confirmed non-bulletproof glass without changing native `lookingatprop`, firing, damage, collision order, or weapon spread |
| `src/game/sight.c` | Expose sight-validity/friendliness helpers to adapter | Eligibility and relationship | Avoid duplicating sight rules | Question; prefer existing public APIs if sufficient |
| `src/game/radar.c`, `src/include/game/radar.h` | Expose one read-only R-Tracker marker classification and capture the Combat Simulator renderer's final begin/dot/end stream | R-Tracker category plus native radar availability, final marker identity/category/position/color/height, option context, and observed King of the Hill marker presence | A second copy of cheat, cloak, death, option, team, scenario, and marker-replacement rules could drift from the visual radar and disclose different targets | R-Tracker classification and bounded Combat Simulator capture are implemented; the capture is observational, supplies exact hill-radar parity, and leaves rendering unchanged |
| `src/game/propobj.c`, `src/include/game/propobj.h` | Expose pure IR/X-Ray renderer queries and the native potential-interaction predicate | Conditional-scenery/infrared highlight state, X-Ray range, and broad object interaction semantics before range/facing checks | Copied flag, movement-state, or eraser math could drift and announce a different object set | `objIsHighlightedByInfrared`, `objGetXrayHighlightDistance`, and `objIsPotentiallyInteractable` are shared with their native consumers |
| `src/game/propsnd.c` | Reuse public read-only distance-volume and pan calculations for procedural spatial cues | World position, distance, range, volume, and pan | Procedural cues should retain the tested spatial behavior without allocating or stopping gameplay channels | No hook needed; beacon and hazard cores call `psCalculateVolumeFromDistance` and `psCalculatePan` |
| `src/include/constants.h` | Formerly reserved `PSTYPE_ACCESSIBILITY_TARGETING`; remove the unused owner after every active targeting lane moved to fixed procedural audio | No remaining targeting property-sound payload | Keeping an unreachable native-channel fallback enlarged lifecycle and telemetry state and reserved an engine-global owner value | Removed during the accessibility architecture debt audit; targeting no longer creates or owns a game sound channel |
| `port/include/input.h` | Use provisional context-sensitive PC F1 through F12 accessibility and diagnostic keys and expose modifier bits | Development-only action identifiers | Gameplay uses F1 for player status and team-only Shift+F1 for team status; Shift+F2 captures diagnostic state; Combat Simulator uses F3 for a radar pulse and Shift+F3 for contact alerts; plain F4 controls the cane, Shift+F4 controls the compass, F5/F6/F8 scanners, F7 people, and F9–F12 markers; menus retain their own contexts; modified OS/debug chords must not trigger unmodified actions | F1–F3 and F10–F12 are named SDL scancodes; replacement of player-facing shortcuts by Milestone 6 actions/settings remains required |
| `port/src/input.c` | Add configurable accessibility actions or a dispatch boundary | Repeat, status, beacon/scan, cancel, navigation commands | Current binding model represents game controls, not a separate action set | Proposed for Milestone 6; provisional keys require no binding-model change |
| `port/src/input.c`, `port/src/optionsmenu.c`, `src/include/constants.h`, `src/game/bondmove.c` | Add a configurable reset-view gameplay action using the unused extended control bit | Pressed edge from End, R3, or a player-selected replacement binding | Gives a deterministic horizontal-orientation recovery command without changing yaw or bypassing the binding system | Implemented as `CK_1000`/`BUTTON_RESET_VIEW`; PC defaults are End and right-stick click |
| `port/src/optionsmenu.c` | Add an accessibility settings entry/dialog | Existing registered values, including proven beacon actions | Users need discoverable control without editing `pd.ini` | Proposed for Milestone 6 after beacon behavior is tested |

`src/game/player.c` is a confirmed future semantic source but does not need Milestone 4 hooks. `src/game/mainmenu.c` has one read-only semantic-provider case for its custom-rendered mission list; it combines the same localized location and subtitle fields drawn by the renderer and appends the visible difficulty-completion state, while containing no speech policy.

## Known uncertainties and required experiments

1. **Windows speech technology:** Milestone 3 implements pinned Tolk commit `e5149f0cb6ef9b941673017e0e7b7c409e485fbe` as a dynamically loaded shared library. NVDA 2026.1 runtime requests, Unicode conversion, cancellation, missing-dependency behavior, and clean unload passed; other readers and future compatibility remain unverified.
2. **Threading:** determine whether native speech can be pumped without blocking and which calls must occur on the main thread or a COM-initialized worker.
3. **Menu semantics:** Milestone 4 implements the stable post-input observer and current type/provider matrix. Compilation and lifecycle smoke evidence exist; callback lifetime, localization variants, compound-control usefulness, full input parity, first-focus timing, and the complete scripted interaction matrix still need runtime and blind-user verification.
4. **Localization:** test all supported ROM configurations for string resolution and region-specific control codes. Existing game-derived menu, weapon, objective, and description text follows the normal language APIs. Accessibility-authored R-Tracker state phrases (`R-Tracker on`, `R-Tracker off`, and `No tracked targets`) remain centralized in the accessibility adapter but do not yet have language IDs or translations; add an accessibility string namespace and translation workflow before claiming non-English support rather than editing generated/ROM-derived assets.
5. **HUD duplication:** correlate subtitle splitting, HUD duplicate suppression, objective-generated HUD messages, audio channels, and cutscene transitions.
6. **Multiple players:** define which local player's focus/status owns speech and how simultaneous events are identified or suppressed.
7. **Input:** determine a collision-free binding model and whether accessibility commands work while paused, in menus, and during gameplay.
8. **Status sampling:** choose a stable tick placement and verify health, shield, weapon, ammo, death, stage changes, scripted changes, and pause behaviour.
9. **Target truth:** define stable semantic identities and ensure target/scanner output does not reveal cloaked, occluded, scripted, or otherwise unknown entities.
10. **Navigation model:** inspect room/portal/pad data and prototype one training path; do not assume source geometry yields a usable route graph.
11. **Log storage:** verify `$S` resolution in portable and installed Windows modes; measure real volume and write cost before choosing buffering, size limits, or session rollover.
12. **Shutdown/crash:** verify cancellation and cleanup during normal exit, initialization failure, window close, and crash-handler interaction without making speech a crash dependency.
