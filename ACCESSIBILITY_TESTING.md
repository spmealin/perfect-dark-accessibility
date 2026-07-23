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

The temporary `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS` CMake option defaults to `OFF`. Enable it for a diagnostic build in the required MinGW64 environment with `cmake -G"Unix Makefiles" -Bbuild -DACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON .`, then rebuild normally. Return to the normal build with the same configure command using `OFF`.

When compiled in and accessibility logging is active, `performance/frame_window` is emitted approximately once per real-time second regardless of whether targeting, beacons, or hazards currently have a selected object. It records rendered-frame rate, the longest observed inter-frame gap, logical game-tick rate and delta fields, stage/menu context, Windows working-set and private-byte totals and session-baseline deltas, desired oscillator states including the enabled combat-slot count, and fixed-buffer mixer call/pass-through/active/frame counters. Use it to correlate a reported slowdown with memory growth, an oscillator that remained enabled, or continued expensive mixing. Hazard `scan` audits additionally include the current, average, and maximum scan duration in microseconds for the preceding audit window. These records are diagnostic observations only and do not allocate or lock in the audio callback. The session-start record reports `performance_diagnostics=1` and its interval when present, or zero when compiled out.

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

The project owner subsequently performed blind-user acceptance testing and reported that the spoken-menu behavior was working perfectly after two requested refinements: dialog titles are spoken on entry/return but not for every option, and sliders are reported as percentages rather than raw engine units. On that acceptance result, the owner declared Milestone 4 complete. A focused semantic harness, exhaustive control-family matrix, performance measurements, other-region builds, and broader backend-failure combinations were not supplied as part of that user acceptance and remain useful regression follow-up rather than claims made by this milestone.

After that acceptance pass, the firing-range training-information dialog exposed another nonvisual gap: its weapon description and challenge fields are rendered in labels and a deliberately non-focusable `DESCRIPTION_FRWEAPON` scrollable panel, so focus narration previously moved directly to `OK` or `Resume`. The menu observer now supports explicitly opted-in `MENUACCESSIBILITYPART_SUMMARY` providers. The firing-range confirmation handler uses that path to provide the localized weapon name, difficulty, applicable goal/accuracy/target/time/ammunition values, and `frGetWeaponDescription()` text. The core speaks the summary once on dialog entry, omits it during normal focus movement, and reconstructs the complete title/summary/current-focus announcement for F5 repeat. An initial implementation unsafely queried every non-null item union as though it were a function pointer and crashed while loading a profile; `MENUITEMFLAG_ACCESSIBILITYSUMMARY` now gates every summary call, preventing dialog pointers from being invoked. The updated default MinGW64 build passes; profile loading plus audible runtime and blind-user verification of several weapons and both pre-game/in-game variants remain pending.

The project owner clarified that the desired CI terminal is the separate `Weapons Available` laptop, not the challenge-goals dialog. That screen renders manufacturer, weapon name, primary function, secondary function, and a marquee description beside a simple weapon list. Its list now opts into a rich `MENUACCESSIBILITYPART_OPTION` result, assembled from the same localized weapon definition and function strings used by those visual fields. Blind-user acceptance testing subsequently confirmed that the weapon menu reads properly. Broader regression coverage across multiple weapons, regions, and visual behavior remains useful follow-up.

The CI device-information terminal exposed the same nonvisual-content pattern in its details dialog: the selected device name is the title and the localized `dtGetDescription()` text is rendered in a non-focusable `DESCRIPTION_DEVICETRAINING` panel while initial focus lands on `OK`. The `OK`/`Resume` handler now opts into the existing dialog-summary contract and returns that localized description. On entry, the screen reader therefore announces the device title, its full information text, and the focused button; F5 reconstructs the same announcement. The default MinGW64 build passes, while audible runtime acceptance remains pending.

The firing-range weapon list visually renders three proficiency stars beside every weapon. `ciGetFiringRangeScore` is the exact saved two-bit value used by the renderer: zero fills none, one fills bronze, two fills bronze and silver, and three fills all three. The accessibility option provider appends localized `Bronze Completed`, `Silver Completed`, and `Gold Completed` phrases for precisely the filled stars. Verify representative weapons at scores zero through three and confirm the spoken completion set exactly matches the visible filled stars, including after completing a new range level and returning to the list.

The holo-training details dialog renders `htGetDescription()` in a non-focusable `DESCRIPTION_HOLOTRAINING` panel while focus starts on `OK` or `Resume`. That focused handler now supplies the identical localized value through the existing dialog-summary contract. For each unlocked holo exercise, open its details and confirm the exercise title, full static description, and focused control are announced once; move focus between the buttons and confirm the description is not repeated; press F5 and confirm the title, description, and current focus are reconstructed.

Source review of the PC default bind tables confirmed that End and SDL's right-stick controller button were both unbound; left-stick click was already the default cycle-crouch binding. The new `Reset View` extended action uses the previously unused `CK_1000` bit, persists under the new `RESET_VIEW` name so legacy `CK_1000=NONE` configuration entries do not suppress it, and defaults to End plus right-stick click only in the PC scheme. Runtime acceptance should look sharply up and down with both mouse and controller, press each binding independently, and confirm the camera becomes exactly horizontal without changing heading or continuing to drift. Repeat while standing still, moving, aiming, crouched, and near an interactable. Confirm pause/menu input does not reset the gameplay camera, verify the extended controls menu lists and can rebind `Reset View`, and verify reset-to-PC-defaults restores both bindings while reset-to-N64-defaults leaves the action unbound.

### Milestone 5 engineering evidence (2026-07-19)

The Carrington Institute interactable-beacon implementation compiles for the default `ntsc-final` x86-64 Windows target. Configuration used the Unix Makefiles generator and the documented MinGW64 toolchain; `cmake --build build -j4 -- -O` completed and produced `build/pd.x86_64.exe`. The build emitted existing project and vendored-Tolk warnings but no warning from `accessibility_beacon.c`.

Source review confirms that the implementation is inert unless both top-level accessibility and `Accessibility.InteractableBeacons=1` are effective. In unobscured gameplay, F5 independently toggles interactable-object beacons, F6 toggles door beacons, and F8 toggles pickup beacons; any combination can be active. F5/F6 retain their existing repeat/cancel meanings in menus, while F8 is ignored there. The scan is restricted to single-player `STAGE_CITRAINING`, active props, a 1,200-unit radius, and the current/directly-adjacent room boundary. Interactable objects require a CI tag or established terminal/interactable flag; pickups use the engine's object-type and collectable/uncollectable semantics; doors are canonicalized through sibling links. It does not call `propFindForInteract`, `objTestForInteract`, `doorTestForInteract`, `propobjInteract`, or `propdoorInteract`, so scanning cannot select, collect, or activate a target.

For F5, F6, F7, and F8, confirm every successful gameplay toggle produces exactly one centered two-beep confirmation: 880 Hz then 1320 Hz when the category becomes active, and 880 Hz then 440 Hz when it becomes inactive. Each beep lasts 35 ms with a 25 ms gap. Confirm the earcon reports the resulting state even when that category currently has no eligible world target, that menu-bound F5/F6 actions do not play it, and that rapid toggles do not affect F4 or the weapon-function lane. Correlate each pattern with `beacon/toggle_confirmation`.

Enable each category in turn, open and close a normal menu, pause and resume, enter and leave a computer/device-training dialog, and cross a cutscene or other temporary gameplay-scope boundary where practical. Every active beacon must stop immediately while suppressed, F5/F6 menu actions must retain their menu meanings, and no toggle confirmation may play merely because gameplay resumes. On return, the same category selections must automatically perform a fresh scan and resume against current targets without another key press. Correlate one `beacon/scope state=suspended` and one `state=resumed` record per transition; verify repeated suppressed frames do not repeatedly stop the oscillator or grow the log. Stage teardown, configuration disablement, and explicit F5–F8 toggles must still clear or change state as documented.

After blind-user testing showed that the original toggle-time snapshot remained attached to an older door or object while the player moved, enabled categories were changed to refresh every 30 logical ticks (twice per second at 60 Hz). Each enabled category retains up to three eligible targets. Previously scheduled identities remain members until they disappear or an unscheduled candidate is at least 150 units closer than the farthest retained member. Automatic scans log summaries and schedule changes without repeating every unchanged candidate record; toggle scans retain the full candidate audit.

The retained targets are interleaved by category on one global round-robin timeline. The slot gap is `max(18 ticks, 45 ticks / target count)`: one target pulses every 45 ticks, two alternate every 22 ticks, and three through six use an 18-tick (approximately 300 ms) gap. Before every pulse, the scheduler validates the next target, stops the prior accessibility cue, transfers its tracked property-sound slot to the new category, and only then starts the new positioned cue. This enforces at most one accessibility-owned beacon playback at any instant and prevents simultaneous door/object starts. Current CI policy caps each category at three targets; the cap and minimum gap are named constants so a future enemy category can intentionally use a larger, denser sensory profile.

Blind-user acceptance testing subsequently confirmed that automatic refresh and multi-target round-robin playback work well while moving through Carrington Institute. The accepted behavior includes hearing multiple nearby targets in succession without manual retoggling or simultaneous beacon starts.

The two categories use existing positioned one-shot sounds: `SFX_MENU_FOCUS` for interactable objects and `SFX_MENU_SUBFOCUS` for doors. Both are owned by `PSTYPE_ACCESSIBILITY_BEACON`; the runtime transfers one tracked channel between scheduled targets and categories. A stage-stop hook clears it before prop/audio teardown. Candidate, ordering, schedule membership/cursor, selection, door state, sound channel, volume/pan, pulse, invalidation, command, and lifecycle details are written through the existing accessibility logger when logging is enabled.

After a report of progressive game/video slowdown, an allocation audit found no beacon heap allocation: candidate storage is a fixed 64-entry array, logger scratch allocations are freed per event, and menu/speech allocations have paired replacement or shutdown frees. The available 71-second runtime log showed six door pulses, channel 8 reused for five pulses, channel 10 used transiently once, no allocation failure, an explicit toggle stop, and a shutdown reset. This is evidence of reclamation in that short run, not proof against a long-session issue. The pulse path was subsequently hardened to reclaim the category's exact tracked prop-sound slot whenever it remains free or accessibility-owned, and stale-target cleanup now stops that exact slot when the saved prop identity is unavailable. It never stops a tracked slot whose current type shows that gameplay has reused it.

While either beacon category is active, a rate-limited `beacon/telemetry` record is emitted immediately and every 30 seconds. On Windows it records working-set and private-byte totals/deltas through `GetProcessMemoryInfo`, plus current sound-state count, total/in-use/stopped prop-sound channels, accessibility-owned channel count, schedule size/cursor/next tick, and both tracked category ownership states. A long acceptance run should keep `beacon_channels` at no more than one, show the cursor advancing through every scheduled identity, and distinguish a steadily rising `private_delta` from normal working-set fluctuation. Telemetry resets its baseline when all beacons are cleared. Other platforms report memory as unavailable but retain the audio-channel counters.

Runtime and blind-user evidence now confirms audible laptop and office-door beacons, independent F5/F6 category control, and successful spatial use in the CI level. Empty-result, exhaustive state-transition, and extended long-session coverage remain incomplete, so Milestone 5 should not yet be described as fully accessibility-validated.

The Night Vision device exercise exposed a separate interactable-object eligibility mismatch. Runtime session `1784765247` recorded the visible CI light switch (`MODEL_LIGHTSWITCH`, prop 129, room 47) as `object_activation_disabled` solely because its setup flags include `OBJFLAG_DEACTIVATED`, even though it carries `OBJFLAG3_INTERACTABLE` and the engine subsequently accepted the player's activation during the exercise. `objTestForInteract` does not treat `OBJFLAG_DEACTIVATED` as an activation prohibition. The scanner now follows that behavior: only `OBJFLAG_CANNOT_ACTIVATE` disables an otherwise eligible interactable, while `OBJFLAG2_INVISIBLE` excludes genuinely hidden state. Runtime session `1784815549` confirmed that prop 129 entered the automatically refreshed schedule after the correction. It also confirmed that the visibly present switch remains in the scanner outside its exercise even though activating it then has no effect; the project owner explicitly accepted that conservative false positive in preference to suppressing the switch when it becomes useful. During Night Vision, confirm the switch enters or remains in the schedule without retoggling F5. Correlate `object_invisible`, `interaction_flag`, `schedule_target`, and `pulse` records with the transition. Also sample a setup-defined deactivated terminal in another stage before claiming full-game coverage.

The subsequent procedural-cue revision replaces the two menu-derived samples and their property-sound channel with an independent fixed-size chirp voice in the existing accessibility mixer. Door pulses are 440 Hz and interactable-object pulses are 880 Hz. Each request is 100 ms with a 5 ms attack and 20 ms release; it carries the established distance attenuation and stereo pan into the mixer and cannot interrupt the continuous fine-aiming tone. Source review confirms that the chirp bridge uses atomics, the audio path retains one fixed mix buffer, and the beacon path no longer allocates or consumes a property-sound channel. The default `ntsc-final` x86-64 MinGW64 build completed successfully on 2026-07-20 and produced `build/pd.x86_64.exe`; only pre-existing warnings were observed. Audible category distinction, direction/distance behavior, aiming-tone overlap, and long-session acceptance for this revision remain to be recorded below.

Acceptance feedback confirmed the two procedural chirps work well but exposed that door candidates still followed the prototype's room-adjacency policy without a line-of-sight gate. Door scans and pre-pulse validation now both require `cdTestLos06` against `CDTYPE_BG`. Test a nearby visible closed door, then place opaque level geometry between the player and that door and wait through at least two refresh intervals; its chirp must stop. Verify that the target door does not block itself and that uncovering it causes automatic reacquisition without toggling F6.

The same background-only LOS policy now applies unconditionally to interactable objects rather than only to objects carrying `OBJFLAG2_INTERACTCHECKLOS`. Test a visible CI laptop or terminal, move behind opaque level geometry, and wait through at least two refresh intervals; its 880 Hz chirp must stop and automatically return when LOS is restored without toggling F5.

Start CI device training for the Data Uplink with F8 enabled and F5/F6 disabled. Before the exercise exposes the device, there must be no pickup pattern for its disabled, invisible, uncollectable setup object. Once the Uplink appears on the table, confirm its position is represented by exactly three quick 880 Hz chirps: 35 ms per chirp with 25 ms gaps. Confirm the pattern follows distance and stereo pan, stops behind door/background sight blockers, and disappears automatically within the next half-second refresh after the player collects it. Repeat with representative weapons, ammunition, keys, shields, scripted collectible objects, manually activated pickups, and walk-over pickups where available. Uncollectable/invisible objects and projectile pickups still reserved for another character or not yet settled must remain silent. Toggle F5, F6, and F8 in several combinations and verify each independently controls only its category; all targets still share one staggered timeline without simultaneous starts or a new native game-audio channel.

The project owner's first enabled runtime pass confirmed that the laptop beacon sounded, proving the command, scan, pulse scheduling, and positioned object-audio path, but no office-door beacon was initially available. The comprehensive log identified the nearby unlocked, healthy door between rooms 14 and 16 as prop 9 and showed it was rejected solely because its setup flags included `OBJFLAG_DEACTIVATED`. The game's `doorTestForInteract` does not reject doors on that flag; it checks `OBJFLAG_CANNOT_ACTIVATE` and `maxfrac`. The beacon predicate was corrected to keep `OBJFLAG_DEACTIVATED` object-only, and a subsequent blind-user retest confirmed the office-door beacon.

#### Non-hostile-character beacons

Confirm the session-start record reports `non_hostile_beacons=1`. In Carrington Institute gameplay, leave F5/F6/F8 off, press F7, and approach known staff. Every eligible friendly or neutral person should emit exactly two rapid positioned 440 Hz chirps; nearby doors must remain silent. Toggle F7 off and confirm the character cues stop without affecting hostile targeting or other beacon categories. Then enable doors and people together and confirm one-chirp doors and two-chirp people remain countable and never start simultaneously.

Test a nearby hostile and confirm it receives no F7 pattern. Where controllable, test a character before and after an allegiance change, death, knockout, hidden/untargetable state, and cloak with and without IR perception. Put doors, objects, or background geometry between the camera and the person's body midpoint and confirm the cue stops, then automatically reacquires after sight is restored. Move among more than three non-hostile characters and confirm stable nearest-target retention with the existing 150-unit replacement margin. Repeat outside CI to verify that F7 remains available while F5/F6/F8 stay CI-scoped. Menus, pause, cutscenes, player death, multiplayer, feature disable, stage teardown, and shutdown must clear the pattern.

Correlate `beacon/candidate` reasons (`friendly_character`, `neutral_character`, `character_hostile`, life/visibility exclusions), `scan_result`, `schedule_target`, `pulse_count=2`, and `category_state non_hostile_active` with perceived output. Run with all four categories and inspect periodic telemetry for a bounded schedule of at most 12 retained targets, one shared chirp lane, stable fixed storage, and no progressive frame, channel, or memory growth.

#### CamSpy beacon perspective

Enable F6 doors and F7 people before deploying a CamSpy. Identify a door and non-hostile person near Joanna but outside the CamSpy's range or rooms, then switch to the CamSpy and confirm those old-body cues stop without retoggling. Move and turn the CamSpy toward different eligible doors and people; range, stereo direction, room connectivity, and line of sight must follow the remote camera. A person cue in remote view must correspond to a character actually rendered in the current viewport; turn that person offscreen and confirm the cue leaves the next refresh. The CamSpy itself must never produce a two-chirp people cue. Switch back to Joanna and confirm her nearby set returns automatically with F6/F7 still active. Destroy, collect, or deactivate the CamSpy and repeat rapid perspective changes; no stale chirp or invalid-prop access may occur.

With F5/F8 also active in Carrington Institute, confirm interactable and pickup cues pause during CamSpy viewing and resume from Joanna after returning. Correlate `beacon/observer_change`, `scan_start observer_remote`, candidate reasons, rebuilt schedules, and pulses with each visible transition. Run several transitions with advanced diagnostics and verify fixed result/schedule storage, one shared chirp lane, and no new memory, channel, or frame-time growth.

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

Exercise one real example each of selectable action, checkbox, slider, closed/open dropdown, standard list, custom-rendered list, keyboard, focusable scrollable content, carousel, ranking, and player stats. For each, record label source, role, value, changed-value/subfocus timing, position/count, boundaries, disabled state, cancel/back behavior, repeat output, and semantic-provider failures. Confirm slider minimum, midpoint, and maximum are announced as percentages rather than raw engine units. Include mouse focus parity and controller/keyboard parity even when the blind task uses one input method.

The implementation must also run a source audit that compares every current focusable `MENUITEMTYPE_*` and every focusable `MENUITEMFLAG_LIST_CUSTOMRENDER` handler with the semantic resolver/provider table. Unknown numbered or future types must fail the development audit and log safely at runtime; they must never receive guessed speech.

### Script C: New Agent to settings blind task

Use a temporary lawful save/config directory and start with no selected profile. Confirm the startup `Perfect Dark` custom list announces its actual rows, including `New Agent...`; create a uniquely named agent through the on-screen/physical keyboard; complete any save-location dialog without overwriting user data; reach `Perfect Menu`; swipe to Options; enter a chosen standard or Extended settings dialog; change one setting; verify its new value; and return to a known context. The observer may explain scope and emergency stop beforehand but must not coach individual moves.

Record completion, wrong turns, missing/excessive/late output, rapid-navigation replacement, repeated/cancelled announcements, recovery, time, and tester confidence. Main-menu definitions vary with profile, mission, multiplayer, unlock, memory, platform, and region state, so the report must capture its actual start conditions rather than assume one universal sequence.

## Gameplay feature scripts

### HUD/objectives

With `Accessibility.HudMessages=1`, start Carrington Institute holo-training session 1 and confirm its instructional HUD messages speak automatically in displayed order without interaction. Trigger another accepted ordinary HUD message elsewhere to verify the common hook is not training-specific. Confirm an in-game subtitle/dialogue and a cutscene subtitle are not spoken by this HUD feature, while `hud/message_accepted` and `hud/speech_suppressed reason=subtitle` retain their metadata. Trigger a suppressed duplicate and confirm there is no second admission or speech request. Check multiline/control whitespace is spoken as one normalized sentence, and confirm menu focus speech can interrupt queued HUD speech without preventing a later HUD message. Repeat with `HudMessages=0` and confirm accepted messages are logged with `reason=feature_disabled` but not spoken.

For the remaining Milestone 7 work, trigger objective completion, objective failure if safely reproducible, briefing review, and pause objective review. Confirm correct player context, ordering, no objective double-speech, and usable recovery after an interruption.

### Status

Query at full and partial health/shield; change weapons/functions; test loaded and reserve ammunition, reload, empty ammo, dual wield, pickups, death/restart, pause, and a scripted/training health change. Compare announcements to semantic APIs or controlled in-game state, not solely a visual bar.

### Targeting and scanner

#### Environmental damaging lasers

With `Accessibility.EnvironmentalHazards=1`, start Carrington Institute holo-training 3 and leave F5/F6/F8 beacons off for the first pass. Approach each horizontal laser while looking toward it. At no more than 500 world units and within the 25-degree facing cone, confirm one 220 Hz tone fades in and audibly sweeps from one physical endpoint to the other and back over 90 ticks. Turn just outside the cone, turn fully away, retreat beyond range, and place opaque background geometry between the camera and beam; each condition must fade the hazard lane out. Restore eligibility and confirm automatic reacquisition without a key press.

Move through adjacent laser bars and confirm the nearest eligible bar replaces the prior one without rapid oscillation; the 75-unit margin should retain the current identity until another is materially nearer. Verify fully faded/open, disabled, and non-colliding lasers remain silent. Exercise standing, ducking, and crouching, then pause, open a menu, abort/complete the exercise, leave the stage, disable the setting, and shut down. In each case confirm the lane stops. Repeat with beacons and the firing-range fine-aim tone active where practical to confirm the three procedural voices do not interrupt one another. Run several sessions and check for stuck sound, frame degradation, or growing memory.

Correlate `hazard/scan`, `selection`, `selection_lost`, `sweep`, `scan_guard`, and `reset` records with the observed beam. Validate endpoints, closest/source distances, facing dot, sweep phase, source position, attenuation, and pan. `scan` aggregates all rejection categories and `sweep` captures the selected source, each rate-limited to once per second; unchanged ineligible props do not create per-frame log records.

#### Hostile-character targeting

With `Accessibility.TargetingFeedback=1`, start Holo Training 4 and verify that each ordinary hostile receives a positioned 440 Hz oscillator cue when active, combat-capable, rendered in the viewport, and unobstructed. The current unarmed function defines `X=60` world units. At a near-body-surface cue distance of at least 300 (`5X`), expect an approximately 180 ms chirp every 750 ms. Approach slowly: period and length should fall smoothly until reaching approximately 50 ms every 200 ms near cue distance 60, with each accumulated 40 ms period reduction advancing the next pulse rather than waiting out the previous cadence. At or inside 60, the cue must switch to a constant spatial tone. Once entered it should remain constant through small collision/animation movement and switch back to chirps only beyond 66 (`1.1X`); approaching from outside must not enter constant mode early in the 60–66 band. Cross both boundaries repeatedly without attacking and confirm the continuous transition ramps without a click. Correlate perception with `targeting/combat_slot_cadence` records: `continuous` must first change at `X`, `punch_range_exit` must report `1.1X`, and `trigger_now` should mark cadence advances and the transition out of constant mode. Confirm that the constant tone usefully indicates punch range while recognizing that off-target aim, geometry, and animation timing can still prevent a hit.

Multiple enemies should retain independent spatial voices whose starts are staggered rather than exactly simultaneous. Point directly at each hostile while still unarmed and confirm the centered alignment tone starts at a fixed 660 Hz and stops immediately when aim leaves the character. Verify moving characters reacquire automatically. Knock out all three training opponents and confirm each voice stops as soon as the knockout fall begins and remains absent while the body lies on the floor. Test partial cover, full occlusion, turning away, pause/menu state, training completion and abort, and several sessions without restarting.

In a later mission with friendly or neutral characters, confirm only characters classified as hostile receive positive targeting feedback. Where practical, also test hidden/untargetable state, a cloaked hostile with and without IR perception, an aimed hostile visible only around cover, and a group large enough to exercise the fixed candidate capacity. The accessibility layer must not identify or disclose characters that the existing semantic visibility and relationship rules exclude.

Correlate `targeting/combat_candidate`, `scope_gate`, `observation`, `combat_slot_assign`, `combat_slot_cadence`, `combat_slot_release`, `combat_presence_stop`, `aim_acquisition`, `aim_loss`, `alignment_start`, `alignment_update`, `alignment_stop`, and `telemetry` records with the perceived character set. Confirm cadence logs contain center distance, near-surface cue distance, punch range, `1.1X` exit and `5X` far thresholds, zone, normalized proximity, period, duration, continuous mode, immediate-trigger state, volume, and pan. Confirm combat reports `profile=2`/`source=2`, profile changes reset old identities and owned sounds, eliminated characters report `dead_dying_or_knocked_out`, no more than ten oscillator slots are assigned, phase staggering avoids exact starts, and the fixed-capacity path shows no game sound-channel or memory growth. Stress one through ten simultaneous enemies, then more than ten to verify deterministic bounded selection.

#### Device-training target alignment

With `Accessibility.TargetingFeedback=1`, start the Data Uplink exercise. Before equipping the Uplink, point at the designated terminal and confirm silence. Equip it, point directly at the designated terminal, and confirm the same fixed 660 Hz centered alignment tone used for a generic valid combat aim. Point at nearby computers, doors, the terminal edge/background, and other interactables; each must remain silent. Acquire the tone from beyond interaction range and verify that ordinary use still requires the game's stated range, so the cue is understood as target identity rather than action readiness. Complete, fail/abort, pause, open a menu, unequip the device, and leave CI; each transition must stop the tone.

Repeat with the ECM Mine exercise and hub. Only tagged hub `0x32` should acquire the tone. After acquiring it, make deliberate throws that land correctly, miss, or strike intervening geometry. Confirm the tone means “correct destination surface” and never announces or implies a guaranteed landing. Verify there is no positioned presence beacon from the targeting subsystem in either device profile and that independent F5/F6/F8 beacons retain their existing behavior. Logs should report `profile=3`/`source=3`, the expected weapon and target tag, raw aim identity, eligibility reason, acquisition/loss, and clean profile reset without new allocations or sound channels.

Repeat with the Door Decoder exercise and door panel. Before equipping the Decoder, point at panel tag `0x35` and confirm silence. Equip it and confirm the fixed 660 Hz tone sounds only while the raw aim ray is on that panel. Nearby CI hubs, monitors, doors, and other interactables must remain silent. Activate the panel and verify the normal decoder attachment and unlocking sequence remains authoritative; the tone identifies the accepted panel but does not claim that activation range or other use conditions are satisfied. Abort, complete, unequip, pause, open a menu, and leave CI, confirming clean tone loss and `profile=3`/`source=3` logs with `weapon=57` and `target_tag=53`.

Repeat in the CI CamSpy exercise with `Accessibility.TargetingFeedback=1`. Before the CamSpy startup completes, and while looking through Joanna's view, the photo-target tone must be silent. Enter the remote view and frame the exercise's required holograph object: the fixed 660 Hz tone should start exactly when the whole healthy target is inside the viewport and it is less than 400 horizontal units away. Move the center of view onto nearby scenery while keeping the complete target in frame and confirm the tone remains, matching the game's frame-based photograph rule. Clip any target edge outside the viewport, back beyond range, place the target behind the camera, and destroy it where safely testable; each must stop or prevent the tone. Take a successful picture and confirm completed criteria no longer remain targeted. Exit and re-enter the CamSpy, abort/complete training, pause, open a menu, destroy or collect the CamSpy, and leave the stage; no stale alignment tone may remain.

Run the same checks on a campaign objective that uses `OBJECTIVETYPE_HOLOGRAPH`. Confirm only the objective-tagged object is identified and unrelated people, objects, or scenery remain silent. Correlate `targeting/camspy_candidate` fields (`criterion`, `status`, `tag`, health, rendered state, depth, distance, projected bounds, reason, and raw aim identity) with `aim_acquisition`, `alignment_start`, `aim_loss`, and the visible photograph result. Inspect repeated deployments for the fixed 32-candidate bound, no runtime allocation or new game sound channel, and no frame-time or memory growth.

Acceptance session `1784764601` exposed the initial mismatch. The target for criterion tag `14` was repeatedly `accepted=1 reason=eligible` at 385–394 units with complete bounds inside the 320x220 viewport, and the game then spoke `Info Room PC successfully holographed`; however, `raw_aim_prop` remained null and no alignment began. The corrected policy therefore uses photograph eligibility itself rather than a weapon ray. The same session's unexpected people patterns came from character props `97` and `102`, around 370–455 units from the CamSpy; historical detailed records showed neither carrying `PROPFLAG_ONTHISSCREENTHISTICK`. The remote-only render gate targets that false-positive class without narrowing Joanna's ordinary spatial scanner. Both corrections require a fresh blind-user pass.

#### Weapon-function state cues

With `Accessibility.WeaponFunctionCues=1`, equip a weapon with two persistent functions and press R1/right bumper. Switching to the secondary function must produce exactly two centered 1000 Hz beeps; returning to primary must produce exactly one. Each beep should be brief (35 ms), the two-beep gap should be clearly countable at 30 ms, and output should begin only after the same semantic value that drives the visual indicator changes. Repeat using the active-menu function selector and any configured keyboard binding: input path must not affect the pattern.

Try a weapon with no alternate function, temporary alternate-function weapons, an unavailable function, rapid repeated presses, weapon changes, dual wielding, firing/reloading, pause/menu transitions, death, and stage changes. Equipping a weapon or entering a stage must not announce its stored function as a new toggle. Confirm beacon chirps, hazard sweeps, aiming tones, and combat slots can overlap without interrupting the function pattern. Correlate every audible pattern with `weapon_function/state_change`; advanced `performance/frame_window` records should advance `weapon_function_sequence` without memory or channel growth. Record multiplayer behavior as unvalidated rather than accepted until independently tested.

Use a controlled room with known eligible and ineligible entities. Test friendly/hostile/neutral where applicable, occlusion/cloak rules, target loss, rapid crossings, empty scan, overlapping results, collected/opened/destroyed objects, and multiple local-player context. Explicitly audit for hidden-information leaks.

For the current Carrington Institute firing-range proof, first verify that the pre-session `OK` and `Cancel` controls announce their visible captions rather than only `button`. Start an exercise with F5/F6/F8 beacons off. Confirm one positioned `SFX_MENU_SELECT` pulse per visible target in round-robin order. Move the reticle onto a target and confirm a centered continuous sine tone starts; move from the outer scoring ring toward the center and confirm pitch rises smoothly from approximately 660 toward 1320 Hz, then falls smoothly when moving away from center. Fire at several deliberately different pitches and compare the logged `aim_distance`/`quality` and reported scoring rings: distance below 18 should be bullseye, below 37 ring 1, below 56 ring 2, and larger valid distances ring 3. The accessibility value must follow the non-random query point and must not fluctuate with weapon spread. Move directly between targets and confirm the tone follows the new identity without a stale pitch. Move off target and confirm it stops. Hold over a target while it rotates: its positioned presence pulse must continue, the tone must stop as soon as the range reports `facing_away`, and it must resume when the same target faces the player again. Confirm `alignment_start`, `alignment_update`, `alignment_stop`, `observation`, `aim_loss`, and `aim_acquisition` logs record matching identity, shootability, distance, quality, and frequency without per-frame log flooding. There is no F9 binding. Destroy targets and confirm replacements enter automatically. Repeat with one and several simultaneous targets, then with F5/F6/F8 enabled to assess masking. Exercise pause, menu, exercise completion/failure, death where practical, stage exit, feature disable, and clean shutdown. On both completed and failed post-session dialogs, confirm the entry announcement and F5 repeat include completion/failure reason, score, targets destroyed, difficulty, time, weapon, accuracy, all four scoring-zone hit/point totals, hit total, and the focused Continue/OK control. Repeat several exercises without restarting the game and inspect `targeting/telemetry` for no more than one targeting property-sound channel, bounded alignment updates, no allocations from the tone path, stable sound-state counts, and non-linear memory growth. Confirm targeting events contain no non-finite hit positions, `screen=nan`, infinite projected bounds, or sustained `projection_capture_unavailable` results; a defensive rejection should be investigated if it recurs. Record that multiplayer remains outside this proof and test ordinary hostile characters separately under the combat procedure above.

### Navigation

#### Virtual cane prototype

Confirm the effective session-start record contains `virtual_cane_mode=1`. During unobscured single-player walking gameplay, press F4 repeatedly and verify the order is Slow to Fast to Off to Slow. There must be no speech or separate confirmation earcon. Left Alt+F4 and Right Alt+F4 must not change cane state; if the operating system leaves the game running, releasing Alt while F4 remains held must not produce a delayed mode change. Menus, pause, cutscenes, death, non-walking movement, and multiplayer must suppress the command and stop all cane audio.

Use controlled geometry for the first pass. Face a flat wall, an angled wall, an inside and outside corner, a doorway, a narrow opening, a pillar/crate, a closed/partly open/open door, a small traversable step, a low obstruction while standing and crouching, a pickup/non-solid decoration, and open space. Verify the audible sequence always travels left to right through -45, -30, -15, 0, 15, 30, and 45 degrees; a miss is silent; characters are not cane targets; door/object collision follows whether the player can currently move through it; and open space produces a silent cycle rather than a confirmation cue. Correlate each observation with the aggregate `cane/sweep` sample fields, especially raw collision point, normal, obstacle/type, bbox, distance, pan, and result. Validate that the returned collision point sounds like the barrier surface rather than the stopped center of the player cylinder.

Measure Slow as 120 logical ticks including a 30-tick end pause and Fast as 60 logical ticks including a 15-tick end pause in the default NTSC-final build. Walk and turn continuously during both modes: each scheduled sample must use its own live origin and camera direction, and movement must not restart the sequence. Ordinary PC render/interpolation frames with `lvupdate60=0` must preserve the in-progress sweep rather than stopping and restarting at the leftmost sample; verify this at a steady 60 fps and while IR/X-Ray scanner audio or targeting feedback is active. Induce a hitch where practical and confirm no more than one query occurs in a logical tick, overdue samples increment `skipped`, and no catch-up burst is heard. Repeat across room and stage transitions and confirm all seven requested/active mixer bits clear on stop.

Deploy a CamSpy while a cane sweep is active. On the same perspective transition, the incomplete Joanna sweep must stop and a fresh sweep must begin from the CamSpy position, rooms, look direction, and collision cylinder; it must not wait for movement or an F4 toggle. Fly toward walls, angled surfaces, doors, and openings that are remote from Joanna and compare the cue sequence with the CamSpy image. Return to Joanna and confirm an immediate fresh body-origin sweep with the selected Slow/Fast mode preserved. Correlate `cane/observer_change` and each sample's `observer`/`remote` fields; test rapid switches and CamSpy destruction for stale mixer bits, collision errors, or a frame-time regression.

Exercise simultaneous door/interactable beacons, hostile combat slots, fine aim, laser hazards, weapon-function cues, speech, music, and ordinary effects. The 330 Hz, 35 ms cane chirps must not steal, stop, or retune another lane. Run at least 15 minutes of repeated movement, combat, menus, and holo-training sessions. With `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON`, compare Off and Fast using the same route. Record query count, total/maximum microseconds, skipped/missed cycles, frame rate/gaps, memory deltas, cane command/start/stop counters, and requested/active masks. Investigate any individual query over 2 ms, sweep average over 0.5 ms, sustained frame regression, new recurring maximum gap, memory growth, stuck mask, or audio/speech choppiness before acceptance. These timing thresholds are investigation triggers, not proof of a universal performance budget.

After engineering checks, have a blind tester distinguish a flat wall, angled wall, opening, and intermittent obstacle pattern, then keep Fast enabled while moving in a combat-like situation. Record whether geometry is understandable, responsive, masked, overwhelming, or misleading. This slice is not accessibility-accepted until that task evidence exists, and it does not complete Milestone 11 route guidance.

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

Correlate perceived output with `rtracker/announcement`, `scope`, `slot_assign`, `candidate`, `slot_release`, `overflow`, and `scan_summary` events. With `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON`, also inspect `tracker_enabled_slots`, R-Tracker scan timing, frame gaps, memory deltas, and mixer activity over repeated sessions. Investigate scans above the specification's thresholds or any sustained growth/choppiness. The complete semantic and acoustic contract is `ACCESSIBILITY_RTRACKER_AUDIO_SPEC.md`.

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

## Coverage language

Use precise statements such as “A blind tester independently opened the Solo Missions destination from the documented main-menu start state on this build.” Avoid “menus are accessible,” “screen-reader support is complete,” or “the game is playable” until the tested coverage truly supports those claims.
