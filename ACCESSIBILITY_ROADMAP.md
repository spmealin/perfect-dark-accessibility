# Accessibility roadmap

This roadmap orders work by user task and evidence. Milestones are not promises of dates or full-game coverage. Each must leave the game buildable with accessibility disabled and must record blind-user evidence before being called accessibility-complete.

## Milestone 1 — Baseline and architecture

- **Goal:** Establish repository facts, scope, boundaries, test vocabulary, and a reviewable hook ledger.
- **User-visible result:** None; documentation accurately says no accessibility feature exists yet.
- **Systems:** Build configuration, repository layout, menu/HUD/objective/player/target traces, testing process.
- **Acceptance:** Five root documents agree on terminology and milestone order; cited paths and symbols exist; baseline Windows build succeeds; no binaries/ROM content are added.
- **Logging/evidence:** Record commit, `ROMID`, commands, executable, build result, and clean/expected Git status.
- **Risks and unknowns:** Documentation can become stale and repository call paths may change upstream.
- **Explicit non-goals:** Do not implement hooks or claim that documentation validates a design through use.

## Milestone 2 — Minimal initialization and playtest logging proof

- **Status:** Implementation and Windows runtime verification complete. Disabled and enabled normal-exit runs passed in the real executable; lifecycle edge cases passed in a focused harness.
- **Detailed plan:** `milestones/ACCESSIBILITY_MILESTONE_02_PLAN.md` is the authoritative implementation handoff.
- **Goal:** Add a disabled-by-default accessibility service with configuration, safe lifecycle, and opt-in structured session logging.
- **Later default change:** These controls now default on during blind-user acceptance testing and remain configurable.
- **User-visible result:** With both settings explicitly enabled, startup and shutdown produce a comprehensive local diagnostic log; normal play is otherwise unchanged.
- **Systems:** `port/src/main.c`, config/filesystem/system services, new core/log modules, and CMake. No frame-tick hook is needed.
- **Acceptance:** Disabled mode creates no accessibility log; enabled logging records schema/build/session/start/stop; JSON lines parse; open/write failure is nonfatal; repeated init/shutdown is safe; the Windows baseline builds, with a non-Windows compile checked when an environment is available.
- **Logging/evidence:** The feature is its own evidence source. Log all feature-relevant diagnostics without redaction, overwrite the prior session at launch, and keep the runtime file ignored by Git.
- **Risks and unknowns:** Constructor order, shutdown order, save-path behaviour, synchronous write cost, long-session file size, and write failure need proof.
- **Explicit non-goals:** No speech, menu narration, or gameplay assistance yet.

## Milestone 3 — Windows speech backend proof

- **Status:** Implementation and machine-verifiable Windows/NVDA runtime proof complete. Tolk loaded dynamically, NVDA 2026.1 accepted fixed and multilingual output, cancellation succeeded, and dependency failures remained nonfatal. Audible and braille perception were not independently observable by the implementation agent.
- **Detailed plan:** `milestones/ACCESSIBILITY_MILESTONE_03_PLAN.md` is the executed implementation record. It selects pinned Tolk commit `e5149f0cb6ef9b941673017e0e7b7c409e485fbe` with NVDA as the validated path.
- **Goal:** Select and validate one replaceable Windows speech path behind the backend interface.
- **User-visible result:** An explicit test action speaks a fixed, non-game string and can be interrupted; absent/unavailable speech does not block startup.
- **Systems:** Core backend boundary, dynamically loaded Tolk Windows backend, null backend, UTF-8 conversion, configuration, and development-output packaging.
- **Acceptance:** Availability, call durations, cancellation, strict Unicode conversion, startup/shutdown, alternate-working-directory loading, missing DLL/controller/exports, and disabled states are recorded. Human-observed first-audio latency, braille output, other readers, i686 runtime, and non-Windows compilation remain future evidence rather than claimed passes.
- **Logging/evidence:** Record dependency paths, export resolution, active reader/capabilities, complete request/conversion/backend results and errors, native identifiers/handles when useful, and timing.
- **Risks and unknowns:** Screen-reader coexistence, native dependencies, and COM/thread constraints may change the technology choice.
- **Explicit non-goals:** Do not wire gameplay systems directly to the backend or select a backend without measured evidence.

## Milestone 4 — Menu-agnostic focus narration

- **Status:** Complete. The MinGW64 build passed, accessibility/logging/speech/menu narration default on for testing, and the project owner completed blind-user acceptance testing and accepted the spoken-menu behavior, including context-only menu titles and percentage-based sliders.
- **Detailed plan:** `milestones/ACCESSIBILITY_MILESTONE_04_PLAN.md` is the authoritative handoff.
- **Goal:** Let a blind user understand and operate the startup/New Agent/settings path while establishing shared semantics for every focusable control family used by the menu engine.
- **User-visible result:** Final dialog context and focused control semantics are spoken; values and internal list/grid focus update predictably; repeat and cancel work; rapid navigation replaces stale output.
- **Systems:** One post-`menuProcessInput` observer, runtime item-data query, type-based semantic adapter, generic custom-render semantic operation, replaceable menu announcement dispatcher, and provisional PC repeat/cancel commands.
- **Acceptance:** The startup list announces `New Agent...` without recognizing that screen; the keyboard and settings route work; a source/runtime audit covers selectable, checkbox, slider, dropdown, standard/custom list, keyboard, scrollable, carousel, ranking, and player-stats controls; keyboard/controller/mouse focus produces equivalent semantics; backend/log failures are nonfatal; a blind tester reaches and changes a chosen setting unaided.
- **Logging/evidence:** Record all feature-relevant raw and normalized dialog/control text, IDs, pointers, types, flags, handler operations, values, indexes/counts, input, diff/replacement decisions, output/cancel results, failures, and timings. Aggregate unchanged-frame evidence only when needed for performance.
- **Risks and unknowns:** Dynamic callback lifetimes, custom-rendered rows, compound controls, localization/control codes, multiple local players, and provisional input collisions need measured evidence.
- **Explicit non-goals:** Structural support for all current focusable control families is not a claim that every menu or briefing has been blind-user validated. Rich briefing/objective reading and permanent configurable actions remain later milestones.

## Milestone 5 — Carrington Institute interactable beacons

- **Status:** The original existing-sample implementation, automatic refresh, and multi-target scheduling passed blind-user acceptance. The procedural-chirp cue revision builds successfully and is pending runtime acceptance.
- **Detailed plan:** `milestones/ACCESSIBILITY_MILESTONE_05_PLAN.md` is the authoritative implementation handoff.
- **Goal:** Let a blind player deliberately select and spatially locate two useful classes of nearby Carrington Institute props: interactable objects and doors.
- **User-visible result:** F5 independently toggles positioned 880 Hz chirps on eligible interactable objects and F6 does the same with 440 Hz chirps on eligible doors, allowing neither, either, or both categories to run. When both are active their pulses are staggered. No new audio asset is required.
- **Systems:** Accessibility gameplay coordinator, read-only prop eligibility, active-prop traversal, door sibling canonicalization, player-relative ordering, procedural stereo audio, provisional PC actions, configuration, and structured logging.
- **Acceptance:** In Carrington Institute training, results are stable, range-limited, and limited to the two named categories; cues remain attached to their props; open/closed/locked/destroyed/deactivated state updates safely; menu/pause/cutscene/death transitions silence the beacon; a blind tester can distinguish the categories and independently locate a designated laptop or terminal and a designated door.
- **Logging/evidence:** Record every scan and per-category toggle, player pose/rooms, every considered prop and raw identity, inclusion/exclusion reason, canonical door group, exact distance/bearing/order, each category's selected target, chirp frequency/volume/pan/pulse lifecycle, invalidation, and timing.
- **Risks and unknowns:** Interaction checks currently mix actionability with on-screen/facing constraints; closed doors require different knowledge checks than ordinary objects; prop lifetimes and linked doors need safe handling; stereo panning may require turning to resolve front from rear; chirp duration, level, and category contrast need runtime validation.
- **Explicit non-goals:** No external audio, HRTF, global radar, automatic movement/interaction, other stages, other prop categories, permanent bindings/options UI, or claim of a generalized scanner.

## Milestone 6 — Discoverable accessibility settings

- **Goal:** Expose stable accessibility configuration in the existing PC options UI and define collision-free input actions.
- **User-visible result:** Users can enable/disable speech, categories, verbosity, logging, repeat, cancel, and the proven beacon actions without hand-editing `pd.ini`.
- **Systems:** `port/src/optionsmenu.c`, config registry, input binding model, menu narration.
- **Acceptance:** Settings are themselves narrated, persist across restart, have safe defaults, and remain operable when a backend is unavailable; reset/default behaviour is documented; input conflicts are detected or avoided.
- **Logging/evidence:** Record setting category, old/new values, originating input/action, configuration path, and any raw detail useful for diagnosing persistence or conflicts.
- **Risks and unknowns:** The present binding model targets game controls and may need a separate accessibility action set; settings may be needed before a profile loads.
- **Explicit non-goals:** Do not add bindings for unimplemented scanner categories or navigation behaviours.

## Milestone 7 — HUD, subtitles, briefings, and objectives

- **Goal:** Make mission text and objective changes available without duplicate or stale speech.
- **User-visible result:** Accepted HUD messages and eligible subtitles are announced; briefings/current objectives can be read deliberately; objective state changes take priority.
- **Systems:** `hudmsgCreateFromArgs`, `hudmsgCreateAsSubtitle`, `objectivesCheckAll`, `setupLoadBriefing`, scrollable/objective menu adapters.
- **Acceptance:** Scripts cover pickups/system messages, dialogue with subtitles visually on/off, split subtitles, duplicate suppression, completed/incomplete/failed objectives, briefing review, pause objective review, and cutscene transition. Objective text is not spoken twice through HUD and objective paths.
- **Logging/evidence:** Source category, full resolved text, objective IDs/state, HUD type/flags, audio channel details, player context, and dedupe/replacement decisions.
- **Risks and unknowns:** Voice audio, subtitle timing, splitting, and control codes can differ by region/version.
- **Explicit non-goals:** Do not promise audio description of uncaptioned cinematic action or all mission text coverage.

Implementation status: the generic HUD-admission slice is implemented behind `Accessibility.HudMessages=1`. It queues every successfully admitted non-subtitle HUD message for non-interrupting speech and excludes the explicit in-game/cutscene subtitle types. Subtitle narration, briefing/objective queries, priority objective events, full queue expiry/replacement policy, and Milestone 7 acceptance remain pending.

## Milestone 8 — Player status and inventory queries

- **Goal:** Provide calm, accurate access to health, shield, equipped weapon/function, ammo, and inventory.
- **User-visible result:** On-demand status is concise; important health/ammo thresholds are optional and non-repetitive; weapon changes are announced.
- **Systems:** player snapshot, `playerGetHealthFrac`, `playerGetShieldFrac`, bondgun name/ammo APIs, inventory APIs, action bindings.
- **Acceptance:** Test full/partial/zero health and shield, single/dual weapons, primary/secondary functions, reload/reserve states, no weapon, pickups, death/restart, pause, scripted health changes, and rapid weapon cycling. Values match game state within the defined sampling tick.
- **Logging/evidence:** Raw and formatted health/shield/ammo values, weapon/inventory IDs and names, change context, player/profile context, and queue decisions.
- **Risks and unknowns:** Health changes arise outside gun damage, dual-wield ammo semantics are complex, and polling and event hooks must not conflict.
- **Explicit non-goals:** No automatic tactical advice, aim changes, or inventory selection on the user's behalf.

## Milestone 9 — Targeting feedback

- **Status:** Firing-range slice implemented; fine-aim tone runtime and blind-user acceptance pending. The branch contains a generic fixed-capacity targeting core, a Carrington Institute firing-range source adapter, serialized positioned visible-target pulses, and a centered continuous pitch lane gated by current shootability. The tone reuses the existing non-random aim-query collision point and maps target-center proximity to 660–1320 Hz; it does not include weapon spread or cast another ray. Back-facing range targets retain presence feedback but do not produce positive alignment feedback. This does not complete broader character/combat, relationship, high-value-zone policy, speech/repeat, special-sight, or multiplayer coverage.
- **Detailed plan and execution record:** `ACCESSIBILITY_MILESTONE_09_PLAN.md` remains active until the firing-range proof is runtime-tested and accepted.
- **Goal:** Tell a player when a valid aimed target changes and provide truthful relationship/distance cues.
- **User-visible result:** Optional speech/earcons identify acquired/lost targets and a repeat command describes the current known target.
- **Systems:** `lv.c` aimed-prop selection, `sightTick`, prop/character/object categories, friendliness rules, position/orientation.
- **Acceptance:** Scripts cover hostile/friendly/non-targetable objects, cloaked/occluded cases, target loss, rapid crossing, multiple players, sights with special behaviour, pause/cutscene, and no target. A blind tester can acquire a specified visible target without output flooding.
- **Logging/evidence:** Target category, semantic/raw IDs, pointer, resolved name, relationship, exact position/distance, visibility/knowledge state, acquisition/loss, and suppress reason. Diagnostic logging does not imply that hidden fields are announced.
- **Risks and unknowns:** Target state may leak information, fluctuate per frame, or have sight/weapon-specific rules.
- **Explicit non-goals:** No aim automation, snap-to-target, enemy radar, or hit guarantee.

### Prioritized environmental-hazard slice

- **Status:** Generic damaging-laser implementation builds; runtime and blind-user acceptance are pending.
- **Goal:** Make nearby, directly faced laser barriers perceptible without creating a constant environmental alarm.
- **User-visible result:** The nearest active damaging laser within 500 units emits an automatically enabled 220 Hz spatial sweep along its beam and back; turning away or losing sight silences it.
- **Systems:** Active-prop traversal, semantic laser-door state, live model bounds/rotation, closest-point and camera-facing tests, background line of sight, prop-sound spatial math, and a third fixed procedural mixer voice.
- **Acceptance:** In holo-training 3, approach every laser at standing, ducking, and crouching heights; confirm the sweep follows the visible beam, stops outside the forward cone/range or behind geometry, advances to the next bar without chatter, and coexists with aiming and beacon cues. Repeat through exercise start, completion/abort, pause, menu, death where practical, stage exit, feature disable, and several sessions without growth or stuck audio.
- **Logging/evidence:** Record selected identity, endpoints, closest/source distance, facing dot, line-of-sight and state exclusions, sweep phase/source, attenuation, pan, selection changes, lifecycle resets, and aggregate scans.
- **Risks and unknowns:** Stereo cannot fully encode elevation or front/rear position; 500-unit range, 25-degree cone, 90-tick cycle, 75-unit hysteresis, level, and masking require blind-user tuning.
- **Explicit non-goals:** No global hazard radar, warning for inactive/occluded/rearward lasers, automated crouching, route advice, or non-laser hazard claim.

## Milestone 10 — Broader interaction scanner

- **Goal:** Generalize the Carrington Institute beacon proof into a deliberate, knowledge-safe query for a broader set of nearby actionable things.
- **User-visible result:** A scan summarizes eligible doors, pickups, terminals, characters, and mission objects by direction/distance; results can be stepped or repeated, while positioned beacons remain available for supported categories.
- **Systems:** Milestone 5 beacon core, prop/object types, generalized interaction eligibility, rooms/visibility, localized object names, spatial formatter, and permanent action bindings.
- **Acceptance:** In a controlled area, results are stable, ordered, range-limited, knowledge-safe, and actionable; opened/collected/destroyed state updates; overlapping objects and empty scans recover cleanly; a blind tester locates and activates chosen objects.
- **Logging/evidence:** Query origin and position, all considered props with semantic/raw IDs and pointers, eligibility/filter reasons, exact ordering/distance, selected result, and movement context.
- **Risks and unknowns:** Engine object names may be absent or visual-only, while visibility, knowledge, and actionability can differ.
- **Explicit non-goals:** No global enemy radar, hidden-object disclosure, or automatic interaction.

## Milestone 11 — Navigation prototype

- **Goal:** Evaluate nonvisual orientation and route guidance in one bounded Carrington Institute/training route.
- **User-visible result:** Heading/landmark queries and optional cues support following a short route, detecting deviation, and recovering.
- **Systems:** player pose, rooms/portals/pads, doors/elevators, landmarks, route model, speech/earcons.
- **Acceptance:** Define start/end and allowable assistance; measure independent completions, wrong turns, recovery time, cue load, and motion comfort; handle pause, door state, elevator/vertical change, leaving/rejoining route, and restart. At least two blind-user iterations inform revisions.
- **Logging/evidence:** Exact coordinates/orientation, room/route nodes and segments, considered paths, cue decisions, deviations/recovery, user input/query, and timestamps.
- **Risks and unknowns:** Source navigation data may not map to human routes; vertical transitions and dynamic doors may invalidate cues.
- **Explicit non-goals:** Do not generalize beyond the named route or move/steer the player automatically.

## Milestone 12 — One end-to-end playable segment

- **Goal:** Combine menus, briefing/objectives, status, targeting, scanner, and navigation for one explicitly named training or campaign segment.
- **User-visible result:** A blind player can start, understand the goal, perform required interactions/combat, recognize success/failure, and return to a known menu state.
- **Systems:** All proven adapters plus segment-specific semantic gaps, documented narrowly.
- **Acceptance:** Publish the exact segment, starting state, required settings, task script, permitted assistance, completion definition, median attempts/time, known blockers, and regression suite. Multiple independent blind users complete it without live sighted direction.
- **Logging/evidence:** Correlatable raw session events, full state snapshots where helpful, and tester notes. Temporary high-volume diagnostics may remain behind the explicit logging switch when useful to later milestones.
- **Risks and unknowns:** Segment-specific scripts and state can conceal brittle assumptions; success may depend on tester game familiarity.
- **Explicit non-goals:** One segment does not prove campaign coverage; avoid one-off hard-coded narration that cannot become a maintained semantic rule.

## Milestone 13 — Broader coverage and release readiness

- **Goal:** Expand proven patterns, harden compatibility, document coverage, and establish sustainable upstream maintenance.
- **User-visible result:** A published matrix accurately identifies accessible menus/modes/stages, limitations, setup, shortcuts, privacy, and troubleshooting.
- **Systems:** Remaining menu types/stages, localization, packaging, backend fallbacks, performance, automated regression, documentation/release process.
- **Acceptance:** Supported OS/ROM configurations build; long sessions meet performance/log-size targets; backend failure and upgrade paths work; accessibility-disabled regression is clean; coverage claims match blind-user evidence; hooks and dependencies pass upstream review.
- **Logging/evidence:** Schema is versioned, documented, user-controlled, and migratable. Measure volume and performance before deciding on buffering, retention, or size policy.
- **Risks and unknowns:** Breadth can outrun quality, upstream changes can invalidate hooks, and platform/backend coverage will vary.
- **Explicit non-goals:** Do not label the game fully accessible without explicit mode/stage evidence, and do not make unsupported platforms fail because speech is absent.

## Cross-milestone exit rule

A milestone exits only when its build and runtime checks pass, its user task can be repeated, its comprehensive log evidence explains key decisions, accessibility-disabled behaviour is checked, open risks are recorded, and the required blind-user result is met. If independent testing is not yet available, label the milestone “engineering complete, accessibility validation pending.”
