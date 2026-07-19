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

For a normal startup smoke test:

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

Milestone 2 implements an opt-in `$S/accessibility.log` separate from `pd.log`. It is created only when both `Accessibility.Enabled=1` and `Accessibility.LoggingEnabled=1`. The current schema records lifecycle events; later milestones will add feature events.

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

Privacy redaction and data minimization are not requirements for development logs. Never include ROM contents, extracted copyrighted assets, passwords, authentication tokens, or unrelated operating-system secrets. Logs stay disabled by default, local, ignored by Git, and never upload automatically. Tell testers that a shared log may contain comprehensive raw diagnostics.

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

## Menu interaction scripts

### Script A: deterministic focus basics

Precondition: a documented profile/start state that reaches a known dialog; speech enabled; logging explicitly enabled and its comprehensive contents explained to the tester.

1. Open the dialog and listen for title/context followed by final initial focus.
2. Move once in each supported direction and compare spoken focus to the actual semantic item.
3. Navigate rapidly through at least five items; confirm stale items are replaced rather than read in a long tail.
4. Invoke repeat; confirm current context is spoken even if deduplication would suppress it.
5. Invoke cancel; confirm speech stops and game focus does not change.
6. Open a child dialog, return, and confirm the restored/current focus.
7. Attempt a disabled item and confirm availability is understandable without an incorrect activation claim.

### Script B: values and types

Exercise one real example each of selectable action, checkbox, slider, list, dropdown, and scrollable content. For each, record label, role, value, changed-value timing, boundaries, cancel/back behaviour, and unsupported-state output. Include mouse focus parity as a regression even when the blind task uses keyboard/controller.

### Script C: main-menu blind task

Choose one stable destination visible in `src/game/mainmenu.c`, document the exact start state and profile assumptions, and ask the tester to reach it without live sighted direction. The observer may explain the test and emergency stop beforehand but must not coach individual moves. Record completion, wrong turns, repeated/cancelled announcements, recovery, time, and tester confidence.

Main-menu definitions can vary with game state (for example profile, mission, multiplayer, or unlock state), so reports must not assume one universal sequence.

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
