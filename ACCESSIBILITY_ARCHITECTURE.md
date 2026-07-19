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

`CMakeLists.txt` recursively includes C and C++ sources under `src/game` and `port`, then generates asset headers for the selected `ROMID`. Milestone 2 adds an explicit `SRC_ACCESSIBILITY` list for `src/accessibility/accessibility.c` and `src/accessibility/accessibility_log.c`. Future platform backend selection still needs an explicit design rather than compiling mutually exclusive implementations accidentally.

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

This means focus identity is centralized but accessible names and values are type-dependent. The first menu proof should support a deliberately limited type set and log unsupported cases rather than guess.

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

This is sufficient for a target-change experiment, but not yet for a truthful world scanner. Object naming, visibility/knowledge rules, multiplayer ownership, distance units, room transitions, and route graph quality still need investigation.

### Input, audio, native platform, and repository boundaries

`port/src/input.c` and `port/include/input.h` implement SDL keyboard, mouse, and controller input, binding persistence, and direct key/button queries. Existing bindings primarily represent emulated game controls. Accessibility commands such as repeat, cancel, status, and scan need a collision-free action design rather than scattered hard-coded keys.

Native audio output is initialized in `port/src/audio.c`; the game-facing chain includes `src/lib/audiomgr.c`, `src/lib/snd.c`, `src/game/propsnd.c`, and `src/game/music.c`. `src/game/chraicommands.c:aiSpeak` and prop-sound functions associate dialogue audio with subtitle text. Speech should remain a separate service so it does not enter the game sound mixer accidentally; later earcons may deliberately use an appropriate audio interface after volume and channel behaviour are tested.

`src/include/platform.h` defines `PLATFORM_WIN32`, POSIX platform variants, architecture/endian macros, and `PD_CONSTRUCTOR`. Windows-specific implementation is spread across guarded code in port services such as `port/src/system.c`, `port/src/fs.c`, and `port/src/crash.c`, with the SDL video backend under `port/fast3d`. `dist/windows/icon.rc` is packaging metadata, not a speech integration point.

`port/fast3d` carries its own license and is effectively a third-party rendering subsystem. `port/include/external/minimp3.h` is vendored, and `tools/recomp` is a Git submodule. Accessibility work should avoid all three. The ignored `build/` directory, ignored `src/generated/`, ignored `extracted/` content, and region-specific generated asset outputs are not sources to edit or commit.

Version conditionals such as `VERSION`, `PAL`, and `PLATFORM_N64` occur in relevant game files, including menu, HUD, and subtitle paths. Canonical asset descriptions under `src/assets/<ROMID>/` also differ by ROM. New hooks must compile on each supported branch of those conditionals and must use normal localization/state APIs rather than addresses or data from one ROM configuration.

## Accessibility module layout

Milestones 2 and 3 implement:

```text
src/accessibility/
  accessibility.c          lifecycle and feature coordinator
  accessibility_log.c      comprehensive structured development log
  accessibility_speech.c   speech lifecycle and UTF-8 output boundary
src/include/accessibility/
  accessibility.h
  accessibility_log.h
  accessibility_speech.h
  accessibility_speech_backend.h
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

Milestones 2 and 3 implement these `pd.ini` keys, all defaulting to off:

```ini
Accessibility.Enabled=0
Accessibility.LoggingEnabled=0
Accessibility.SpeechEnabled=0
```

Later features may add:

```ini
Accessibility.MenuNarration=1
Accessibility.HudNarration=1
Accessibility.ObjectiveNarration=1
Accessibility.StatusNarration=1
Accessibility.Verbosity=1
```

The implemented keys are constructor-registered bounded integers in the existing config registry. Accessibility settings belong in `pd.ini`, not only in a selected Perfect Dark profile, because startup menus need them. Later key names/ranges remain provisional, and a later in-game settings page should use the same values.

### Playtest logging

Milestone 2 writes `$S/accessibility.log` only when both accessibility and logging are explicitly enabled. It truncates the prior session, writes synchronous/flushed JSON Lines, and records schema, sequence, session, monotonic microseconds, complete build metadata, category, event, and a detailed message. Open/write/flush/close failures disable the logger nonfatally. The current logger is main-thread-only and records lifecycle events; later hooks will add feature context and queue decisions.

The project owner has prioritized diagnostic completeness over privacy minimization during development. The logger may include resolved text, player/profile names, paths, command arguments, precise coordinates, input history, native handles, pointers, and any other feature-relevant state. Do not add redaction or field filtering. The log remains local, disabled by default, ignored by Git, and never uploaded automatically. Never include ROM contents, extracted copyrighted assets, passwords, authentication tokens, or unrelated operating-system secrets. Size limits, rotation, and public-distribution privacy policy are deferred until actual logging volume is measured.

## Feature architecture

### Menu narration

Use two events: dialog activated and focus changed. Resolve the final pre-focused item before the first focus announcement. The menu adapter derives label, role, current value, disabled state, and position only from supported item types and handler operations. Dynamic callbacks must be evaluated in the same valid menu/player context as rendering.

Initial support should be limited to selectable actions, checkboxes, sliders, lists, and dropdowns. Unsupported item types emit one developer-log marker, not guessed speech. Briefing scrollables and objective panels need dedicated semantic readers because their useful unit is content, not focus alone.

### HUD and objective narration

Publish an accepted-HUD event after `hudmsgCreateFromArgs` commits a message. Carry type, flags, player, and audio channel so policy can distinguish subtitles from pickups and system notices. Publish objective index and new state from `objectivesCheckAll`; the objective adapter can resolve localized objective text separately. Objective state events take precedence over a redundant generic HUD version.

### Status queries

Capture a per-player snapshot at a stable logical tick: health fraction, shield fraction, equipped weapons/functions, relevant loaded/reserve ammunition, death/pause state, and stage context. A user command formats this snapshot on demand. Automatic threshold announcements compare semantic snapshots and use hysteresis to avoid chatter.

### Target and scanner

A target event may observe changes to the validated aimed-at prop after gameplay target selection. The adapter must map props to safe categories/names and use existing friendliness tests when valid. A scanner is a separate user-triggered query over nearby eligible props; it must not reuse render visibility as its entire semantic model or reveal hidden mission information.

### Navigation

Navigation is intentionally an experiment. Start with player position/orientation, rooms, a small set of known landmarks, and route-deviation logging in one training environment. Evaluate spoken clock directions versus earcons, metric versus qualitative distance, cue cadence, door/elevator transitions, and recovery after leaving a route. Do not generalize to all stages until route data and blind task completion support it.

## Upstream hook ledger

This table records implemented and anticipated changes to established files so future diffs remain deliberate.

| Established file | Proposed narrow hook or reason | Semantic payload | Why polling alone may be insufficient | Status |
| --- | --- | --- | --- | --- |
| `CMakeLists.txt` | Register core sources and select exactly one native/null speech backend | Build platform/configuration only | `src/accessibility` is outside the game glob and platform backends must not compile together | Implemented through Milestone 3; Windows also builds/packages Tolk |
| `port/src/main.c` | Initialize after `configInit`; shut down in `cleanup` | Lifecycle and logger availability | First UI may occur before a later tick; resources need ordered shutdown | Implemented in Milestone 2 with two calls |
| `port/src/pdmain.c` | Call one accessibility logical tick at a stable point | Frame and per-player context | Coordinator needs one deterministic observation/queue pump point | Proposed; placement experiment required |
| `src/game/menu.c` | Publish final initial focus and changed focus/dialog | Dialog definition, focused item, player/menu context | Focus transitions and dynamic context can be lost or repeated when inferred externally | Proposed |
| `src/game/menuitem.c` | Expose supported role/value semantics through one adapter boundary | Item type, resolved value/list state | Values are dispatched by type and handler operations | Proposed only if menu adapter cannot query safely |
| `src/game/hudmsg.c` | Publish after a message passes suppression and is queued | Text, type, flags, player, audio channel | Polling the HUD array loses admission order and reason | Proposed |
| `src/game/objectives.c` | Publish inside the changed-status branch of `objectivesCheckAll` | Objective index, previous/new state | The existing HUD text can duplicate or omit useful objective identity | Proposed |
| `src/game/chraction.c` | Optional later directional damage event after actual player damage | Victim player, magnitude band, direction/source category | Snapshot detects loss but not source/direction | Question; not needed for first status query |
| `src/game/lv.c` | Publish or expose validated aimed-target changes after selection/filtering | Player and target prop semantic handle | Transient target order may be lost between polls | Question; first try stable tick observation |
| `src/game/sight.c` | Expose sight-validity/friendliness helpers to adapter | Eligibility and relationship | Avoid duplicating sight rules | Question; prefer existing public APIs if sufficient |
| `port/src/input.c` | Add configurable accessibility actions or a dispatch boundary | Repeat, status, scan, cancel, navigation commands | Current binding model represents game controls, not a separate action set | Proposed after menu narration proof |
| `port/src/optionsmenu.c` | Add an accessibility settings entry/dialog | Existing registered values | Users need discoverable control without editing `pd.ini` | Proposed after stable config/API |

`src/game/mainmenu.c`, `src/game/bondgun.c`, and `src/game/player.c` are confirmed semantic sources but should not need first-pass hooks: generic menu narration and snapshot/query APIs can consume them. Add them to the ledger only if implementation evidence proves otherwise.

## Known uncertainties and required experiments

1. **Windows speech technology:** Milestone 3 implements pinned Tolk commit `e5149f0cb6ef9b941673017e0e7b7c409e485fbe` as a dynamically loaded shared library. NVDA 2026.1 runtime requests, Unicode conversion, cancellation, missing-dependency behavior, and clean unload passed; other readers and future compatibility remain unverified.
2. **Threading:** determine whether native speech can be pumped without blocking and which calls must occur on the main thread or a COM-initialized worker.
3. **Menu semantics:** create a type/handler matrix from real dialogs; verify dynamic label lifetime, multiline/control-code cleanup, disabled state, list position, and initial-focus timing.
4. **Localization:** test all supported ROM configurations for string resolution and region-specific control codes; determine how new accessibility-only strings will be translated.
5. **HUD duplication:** correlate subtitle splitting, HUD duplicate suppression, objective-generated HUD messages, audio channels, and cutscene transitions.
6. **Multiple players:** define which local player's focus/status owns speech and how simultaneous events are identified or suppressed.
7. **Input:** determine a collision-free binding model and whether accessibility commands work while paused, in menus, and during gameplay.
8. **Status sampling:** choose a stable tick placement and verify health, shield, weapon, ammo, death, stage changes, scripted changes, and pause behaviour.
9. **Target truth:** define stable semantic identities and ensure target/scanner output does not reveal cloaked, occluded, scripted, or otherwise unknown entities.
10. **Navigation model:** inspect room/portal/pad data and prototype one training path; do not assume source geometry yields a usable route graph.
11. **Log storage:** verify `$S` resolution in portable and installed Windows modes; measure real volume and write cost before choosing buffering, size limits, or session rollover.
12. **Shutdown/crash:** verify cancellation and cleanup during normal exit, initialization failure, window close, and crash-handler interaction without making speech a crash dependency.
