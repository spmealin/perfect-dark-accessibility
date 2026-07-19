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

- **Goal:** Add a disabled-by-default accessibility service with configuration, safe lifecycle, and opt-in structured session logging.
- **User-visible result:** With an explicit config/command choice, startup and shutdown produce a bounded accessibility log; normal play is otherwise unchanged.
- **Systems:** `port/src/main.c`, `port/src/pdmain.c` if a pump is required, config/filesystem services, new core/log modules, CMake.
- **Acceptance:** Disabled mode creates no accessibility log; enabled logging records schema/build/session/start/stop; write failure is nonfatal; repeated init/shutdown is safe; Windows baseline and at least one non-Windows compile path are checked when CI is available.
- **Logging/evidence:** The feature is its own evidence source; test rotation/size limit and redaction. Keep a fixture showing the schema, not user game data.
- **Risks and unknowns:** Constructor order, shutdown order, save-path behaviour, file rotation, and write failure need proof.
- **Explicit non-goals:** No speech, menu narration, or gameplay assistance yet.

## Milestone 3 — Windows speech backend proof

- **Goal:** Select and validate one replaceable Windows speech path behind the backend interface.
- **User-visible result:** An explicit test action speaks a fixed, non-game string and can be interrupted; absent/unavailable speech does not block startup.
- **Systems:** Native backend, null backend, UTF-8 conversion, queue pump, configuration, packaging experiment.
- **Acceptance:** Availability is reported; first-speech latency and cancellation are measured; calls do not stall a representative frame loop; Unicode examples work; startup/shutdown and no-voice/no-technology cases are nonfatal; licensing/deployment notes are recorded.
- **Logging/evidence:** Record request, queue decision, backend result/error category, and latency without native handles or machine identity.
- **Risks and unknowns:** Screen-reader coexistence, native dependencies, and COM/thread constraints may change the technology choice.
- **Explicit non-goals:** Do not wire gameplay systems directly to the backend or select a backend without measured evidence.

## Milestone 4 — Main-menu narration slice

- **Goal:** Let a blind user understand and operate the startup/main-menu path with reliable focus and values.
- **User-visible result:** Dialog context and supported focused controls are spoken; repeat and cancel work; opening/closing dialogs produces predictable context.
- **Systems:** `menuOpenDialog`, `dialogChangeItemFocus`/`dialogTick`, text resolution, item handlers/types, announcement queue.
- **Acceptance:** A defined script covers first focus, directional/mouse focus parity, selectable controls, checkbox, slider, list/dropdown, disabled item, back, repeat, and rapid navigation; unsupported types are logged without misleading speech; a blind tester reaches a chosen main-menu destination unaided.
- **Logging/evidence:** Dialog/focus semantic IDs, role/value, queue decision, and tester timestamps. Redact free-form profile names.
- **Risks and unknowns:** Dynamic labels, item callbacks, initial focus, and rapid transitions have lifetime/context hazards.
- **Explicit non-goals:** Do not claim every game menu or briefing is accessible.

## Milestone 5 — Discoverable accessibility settings

- **Goal:** Expose stable accessibility configuration in the existing PC options UI and define collision-free input actions.
- **User-visible result:** Users can enable/disable speech, categories, verbosity, logging, repeat, and cancel without hand-editing `pd.ini`.
- **Systems:** `port/src/optionsmenu.c`, config registry, input binding model, menu narration.
- **Acceptance:** Settings are themselves narrated, persist across restart, have safe defaults, and remain operable when a backend is unavailable; reset/default behaviour is documented; input conflicts are detected or avoided.
- **Logging/evidence:** Record setting category and new non-sensitive value only while logging is enabled; never log arbitrary keys typed.
- **Risks and unknowns:** The present binding model targets game controls and may need a separate accessibility action set; settings may be needed before a profile loads.
- **Explicit non-goals:** Do not add scanner/navigation bindings until those behaviours exist.

## Milestone 6 — HUD, subtitles, briefings, and objectives

- **Goal:** Make mission text and objective changes available without duplicate or stale speech.
- **User-visible result:** Accepted HUD messages and eligible subtitles are announced; briefings/current objectives can be read deliberately; objective state changes take priority.
- **Systems:** `hudmsgCreateFromArgs`, `hudmsgCreateAsSubtitle`, `objectivesCheckAll`, `setupLoadBriefing`, scrollable/objective menu adapters.
- **Acceptance:** Scripts cover pickups/system messages, dialogue with subtitles visually on/off, split subtitles, duplicate suppression, completed/incomplete/failed objectives, briefing review, pause objective review, and cutscene transition. Objective text is not spoken twice through HUD and objective paths.
- **Logging/evidence:** Source category, semantic objective ID/state, HUD type/flags, audio channel category, dedupe/replacement decision; avoid logging player-created names.
- **Risks and unknowns:** Voice audio, subtitle timing, splitting, and control codes can differ by region/version.
- **Explicit non-goals:** Do not promise audio description of uncaptioned cinematic action or all mission text coverage.

## Milestone 7 — Player status and inventory queries

- **Goal:** Provide calm, accurate access to health, shield, equipped weapon/function, ammo, and inventory.
- **User-visible result:** On-demand status is concise; important health/ammo thresholds are optional and non-repetitive; weapon changes are announced.
- **Systems:** player snapshot, `playerGetHealthFrac`, `playerGetShieldFrac`, bondgun name/ammo APIs, inventory APIs, action bindings.
- **Acceptance:** Test full/partial/zero health and shield, single/dual weapons, primary/secondary functions, reload/reserve states, no weapon, pickups, death/restart, pause, scripted health changes, and rapid weapon cycling. Values match game state within the defined sampling tick.
- **Logging/evidence:** Quantized values and semantic weapon/ammo IDs, change reason when known, queue decisions. Do not log save/profile names.
- **Risks and unknowns:** Health changes arise outside gun damage, dual-wield ammo semantics are complex, and polling and event hooks must not conflict.
- **Explicit non-goals:** No automatic tactical advice, aim changes, or inventory selection on the user's behalf.

## Milestone 8 — Targeting feedback

- **Goal:** Tell a player when a valid aimed target changes and provide truthful relationship/distance cues.
- **User-visible result:** Optional speech/earcons identify acquired/lost targets and a repeat command describes the current known target.
- **Systems:** `lv.c` aimed-prop selection, `sightTick`, prop/character/object categories, friendliness rules, position/orientation.
- **Acceptance:** Scripts cover hostile/friendly/non-targetable objects, cloaked/occluded cases, target loss, rapid crossing, multiple players, sights with special behaviour, pause/cutscene, and no target. A blind tester can acquire a specified visible target without output flooding.
- **Logging/evidence:** Safe target category/semantic ID, relationship, distance band, acquisition/loss, suppress reason; never raw pointers or hidden names.
- **Risks and unknowns:** Target state may leak information, fluctuate per frame, or have sight/weapon-specific rules.
- **Explicit non-goals:** No aim automation, snap-to-target, enemy radar, or hit guarantee.

## Milestone 9 — Interaction scanner

- **Goal:** Let a player deliberately discover nearby actionable objects without narrating the whole scene.
- **User-visible result:** A scan summarizes eligible doors, pickups, terminals, characters, and mission objects by direction/distance; results can be stepped or repeated.
- **Systems:** prop/object types, interaction eligibility, rooms/visibility, localized object names, spatial formatter, action bindings.
- **Acceptance:** In a controlled area, results are stable, ordered, range-limited, knowledge-safe, and actionable; opened/collected/destroyed state updates; overlapping objects and empty scans recover cleanly; a blind tester locates and activates chosen objects.
- **Logging/evidence:** Query origin category, eligible result semantic IDs, ordering/distance bands, selected result, filter reasons—not precise full-session movement trails by default.
- **Risks and unknowns:** Engine object names may be absent or visual-only, while visibility, knowledge, and actionability can differ.
- **Explicit non-goals:** No global enemy radar, hidden-object disclosure, or automatic interaction.

## Milestone 10 — Navigation prototype

- **Goal:** Evaluate nonvisual orientation and route guidance in one bounded Carrington Institute/training route.
- **User-visible result:** Heading/landmark queries and optional cues support following a short route, detecting deviation, and recovering.
- **Systems:** player pose, rooms/portals/pads, doors/elevators, landmarks, route model, speech/earcons.
- **Acceptance:** Define start/end and allowable assistance; measure independent completions, wrong turns, recovery time, cue load, and motion comfort; handle pause, door state, elevator/vertical change, leaving/rejoining route, and restart. At least two blind-user iterations inform revisions.
- **Logging/evidence:** Coarse route node/segment, cue kind, deviation/recovery, user query, and timestamps. Fine-grained coordinates are off by default and require explicit diagnostic consent.
- **Risks and unknowns:** Source navigation data may not map to human routes; vertical transitions and dynamic doors may invalidate cues.
- **Explicit non-goals:** Do not generalize beyond the named route or move/steer the player automatically.

## Milestone 11 — One end-to-end playable segment

- **Goal:** Combine menus, briefing/objectives, status, targeting, scanner, and navigation for one explicitly named training or campaign segment.
- **User-visible result:** A blind player can start, understand the goal, perform required interactions/combat, recognize success/failure, and return to a known menu state.
- **Systems:** All proven adapters plus segment-specific semantic gaps, documented narrowly.
- **Acceptance:** Publish the exact segment, starting state, required settings, task script, permitted assistance, completion definition, median attempts/time, known blockers, and regression suite. Multiple independent blind users complete it without live sighted direction.
- **Logging/evidence:** Correlatable session events with tester notes and consent; segment-specific temporary diagnostics are removed or disabled before merge.
- **Risks and unknowns:** Segment-specific scripts and state can conceal brittle assumptions; success may depend on tester game familiarity.
- **Explicit non-goals:** One segment does not prove campaign coverage; avoid one-off hard-coded narration that cannot become a maintained semantic rule.

## Milestone 12 — Broader coverage and release readiness

- **Goal:** Expand proven patterns, harden compatibility, document coverage, and establish sustainable upstream maintenance.
- **User-visible result:** A published matrix accurately identifies accessible menus/modes/stages, limitations, setup, shortcuts, privacy, and troubleshooting.
- **Systems:** Remaining menu types/stages, localization, packaging, backend fallbacks, performance, automated regression, documentation/release process.
- **Acceptance:** Supported OS/ROM configurations build; long sessions meet performance/log-size targets; backend failure and upgrade paths work; accessibility-disabled regression is clean; coverage claims match blind-user evidence; hooks and dependencies pass upstream review.
- **Logging/evidence:** Schema is versioned, bounded, documented, user-controlled, and migratable; aggregate results are consented and anonymized.
- **Risks and unknowns:** Breadth can outrun quality, upstream changes can invalidate hooks, and platform/backend coverage will vary.
- **Explicit non-goals:** Do not label the game fully accessible without explicit mode/stage evidence, and do not make unsupported platforms fail because speech is absent.

## Cross-milestone exit rule

A milestone exits only when its build and runtime checks pass, its user task can be repeated, its log evidence explains key decisions without private data, accessibility-disabled behaviour is checked, open risks are recorded, and the required blind-user result is met. If independent testing is not yet available, label the milestone “engineering complete, accessibility validation pending.”
