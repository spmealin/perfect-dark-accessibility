# Accessibility testing and evidence guide

## What each kind of evidence proves

Keep these results separate in issues, commits, and handoffs:

| Evidence | What it can establish | What it cannot establish |
| --- | --- | --- |
| Compilation | Sources, headers, generated inputs, and link dependencies integrate for one configuration | Startup, speech, correct semantics, timing, usability, or accessibility |
| Runtime smoke test | The executable starts, reaches a checkpoint, and exits without an observed failure | Speech correctness, task completion, broad regression, or blind usability |
| Speech-output test | Requests reach a backend with expected text/order/cancel behaviour in tested conditions | That the information is sufficient, well timed, understandable, or compatible everywhere |
| Interaction test | A defined user task works from a known state with specified inputs and recovery checks | Independent usability outside that script or full-game coverage |
| Independent blind-user test | The stated task can be completed by the target user under recorded conditions | Untested menus, stages, modes, configurations, or users |

Never summarize all five as “tested” without stating which evidence exists.

## Build baseline

For this checkout, use the MSYS2 MinGW64 environment:

```sh
cmake -G"Unix Makefiles" -Bbuild .
cmake --build build -j4 -- -O
```

The default output is `build/pd.x86_64.exe`. Record:

- Git commit and branch;
- dirty/clean status and relevant patch identifier;
- OS and architecture;
- compiler and CMake versions;
- generator and full commands;
- `ROMID`/region selected, without a ROM filename, path, or hash;
- executable name and whether configuration was fresh or incremental;
- warnings/errors and exit result.

The project supports multiple ROM configurations, so region-sensitive changes should compile in the configurations documented by `README.md` or explicitly record what remains untested. Do not upload a ROM, generated asset tree, or built executable as ordinary test evidence.

For a clean x86-64 Windows package, run
`powershell -ExecutionPolicy Bypass -File tools\build_windows_dist.ps1`.
Confirm `build/dist/pd.zip` contains only the executable, six DLLs including
Tolk and the NVDA controller, project/Tolk/NVDA licenses, the generated `pd.ini`,
and `data/put_your_rom_here.txt`. It must not contain a ROM, extracted asset,
`eeprom.bin`, multiplayer setup, accessibility/general/crash log, diagnostic
capture, or developer configuration. Extract to a temporary directory, point
the executable at an external legally obtained ROM using `--basedir`, and
confirm the default package session reports `enabled=0`. Repeat with
`-EnableAccessibility` only when validating the accessible distribution.

## Runtime smoke procedure

Use a legally obtained supported ROM placed as described by `README.md`. Do not include its location or fingerprint in reports.

For project-owner blind-user acceptance builds, every implemented accessibility feature must default to enabled. A feature may default off during its initial engineering investigation, but it must be switched on before acceptance handoff. Inspect the effective `pd.ini` beside the executable and the accessibility session-start record rather than assuming a compiled default took effect. The tester must not have to discover or manually enable the feature being accepted.

For a normal startup smoke test:

Configure and compile every Windows build from the MSYS2 MinGW64 environment.
The build copies its four non-system MinGW dependencies beside the executable,
so the resulting `build/pd.x86_64.exe` may then be launched from Windows
Explorer, PowerShell, Command Prompt, or the MinGW64 shell. Before a direct
Windows launch test, verify `libwinpthread-1.dll`, `libgcc_s_seh-1.dll`,
`SDL2.dll`, and `zlib1.dll` are adjacent to the x86-64 executable. An i686 build
uses `libgcc_s_dw2-1.dll` instead. Keep using an initialized MinGW64 shell for
configure and build commands; copied runtime files do not provide build tools.

1. Back up the relevant `pd.ini` and save data outside the repository if the test changes persistent settings.
2. Record whether portable/save-directory behaviour or command-line flags differ from defaults.
3. Start the exact executable built in the recorded configuration.
4. Verify accessibility disabled in an ordinary build: no backend startup, no accessibility log, no new speech, and normal menu/input/audio behaviour. A build compiled with `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON` is the documented exception when logging remains enabled for an explicit disabled-control capture.
5. Enable only the feature under test, restart when required, and verify the documented checkpoint.
6. Exercise backend unavailable, log-write failure where safely reproducible, rapid input, pause, stage/menu transition, and normal exit.
7. Confirm configuration persistence and clean shutdown.
8. Capture `git status --short`; generated and user data must remain ignored and uncommitted.

`--log` enables the existing engine `pd.log`; it is useful for engine diagnosis but is not a substitute for the proposed accessibility playtest log.

## Structured accessibility log

`$S/accessibility.log` is separate from `pd.log`. In an ordinary build it is created only when both `Accessibility.Enabled=1` and `Accessibility.LoggingEnabled=1`; both default to one for blind-user acceptance testing but remain configurable. A diagnostic build may also create it for the explicit accessibility-disabled graphics control. The schema records lifecycle, speech, menu, gameplay-feature, and optional performance events.

The main-thread logger uses fixed 64 KiB I/O storage and a fixed 4 KiB common-format buffer. Oversized records retain their full text through exact temporary storage rather than truncation. The first record after each one-second interval flushes the batch; lifecycle boundaries and orderly shutdown also flush instead of forcing a disk flush for every event. Validate parseable JSONL, monotonic sequences, embedded control/invalid UTF-8 escaping, an event larger than 4 KiB, prompt `session_start`, final `session_stop`, and nonfatal open/write/flush/close failure paths.

### Shift+F2 incident capture

With accessibility and logging enabled, enter ordinary gameplay and move, turn, use controls, and face a known prop for at least 15 seconds. Press Shift+F2 and confirm the screen reader says `Diagnostic capture 1 saved`. Allow at least one more second of play or exit normally so the buffered log flushes. In `accessibility.log`, verify one matching `incident/capture_begin` and `incident/capture_end`, ordered history samples up to the fixed 60-entry capacity, current player and observer state, objective and raw-requirement records, inventory, beacon state and schedule, nearby active props, and a view-blocker result. Confirm the aimed scanner-supported prop has an `incident/beacon_candidate` record with semantic, pickup, category, cached-projection age/capacity/truncation/bounds/viewport state, range, room, and LOS fields plus a truthful final reason. The candidate summary must report the 16-record capacity, focus-cone count, and truncation. The audit must not alter the next live beacon result set, schedule cursor, pulse timing, or category state. The end record must report `duration_us`. Quote both the spoken capture number and the log's session identifier in a bug report.

Repeat while using the CamSpy and confirm observer position, room, direction, nearby-prop range, focus selection, and blocker ray originate from its current camera rather than Joanna. Put an interactable just inside and outside the documented 0.75 focus-dot boundary, then place more than 16 supported props in the cone where practical; verify deterministic alignment/distance ordering and truthful truncation without allocation. Repeat in pause/menu, cutscene, death, and stage-transition contexts to verify state is captured or reset without stale props or history. F2 alone, Control+Shift+F2, and Alt+Shift+F2 must not capture. With logging disabled, Shift+F2 must say `Diagnostic logging unavailable` without creating a log or affecting gameplay. Trigger several captures in one session and confirm identifiers increase, the history remains bounded, JSON Lines remain parseable, no game/audio memory grows per sample, and collection duration is acceptable. A new process may restart capture numbering at one; session identifiers disambiguate it.

The temporary `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS` CMake option defaults to `OFF`. Enable it for a diagnostic build in the required MinGW64 environment with `cmake -G"Unix Makefiles" -Bbuild -DACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON .`, then rebuild normally. Return to the normal build with the same configure command using `OFF`.

When compiled in and accessibility logging is active, `performance/frame_window` is emitted approximately once per real-time second regardless of whether targeting, beacons, or hazards currently have a selected object. It records rendered-frame rate, the longest observed inter-frame gap, logical game-tick rate and delta fields, stage/menu context, Windows working-set and private-byte totals and session-baseline deltas, desired oscillator states including the enabled combat-slot count, and fixed-buffer mixer call/pass-through/active/frame counters. Use it to correlate a reported slowdown with memory growth, an oscillator that remained enabled, or continued expensive mixing. Hazard `scan` audits additionally include the current, average, and maximum scan duration in microseconds for the preceding audit window. These records are diagnostic observations only and do not allocate or lock in the audio callback. The session-start record reports `performance_diagnostics=1` and its interval when present, or zero when compiled out.

The same flag now emits `graphics/metadata` once and `graphics/frame_window` once per second. The latter reports totals and maxima for frame interval, `videoStartFrame`, SDL events/dimensions, framebuffer maintenance/setup, display-list translation, composite/resolve, renderer end, explicit frame limiting, `SDL_GL_SwapWindow`, and `videoEndFrame`. The frame path writes only to fixed memory. It continuously retains 180 pre-trigger frames; three consecutive frames of at least 50 ms, one frame of at least 250 ms, or a one-second render rate below 30 FPS copies that history into a fixed 480-frame episode. After two one-second windows above 50 FPS, or at orderly shutdown, the log receives an episode summary, the ten worst retained frames, and twelve evenly sampled timeline records. `relative_to_trigger` identifies frames before and after the gate. Capacity exhaustion sets `truncated=1` rather than allocating.

For a reproduction, start `tools/accessibility/capture_graphics_diagnostics.ps1` in normal PowerShell, then launch the executable separately through the required MinGW64 environment. The collector waits for the exact executable path, samples process/NVIDIA state every 500 ms, samples Windows per-process GPU counters every fourth sample, flushes bounded batches, and archives configuration, accessibility log, Git identity, power plan, and relevant System events beneath ignored `build/diagnostics/`. After the run:

```powershell
.\tools\accessibility\analyze_graphics_diagnostics.ps1 `
  -LogPath .\build\diagnostics\<capture>\accessibility.log `
  -CollectorDirectory .\build\diagnostics\<capture>
```

Confirm that a deliberate three-frame test stall in a developer-only harness retains records with negative `relative_to_trigger`, that the episode does not write detailed records until recovery, that normal 60 FPS play creates no false episode, and that a forced normal shutdown flushes an active episode. Compare an all-features-enabled run with `Accessibility.Enabled=0` and `LoggingEnabled=1`; diagnostic builds must retain graphics/performance logging in the disabled control while speech and accessibility gameplay features remain inactive. Follow the focused A/B decision table and fix criteria in `documentation/GRAPHICS_SLOWDOWN_INVESTIGATION_PLAN.md`.

A JSON Lines or equivalently parseable record could look like this, with the schema finalized in implementation:

```json
{"schema":1,"session":"random-short-id","t_ms":1842,"build":"514bf7aff","rom_config":"ntsc-final","player":0,"category":"menu","event":"focus","semantic_id":"main_menu.solo","decision":"spoken"}
```

Required design properties:

- monotonic timestamp for event correlation;
- explicit schema and build/configuration identifiers;
- session-local random identifier, not a device/user identifier;
- normalized category, event, priority, and queue/backend decision;
- detailed errors that may include native handles, pointers, full paths, and raw identifiers when useful;
- graceful write failure;
- a visible setting and documentation for enable/disable/delete;
- comprehensive feature-relevant diagnostics, including free-form text, profile names, precise coordinates, input events, environment/path details, and continuous state where useful.

Privacy redaction and data minimization are not requirements for development logs. Never include ROM contents, extracted copyrighted assets, passwords, authentication tokens, or unrelated operating-system secrets. Logs default to enabled during blind-user acceptance testing, stay local, remain configurable, are ignored by Git, and never upload automatically. Tell testers that a shared log may contain comprehensive raw diagnostics.

## Speech backend test matrix

For the backend proof, record each case independently:

- backend/voice available;
- backend absent or initialization refused;
- no installed voice where reproducible;
- English ASCII, punctuation, numbers, and representative localized UTF-8;
- empty, long, multiline, and control-code-cleaned text;
- queue normal order;
- high-priority interruption;
- replacement of rapid focus/value events;
- user cancel and immediate speech afterward;
- shutdown during pending speech;
- repeated initialization/shutdown;
- window focus changes and screen reader running/not running when applicable;
- frame-time impact and first-utterance latency.

Pass criteria must name numerical latency/frame budgets after the technology probe; do not invent them in advance. A native error must disable or degrade speech without terminating the game.

### Milestone 3 Windows proof (2026-07-19)

The backend and menu speech are enabled by default for acceptance testing. The separate fixed diagnostic request still requires `--accessibility-speech-test`. The effective defaults are:

```ini
[Accessibility]
Enabled=1
LoggingEnabled=1
SpeechEnabled=1
MenuNarration=1
HudMessages=1
EnvironmentalHazards=1
InteractableBeacons=1
IRScannerAudio=1
NonHostileBeacons=1
TargetingFeedback=1
WeaponFunctionCues=1
XRayScannerAudio=1
VirtualCaneMode=1
VirtualCaneNearFrequency=600
VirtualCaneFarFrequency=300
VirtualCaneTerrainReach=450
VirtualCaneTerrainHeightThreshold=12
VirtualCaneVolume=0.184
RTrackerAudio=1
```

Then launch `build/pd.x86_64.exe --accessibility-speech-test`. The flag never enables accessibility or speech by itself. The exact request is `Perfect Dark accessibility speech test.` With speech enabled but no flag, backend detection occurs without an output request. Development output must contain `Tolk.dll` and the architecture-matching NVDA controller beside the executable; notices are copied to `build/licenses/tolk/`.

Recorded environment and evidence:

- branch `accessibility`, baseline commit `39f41ef91`, dirty Milestone 3 implementation patch; x86-64 Windows build 26200;
- NVDA 2026.1 (`2026.1.0.55743`) running; Tolk commit `e5149f0cb6ef9b941673017e0e7b7c409e485fbe`;
- `nvdaControllerClient64.dll` size 153,600 bytes and SHA-256 `41c1f5df5997e798fcfbf7c8f2589de811e768b069a60710600cf57cb23a0b09`;
- the documented MinGW64 configure and build commands completed successfully and produced the executable, Tolk DLL, controller DLL, and both license files;
- import inspection showed no static Tolk import in the game executable; the built Tolk DLL exposed all eight required C exports;
- all four `Accessibility.Enabled`/`SpeechEnabled` combinations started and exited normally; disabled combinations did not load Tolk, and the enabled/no-flag run made zero output requests;
- the enabled/test-flag run loaded Tolk, detected `reader=NVDA has_speech=1 has_braille=1`, accepted the exact fixed output once, and exited normally;
- the same test with logging disabled still loaded Tolk and exited normally without creating `accessibility.log`; forcing the log path to be a directory likewise did not prevent Tolk initialization or normal exit;
- launching with a working directory outside `build` still loaded the absolute sibling Tolk path, detected NVDA, and made no unsolicited output request;
- missing Tolk, missing controller, and a harmless substitute DLL missing seven exports were nonfatal. All eight resolution attempts were logged for the substitute;
- a focused harness accepted ASCII, punctuation/numbers, `café`, Japanese text, and a supplementary-plane character; rejected invalid UTF-8 with Windows error 1113; accepted a 4,024-byte request; cancelled it; accepted subsequent output; tolerated duplicate init/shutdown; and safely rejected output/cancel after shutdown;
- an actual logger harness escaped the invalid byte as `\u00ff`, and the resulting JSONL parsed successfully;
- representative measured calls were: real-game Tolk initialization/detection 1,523 microseconds, fixed output call 589 microseconds; harness initialization 4,831 microseconds, Unicode output 434 microseconds, long output 331 microseconds, explicit cancellation 358 microseconds, follow-up output 99 microseconds, and shutdown 226 microseconds.

Tolk/NVDA API acceptance is machine-verifiable, but the implementation agent cannot independently attest what a person heard or saw on a braille display. Human-observed first-audio latency, audible interruption, braille output, a true no-screen-reader run, other readers, i686 runtime, and non-Windows compilation were not available and are not claimed. The missing-controller case does verify the backend's no-active-reader result without altering the user's installed NVDA. No Tolk call occurs in a gameplay/frame tick, so these call timings are transport evidence rather than a frame-time benchmark.

### Milestone 4 implementation evidence (2026-07-19)

The menu-narration implementation compiles for the default `ntsc-final` x86-64 Windows target. A source audit found resolvers for all currently defined focusable families and semantic-provider cases for all 12 focusable custom-rendered-list definitions (nine unique handlers; three definitions reuse an audited handler). `Tolk.dll` and `nvdaControllerClient64.dll` remain beside the executable.

An isolated startup/shutdown smoke used `--savedir ./build/m4-smoke`, narration/logging enabled, and speech disabled. The executable was launched through an initialized MSYS2 MinGW64 environment and closed through its window's normal close event. It loaded the lawful local ROM, created the game window, wrote parseable accessibility lifecycle records, reset the menu observer, and exited with status zero. Accessibility session `1784499754` recorded `menu_narration=1`; no menu observations occurred because the automated smoke did not progress through the title sequence.

The project owner subsequently performed blind-user acceptance testing and reported that the spoken-menu behavior was working perfectly after two requested refinements: dialog titles are spoken on entry/return but not for every option, and ordinary sliders without semantic display labels are reported as percentages rather than raw engine units. On that acceptance result, the owner declared Milestone 4 complete. A later Combat Simulator Advanced Setup audit found that the global percentage rule hid meaningful labels already produced by slider handlers and that selectable narration omitted visible right-side values. The resolver now prefers a slider's `MENUOP_GETSLIDERLABEL` output and falls back to percentage only when that output is empty; it also announces selectable right-side text. Runtime acceptance remains pending for Limits (`Min`, scores, and `No Limit`), Player Handicaps (displayed damage scale), and named Simulant slots. A focused semantic harness, exhaustive control-family matrix, performance measurements, other-region builds, and broader backend-failure combinations were not supplied as part of the original user acceptance and remain useful regression follow-up rather than claims made by this milestone.

After that acceptance pass, the firing-range training-information dialog exposed another nonvisual gap: its weapon description and challenge fields are rendered in labels and a deliberately non-focusable `DESCRIPTION_FRWEAPON` scrollable panel, so focus narration previously moved directly to `OK` or `Resume`. The menu observer now supports explicitly opted-in `MENUACCESSIBILITYPART_SUMMARY` providers. The firing-range confirmation handler uses that path to provide the localized weapon name, difficulty, applicable goal/accuracy/target/time/ammunition values, and `frGetWeaponDescription()` text. The core speaks the summary once on dialog entry, omits it during normal focus movement, and reconstructs the complete title/summary/current-focus announcement for F5 repeat. An initial implementation unsafely queried every non-null item union as though it were a function pointer and crashed while loading a profile; `MENUITEMFLAG_ACCESSIBILITYSUMMARY` now gates every summary call, preventing dialog pointers from being invoked. The updated default MinGW64 build passes; profile loading plus audible runtime and blind-user verification of several weapons and both pre-game/in-game variants remain pending.

From the main menu, choose Exit Game and verify the initial announcement contains the dialog title, the complete visible prompt “Are you sure you want to exit?”, and the focused Yes or No button. Move between Yes and No and confirm only the newly focused button is spoken. Press F5 and confirm it reconstructs the title, prompt, and current button. Cancel the dialog, reopen it, and confirm the prompt is spoken again. This exercises the generic explicitly marked static-label summary path; unmarked decorative and column labels in other dialogs must remain omitted.

The project owner clarified that the desired CI terminal is the separate `Weapons Available` laptop, not the challenge-goals dialog. That screen renders manufacturer, weapon name, primary function, secondary function, and a marquee description beside a simple weapon list. Its list now opts into a rich `MENUACCESSIBILITYPART_OPTION` result, assembled from the same localized weapon definition and function strings used by those visual fields. Blind-user acceptance testing subsequently confirmed that the weapon menu reads properly. Broader regression coverage across multiple weapons, regions, and visual behavior remains useful follow-up.

The CI device-information terminal exposed the same nonvisual-content pattern in its details dialog: the selected device name is the title and the localized `dtGetDescription()` text is rendered in a non-focusable `DESCRIPTION_DEVICETRAINING` panel while initial focus lands on `OK`. The `OK`/`Resume` handler now opts into the existing dialog-summary contract and returns that localized description. On entry, the screen reader therefore announces the device title, its full information text, and the focused button; F5 reconstructs the same announcement. The default MinGW64 build passes, while audible runtime acceptance remains pending.

The firing-range weapon list visually renders three proficiency stars beside every weapon. `ciGetFiringRangeScore` is the exact saved two-bit value used by the renderer: zero fills none, one fills bronze, two fills bronze and silver, and three fills all three. The accessibility option provider appends localized `Bronze Completed`, `Silver Completed`, and `Gold Completed` phrases for precisely the filled stars. Verify representative weapons at scores zero through three and confirm the spoken completion set exactly matches the visible filled stars, including after completing a new range level and returning to the list.

The holo-training details dialog renders `htGetDescription()` in a non-focusable `DESCRIPTION_HOLOTRAINING` panel while focus starts on `OK` or `Resume`. That focused handler now supplies the identical localized value through the existing dialog-summary contract. For each unlocked holo exercise, open its details and confirm the exercise title, full static description, and focused control are announced once; move focus between the buttons and confirm the description is not repeated; press F5 and confirm the title, description, and current focus are reconstructed.

In the Combat Simulator challenge list, select several available challenges and open each confirmation dialog. Confirm that the complete visible localized challenge description is announced before `Accept` or `Cancel`, that switching challenges yields the newly selected description without stale text, and that ordinary focus movement reads only the focused control. Open the current-challenge details screen and repeat with `Start` or `Abort`; press F5 in each flow and confirm it reconstructs the title, full description, and current control. Exercise the 4 MB confirmation variant where that build/runtime configuration is available. Compare speech and accessibility-log text with the complete value returned by `menuitemScrollableGetText(DESCRIPTION_MPCONFIG)` or `menuitemScrollableGetText(DESCRIPTION_MPCHALLENGE)`.

In Mission Select, compare each spoken option with both lines of its rendered mission name and its three rendered difficulty stars. Confirm regular missions include the localized subtitle, including `dataDyne Central - Defection`, `dataDyne Central - Investigation`, and `dataDyne Central - Extraction`, while unlocked special missions retain their single-line names. Confirm solo play announces every bright Agent, Special Agent, and Perfect Agent marker through the highest completed difficulty; co-op announces only its independently bright difficulty markers; anti announces no completion suffix because the renderer shows no stars. Include profiles with no completion, one completion, and all completions. In both Combat Challenges and Completed Challenges, compare each option with its one-to-four-player star row and confirm speech reports exactly the bright player-count markers. Repeat in a 4 MB configuration where only the one- and two-player stars are rendered. Confirm incomplete/dim stars are never described as completed.

Finish a one-local-player Combat Simulator session with an unsaved player and reach the Save Player dialog. Confirm entry announces `Save Player. Save new player and statistics? Save Now, button.` Move to `No Thanks!` and back, confirming the question is not repeated during ordinary focus changes. Press F5 on either button and confirm it reconstructs the title, question, and current control. The `menu/snapshot_changed` or repeat record must contain the localized label in `summary` and report `summary_spoken=1` on entry or repeat.

Continue through every post-session results page. On Challenge Completed, Challenge Failed, or Challenge Cheated, confirm each displayed team name and score is announced in visual ranking order. On Game Over, confirm speech includes the current player's placement, title, weapon of choice, and every displayed award. On Player Ranking, confirm every displayed player name, death count, and score is announced in visual order. On Stats for Player, confirm the selected player's suicide count and the displayed kills and deaths against every opponent are announced; switch the selected player and confirm all values update. Press F5 on every page and compare speech and `menu/snapshot_changed` text with the visible rows. Scrolling must not substitute a raw scroll offset for the semantic table.

Source review of the PC default bind tables confirmed that End and SDL's right-stick controller button were both unbound; left-stick click was already the default cycle-crouch binding. The new `Reset View` extended action uses the previously unused `CK_1000` bit, persists under the new `RESET_VIEW` name so legacy `CK_1000=NONE` configuration entries do not suppress it, and defaults to End plus right-stick click only in the PC scheme. Runtime acceptance should look sharply up and down with both mouse and controller, press each binding independently, and confirm the camera becomes exactly horizontal without changing heading or continuing to drift. Repeat while standing still, moving, aiming, crouched, and near an interactable. Confirm pause/menu input does not reset the gameplay camera, verify the extended controls menu lists and can rebind `Reset View`, and verify reset-to-PC-defaults restores both bindings while reset-to-N64-defaults leaves the action unbound.

### Milestone 5 engineering evidence (2026-07-19)

The Carrington Institute interactable-beacon implementation compiles for the default `ntsc-final` x86-64 Windows target. Configuration used the Unix Makefiles generator and the documented MinGW64 toolchain; `cmake --build build -j4 -- -O` completed and produced `build/pd.x86_64.exe`. The build emitted existing project and vendored-Tolk warnings but no warning from `accessibility_beacon.c`.

Source review confirms that the implementation is inert unless both top-level accessibility and `Accessibility.InteractableBeacons=1` are effective. In unobscured gameplay, F5 independently toggles interactable-object beacons, F6 toggles door beacons, and F8 toggles pickup beacons; any combination can be active. F5/F6 retain their existing repeat/cancel meanings in menus, while F8 is ignored there. The scan supports any one-local-player mission or Combat Simulator match and is limited to active props, the current/directly-adjacent room boundary, a 1,200-unit radius for ordinary categories, and a 1,800-unit radius for rendered doors. Interactable objects share the broad predicate used before `objTestForInteract` applies range/facing, including CI tags, alarms, thrown laptops, Hacker Central terminals, explicit interactables, lift controls, and movement-state-eligible vehicles/grabbable props. Pickups use the engine's object-type and collectable/uncollectable semantics; doors are canonicalized through sibling links. Scanning does not call the interaction or collection actions, so it cannot select, collect, mount, grab, or activate a target.

Start DataDyne Central: Defection and exercise F5/F6/F7/F8 separately and together through the lobby, offices, elevators, alarm panels, security systems, weapons/ammunition, key items, scripted doors, and mission transitions. Verify only healthy, active, visible, native-potentially-interactable objects enter F5; an object outside actual use range may sound, but a hidden or `OBJFLAG_CANNOT_ACTIVATE` object must not. Verify elevators and sibling doors produce one canonical cue, locked doors remain discoverable without being described as unlocked, collected items disappear, newly spawned/dropped items appear, and LOS/room/range rules do not disclose unseen mission props. Continue through Investigation and Extraction to record level-specific flags, models, scripted state changes, false positives, and missing semantic categories.

For interactable viewport acceptance, stand near a supported computer and rotate until it is behind the camera, then aim above and below it until its projected rectangle leaves the viewport. F5 must remain silent in all three off-screen cases even when the object is nearby, room-connected, and has a clear semantic ray; turning back until any part of its model intersects the screen must restore it on the next bounded refresh. Turning away after a chirp must cause pre-pulse validation to reject it before another cue. Correlate the specific cached-projection reasons (`projection_cache_unavailable`, `projection_not_captured`, `object_not_rendered_at_capture`, `projection_failed`, `projection_non_finite`, or `object_outside_viewport`) with the Shift+F2 projection fields. A valid record must be no more than two logical ticks old and match the current stage, player, prop and object identities. The 2026-08-01 diagnostic session `1785592723` provides the off-screen regression: prop 210 was admitted with bearing `-95.202` and emitted at tick 55 because the former path had no projected-viewport gate. Session `1785689076` captures 3 and 4 provide the invalid-post-render regression: visible, native-interactable model 426 terminals prop 347/tag 5 and prop 345/tag 4 passed range, room and LOS but the live scanner stored neither. Both and the remaining same-class objective terminals must enter F5 from the pre-render cache without permitting an off-screen terminal.

Run a Combat Simulator match with exactly one local player and representative hostile, friendly/team, and neutral simulants. Confirm F5/F6/F8 operate on arena objects, doors, and pickups; F7 admits only friendly or neutral characters; hostile simulants enter the existing combat-targeting voices and are removed on death/knockout. Repeat with no simulants and several simulants, radar/options variations, respawn, pause, endscreen, and match restart. The `beacon/scan_complete gameplay=combat_sim` field must distinguish this evidence from `gameplay=mission`. Split-screen must remain suppressed rather than mixing multiple listener perspectives.

For F5, F6, F7, and F8, confirm every successful gameplay toggle produces exactly one centered two-beep confirmation: 880 Hz then 1320 Hz when the category becomes active, and 880 Hz then 440 Hz when it becomes inactive. Each beep lasts 35 ms with a 25 ms gap. Confirm the earcon reports the resulting state even when that category currently has no eligible world target, that menu-bound F5/F6 actions do not play it, and that rapid toggles do not affect F4 or the weapon-function lane. Correlate each pattern with `beacon/toggle_confirmation`.

Enable each category in turn, open and close a normal menu, pause and resume, enter and leave a computer/device-training dialog, and cross a cutscene or other temporary gameplay-scope boundary where practical. Every active beacon must stop immediately while suppressed, F5/F6 menu actions must retain their menu meanings, and no toggle confirmation may play merely because gameplay resumes. On return, the same category selections must automatically perform a fresh scan and resume against current targets without another key press. Correlate one `beacon/scope state=suspended` and one `state=resumed` record per transition; verify repeated suppressed frames do not repeatedly stop the oscillator or grow the log. Stage teardown, configuration disablement, and explicit F5–F8 toggles must still clear or change state as documented.

After blind-user testing showed that the original toggle-time snapshot remained attached to an older door or object while the player moved, enabled categories were changed to refresh every 30 logical ticks (twice per second at 60 Hz). Each enabled category retains up to three eligible targets. Previously scheduled identities remain members until they disappear or an unscheduled candidate is at least 150 units closer than the farthest retained member. Automatic scans log summaries and schedule changes without repeating every unchanged candidate record; toggle scans retain the full candidate audit.

Retained interactables and pickups are interleaved by category on one global round-robin timeline. The slot gap is `max(18 ticks, 45 ticks / target count)`: one target pulses every 45 ticks, two alternate every 22 ticks, and three through six use an 18-tick (approximately 300 ms) gap. Before every pulse, the scheduler validates the next target, stops the prior serialized cue, and only then starts the new positioned cue. Doors no longer consume this schedule; up to three retained doors use independent fixed voices. Current policy caps each category at three targets; the cap and minimum gap are named constants so a future category can intentionally use a larger, denser sensory profile.

Blind-user acceptance testing subsequently confirmed that automatic refresh and multi-target round-robin playback work well while moving through Carrington Institute. The accepted behavior includes hearing multiple nearby targets in succession without manual retoggling or simultaneous beacon starts.

Door audio now bypasses the object/pickup round-robin. Test with F5 and F6 enabled while at least three computers and one to three doors are visible: every retained door must repeat on its own 750 ms cadence and must not slow down as interactable results enter the schedule. With multiple doors, starts must remain staggered rather than coincident. Verify the louder 0.24 backend level and extended 1,800-unit discovery range remain useful without admitting off-screen, occluded, or duplicate sibling leaves. Correlate `door_chirp_assign`, `door_chirp_release`, `door_chirp_stop`, `door_chirp_slots`, and performance `door_enabled_slots` fields; all counts must remain at or below three and return to zero on suppression, toggle-off, stage teardown, and shutdown.

Interactables and pickups use the serialized procedural chirp voice. Doors use three preallocated procedural voices, without allocating or owning native property-sound channels. Stage-stop and suppression hooks clear every voice before prop/audio teardown. Candidate, ordering, schedule membership/cursor, door-slot assignment, volume/pan, invalidation, command, and lifecycle details are written through the existing accessibility logger when logging is enabled.

After a report of progressive game/video slowdown, an allocation audit found no beacon heap allocation: candidate storage is a fixed 64-entry array, logger scratch allocations were freed per event, and menu/speech allocations had paired replacement or shutdown frees. The available 71-second runtime log showed six door pulses, channel 8 reused for five pulses, channel 10 used transiently once, no allocation failure, an explicit toggle stop, and a shutdown reset. This is evidence of reclamation in that short run, not proof against a long-session issue. The pulse path was subsequently hardened to reclaim the category's exact tracked prop-sound slot whenever it remains free or accessibility-owned, and stale-target cleanup now stops that exact slot when the saved prop identity is unavailable. It never stops a tracked slot whose current type shows that gameplay has reused it. The later architecture debt pass replaced routine logger formatting and retained menu announcements with fixed storage and batched ordinary log flushes; long speech conversion and exceptionally large log records remain bounded, paired temporary allocations.

While either beacon category is active, a rate-limited `beacon/telemetry` record is emitted immediately and every 30 seconds. On Windows it records working-set and private-byte totals/deltas through `GetProcessMemoryInfo`, plus current sound-state count, total/in-use/stopped prop-sound channels, accessibility-owned channel count, schedule size/cursor/next tick, and both tracked category ownership states. A long acceptance run should keep `beacon_channels` at no more than one, show the cursor advancing through every scheduled identity, and distinguish a steadily rising `private_delta` from normal working-set fluctuation. Telemetry resets its baseline when all beacons are cleared. Other platforms report memory as unavailable but retain the audio-channel counters.

Runtime and blind-user evidence now confirms audible laptop and office-door beacons, independent F5/F6 category control, and successful spatial use in the CI level. Empty-result, exhaustive state-transition, and extended long-session coverage remain incomplete, so Milestone 5 should not yet be described as fully accessibility-validated.

The Night Vision device exercise exposed a separate interactable-object eligibility mismatch. Runtime session `1784765247` recorded the visible CI light switch (`MODEL_LIGHTSWITCH`, prop 129, room 47) as `object_activation_disabled` solely because its setup flags include `OBJFLAG_DEACTIVATED`, even though it carries `OBJFLAG3_INTERACTABLE` and the engine subsequently accepted the player's activation during the exercise. `objTestForInteract` does not treat `OBJFLAG_DEACTIVATED` as an activation prohibition. The scanner now follows that behavior: only `OBJFLAG_CANNOT_ACTIVATE` disables an otherwise eligible interactable, while `OBJFLAG2_INVISIBLE` excludes genuinely hidden state. Runtime session `1784815549` confirmed that prop 129 entered the automatically refreshed schedule after the correction. It also confirmed that the visibly present switch remains in the scanner outside its exercise even though activating it then has no effect; the project owner explicitly accepted that conservative false positive in preference to suppressing the switch when it becomes useful. During Night Vision, confirm the switch enters or remains in the schedule without retoggling F5. Correlate `object_invisible`, `interaction_flag`, `schedule_target`, and `pulse` records with the transition. Also sample a setup-defined deactivated terminal in another stage before claiming full-game coverage.

The subsequent procedural-cue revision replaces the two menu-derived samples and their property-sound channel with an independent fixed-size chirp voice in the existing accessibility mixer. Door pulses are 440 Hz and interactable-object pulses are 880 Hz. Each request is 100 ms with a 5 ms attack and 20 ms release; it carries the established distance attenuation and stereo pan into the mixer and cannot interrupt the continuous fine-aiming tone. Source review confirms that the chirp bridge uses atomics, the audio path retains one fixed mix buffer, and the beacon path no longer allocates or consumes a property-sound channel. The default `ntsc-final` x86-64 MinGW64 build completed successfully on 2026-07-20 and produced `build/pd.x86_64.exe`; only pre-existing warnings were observed. Audible category distinction, direction/distance behavior, aiming-tone overlap, and long-session acceptance for this revision remain to be recorded below.

Acceptance feedback confirmed the two procedural chirps work well but exposed that door candidates still followed the prototype's room-adjacency policy without a line-of-sight gate. Door scans and pre-pulse validation now both require `cdTestLos06` against `CDTYPE_BG`. Test a nearby visible closed door, then place opaque level geometry between the player and that door and wait through at least two refresh intervals; its chirp must stop. Verify that the target door does not block itself and that uncovering it causes automatic reacquisition without toggling F6.

The same semantic visual-LOS policy now applies unconditionally to interactables, doors, pickups, people, and combat-presence targets. Proximity and room adjacency are prefilters only. Each ray tests sight-blocking background, doors, ordinary objects, and path blockers while `CDTYPE_AIOPAQUE` excludes props carrying `OBJFLAG_AISEETHROUGH`. The queried object temporarily excludes its own perimeter; a canonical doorway excludes all of its linked leaves, preventing target-self rejection without ignoring intervening opaque props. Recessed objects and doors may pass through one of five conservative points on the closest model-box face when their setup origin is inside supporting geometry. If all ordinary interactable probes fail, an on-screen object whose native interaction does not request LOS may retry those five points eight units closer to the camera. Native render-post-background monitors use a 24-unit bounded retry; every endpoint must still pass a collision test, and render state alone cannot admit it. Detailed `beacon/scan_result` records expose `los_sample=0` for the origin, `1` through `5` for an ordinary surface sample, `6` through `10` for an embedded retry, and `los_queries` for the number of early-exit probes.

For transparent-obstruction coverage, place an eligible hostile, non-hostile
person, interactable, pickup, and visible door behind the DataDyne pane from
incident captures 3 and 4, or another prop carrying
`OBJFLAG_AISEETHROUGH`. Each applicable presence cue must remain available
while the intact pane is between the camera and target. Interpose an opaque
crate, opaque breakable path blocker, closed opaque door, and background wall;
each must suppress the corresponding cue and automatically reacquire when
removed. Confirm transparent but invincible glass also permits visual
presence, while breakable but opaque scenery does not. Point the crosshair at
an enemy through intact glass: the enemy presence cue may sound, but the
actual alignment tone must select the glass obstruction or remain silent
rather than claiming a shot on the enemy. Break the pane and confirm alignment
then follows the exposed target. Repeat from the CamSpy for supported door and
people categories. Correlate target flags, `line_of_sight`,
`visibility_sample`, scanner rejection reasons, and the raw attack-query prop.

Regression-test the DataDyne Investigation bot-activation maintenance terminal identified in sessions `1785248208` and `1785259338` as prop 81, tag `0x02`, `MODEL_TVSCREEN`. The later captures showed it on-screen, healthy, active, explicitly interactable, and rejected only after all 11 LOS probes failed at both approximately 219 and 133 units. Its setup carries `OBJFLAG_MONITOR_RENDERPOSTBG`, and `PAD_EAR_0214` places the rendered plane about 14 units inside the mounting geometry, beyond the generic eight-unit retry. With F5 active and the terminal visibly exposed, confirm it enters the schedule and that a detailed scan reports embedded `los_sample=6` through `10`. Then test a visible CI laptop or ordinary terminal, move behind an opaque wall or closed world-geometry partition while remaining nearby, and wait through at least two refresh intervals; its 880 Hz chirp must stop and the log must report `line_of_sight_blocked`. Pay particular attention to a render-post-background monitor behind an unrelated opaque wall: it must remain excluded beyond the bounded 24-unit mounting allowance. Restore the direct path and confirm automatic reacquisition without toggling F5. Repeat the occlusion transition after the target is already scheduled to prove the pre-pulse validation uses the same LOS policy.

Start CI device training for the Data Uplink with F8 enabled and F5/F6 disabled. Before the exercise exposes the device, there must be no pickup pattern for its disabled, invisible, uncollectable setup object. Once the Uplink appears on the table, confirm its position is represented by exactly three quick 880 Hz chirps: 35 ms per chirp with 25 ms gaps. Confirm the pattern follows distance and stereo pan, stops behind door/background sight blockers, and disappears automatically within the next half-second refresh after the player collects it. Repeat with representative weapons, ammunition, keys, shields, scripted collectible objects, manually activated pickups, and walk-over pickups where available. Uncollectable/invisible objects and projectile pickups still reserved for another character or not yet settled must remain silent. Toggle F5, F6, and F8 in several combinations and verify each independently controls only its category; all targets still share one staggered timeline without simultaneous starts or a new native game-audio channel.

The project owner's first enabled runtime pass confirmed that the laptop beacon sounded, proving the command, scan, pulse scheduling, and positioned object-audio path, but no office-door beacon was initially available. The comprehensive log identified the nearby unlocked, healthy door between rooms 14 and 16 as prop 9 and showed it was rejected solely because its setup flags included `OBJFLAG_DEACTIVATED`. The game's `doorTestForInteract` does not reject doors on that flag; it checks `OBJFLAG_CANNOT_ACTIVATE` and `maxfrac`. The beacon predicate was corrected to keep `OBJFLAG_DEACTIVATED` object-only, and a subsequent blind-user retest confirmed the office-door beacon.

#### Non-hostile-character beacons

Confirm the session-start record reports `non_hostile_beacons=1`. In Carrington Institute gameplay, leave F5/F6/F8 off, press F7, and approach known staff. Every retained friendly or neutral person should emit a continuous positioned chord with a 440 Hz fundamental and quieter 550 Hz major third. Confirm the static consonant drone is readily distinguishable from one-chirp doors, three-chirp pickups, hostile pulses, and player markers' moving 300–600 Hz base plus 800 Hz identity chirps. Toggle F7 off and confirm all character drones fade out promptly without affecting hostile targeting or other beacon categories. Then enable doors, pickups, people, and at least one player marker together and confirm their identities remain unambiguous.

With F6 enabled, face a nearby single or paired door and confirm exactly one positioned chirp source is scheduled while at least one leaf is rendered. Turn until the entire sibling group leaves the active view and confirm its cue stops before the next pulse, then turn back and confirm automatic reacquisition. For a paired door, correlate both leaf candidates with one `canonical_propnum`, one `scan_result`, and at most one `schedule_target`; the other leaf must log `duplicate_canonical_door`. Repeat while another physically distinct door is simultaneously on-screen so legitimate multiple-door round-robin feedback is not collapsed.

Regression-test the paired Investigation doorway recorded in session `1785259338`, capture 3, as props 29 and 30 in rooms 18/19. Both leaves were active, usable, on-screen, and approximately 190 units away, but their embedded model origins failed LOS and produced an empty schedule. Confirm the corrected query accepts one bounded surface sample (`los_sample=1` through `5`), emits exactly one 440 Hz doorway chirp, and restores both collision perimeters after every scan and pre-pulse validation. Close another opaque door or interpose opaque scenery between the camera and this pair and confirm it still blocks the cue. Repeat with a single-leaf door to cover the non-sibling path.

Test a nearby hostile and confirm it receives no F7 drone. Where controllable, test a character before and after an allegiance change, death, knockout, hidden/untargetable state, and cloak with and without IR perception. Put doors, objects, or background geometry between the camera and the person's body midpoint and confirm the cue fades out, then automatically reacquires after sight is restored. Stand near two and then three eligible people and confirm all retained drones remain continuously spatialized at their independent bearings. Move among more than three non-hostile characters and confirm stable nearest-target retention with the existing 150-unit replacement margin; a replacement must fade/restart its fixed slot without a click or stale direction. Repeat outside CI and in a one-local-player Combat Simulator match to verify all four scanner categories remain available. Menus, pause, cutscenes, player death, split-screen, feature disable, stage teardown, and shutdown must suppress or clear the drone according to the documented state-preservation policy.

Enable distinct combinations of F5, F6, F7, and F8, then complete, abort, restart, and advance between consecutive missions and Combat Simulator matches. Every stage teardown must stop the active chirp and all friendly drones, then discard all prop identities, result indices, schedules, drone slots, observer pointers, and timing state while retaining the exact four category selections. Eligible gameplay in the next stage must perform a fresh scan and resume those categories without a toggle earcon. Verify that a category deliberately left off remains off, including F7 when moving into or out of a mission where non-hostile cues are unwanted. Initialization and orderly shutdown must still clear all four selections, and no pointer or prop number from the prior stage may appear in the new schedule or drone assignments.

In DataDyne Central: Defection, approach Cassandra De Vries before obtaining her necklace. Confirm her native blue-sight protected state produces the F7 continuous people drone even though her scripted team is hostile. Aim at her and confirm the alignment tone uses the rapid 90 ms sound/10 ms gap pattern, while the hostile distance-cadence voices remain silent for her. Move the crosshair between her and an ordinary hostile and confirm the hostile lock remains solid at the same pitch. Knock her out and confirm both cues stop. Correlate the log's `protected_character`, `protected_nonlethal_target`, `aimonly=1`, `interrupted=1`, `pattern=90ms_on_10ms_off`, and life-state exclusions; do not special-case her stage character identifier.

Correlate `beacon/candidate` reasons (`friendly_character`, `neutral_character`, `character_hostile`, life/visibility exclusions), `scan_result`, `friendly_drone_assign`, `friendly_drone_release`, `friendly_drone_stop`, and `category_state non_hostile_active` with perceived output. Run with all four categories and inspect periodic telemetry for at most nine serialized chirp targets plus three fixed friendly-drone slots, stable identities, bounded mixer work, and no progressive frame, channel, or memory growth. Advanced `performance/frame_window` records must expose `friendly_enabled_slots`.

#### CamSpy beacon perspective

Enable F6 doors and F7 people before deploying a CamSpy. Identify a door and non-hostile person near Joanna but outside the CamSpy's range or rooms, then switch to the CamSpy and confirm those old-body cues stop without retoggling. Move and turn the CamSpy toward different eligible doors and people; range, stereo direction, room connectivity, and line of sight must follow the remote camera. A person drone in remote view must correspond to a character actually rendered in the current viewport; turn that person offscreen and confirm the drone leaves the next refresh. The CamSpy itself must never produce a people drone. Switch back to Joanna and confirm her nearby set returns automatically with F6/F7 still active. Destroy, collect, or deactivate the CamSpy and repeat rapid perspective changes; no stale drone, stale chirp, or invalid-prop access may occur.

With F5/F8 also active in Carrington Institute, confirm interactable and pickup cues pause during CamSpy viewing and resume from Joanna after returning. Correlate `beacon/observer_change`, `scan_start observer_remote`, candidate reasons, rebuilt schedules, pulses, and friendly-drone slot events with each visible transition. Run several transitions with advanced diagnostics and verify fixed result/schedule/drone storage, one shared object chirp lane, three friendly voices, and no new memory, channel, or frame-time growth.

In DataDyne Research, deploy the CamSpy, move it beyond Joanna's immediate 100-unit retrieval radius, and exit remote control without destroying it. With F8 enabled, confirm the inactive deployed CamSpy receives the three-chirp pickup pattern from Joanna's position, obeys the ordinary 1,200-unit range, room, and door/background line-of-sight rules, and disappears promptly when retrieved. It must remain absent while actively controlled, held, hidden, destroyed, or viewed from its own remote perspective, and it must never receive the F7 people pattern. Correlate `inactive_deployed_camspy`, `camspy_currently_active`, the character-backed result identity, schedule membership, pulse, and post-retrieval invalidation.

After the channel-reuse hardening, the project owner reported that the prior game/video choppiness was gone. The corresponding 297.5-second telemetry window contained 170 successful pulses, 144 exact-channel reuses, zero allocation failures, no more than one accessibility-owned channel at a time, no more than four total property-sound channels in use, and sound-state counts that repeatedly returned to zero. Working set grew 6.18 MiB and private bytes grew 7.11 MiB, predominantly during initial loading rather than as sustained linear growth. The session ended through the normal beacon, menu, and speech shutdown paths. This supports the fix for the observed slowdown but does not replace a longer soak test.

## Menu interaction scripts

### Active weapon menu

With `Accessibility.MenuNarration=1`, hold the active-menu control in ordinary single-player gameplay and use each supported directional input path to highlight weapon/device slots. Confirm every occupied non-center slot announces exactly the localized label drawn in that slot, including `Unarmed`. Hold one direction and confirm it speaks once rather than every tick. Release direction to return to the center, then select the same slot again and confirm it speaks again. Move directly between several slots and confirm each settled selection interrupts the prior short announcement with the new label; empty slots and the center `Weapon` label remain silent.

Release the active-menu control while speech is underway and confirm the selected item is still applied normally and the short announcement remains understandable. Reopen the menu, change inventory, collect a device, select a cloak with a changing duration label, die, pause, enter a cutscene, and change stages; verify there is no stale or repeated selection. Function and bot-order screens must remain silent in this bounded slice. Correlate speech with one `active_menu/focus` record per deliberate selection and `active_menu/context_cleared` on relevant closure/scope loss. Multiplayer output remains unvalidated and is intentionally suppressed for non-primary local players.

### Script A: deterministic focus basics

Precondition: a documented profile/start state that reaches a known dialog; speech enabled; logging explicitly enabled and its comprehensive contents explained to the tester.

1. Open the dialog and listen for title/context followed by final initial focus.
2. Move once in each supported direction and compare spoken focus to the actual semantic item.
   Confirm the dialog title is not repeated for these within-dialog focus changes, but is spoken again after leaving and returning to the dialog.
3. Navigate rapidly through at least five items; confirm stale items are replaced rather than read in a long tail.
4. Invoke repeat; confirm current context is spoken even if deduplication would suppress it.
5. Invoke cancel; confirm speech stops and game focus does not change.
6. Open a child dialog, return, and confirm the restored/current focus.
7. Attempt a disabled item and confirm availability is understandable without an incorrect activation claim.

### Script B: complete focusable-control matrix

Exercise one real example each of selectable action, checkbox, slider, closed/open dropdown, standard list, custom-rendered list, keyboard, focusable scrollable content, carousel, ranking, and player stats. For each, record label source, role, value, changed-value/subfocus timing, position/count, boundaries, disabled state, cancel/back behavior, repeat output, and semantic-provider failures. Confirm a slider with a nonempty `MENUOP_GETSLIDERLABEL` result announces that semantic display label at its minimum, midpoint, and maximum; confirm an ordinary slider without one falls back to a rounded percentage rather than raw engine units. Confirm a selectable with visible right-side text announces both its left label and right-side value. Include mouse focus parity and controller/keyboard parity even when the blind task uses one input method.

The implementation must also run a source audit that compares every current focusable `MENUITEMTYPE_*` and every focusable `MENUITEMFLAG_LIST_CUSTOMRENDER` handler with the semantic resolver/provider table. Unknown numbered or future types must fail the development audit and log safely at runtime; they must never receive guessed speech.

### Script C: New Agent to settings blind task

Use a temporary lawful save/config directory and start with no selected profile. Confirm the startup `Perfect Dark` custom list announces its actual rows, including `New Agent...`; create a uniquely named agent through the on-screen/physical keyboard; complete any save-location dialog without overwriting user data; reach `Perfect Menu`; swipe to Options; enter a chosen standard or Extended settings dialog; change one setting; verify its new value; and return to a known context. The observer may explain scope and emergency stop beforehand but must not coach individual moves.

Record completion, wrong turns, missing/excessive/late output, rapid-navigation replacement, repeated/cancelled announcements, recovery, time, and tester confidence. Main-menu definitions vary with profile, mission, multiplayer, unlock, memory, platform, and region state, so the report must capture its actual start conditions rather than assume one universal sequence.

## Gameplay feature scripts

### HUD/objectives

With `Accessibility.HudMessages=1`, start Carrington Institute holo-training session 1 and confirm its instructional HUD messages speak automatically in displayed order without interaction. Trigger another accepted ordinary HUD message elsewhere to verify the common hook is not training-specific. Confirm an in-game subtitle/dialogue and a cutscene subtitle are not spoken by this HUD feature, while `hud/message_accepted` and `hud/speech_suppressed reason=subtitle` retain their metadata. Trigger a suppressed duplicate and confirm there is no second admission or speech request. Check multiline/control whitespace is spoken as one normalized sentence, and confirm menu focus speech can interrupt queued HUD speech without preventing a later HUD message. Repeat with `HudMessages=0` and confirm accepted messages are logged with `reason=feature_disabled` but not spoken.

Select DataDyne Central: Defection at each unlocked difficulty and enter the pre-mission Overview. Confirm the stage/Overview title is followed by every visually listed objective in the same order and numbering, then the focused Accept or Decline button. Objectives excluded by the selected difficulty must not be spoken. Move between Accept and Decline and confirm the objective list does not repeat; press F5 and confirm the complete title, list, and current button are reconstructed. Decline, choose another mission or difficulty, and verify the new dialog does not retain stale objective text. Also check a mission with the maximum available objective count and the “No briefing for this mission” fallback where available.

During DataDyne Central: Defection, pause before completing an objective and confirm the Status entry announcement contains the stage title, every visible difficulty-applicable objective in display order, each visible localized state, and the focused Abort control. Complete and, if safely reproducible, fail objectives; reopen pause and verify the spoken Complete, Incomplete, or Failed states match the visual rows. Navigate to sibling pause dialogs and back, and press F5, confirming the full current Status summary is restored without stale states or repetition during ordinary focus changes. Repeat in the vertical two-player pause layout where available.

On the pause Briefing screen for DataDyne Central: Defection, confirm the announcement continues beyond “Objective One: Secure” through every section and final objective. Compare the normalized speech/log text with the complete string returned by `menuitemScrollableGetText(DESCRIPTION_BRIEFING)`; no text may disappear at the former 768-byte boundary. Scroll to the bottom, press F5, leave and return, and confirm complete repeatable output without stale text. Test the longest available briefing and inspect announcement/backend conversion results for accepted output, full byte/unit counts, bounded fixed snapshot storage, and no progressive memory growth.

On the pause Inventory screen, move through Unarmed, ordinary weapons, devices, and a mission-specific item. Each focused row must announce its visible list name, actual manufacturer when present, primary and secondary functions when present, and the full marquee description. A device also reports `checked` when active and `not checked` when inactive, matching its visual checkbox. Compare CamSpy mode variants and the Attack Ship necklace case to the visual callbacks. Confirm item selection/equipping behavior, device checkbox state, weapon models, labels, and marquee rendering remain unchanged. The first row includes the Inventory title; later rows speak only the newly focused rich option, and F5 reconstructs the title and current option. Open the Abort confirmation and confirm its complete visible question precedes the focused Cancel or Abort button.

Complete and fail a mission and inspect every endscreen page. The first results screen must announce exactly the visible mission status, agent status, mission time, optional target time, difficulty, optional newly unlocked cheat, weapon of choice, kills, accuracy, and visible shot rows before `Press START`. When a cheat announcement replaces one or more shooting-stat rows visually, speech must follow the same visibility rule. The Objectives sibling must announce every difficulty-applicable objective with its displayed Complete, Incomplete, or Failed state. Continue to Retry or Next Mission and confirm their objective summaries omit status, matching those screens. Repeat with the vertical two-player endscreen layout where available, and use F5 on each page to verify the same complete current summary without stale data from its sibling.

For the remaining Milestone 7 work, trigger automatic objective completion/failure announcements and briefing review. Confirm correct player context, ordering, no objective double-speech, and usable recovery after an interruption.

### Status

Query at full and partial health/shield; change weapons/functions; test loaded and reserve ammunition, reload, empty ammo, dual wield, pickups, death/restart, pause, and a scripted/training health change. Compare announcements to semantic APIs or controlled in-game state, not solely a visual bar.

### Targeting and scanner

#### Environmental damaging lasers

With `Accessibility.EnvironmentalHazards=1`, start Carrington Institute holo-training 3 and leave F5/F6/F8 beacons off for the first pass. Approach each horizontal laser while looking toward it. At no more than 500 world units and within the 25-degree facing cone, confirm one 220 Hz tone fades in and audibly sweeps from one physical endpoint to the other and back over 90 ticks. Turn just outside the cone, turn fully away, retreat beyond range, and place opaque background geometry between the camera and beam; each condition must fade the hazard lane out. Restore eligibility and confirm automatic reacquisition without a key press.

Move through adjacent laser bars and confirm the nearest eligible bar replaces the prior one without rapid oscillation; the 75-unit margin should retain the current identity until another is materially nearer. Verify fully faded/open, disabled, and non-colliding lasers remain silent. Exercise standing, ducking, and crouching, then pause, open a menu, abort/complete the exercise, leave the stage, disable the setting, and shut down. In each case confirm the lane stops. Repeat with beacons and the firing-range fine-aim tone active where practical to confirm the three procedural voices do not interrupt one another. Run several sessions and check for stuck sound, frame degradation, or growing memory.

Correlate `hazard/scan`, `selection`, `selection_lost`, `sweep`, `scan_guard`, and `reset` records with the observed beam. Validate endpoints, closest/source distances, facing dot, sweep phase, source position, attenuation, and pan. `scan` aggregates all rejection categories and `sweep` captures the selected source, each rate-limited to once per second; unchanged ineligible props do not create per-frame log records.

#### Hostile-character targeting

For long-range acceptance, use visible hostiles between 4,000 and 12,000 world units. At normal FOV the ordinary 4,000/5,500/6,000 curve must remain audible through long mission sightlines and stop at 6,000; narrowing the live FOV must blend continuously toward the stronger scoped 6,000/9,000/12,000 curve without a sudden gain step. At a 25-percent or greater FOV reduction the complete scoped curve must apply. Verify strong scoped feedback around 6,200–6,300, fading feedback between 9,000 and 12,000, and silence at 12,000. Turning or zooming until the hostile leaves the narrowed viewport, losing semantic line of sight, changing relationship, or eliminating the target must still stop its voice. Correlate the rate-limited `targeting/observation` fields `view_fovy`, `default_fovy`, `zoom_blend`, `range_profile`, and the three effective distances with `combat_slot_cadence` volume. Regression session `1785680476` captured prop 4 near 5,059 units and later assigned it near 4,934 with `volume=0.0000`; session `1785684498` capture 2 found the same class of failure at normal FOV, including prop 4 near 4,755 while the old ordinary 600/3,500/4,000 curve produced zero gain. Session `1785694393` capture 1 found visible, unobstructed hostile props 0 and 16 around 6,200–6,275 units, both admitted semantically but assigned `volume=0.0000` by the former 6,000 scoped ceiling; direct crosshair placement still produced the independent 660 Hz lock. Repeat with a custom player FOV to confirm the blend remains relative to the configured default rather than 60 degrees.

Equip the Sniper Rifle, enter its zoom view, and place one visible hostile around each edge and then near the crosshairs. The selected hostile must chirp for about 100 ms every 160 ms; left/right screen error must reach useful stereo separation relative to the narrowed viewport, and the second half must rise for a target above or fall for a target below the crosshairs. Centering vertically must restore one uniform normal enemy carrier. With two adjacent enemies, move slowly across their midpoint and confirm the selected identity does not chatter; a meaningfully closer target must take over. Leave zoom, switch weapons, occlude or eliminate the target, and verify precision guidance stops or returns that combat slot to ordinary distance cadence. Merely crossing a projected rectangle must never start the solid alignment tone; that tone must still require the native/raw valid aim result. Correlate `precision_guidance_select`, `precision_guidance_stop`, `precision_guidance` in `targeting/observation`, and `cue=sniper_precision` cadence records.

With `Accessibility.TargetingFeedback=1`, start Holo Training 4 and verify that each ordinary hostile receives a positioned bright carrier whose level/aligned base is 900 Hz, with a quieter harmonic at twice the current carrier frequency, when active, combat-capable, rendered in the viewport, and unobstructed. Vertical displacement may move the carrier down toward 600 Hz for a target below or up toward 1,500 Hz for a target above. It must be readily distinguishable from pure-tone doors, people, interactables, and fine-aim feedback. Repeat in a mission with long sight lines at controlled distances around 600, 4,000, 5,500, and 6,000 world units: the cue should remain full through 4,000, fade through 5,500, and become silent at 6,000 without admitting an offscreen or occluded enemy. Compare the 0.25 combat level against speech, weapons, music, and environmental effects, and stress several simultaneous slots for useful direction without unacceptable clipping. The current unarmed function defines `X=60` world units. At a near-body-surface cue distance of at least 300 (`5X`), expect an approximately 180 ms chirp every 500 ms. Approach slowly: period and length should fall smoothly until reaching approximately 50 ms every 200 ms near cue distance 60, with each accumulated 40 ms period reduction advancing the next pulse rather than waiting out the previous cadence. At or inside 60, the cue must switch to a constant spatial tone. Once entered it should remain constant through small collision/animation movement and switch back to chirps only beyond 66 (`1.1X`); approaching from outside must not enter constant mode early in the 60–66 band. Cross both boundaries repeatedly without attacking and confirm the continuous transition ramps without a click. Correlate perception with `targeting/combat_slot_assign`, `combat_slot_cadence`, and session-start records: `base_frequency_hz` must be 900, level/aligned output must remain near 900 Hz, and `target_frequency_hz` plus the start/end frequencies must follow the documented elevation mapping. `enemy_volume` must be 0.25, `continuous` must first change at `X`, `punch_range_exit` must report `1.1X`, and `trigger_now` should mark cadence advances and the transition out of constant mode. Confirm that the constant tone usefully indicates punch range while recognizing that off-target aim, geometry, and animation timing can still prevent a hit.

Multiple enemies should retain independent spatial voices whose starts are staggered rather than exactly simultaneous. Point directly at each hostile while still unarmed and confirm the centered alignment tone starts at a fixed 660 Hz. Move narrowly off and immediately back onto the same still-visible character: a one- or two-frame query loss may retain the tone, must log `aim_loss_grace_start` and `aim_loss_grace_cancel`, and must not generate a stop/start pair. Remain off aim and confirm it stops no later than the third observation. Moving directly to another valid target must switch immediately without inheriting the grace period. Verify moving characters reacquire automatically. Knock out all three training opponents and confirm each voice stops as soon as the knockout fall begins and remains absent while the body lies on the floor. Test partial cover, full occlusion, turning away, pause/menu state, training completion and abort, and several sessions without restarting.

In Carrington Villa, aim a gun at the destructible A51 crates that contain ammo pickups. The exact crosshair query must produce the interrupted 90 ms sound/10 ms gap lock, with `category=9`, `alignment_source=loot_container_raw_query`, and an accepted `targeting/loot_container_aim` record containing both outer and child prop identities. Move among an item-bearing crate, an empty crate, either invincible crate, ordinary scenery, glass, and a hostile: only the item-bearing destructible crate and glass may use the interrupted lock, while the hostile remains solid. Destroy the container and confirm its lock stops; verify the released item enters the ordinary F8 pickup scanner. Repeat with gunfire, melee, and an attack type blocked by native immunity flags. Confirm the feature does not create a positioned presence voice or identify a container before the exact attack query hits it.

In DataDyne Central: Extraction, exercise the Falcon 2 (scope) and both CMP150 functions against several armed DataDyne guards while Dr. Caroll is nearby. Confirm the guards remain eligible for the game's native standard auto aim: their setup assigns `TEAM_ENEMY`, an attached CMP150, and no `CHRCFLAG_NOAUTOAIM`; both player weapons use `INVAIMFLAG_AUTOAIM`, while CMP150 secondary follows its native tracked-prop lock-on path. Dr. Caroll is `TEAM_ALLY` and must not produce hostile alignment or presence feedback. In the rate-limited `targeting/scope_gate` records, confirm `autoaim_x_enabled=1`, `autoaim_y_enabled=1`, and matching non-null X/Y prop identities while native assistance selects a guard; compare those identities with accessibility acquisition/loss records. If either enabled value is zero, verify the loaded profile's Auto Aim option before attributing the problem to level scripting.

For the experimental combat-elevation cue, compare enemies on the same floor, balconies above, and lower floors while keeping horizontal bearing and distance as similar as practical. With the default 900 Hz carrier, every ordinary noncontinuous enemy chirp must begin with 900 Hz. Its second half must remain at 900 Hz when the target midpoint is within five degrees of the engine's current shot-query aim point, rise smoothly toward 1,500 Hz for a target increasingly above that point, or fall toward 600 Hz for one below it. An aligned target should therefore sound like one uniform normal enemy beep; an unaligned target should sound like that normal beep followed by the required higher or lower correction. Hold position and aim vertically toward an elevated enemy: the second half must converge toward the first half as the aim point aligns, then become lower than the first half if the aim point passes above the target. At or inside melee range, confirm the existing continuous cue uses the smoothed elevation pitch across the whole tone and resolves to 900 Hz at alignment. Repeat while auto-aim or manual gun swivel moves the weapon aim away from the camera center; the cue must follow the shot-query aim point rather than Joanna's world elevation or the camera center. Walk, crouch, pass beneath an enemy, and transition across the dead-zone boundary without rapid stepping or identity swaps, while pan, distance cadence, volume, and close-range behavior remain unchanged. Exercise multiple simultaneous enemies at different elevations and verify their ten fixed voices retain independent contours. Security cameras must retain their continuous linear 1,600-to-1,000 Hz sweep rather than adopting the midpoint contour. Correlate `combat_candidate`, `combat_slot_assign`, and `combat_slot_cadence` fields `target_screen`, `aim_screen`, `raw_elevation_degrees`, `elevation_degrees`, `elevation_zone`, `base_frequency_hz`, `target_frequency_hz`, start/end frequency, and `contour` with the perceived output. Ordinary chirps should report `contour=base_then_elevation`, close-range tones `contour=continuous_elevation`, and cameras `contour=camera_sweep`. For a stationary target and smoothly moving aim, the captured coordinates and raw angle must also move smoothly; repeated one-frame alternation between an ordinary angle and either 45-degree clamp is a failure.

In DataDyne Research and another mission containing an automated gun, approach an active hostile turret with `Accessibility.TargetingFeedback=1`. Confirm it receives the same positioned combat-presence cadence as a hostile character while on-screen and unobstructed, and that pointing the actual crosshair ray directly at its geometry starts the same centered alignment tone even though native standard auto-aim does not select object props. With the exact ray hitting no prop, move up to three logical screen pixels outside the turret's projected bounds and confirm the same tone remains available with `aim_source=turret_tolerance`; move farther away and confirm immediate loss. Interpose a closed door or another prop under the exact ray and confirm the fallback is suppressed even if the turret's projected bounds remain nearby. Repeat with two overlapping turret bounds and confirm the closest bounds, then closest center, remain stable. Verify scope zoom naturally changes the projected bounds without changing the three-pixel logical tolerance. No test should observe aim pull, snap, redirected shots, or a claim that random weapon spread will hit. Disable or destroy the turret and confirm both cues stop; repeat with a deactivated, ammunition-empty, or malfunctioning autogun where available. Deploy Joanna's Laptop Gun and confirm it is excluded because its target-team mask does not include her team. Correlate `targeting/combat_candidate` records with `category=4`, `obj_type=OBJTYPE_AUTOGUN`, `aim_source=raw_query` or `turret_tolerance`, object flags, line of sight, and the `autogun_inactive_or_non_hostile` rejection; `scope_gate` must show the matching raw or tolerant prop, `tolerant_distance_px`, `tolerance_px=3.000`, and alignment source. Repeat around multiple characters and turrets to exercise fixed-capacity slot assignment without simultaneous starts or stale identities.

Equip a weapon whose selected function displays the native Threat Detector sight, such as the K7 Avenger secondary function. Compare the visible threat boxes directly with accessibility output: every object in one of the four native boxes must receive the ordinary hostile presence cue, and no object absent from those boxes may be added solely by this detector path. Each newly boxed identity must also produce one positioned bright upward sweep from 1,000 to 2,000 Hz. Exercise representative native categories where available: an eligible autogun, Skedar shuttle, grenade or mine, and a deployed Dragon in secondary mode. Point the exact crosshair ray at each boxed object and confirm the centered alignment tone starts without aim pull, then stops immediately off aim. Switch back to primary, switch weapons, move the threat offscreen, remove/destroy it, and leave gameplay; each must release its detector-derived cue and any pending alert.

In the K7 Avenger firing-range exercise containing mine-bearing targets, confirm ordinary range-target pulses and fine aim remain unchanged, but each target produces the distinct positioned new-threat sweep exactly when its native detector box first appears. The alert must use the long-range enemy curve and remain audible at the roughly 1,700–2,200-unit distances seen in the captured exercise. A continuously boxed but unaimed target must not repeat the new-contact alert. Move exact valid aim among adjacent ordinary targets and the mine-bearing threat: every target may produce the ordinary alignment tone, but only the native threat must add an immediate rising sweep repeating approximately every 250 ms. Leaving its geometry for an ordinary target or empty background must stop that repeated layer on the first observation; reacquiring it must restart immediately. A back-facing/unshootable threat must not claim valid aimed-threat feedback. If acquisition occurs during its initial new-contact sweep, that existing sweep must count as the first pulse rather than click or restart.

Hide a threat for one or two observations and restore it to confirm no false new-contact retrigger; remove it long enough to leave the native list and then show it again to confirm one new alert. Fill all four native boxes simultaneously and verify four nonoverlapping positioned alerts play in a stable serialized sequence, while additional ordinary targets do not alert. Add many ordinary enemies to verify detector candidates do not exceed the existing ten combat slots. An autogun present in both the native detector list and generic combat scan must produce one recurring voice, not two. Correlate `targeting/threat_detector_candidate`, `threat_detector_new`, `threat_detector_alert`, `threat_detector_alert_drop`, `threat_detector_aim_start`, `threat_detector_aim_pulse`, `threat_detector_aim_stop`, ordinary slot assignment/release, raw-aim identity, native slot/bounds, category, distance, volume, pan, and reset reasons. A Shift+F2 capture must include four `incident/threat_detector_state` records with effective detector state, sight type, prop identity, object/weapon semantics, and native bounds. The test must not claim more than the four threats visibly exposed by the native sight.

For partial character exposure, find enemies behind low cover and peeking around vertical cover. A visible head, upper torso, lower torso, or meaningful left/right upper-body edge must start the enemy presence cue before the crosshair acquires the exact model. Fully occlude the same enemy behind shooting-blocking geometry and confirm the cue stops. Correlate `combat_candidate` fields `line_of_sight`, `visibility_sample`, and `visibility_queries`: center should normally exit after one query; partial exposure may report `upper`, `lower`, `upper_left`, or `upper_right`; a fully blocked character should report `none` after five queries. Point at exposed geometry and confirm the exact attack-query alignment remains authoritative even if all visibility samples miss. Stress ten visible enemies and inspect frame/performance diagnostics for regression from the bounded maximum of five early-exit collision queries per character; object targets must remain at one query.

In Carrington Institute and at least one DataDyne mission, face an active `OBJTYPE_CCTV` security camera. While its model intersects the viewport and its center has shooting-blocker line of sight, confirm it emits a positioned high electronic scan descending from 1,600 to 1,000 Hz over 140 ms every 500 ms. Turn it offscreen, interpose a wall or closed door, and return; the cue must stop and reacquire automatically. Point the actual crosshair ray at the camera and confirm the ordinary centered 660 Hz alignment tone starts without aim pull or snap, then stops immediately off aim. Disable, deactivate, or destroy the camera and confirm both outputs stop. A natively camera-disabled decorative or inactive CCTV must remain silent. Correlate `targeting/combat_candidate` with `category=7`, `obj_type=OBJTYPE_CCTV`, `aim_source=raw_query`, health/flags, viewport bounds, line of sight, and `camera_inactive_disabled_or_destroyed`; cadence records must identify `cue=security_camera_sweep`, the two frequencies, 500 ms period, and 140 ms duration. Mix cameras with characters and autoguns to verify stable identities, staggered starts, truthful ten-slot overflow, and no stuck or retuned voices.

In a later mission with friendly or neutral characters, confirm only characters classified as hostile receive positive targeting feedback. Where practical, also test hidden/untargetable state, a cloaked hostile with and without IR perception, an aimed hostile visible only around cover, and a group large enough to exercise the fixed candidate capacity. The accessibility layer must not identify or disclose characters that the existing semantic visibility and relationship rules exclude.

Correlate `targeting/combat_candidate`, `scope_gate`, `observation`, `combat_slot_assign`, `combat_slot_cadence`, `combat_slot_release`, `combat_presence_stop`, `aim_acquisition`, `aim_loss`, `alignment_start`, `alignment_update`, `alignment_stop`, and `telemetry` records with the perceived character set. Confirm cadence logs contain center distance, near-surface cue distance, punch range, `1.1X` exit and `5X` far thresholds, zone, normalized proximity, period, duration, continuous mode, immediate-trigger state, volume, and pan. Confirm combat reports `profile=2`/`source=2`, profile changes reset old identities and owned sounds, eliminated characters report `dead_dying_or_knocked_out`, no more than ten oscillator slots are assigned, phase staggering avoids exact starts, and the fixed-capacity path shows no game sound-channel or memory growth. Stress one through ten simultaneous enemies, then more than ten to verify deterministic bounded selection.

#### Special-device target alignment

With `Accessibility.TargetingFeedback=1`, start the Data Uplink exercise. Before equipping the Uplink, point at the designated terminal and confirm silence. Equip it, point directly at the designated terminal, and confirm the same fixed 660 Hz centered alignment tone used for a generic valid combat aim. Point at nearby computers, doors, the terminal edge/background, and other interactables; each must remain silent. Acquire the tone from beyond interaction range and verify that ordinary use still requires the game's stated range, so the cue is understood as target identity rather than action readiness. Complete, fail/abort, pause, open a menu, unequip the device, and leave CI; each transition must stop the tone.

Repeat with the ECM Mine exercise and hub. Only tagged hub `0x32` should acquire the tone. After acquiring it, make deliberate throws that land correctly, miss, or strike intervening geometry. Confirm the tone means “correct destination surface” and never announces or implies a guaranteed landing. Verify there is no positioned presence beacon from the targeting subsystem in either device profile and that independent F5/F6/F8 beacons retain their existing behavior. Logs should report `profile=3`/`source=3`, the expected weapon and target tag, raw aim identity, eligibility reason, acquisition/loss, and clean profile reset without new allocations or sound channels.

On Special Agent or higher in DataDyne Central: Defection, equip the ECM Mine and point directly at the internal security hub. Confirm the fixed 660 Hz alignment tone appears only over native object tag `0x03`. Repeat at the external communications hub, tag `0x04`. Nearby monitors, computers, and other instances of the same general hub model must remain silent. Move off the target, switch weapons, throw a mine correctly and incorrectly, complete either hub objective, and exhaust the available mines; the mission script remains authoritative and the cue must never claim a successful placement. Logs must show `stage=STAGE_DEFECTION`, `weapon=WEAPON_ECMMINE`, the matching tag, raw aim identity, and acquisition/loss through the common device profile.

Repeat with the Door Decoder exercise and door panel. Before equipping the Decoder, point at panel tag `0x35` and confirm silence. Equip it and confirm the fixed 660 Hz tone sounds only while the raw aim ray is on that panel. Nearby CI hubs, monitors, doors, and other interactables must remain silent. Activate the panel and verify the normal decoder attachment and unlocking sequence remains authoritative; the tone identifies the accepted panel but does not claim that activation range or other use conditions are satisfied. Abort, complete, unequip, pause, open a menu, and leave CI, confirming clean tone loss and `profile=3`/`source=3` logs with `weapon=57` and `target_tag=53`.

In DataDyne Research: Investigation, equip the Data Uplink and point at the computer used to unlock the security door. Confirm that only native mission object tag `0x0a` produces the fixed 660 Hz alignment tone, that nearby computers remain silent, and that unequipping the Uplink immediately restores ordinary combat targeting. Activate the computer from valid range and verify the mission script remains authoritative for connection progress and completion. Logs must show `profile=3`/`source=3`, `weapon=54`, `target_tag=10`, the raw aim prop, and acquisition/loss without allocations or a second targeting voice.

Repeat in the CI CamSpy exercise with `Accessibility.TargetingFeedback=1`. Before the CamSpy startup completes, and while looking through Joanna's view, the photo-target tone must be silent. Enter the remote view and frame the exercise's required holograph object: the fixed 660 Hz tone should start exactly when the whole healthy target is inside the viewport and it is less than 400 horizontal units away. Move the center of view onto nearby scenery while keeping the complete target in frame and confirm the tone remains, matching the game's frame-based photograph rule. Clip any target edge outside the viewport, back beyond range, place the target behind the camera, and destroy it where safely testable; each must stop or prevent the tone. Take a successful picture and confirm completed criteria no longer remain targeted. Exit and re-enter the CamSpy, abort/complete training, pause, open a menu, destroy or collect the CamSpy, and leave the stage; no stale alignment tone may remain.

Run the same checks on a campaign objective that uses `OBJECTIVETYPE_HOLOGRAPH`. Confirm only the objective-tagged object is identified and unrelated people, objects, or scenery remain silent. Correlate `targeting/camspy_candidate` fields (`criterion`, `status`, `tag`, health, rendered state, depth, distance, projected bounds, reason, and raw aim identity) with `aim_acquisition`, `alignment_start`, `aim_loss`, and the visible photograph result. Inspect repeated deployments for the fixed 32-candidate bound, no runtime allocation or new game sound channel, and no frame-time or memory growth.

Acceptance session `1784764601` exposed the initial mismatch. The target for criterion tag `14` was repeatedly `accepted=1 reason=eligible` at 385–394 units with complete bounds inside the 320x220 viewport, and the game then spoke `Info Room PC successfully holographed`; however, `raw_aim_prop` remained null and no alignment began. The corrected policy therefore uses photograph eligibility itself rather than a weapon ray. The same session's unexpected people patterns came from character props `97` and `102`, around 370–455 units from the CamSpy; historical detailed records showed neither carrying `PROPFLAG_ONTHISSCREENTHISTICK`. The remote-only render gate targets that false-positive class without narrowing Joanna's ordinary spatial scanner. Both corrections require a fresh blind-user pass.

#### Weapon-function state cues

With `Accessibility.WeaponFunctionCues=1`, `Accessibility.HudMessages=1`, and the native Show Gun Function option enabled, equip a weapon with two persistent functions and press R1/right bumper. Switching to the secondary function must produce exactly two centered 1000 Hz beeps and speak the localized function label drawn beside the ammo display once at normal, non-interrupting priority; returning to primary must produce exactly one beep and speak its visible label once. Each beep should be brief (35 ms), the two-beep gap should be clearly countable at 30 ms, and output should begin only after the same semantic value that drives the visual indicator changes. Repeat using the active-menu function selector and any configured keyboard binding: input path must not affect either output.

Disable `Accessibility.HudMessages` while leaving `Accessibility.WeaponFunctionCues` enabled and confirm the earcons remain but function-name speech stops. Re-enable HUD speech, disable Show Gun Function, and confirm the direct-rendered label and its speech are both absent. Try initial stage entry, equipping a weapon whose stored secondary function is already active, unavailable alternate functions, temporary alternate functions, and rapid toggles. Initial state and weapon changes must remain silent baselines. Correlate each spoken transition with `weapon_function/announced` and `announcement/output_result group=weapon_function`; no generic `hud/message_accepted` event is expected for this direct-rendered text.

In a one-local-player Combat Simulator match, die and wait for the native respawn overlay. When it first appears, speech must announce the localized `Press START` prompt followed by the same integer displayed beneath it. Each subsequent displayed positive integer must be spoken exactly once, without per-frame repeats. Pause and resume during the countdown, respawn early, allow the timer to expire, die again, and end or restart the match; each newly displayed overlay must announce its prompt and current number, with no stale suppression from the previous death. Disable `Accessibility.HudMessages` and confirm both generic HUD narration and this direct-rendered countdown stop. Correlate `respawn_countdown/announced`, `hidden`, and `reset` with `announcement/output_result group=respawn_countdown`. Record multi-local-player speech arbitration as unvalidated.

With `Accessibility.WeaponChangeAnnouncements=1`, hold the active weapon menu,
move focus across several weapons, and verify each focus name is spoken as
before. Release on an ammunition-using weapon and confirm its total ammunition
count is queued only after release and only after the weapon equips; the weapon
name must not be repeated. Use controller quick-forward and quick-back commands
and confirm each successful switch announces the localized gun-HUD weapon name
followed by the same total count. Compare the number with reserve plus both
loaded hands, including dual wielding, an empty weapon, and a weapon whose
secondary function uses a different ammunition type. Select unarmed and a
no-ammunition device: radial selection must add no meaningless count, while a
quick change still speaks its name. Rapidly cycle across multiple weapons and
confirm stale intermediate speech is interrupted by the final settled weapon.
Attempt a blocked or superseded switch and confirm it is never announced later.
Repeat with the setting disabled and correlate `weapon_change/pending`,
`weapon_change/announced`, expiration/suppression, and
`announcement/output_result group=weapon_change` records.

Try a weapon with no alternate function, temporary alternate-function weapons, an unavailable function, rapid repeated presses, weapon changes, dual wielding, firing/reloading, pause/menu transitions, death, and stage changes. Equipping a weapon or entering a stage must not announce its stored function as a new toggle. Confirm beacon chirps, hazard sweeps, aiming tones, and combat slots can overlap without interrupting the function pattern. Correlate every audible pattern with `weapon_function/state_change`; advanced `performance/frame_window` records should advance `weapon_function_sequence` without memory or channel growth. Record multiplayer behavior as unvalidated rather than accepted until independently tested.

Use a controlled room with known eligible and ineligible entities. Test friendly/hostile/neutral where applicable, occlusion/cloak rules, target loss, rapid crossings, empty scan, overlapping results, collected/opened/destroyed objects, and multiple local-player context. Explicitly audit for hidden-information leaks.

For the current Carrington Institute firing-range proof, first verify that the pre-session `OK` and `Cancel` controls announce their visible captions rather than only `button`. Start an exercise with F5/F6/F8 beacons off. Confirm one positioned 100 ms bright harmonic pulse per visible target in round-robin order. It must match the configured enemy carrier and timbre while retaining the firing range's own spatial attenuation and fixed round-robin cadence. Move the reticle onto a target and confirm the separate centered continuous sine tone starts; move from the outer scoring ring toward the center and confirm pitch rises smoothly from approximately 660 toward 1320 Hz, then falls smoothly when moving away from center. Fire at several deliberately different pitches and compare the logged `aim_distance`/`quality` and reported scoring rings: distance below 18 should be bullseye, below 37 ring 1, below 56 ring 2, and larger valid distances ring 3. The accessibility value must follow the non-random query point and must not fluctuate with weapon spread. Move directly between targets and confirm the tone follows the new identity without a stale pitch. Move off target and confirm it stops. Hold over a target while it rotates: its positioned presence pulse must continue, the tone must stop as soon as the range reports `facing_away`, and it must resume when that same target becomes shootable again. Confirm `presence_pulse`, `alignment_start`, `alignment_update`, `alignment_stop`, `observation`, `aim_loss`, and `aim_acquisition` logs record matching identity, procedural lane, frequency, volume, pan, shootability, distance, and quality without per-frame log flooding. There is no F9 binding. Destroy targets and confirm replacements enter automatically. Repeat with one and several simultaneous targets, then with F5/F6/F8 enabled to assess masking. Exercise pause, menu, exercise completion/failure, death where practical, stage exit, feature disable, and clean shutdown. On both completed and failed post-session dialogs, confirm the entry announcement and F5 repeat include completion/failure reason, score, targets destroyed, difficulty, time, weapon, accuracy, all four scoring-zone hit/point totals, hit total, and the focused Continue/OK control. Repeat several exercises without restarting the game and inspect `targeting/telemetry` for bounded alignment updates, `procedural_presence_active=0` after scope/reset transitions, no allocations from the tone path, stable sound-state counts, and non-linear memory growth. The targeting subsystem must never create or own a native property-sound channel. Confirm targeting events contain no non-finite hit positions, `screen=nan`, infinite projected bounds, or sustained `projection_capture_unavailable` results; a defensive rejection should be investigated if it recurs. Record that multiplayer remains outside this proof and test ordinary hostile characters separately under the combat procedure above.

### Navigation

#### Virtual cane prototype

Confirm the effective session-start record contains `virtual_cane_mode=1`, `cane_volume=0.1840`, and the expected remaining cane and enemy tuning fields. During unobscured single-player walking gameplay, press F4 repeatedly and verify the order is Slow to Fast to Off to Slow. Slow must play 880 Hz then 1320 Hz, Fast must play 880 Hz followed by two 1320 Hz beeps, and Off must play 880 Hz then 440 Hz; each beep lasts 35 ms with a 25 ms gap, and there must be no speech. Correlate each change with one `cane/command` record containing the selected mode and earcon fields. Left Alt+F4 and Right Alt+F4 must not change cane state or play an earcon; if the operating system leaves the game running, releasing Alt while F4 remains held must not produce a delayed mode change. Menus, pause, cutscenes, death, non-walking movement, and multiplayer must suppress the command and stop all cane audio.

Use controlled geometry for the first pass. Face a flat wall, an angled wall, an inside and outside corner, a doorway, a narrow opening, a pillar/crate, a closed/partly open/open door, a small traversable step, a low obstruction while standing and crouching, a pickup/non-solid decoration, and open space. Verify each ray reaches 900 world units and the audible sequence always travels left to right through -45, -30, -15, 0, 15, 30, and 45 degrees; a miss is silent; characters are not cane targets; door/object collision follows whether the player can currently move through it; and open space produces a silent cycle rather than a confirmation cue. Check near, 450-unit, and 900-unit obstacles to confirm pitch rises smoothly from approximately 300 Hz at maximum reach through 424 Hz at half reach to 600 Hz at contact while the scaled 112.5/750/975 attenuation thresholds independently change volume. Repeat at all seven angles, in both sweep speeds, and while moving continuously. Correlate each observation with the aggregate `cane/sweep` sample fields, especially raw collision point, normal, obstacle/type, bbox, distance, `frequency_hz`, pan, and result. Validate that the returned collision point sounds like the barrier surface rather than the stopped center of the player cylinder.

In DataDyne Research: Investigation, approach the route-blocking pane represented
by incident captures 3 and 4. Every cane ray whose selected collision is the
healthy pane must play a 90 ms falling octave, beginning at twice the normal
distance frequency and resolving to it; neighboring wall rays must retain
their steady 35 ms chirps. Confirm `cane/sweep` reports `breakable:1`, the
blocking prop, and matching start/end frequencies. Aim an ordinary firearm at
the pane's actual geometry and confirm the centered alignment tone uses the
rapid 90 ms sound/10 ms gap pattern even though native sight filtering does
not retain scenery. Move between the pane and an ordinary hostile and confirm
only the hostile lock is solid. Move off the pane's geometry, equip a
non-attack device, destroy the pane, and repeat against decorative or
invincible glass; each must remain silent. Confirm the alignment log records
`category=8`, `interrupted=1`, and `pattern=90ms_on_10ms_off`, and that
`targeting/path_blocker_aim` distinguishes `eligible`,
`current_attack_incompatible`, and non-breakable state. Repeat with another
`OBJFLAG_PATHBLOCKER`, including an explosion-only obstruction if available,
and verify no enemy-presence voice or offscreen/nearby scanner cue is created.

Acceptance evidence, 2026-07-28: the project owner, testing as the blind
primary user, reported that the implementation in commit `9472c1a69` worked.
This accepts the user-facing cane distinction and direct-aim feedback for the
captured DataDyne route-blocking pane in the default `ntsc-final` Windows
build. No new barrier, masking, performance, or interaction problem was
reported during that test. The report does not yet validate decorative or
invincible glass exclusions, incompatible and explosion-only attacks, another
level's path blockers, CamSpy perspective, or long-session stability; retain
those cases as open engineering coverage rather than inferring them from this
single obstruction.

For terrain contours, approach upward and downward stairs, a shallow and steep ramp, a landing, uneven but effectively flat floor, and a sheer edge from several headings. Reproduce the DataDyne staircase regression from session `1784916426`: near position `150.96,-1334.81,-599.92`, the center ray must resolve the lower stair room and report the downward contour before the former late boundary near `146.26,-1357.92,-683.66`. The first height change of at least 12 units and no more than 200 units above or below the current floor within 450 units must produce a clearly rising 140 ms contour for rising terrain and a clearly falling 140 ms contour for descending terrain at the correct horizontal angle. Ordinary barriers must retain their steady 35 ms chirp. Missing floor and a lower floor more than 200 units below a sheer edge must remain silent rather than reporting inaccessible stacked geometry. A slope beyond a nearer closed door or wall must not sound, and a nearer terrain contour must take priority over a farther wall chirp on the same ray. Confirm that ordinary distance still controls the contour's midpoint pitch and attenuation. Compare `terrain`, `terrain_height`, `terrain_distance`, `terrain_room`, `terrain_flags`, `terrain_queries`, `frequency_hz`, `end_frequency_hz`, and `duration_ms` with every compact `probes` entry: each probe must record its position, resolved room list, selected floor room, ground, delta, flags, and one of `no_floor`, `vertical_range`, `below_threshold`, or `selected`. Record false positives from floor seams, decorative geometry, stacked rooms, lifts, and nearby lower floors.

Back up `pd.ini`, then test one valid non-default tuning set and malformed ordering such as fade below full, maximum below reach, near frequency below far frequency, and terrain reach/threshold outside their bounds. Restart between edits. Confirm the session-start log reports the normalized effective values, the configured valid reach changes collision endpoints, reversed pitch endpoints are exchanged, terrain settings alter the look-ahead and minimum elevation, the malformed set remains bounded and audible, and no rebuild is required. Test `nan`, negative, excessive, and zero cane- and enemy-volume inputs plus enemy frequencies outside 100–4,000 Hz; non-finite values must use defaults, registered bounds must clamp finite values, zero cane volume must mute every wall and terrain contour, and zero enemy volume must cleanly mute combat slots without leaving a stuck continuous tone. Restore the acceptance defaults afterward.

Measure Slow as 120 logical ticks including a 30-tick end pause and Fast as 60 logical ticks including a 15-tick end pause in the default NTSC-final build. Walk and turn continuously during both modes: each scheduled sample must use its own live origin and camera direction, and movement must not restart the sequence. Ordinary PC render/interpolation frames with `lvupdate60=0` must preserve the in-progress sweep rather than stopping and restarting at the leftmost sample; verify this at a steady 60 fps and while IR/X-Ray scanner audio or targeting feedback is active. Induce a hitch where practical and confirm no more than one query occurs in a logical tick, overdue samples increment `skipped`, and no catch-up burst is heard. Repeat across room and stage transitions and confirm all seven requested/active mixer bits clear on stop.

Deploy a CamSpy while a cane sweep is active. On the same perspective transition, the incomplete Joanna sweep must stop and a fresh sweep must begin from the CamSpy position, rooms, look direction, and collision cylinder; it must not wait for movement or an F4 toggle. Fly toward walls, angled surfaces, doors, and openings that are remote from Joanna and compare the cue sequence with the CamSpy image. Return to Joanna and confirm an immediate fresh body-origin sweep with the selected Slow/Fast mode preserved. Correlate `cane/observer_change` and each sample's `observer`/`remote` fields; test rapid switches and CamSpy destruction for stale mixer bits, collision errors, or a frame-time regression.

Exercise simultaneous door/interactable beacons, hostile combat slots, fine aim, laser hazards, weapon-function cues, speech, music, and ordinary effects. The variable-pitch 35 ms wall and 140 ms terrain cane chirps must not steal, stop, or retune another lane. Run at least 15 minutes of repeated movement, combat, menus, and holo-training sessions. With `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON`, compare Off and Fast using the same route. Record query count, total/maximum microseconds, skipped/missed cycles, frame rate/gaps, memory deltas, cane command/start/stop counters, and requested/active masks. Investigate any individual query over 2 ms, sweep average over 0.5 ms, sustained frame regression, new recurring maximum gap, memory growth, stuck mask, or audio/speech choppiness before acceptance. These timing thresholds are investigation triggers, not proof of a universal performance budget.

After engineering checks, have a blind tester distinguish a flat wall, angled wall, opening, and intermittent obstacle pattern, then keep Fast enabled while moving in a combat-like situation. Record whether geometry is understandable, responsive, masked, overwhelming, or misleading. This slice is not accessibility-accepted until that task evidence exists, and it does not complete Milestone 11 route guidance.

#### Player-authored audible markers

Confirm the session-start record contains `audible_markers=1`,
`marker_range=1200`, `marker_volume=1`, `marker_line_of_sight=1`,
`marker_keys=F9,F10,F11,F12`, and `marker_voices=4`. Place each slot in a
different known location and verify F9 through F12 produce one through four
800 Hz identity chirps over the opposed 300–600 Hz base sweeps. Press an
occupied key elsewhere to move it; Shift plus the key must play that slot's
centered identity followed by 400 Hz and permanently silence the old
location. Holding, Alt, Control, and removing an empty slot must not create a
marker.

For every slot, approach from left, right, front, and rear and compare pan and
distance gain. Put the marker outside the viewport with a clear path: it must
remain audible. Then interpose a wall or closed door: it must become silent on
the next logical update. Open the door or round the corner without toggling or
replacing the marker and confirm immediate resumption. Test another floor,
moving doors, and rapid obstruction boundaries for false clear rays or audio
chatter. Correlate `marker/command`, `line_of_sight`, `observer_change`,
`scope`, `summary`, and `reset` records with exact observer/marker positions,
rooms, range, and state.

Place all four markers inside range and line of sight. All four bases must
remain spatially present, while the 35 ms identity chirps have countable 75 ms
gaps, patterns never overlap, and pattern starts remain at least 500 ms apart.
Repeat with the fast cane, enemies, targeting, scanners,
hazards, music, effects, and speech. Record masking, clipping, front/rear
confusion, and preferred range/volume.

While controlling a CamSpy, place and remove a marker and verify position,
range, pan, and line-of-sight rays originate at the CamSpy camera. Switch back
to Joanna and confirm the same stored markers immediately re-evaluate from her
camera. Menus, pause, computers, cutscenes, and death must mute but preserve
slots; stage restart/exit, feature disable, and shutdown must clear them.

Run at least 20 minutes with four active markers and repeated obstruction,
menu, CamSpy, death, and stage transitions. Compare frame and audio behavior
with zero and four markers. Because the implementation adds at most four
portal-aware collision rays per logical tick, investigate sustained frame
regression, any return of audio/video choppiness, growing memory/log rate, or
stuck voices before acceptance. A blind tester must independently use a
marker to recognize a revisited junction and return toward it before this slice
is described as accessibility-accepted.

Publish a route card with start, goal, nodes/landmarks, door/elevator state, allowed assistance, deviation definition, recovery condition, and completion condition. Record wrong turns, cue count, speech load, time to recover, motion discomfort, and whether the tester understood why each cue occurred.

## Independent blind-user sessions

Recruit participants who normally rely on nonvisual access for the tested task. Familiarity with Perfect Dark and first-person games should be recorded but not used to invalidate difficulty.

Before the session:

- explain feature scope, emergency stop, recording/logging, retention, and how to withdraw consent;
- let the tester choose whether logs, audio/video, and observer notes may be retained or shared;
- provide setup and command reference, but not an undisclosed route solution;
- define what assistance the observer may give and how it will be marked.

During the session, favor think-aloud comments if comfortable, but do not demand constant narration. Mark timestamps for confusing, missing, late, excessive, or incorrect output and for recovery attempts.

Afterward, ask what information was missing, what arrived too late, what was too verbose, what commands were hard to discover, and whether the task felt controllable. Treat inability to complete as design evidence, not tester failure.

At least one independent completion is needed for a narrow prototype claim. Milestones that explicitly require multiple iterations or users must meet their own stronger criteria.

## Regression checks

Every user-visible change should check:

- accessibility fully disabled;
- speech enabled with logging disabled, and the reverse;
- backend unavailable;
- keyboard, controller, and existing mouse menu behaviour where relevant;
- pause/unpause, dialog/stage transitions, death/restart, and clean exit;
- single player and any affected local multiplayer context;
- default `ntsc-final` plus region builds affected by text or preprocessor branches;
- no unbounded queue, repeated chatter, stale speech after context change, or material frame-time spike;
- no new tracked build output, ROM data, generated assets, logs, saves, or configuration files.

Automated tests should cover pure formatting, queue priority/replacement/deduplication/expiry, JSON escaping, config bounds, and semantic adapters that can be isolated. They complement, not replace, interaction and blind-user testing.

## Rollback and reproducibility

Before risky runtime tests, preserve user configuration and saves. Each experimental feature needs one obvious off switch; native backend failure should fall back to the null/disabled path. Keep semantic hooks tiny so reverting the accessibility module and its ledgered calls restores baseline behaviour.

### R-Tracker nonvisual-interface acceptance

Build with `Accessibility.RTrackerAudio=1`. In CI device training, equip and activate the R-Tracker and confirm the screen reader says `R-Tracker on`. The IR Scanner should produce one yellow-object 700 Hz voice. Turn through a full circle to verify stereo bearing and the clean-front/modulated-rear distinction; approach and retreat to verify faster nearby and slower distant cadence. Where controlled elevation is available, verify level is one chirp, above is a rising pair, and below is a falling pair without rapid threshold chatter.

Collect the tracked scanner and confirm the voice disappears. Deactivate the device and confirm `R-Tracker off`. Activate it in an empty context and confirm `No tracked targets` is spoken once after the brief settling delay, not repeatedly. Pause, open a menu, enter a cutscene, die, restart, and leave the stage where practical: audio must stop while suppressed and rebuild from current native state without a false off announcement.

In Skedar Ruins, verify all three tracked pillars sound at once and disappear individually when their native markers clear. On Attack Ship, verify simultaneous yellow objects and red tracked characters, including removal of a dead or cloaked tracked character. With the native R-Tracker cheat enabled, verify blue items use the distinct 1000 Hz category and coexist with the other categories. The audited maximum is eight markers; logs must report any overflow beyond the ten fixed slots.

Correlate perceived output with `rtracker/announcement`, `scope`, `slot_assign`, `candidate`, `slot_release`, `overflow`, and `scan_summary` events. With `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON`, also inspect `tracker_enabled_slots`, R-Tracker scan timing, frame gaps, memory deltas, and mixer activity over repeated sessions. Investigate scans above the specification's thresholds or any sustained growth/choppiness. The complete semantic and acoustic contract is `documentation/ACCESSIBILITY_RTRACKER_AUDIO_SPEC.md`.

### Combat Simulator audio-radar acceptance

Build with `Accessibility.CombatRadarAudio=1` and run a normal Combat Simulator match with one local player. Compare an F3 pulse directly with the native radar under Radar enabled, No Radar, display-option Radar off, and No Player on Radar. Exercise free-for-all and team matches; living, dead, respawning, cloaked, and uncloaked opponents; and each scenario that adds or replaces markers. The pulse must include exactly the final visible native markers except the current player, without adding line-of-sight, viewport, room, or path restrictions.

Verify the 800 ms clockwise ordering, minimum separation, stereo pan, front/rear distinction, 650-to-1400 Hz distance direction, native level/above/below patterns, and enemy/objective/ally/other timbres. Hold and rapidly press F3 to confirm edge triggering and newest-snapshot replacement. Test zero, one, and at least 16 markers; empty and unavailable radar must use distinct centered responses. F3 must remain available while automatic alerts are off.

Use both Shift keys with F3 and confirm only one rising or falling toggle earcon occurs. Existing enemies on enable, match entry, respawn, or resumed gameplay must form a silent baseline. Spawn, reveal, remove, and move enemies through the 2,000-unit medium and 750-unit close thresholds. Confirm inward crossings only, outward rearm at 2,300 and 900 units, three-second cooldown, 30-logical-tick disappearance filtering, and no stacked Medium+Close event for a direct Far-to-Close transition. Pause, menus, death, match end/restart, unsupported player counts, accessibility disable, and native radar unavailability must stop output and prevent stale or mass-new alerts.

Run an eight-simulant match for at least 20 minutes while repeating pulses, threshold crossings, deaths, cloak changes, pause, and restarts. Mix in targeting, enemy scanner, weapon fire, music, speech, cane, beacons, markers, and device audio. Correlate `combat_radar/frame`, `marker`, `command`, `contact`, `event`, `scope`, `telemetry`, and `reset` records with perceived output. With `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON`, inspect frame gaps, process-memory deltas, mixer state, fixed queue maximum, and dropped events. Any allocation, stale identity, sustained frame regression, repeated boundary chatter, queue growth, or choppiness blocks acceptance. The full contract is `documentation/ACCESSIBILITY_COMBAT_RADAR_AUDIO_PLAN.md`.

### King of the Hill beacon acceptance

Build with `Accessibility.KingOfTheHillBeacon=1` and start a one-local-player King of the Hill match. With Hill on Radar enabled, confirm a quiet 300-to-600 Hz dual sweep continuously points toward the exact native hill marker, uses rear modulation when behind, and remains available at native radar distances and through intervening walls. Press F3 and confirm the hill also appears once in the ordinary radar snapshot as an objective.

Approach the hill. Once direct line of sight and the configured `Accessibility.MarkerRange` are satisfied, confirm the stronger distance-shaped local base replaces the quiet guide and adds exactly one positioned 800 Hz chirp per second. Break and restore line of sight, cross the range boundary slowly, walk through the exact center, and approach from multiple elevations and headings. The transition must not duplicate two hill voices, steal any of the four F9–F12 marker slots, or chatter at the range boundary.

Repeat with Hill on Radar disabled, the complete radar disabled, and the player's radar display option hidden. In each case the unrestricted guide must be absent, while the nearby line-of-sight beacon remains available. Enable Mobile Hill, score a hill, and confirm the old cue stops during movement/fade and restarts at the new floor-adjusted center only after the scenario installs it. Pause, open a menu, die, respawn, restart/end the match, and disable the feature; no stale hill voice may survive. Correlate `hill_beacon/state`, `scope`, `summary`, and `reset` with `combat_radar/frame` and the visible room/radar marker. Run a dense match long enough to check frame cadence, `hill_enabled` mixer diagnostics, and memory stability.

### IR Scanner highlighted-object acceptance

With `Accessibility.IRScannerAudio=1`, start the CI IR Scanner exercise and activate the scanner. Face the native highlighted secret door and confirm one 700 Hz R-Tracker-style spatial pattern appears after it has rendered on-screen. Turn until the object leaves the viewport and confirm the voice stops after no more than the expected one-frame observation delay; turn back and confirm automatic reacquisition. Verify left/right direction against the accepted engine panner, front/rear modulation, distance cadence, and relative-height pattern.

Approach ordinary doors, props, and characters that receive only the scanner's general palette treatment and confirm they do not gain this special target cue. Deactivate or inhibit the IR Scanner, open a menu, pause, complete/abort the exercise, and leave the stage; every voice must stop. Repeat with `IRScannerAudio=0` while leaving other accessibility features enabled. Correlate `rtracker/infrared_state`, `source_changed`, `scope`, `slot_assign`, `candidate source=ir_scanner category=infrared_highlight`, `slot_release`, and `scan_summary` records with the visible target. Test a later mission object carrying `OBJFLAG3_INFRARED` or conditional-scenery state to confirm the behavior is not CI-tag-specific.

### X-Ray Scanner object acceptance

With `Accessibility.XRayScannerAudio=1`, start the CI X-Ray exercise and activate the scanner. Every object, door, or weapon that the X-Ray renderer recolors inside its eraser radius and that appeared in the preceding rendered frame is eligible; the nearest ten receive the shared 700 Hz spatial pattern. Turn away and back to verify automatic removal/reacquisition after the one-frame observation delay. Move toward and away from several shapes to verify nearest-ten replacement, stable slots, direction, cadence, and elevation without stuck voices.

Locate both hidden exercise switches using the audio and confirm their cues remain available when the X-Ray view renders them through intervening geometry. The cue represents a rendered object, not actionability: ordinary furniture and doors may also sound, and activating a switch remains governed by the game. Confirm characters do not receive this generic object cue. Deactivate/inhibit the scanner, open a menu, pause, complete/abort the exercise, leave the stage, and repeat with `XRayScannerAudio=0`; all owned voices must stop. Equip and aim a Farsight and confirm its X-Ray vision mode does not activate this device feature.

Correlate `rtracker/xray_state`, `source_changed`, `scope`, `candidate source=xray_scanner category=xray_highlight`, `slot_assign`, `slot_release`, `overflow`, and `scan_summary` records with the rendered objects. Check `source_distance` against the logged eraser origin/radius and confirm overflow retains the nearest ten. Repeat in a mission where the scanner is available to verify the source is not CI-tag-specific.

A reproducible report includes the commit/patch, config values relevant to accessibility, start state, steps, expected/actual result, timestamps/session ID, and whether the result reproduces after a clean restart. Do not require another developer to possess the tester's save; provide a lawful setup route when possible.

## Report template

```text
Title:
Date/time and timezone:
Tester/observer identifiers (pseudonyms allowed):
Consent granted for: notes / accessibility log / audio-video / sharing

Build commit and branch:
Dirty patch identifier or changed files:
OS/architecture:
Compiler/CMake/generator:
ROM configuration (not ROM path or hash):
Executable/configuration:
Accessibility settings and backend availability:
Accessibility log schema/session ID:

Feature and evidence type:
Starting state and prerequisites:
Task and success condition:
Permitted observer assistance:
Exact steps/inputs:

Expected semantic output:
Actual output and timing:
Spoken output:
Input sequence:
Task completed independently? yes / no / partial
Recovery attempted and result:
Regressions with accessibility disabled:

Tester informed that the log may contain comprehensive diagnostics? yes / no
Relevant log timestamps:
Severity/user impact:
Open questions:
Suggested next smallest experiment:
Rollback/off-switch result:
```

## On-demand player and team status

With `Accessibility.PlayerStatus=1`, enter active one-local-player mission
gameplay and press F1 at known full, damaged, shielded, and unshielded states.
Confirm the report starts with the rounded health percentage, omits shields at
zero, includes nonzero shields, interrupts stale speech, and contains no device
telemetry. Open a menu, pause, enter a cutscene, die, and hold Alt or Control
while pressing F1; each must remain silent and log the matching suppression
reason. Disable the setting and confirm F1 remains silent.

In a one-local-player Combat Simulator match, compare F1 against the native
player ranking, score and limit. Verify timed matches report remaining time and
unlimited matches report elapsed time. Confirm the native one-minute HUD
message is still spoken once through HUD narration and the last-ten-seconds
alarm remains audible without a second automatic status announcement.

Exercise Hold the Briefcase, Capture the Case, Hacker Central, Pop a Cap, and
King of the Hill. Private countdown/progress values must appear only for the
player whose native HUD shows them. Capture the Case carrier information must
not appear when Show on Radar is disabled. In a team match, Shift+F1 must report
the player's team, native team score/rank, enemy kills, deaths, leader gap,
public scenario state, applicable team limit, and time. In a free-for-all,
mission, or any other non-team context, Shift+F1 must produce no speech.
Correlate accepted and suppressed commands with `status/query` and
`announcement/output_result group=status`.

## Coverage language

Use precise statements such as “A blind tester independently opened the Solo Missions destination from the documented main-menu start state on this build.” Avoid “menus are accessible,” “screen-reader support is complete,” or “the game is playable” until the tested coverage truly supports those claims.
