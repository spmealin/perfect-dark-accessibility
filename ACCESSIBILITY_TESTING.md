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

## Runtime smoke procedure

Use a legally obtained supported ROM placed as described by `README.md`. Do not include its location or fingerprint in reports.

For project-owner blind-user acceptance builds, every implemented accessibility feature must default to enabled. A feature may default off during its initial engineering investigation, but it must be switched on before acceptance handoff. Inspect the effective `pd.ini` beside the executable and the accessibility session-start record rather than assuming a compiled default took effect. The tester must not have to discover or manually enable the feature being accepted.

For a normal startup smoke test:

Launch every built Perfect Dark executable from the MSYS2 MinGW64 environment, including smoke tests, scripted interaction tests, and debugging runs. Launching `build/pd.x86_64.exe` from PowerShell, Command Prompt, or another normal command line can fail with missing-DLL errors because the MinGW runtime search environment is absent.

1. Back up the relevant `pd.ini` and save data outside the repository if the test changes persistent settings.
2. Record whether portable/save-directory behaviour or command-line flags differ from defaults.
3. Start the exact executable built in the recorded configuration.
4. Verify accessibility disabled: no backend startup, no accessibility log, no new speech, and normal menu/input/audio behaviour.
5. Enable only the feature under test, restart when required, and verify the documented checkpoint.
6. Exercise backend unavailable, log-write failure where safely reproducible, rapid input, pause, stage/menu transition, and normal exit.
7. Confirm configuration persistence and clean shutdown.
8. Capture `git status --short`; generated and user data must remain ignored and uncommitted.

`--log` enables the existing engine `pd.log`; it is useful for engine diagnosis but is not a substitute for the proposed accessibility playtest log.

## Structured accessibility log

Milestone 2 implements `$S/accessibility.log` separately from `pd.log`. It is created only when both `Accessibility.Enabled=1` and `Accessibility.LoggingEnabled=1`; both now default to one for blind-user acceptance testing but remain configurable. The schema records lifecycle, speech, and menu events.

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

The project owner subsequently performed blind-user acceptance testing and reported that the spoken-menu behavior was working perfectly after two requested refinements: dialog titles are spoken on entry/return but not for every option, and sliders are reported as percentages rather than raw engine units. On that acceptance result, the owner declared Milestone 4 complete. A focused semantic harness, exhaustive control-family matrix, performance measurements, other-region builds, and broader backend-failure combinations were not supplied as part of that user acceptance and remain useful regression follow-up rather than claims made by this milestone.

After that acceptance pass, the firing-range training-information dialog exposed another nonvisual gap: its weapon description and challenge fields are rendered in labels and a deliberately non-focusable `DESCRIPTION_FRWEAPON` scrollable panel, so focus narration previously moved directly to `OK` or `Resume`. The menu observer now supports explicitly opted-in `MENUACCESSIBILITYPART_SUMMARY` providers. The firing-range confirmation handler uses that path to provide the localized weapon name, difficulty, applicable goal/accuracy/target/time/ammunition values, and `frGetWeaponDescription()` text. The core speaks the summary once on dialog entry, omits it during normal focus movement, and reconstructs the complete title/summary/current-focus announcement for F5 repeat. An initial implementation unsafely queried every non-null item union as though it were a function pointer and crashed while loading a profile; `MENUITEMFLAG_ACCESSIBILITYSUMMARY` now gates every summary call, preventing dialog pointers from being invoked. The updated default MinGW64 build passes; profile loading plus audible runtime and blind-user verification of several weapons and both pre-game/in-game variants remain pending.

The project owner clarified that the desired CI terminal is the separate `Weapons Available` laptop, not the challenge-goals dialog. That screen renders manufacturer, weapon name, primary function, secondary function, and a marquee description beside a simple weapon list. Its list now opts into a rich `MENUACCESSIBILITYPART_OPTION` result, assembled from the same localized weapon definition and function strings used by those visual fields. Blind-user acceptance testing subsequently confirmed that the weapon menu reads properly. Broader regression coverage across multiple weapons, regions, and visual behavior remains useful follow-up.

### Milestone 5 engineering evidence (2026-07-19)

The Carrington Institute interactable-beacon implementation compiles for the default `ntsc-final` x86-64 Windows target. Configuration used the Unix Makefiles generator and the documented MinGW64 toolchain; `cmake --build build -j4 -- -O` completed and produced `build/pd.x86_64.exe`. The build emitted existing project and vendored-Tolk warnings but no warning from `accessibility_beacon.c`.

Source review confirms that the implementation is inert unless both top-level accessibility and `Accessibility.InteractableBeacons=1` are effective. In unobscured gameplay, F5 independently toggles the nearest interactable-object beacon and F6 independently toggles the nearest door beacon; neither, either, or both categories can be active. The same keys retain their existing repeat/cancel meanings in menus. The scan is restricted to single-player `STAGE_CITRAINING`, active props, a 1,200-unit radius, and the current/directly-adjacent room boundary. Interactable objects require a CI tag or established terminal/interactable flag; doors are canonicalized through sibling links. It does not call `propFindForInteract`, `objTestForInteract`, `doorTestForInteract`, `propobjInteract`, or `propdoorInteract`, so scanning cannot select or activate the game's interaction target.

The two categories use existing positioned one-shot sounds: `SFX_MENU_FOCUS` for interactable objects and `SFX_MENU_SUBFOCUS` for doors. Both are owned by the new `PSTYPE_ACCESSIBILITY_BEACON`; the runtime tracks one target and channel per category and offsets door pulse timing when both categories are enabled. A stage-stop hook clears both before prop/audio teardown. Candidate, ordering, selection, door state, sound channel, volume/pan, pulse, invalidation, command, and lifecycle details are written through the existing accessibility logger when logging is enabled.

No executable runtime, audible localization, empty-scan, real CI candidate-set, transition, long-session, or blind-user task evidence has been collected for Milestone 5 yet. Do not describe the beacon as runtime-verified or accessibility-complete until the matrix in `milestones/ACCESSIBILITY_MILESTONE_05_PLAN.md` is exercised.

The project owner's first enabled runtime pass subsequently confirmed that the laptop beacon sounded, proving the command, scan, pulse scheduling, and positioned object-audio path. No office-door beacon was available. The comprehensive log identified the nearby unlocked, healthy door between rooms 14 and 16 as prop 9 and showed it was rejected solely because its setup flags included `OBJFLAG_DEACTIVATED`. The game's `doorTestForInteract` does not reject doors on that flag; it checks `OBJFLAG_CANNOT_ACTIVATE` and `maxfrac`. The beacon predicate was corrected to keep `OBJFLAG_DEACTIVATED` object-only. Door audio and blind-user localization remain pending a rebuilt retest.

## Menu interaction scripts

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

Exercise one real example each of selectable action, checkbox, slider, closed/open dropdown, standard list, custom-rendered list, keyboard, focusable scrollable content, carousel, ranking, and player stats. For each, record label source, role, value, changed-value/subfocus timing, position/count, boundaries, disabled state, cancel/back behavior, repeat output, and semantic-provider failures. Confirm slider minimum, midpoint, and maximum are announced as percentages rather than raw engine units. Include mouse focus parity and controller/keyboard parity even when the blind task uses one input method.

The implementation must also run a source audit that compares every current focusable `MENUITEMTYPE_*` and every focusable `MENUITEMFLAG_LIST_CUSTOMRENDER` handler with the semantic resolver/provider table. Unknown numbered or future types must fail the development audit and log safely at runtime; they must never receive guessed speech.

### Script C: New Agent to settings blind task

Use a temporary lawful save/config directory and start with no selected profile. Confirm the startup `Perfect Dark` custom list announces its actual rows, including `New Agent...`; create a uniquely named agent through the on-screen/physical keyboard; complete any save-location dialog without overwriting user data; reach `Perfect Menu`; swipe to Options; enter a chosen standard or Extended settings dialog; change one setting; verify its new value; and return to a known context. The observer may explain scope and emergency stop beforehand but must not coach individual moves.

Record completion, wrong turns, missing/excessive/late output, rapid-navigation replacement, repeated/cancelled announcements, recovery, time, and tester confidence. Main-menu definitions vary with profile, mission, multiplayer, unlock, memory, platform, and region state, so the report must capture its actual start conditions rather than assume one universal sequence.

## Gameplay feature scripts

### HUD/objectives

Trigger one accepted ordinary HUD message, subtitle/dialogue, suppressed duplicate, objective completion, objective failure if safely reproducible, briefing review, and pause objective review. Confirm correct player context, ordering, no objective double-speech, and usable recovery after an interruption.

### Status

Query at full and partial health/shield; change weapons/functions; test loaded and reserve ammunition, reload, empty ammo, dual wield, pickups, death/restart, pause, and a scripted/training health change. Compare announcements to semantic APIs or controlled in-game state, not solely a visual bar.

### Targeting and scanner

Use a controlled room with known eligible and ineligible entities. Test friendly/hostile/neutral where applicable, occlusion/cloak rules, target loss, rapid crossings, empty scan, overlapping results, collected/opened/destroyed objects, and multiple local-player context. Explicitly audit for hidden-information leaks.

### Navigation

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

## Coverage language

Use precise statements such as “A blind tester independently opened the Solo Missions destination from the documented main-menu start state on this build.” Avoid “menus are accessible,” “screen-reader support is complete,” or “the game is playable” until the tested coverage truly supports those claims.
