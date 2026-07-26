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

## Milestone 5 — Interactable beacons

- **Status:** The original existing-sample implementation, automatic refresh, and multi-target scheduling passed blind-user acceptance. The procedural-chirp cue revision builds successfully and is pending runtime acceptance.
- **Detailed plan:** `milestones/ACCESSIBILITY_MILESTONE_05_PLAN.md` is the authoritative implementation handoff.
- **Goal:** Let a blind player deliberately select and spatially locate useful classes of nearby props: interactable objects, collectible items, and doors.
- **User-visible result:** F5 toggles one positioned 880 Hz chirp on eligible interactable objects, F6 independently toggles one 440 Hz chirp on eligible doors, and F8 independently toggles three quick positioned 880 Hz chirps on collectible items. Any combination can run; all patterns are staggered and require no new audio asset.
- **Systems:** Accessibility gameplay coordinator, read-only prop eligibility, active-prop traversal, door sibling canonicalization, player-relative ordering, procedural stereo audio, provisional PC actions, configuration, and structured logging.
- **Acceptance:** Carrington Institute evidence remains the initial baseline. Repeat through the first DataDyne missions and a one-local-player Combat Simulator match: results are stable and range-limited; cues remain attached to their props; alarms, lift controls, terminals, grabbable objects, ordinary pickups, an inactive deployed CamSpy, and open/closed/locked/destroyed/deactivated doors update safely; menu/pause/cutscene/death transitions silence and resume selected categories; a blind tester can distinguish the patterns and independently locate designated objects.
- **Logging/evidence:** Record every scan and per-category toggle, player pose/rooms, every considered prop and raw identity, inclusion/exclusion reason, canonical door group, exact distance/bearing/order, each category's selected target, chirp frequency/volume/pan/pulse lifecycle, invalidation, and timing.
- **Risks and unknowns:** Interaction checks currently mix actionability with on-screen/facing constraints; closed doors require different knowledge checks than ordinary objects; prop lifetimes and linked doors need safe handling; stereo panning may require turning to resolve front from rear; chirp duration, level, and category contrast need runtime validation.
- **Explicit non-goals:** No external audio, HRTF, automatic movement/interaction, split-screen/cooperative composition, permanent bindings/options UI, or claim that CI acceptance proves campaign-wide coverage.

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

Implementation status: the generic HUD-admission slice is implemented behind `Accessibility.HudMessages=1`. It queues every successfully admitted non-subtitle HUD message for non-interrupting speech and excludes the explicit in-game/cutscene subtitle types. The pre-mission Overview announces its visible difficulty-filtered objective list; pause Status adds current localized states; pause Briefing retains the complete long-form text; and pause Inventory exposes its visible rich item details. Subtitle narration, automatic priority objective events, full queue expiry/replacement policy, and Milestone 7 acceptance remain pending.

## Milestone 8 — Player status and inventory queries

Implementation status: the active weapon/device radial now announces localized highlighted-item labels, including Unarmed, as a bounded inventory-selection slice. Health, shield, ammo, deliberate status/inventory queries, function-screen narration, and the rest of this milestone remain open.

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

### Prioritized device-target alignment slice

- **Status:** Engineering implementation added for the Data Uplink, ECM Mine, and Door Decoder exercises and the DataDyne Research: Investigation Uplink terminal; runtime and blind-user acceptance are pending.
- **Goal:** Identify the exact object accepted by a special item's existing exercise logic without treating every interactable or visually similar model as valid.
- **Systems:** Active device-training state and selected device, equipped right-hand weapon, setup-script object tags, the raw non-shooting aim-query result, and the existing centered targeting oscillator.
- **Initial behavior:** While the matching CI exercise and device are active, pointing the Uplink at terminal tag `0x30`, the ECM Mine at hub tag `0x32`, or the Door Decoder at panel tag `0x35` produces a fixed 660 Hz centered tone. In DataDyne Research: Investigation, equipping the Uplink admits the mission script's terminal tag `0x0a` through the same registry. A separate device profile suppresses positioned presence audio. The Uplink and Decoder still require their normal interaction conditions; ECM output identifies only the correct hub surface and does not predict the thrown mine's trajectory or impact.
- **Acceptance:** Verify correct/wrong objects, device equipped/unequipped, off-aim silence, target disable/removal, exercise start/completion/failure/abort, menus, pause, stage exit, and repeated sessions. For ECM, deliberately produce both correct and incorrect throws after acquiring the tone and confirm the cue never claims that landing is guaranteed.
- **Evidence:** Correlate `targeting/device_candidate`, `aim_acquisition`, `aim_loss`, `alignment_start`, `alignment_update`, `alignment_stop`, and profile-transition records with the exercise script outcome.
- **Risks:** These validity rules exist as setup-script tags rather than a general runtime capability API. The registry supports multiple stage/training-scoped targets per item, but other devices still require independently confirmed semantic contracts before adding rows. The raw query can identify a surface but cannot establish Uplink range or simulate an ECM trajectory.
- **Explicit non-goals:** No model-name heuristic, broad interactable-as-target rule, trajectory prediction, automatic use/throw, off-aim target beacon, or claim that arbitrary mission devices are covered.

Engineering extension: an active CamSpy now contributes incomplete engine holograph criteria to the same alignment-only device profile. Candidate admission mirrors the photograph rule's health, render/front, 400-unit range, and full-viewport requirements. Since photography is frame-based rather than ray-based, any criterion the game could accept drives the fixed tone; screen-center distance only stabilizes identity if several qualify. Runtime revalidation is pending after correcting the initially over-restrictive center-ray requirement.

### Prioritized R-Tracker nonvisual-interface slice

- **Status:** Detailed specification and engineering implementation added; runtime and blind-user acceptance are pending.
- **Detailed specification:** `ACCESSIBILITY_RTRACKER_AUDIO_SPEC.md` defines the authoritative semantic, acoustic, lifecycle, performance, and acceptance contract.
- **Goal:** Provide equal nonvisual access to every marker exposed by the native R-Tracker without inventing line-of-sight, navigation, identity, or objective information.
- **Systems:** Shared radar classification, active-prop traversal, native device/cheat state, ten fixed procedural mixer voices, screen-reader state announcements, configuration, lifecycle reset, structured logging, and optional performance diagnostics.
- **Initial behavior:** Device changes speak `R-Tracker on` and `R-Tracker off`; an empty activation speaks `No tracked targets` once. All admitted targets sound concurrently with stable, staggered slots. Yellow objects use 700 Hz, red tracked characters 520 Hz, and blue cheat items 1000 Hz. Stereo pan conveys bearing, rear modulation conveys front/back, cadence ramps from 1.2 seconds at 4,000 units to 0.2 seconds nearby, and single/rising-double/falling-double patterns convey level/above/below.
- **Acceptance:** Verify the CI IR Scanner; empty activation; all three Skedar Ruins pillars and individual completion removal; Attack Ship's mixed tracked objects/characters and death/cloak removal; blue cheat markers; full-circle bearing; distance and height transitions; pause/menu/cutscene/death/stage/reset cleanup; repeated-session stability; and feature disable.
- **Evidence:** Correlate `rtracker` activation, scope, candidate, slot, overflow, and scan-summary events with perceived audio. With advanced diagnostics enabled, inspect active tracker voices, scan times, frame gaps, mixer activity, and process-memory deltas.
- **Risks:** Ten concurrent voices may mask speech or other cues; stereo plus modulation may not make front/rear sufficiently distinct; vertical and cadence thresholds require blind-user tuning; the current single-player policy leaves cooperative composition unresolved. A scan above 0.5 ms sustained or 2 ms once requires investigation.
- **Explicit non-goals:** No target names, objective status, route guidance, line-of-sight filtering, aim automation, multiplayer radar, ordinary scenario/player radar elements, runtime allocation, or game sound-channel consumption.

### Prioritized IR Scanner highlighted-object slice

- **Status:** Engineering implementation added; runtime and blind-user acceptance are pending.
- **Goal:** Give nonvisual access to the special objects visually highlighted by the IR Scanner, limited to the player's rendered viewport.
- **Systems:** Shared object-renderer highlight predicate, previous-frame onscreen state, native IR device state, R-Tracker fixed voices, independent default-on configuration, lifecycle suppression, and structured logs.
- **Initial behavior:** While the IR Scanner is active, up to ten preceding-frame onscreen objects carrying conditional-scenery or infrared-highlight state use the R-Tracker yellow-object 700 Hz pattern. Ordinary palette-treated scenery and characters are excluded. Turning away removes a cue automatically, and the mutually exclusive scanners share rather than duplicate the ten-voice pool.
- **Acceptance:** In CI training, verify the highlighted secret door appears only while on-screen; then verify turning, distance, elevation, menu/pause/device/stage transitions, feature disable, repeated sessions, and at least one equivalent mission object.
- **Evidence:** Correlate native IR activation and `source=ir_scanner`, `category=infrared_highlight` candidate/slot records with the visible highlight; audit overflow, scan duration, active voice count, memory, and frame cadence.
- **Risks:** `PROPFLAG_ONANYSCREENPREVTICK` intentionally introduces one rendered-frame latency; dense scenes can exceed ten highlighted objects; the special-highlight flag contract may cover linked scenery whose visible state changes during destruction.
- **Explicit non-goals:** No sound for the scanner's general red palette, ordinary characters, off-screen objects, names, line-of-sight inference, hidden route guidance, or additional mixer allocation.

### Prioritized X-Ray Scanner object slice

- **Status:** Engineering implementation added alongside the IR Scanner slice; combined runtime and blind-user acceptance are pending.
- **Goal:** Sonify the object/door/weapon props recolored by the native X-Ray Scanner while preserving its viewport and eraser-radius limits.
- **Systems:** Shared renderer eraser-distance query, previous-frame onscreen state, native device versus Farsight distinction, nearest-ten bounded selection, R-Tracker fixed voices, independent default-on configuration, lifecycle suppression, and logging.
- **Initial behavior:** The nearest ten X-Ray-rendered object/door/weapon props use the 700 Hz R-Tracker pattern and update automatically as the player turns or moves. Characters are excluded from this generic lane. The Farsight's use of X-Ray vision does not activate the scanner feature.
- **Acceptance:** In CI training, use cues to locate both hidden switches while confirming ordinary rendered props can also sound; verify viewport/radius entry and exit, nearest-ten overflow, stable direction, device/menu/pause/stage cleanup, configuration disable, repeated sessions, and a later non-CI scanner context.
- **Evidence:** Correlate X-Ray device/vision state, eraser origin/radius, native source distance, candidate/slot/overflow records, active voice counts, frame cadence, and memory over repeated tests.
- **Risks:** The native view highlights all in-range objects rather than a semantic target subset, so dense audio and ten-slot omission need blind-user evaluation. Nearest-first selection may still mask a desired switch behind closer furniture, and one-frame viewport evidence adds deliberate latency.
- **Explicit non-goals:** No exercise-tag special case, target/actionability claim, character duplication, Farsight support, names, off-screen awareness, automated interaction, or extra allocation.

### Prioritized environmental-hazard slice

- **Status:** Generic damaging-laser implementation builds; runtime and blind-user acceptance are pending.
- **Goal:** Make nearby, directly faced laser barriers perceptible without creating a constant environmental alarm.
- **User-visible result:** The nearest active damaging laser within 500 units emits an automatically enabled 220 Hz spatial sweep along its beam and back; turning away or losing sight silences it.
- **Systems:** Active-prop traversal, semantic laser-door state, live model bounds/rotation, closest-point and camera-facing tests, background line of sight, prop-sound spatial math, and a third fixed procedural mixer voice.
- **Acceptance:** In holo-training 3, approach every laser at standing, ducking, and crouching heights; confirm the sweep follows the visible beam, stops outside the forward cone/range or behind geometry, advances to the next bar without chatter, and coexists with aiming and beacon cues. Repeat through exercise start, completion/abort, pause, menu, death where practical, stage exit, feature disable, and several sessions without growth or stuck audio.
- **Logging/evidence:** Record selected identity, endpoints, closest/source distance, facing dot, line-of-sight and state exclusions, sweep phase/source, attenuation, pan, selection changes, lifecycle resets, and aggregate scans.
- **Risks and unknowns:** Stereo cannot fully encode elevation or front/rear position; 500-unit range, 25-degree cone, 90-tick cycle, 75-unit hysteresis, level, and masking require blind-user tuning.
- **Explicit non-goals:** No global hazard radar, warning for inactive/occluded/rearward lasers, automated crouching, route advice, or non-laser hazard claim.

### Prioritized hostile-character targeting slice

- **Status:** Hostile-character behavior has project-owner runtime acceptance; the autogun extension builds, with DataDyne Research blind-user acceptance pending.
- **Goal:** Extend the generic targeting core from firing-range props to ordinary hostile characters and automated gun turrets without using stage, holo-training, setup, or script-specific identifiers.
- **Systems:** Use the bounded onscreen-prop list, rendered model bounds, existing team/friendly classification, character life/action/visibility/targetability state, autogun health/deactivation/ammunition/malfunction/target-team state, shooting-blocker line of sight, and the finalized attack-query prop. Feed admitted threats into ten fixed procedural presence voices and the direct-aim alignment lane. Keep a separate combat profile so changing between range and combat clears stale identities and audio.
- **Initial behavior:** Active, enabled, combat-capable, rendered, visible hostile characters receive positioned 440 Hz cues in independent staggered slots for up to ten enemies. Eligible hostile autoguns use the same presence and direct-aim lanes; disabled, destroyed, empty, malfunctioning, non-player-targeting, and player-owned autoguns are excluded. The combat-specific audible envelope defaults to full through 600 world units, fading through 3,500, and ending at 4,000, with a 0.14 mixer gain; all four values are bounded `pd.ini` settings. Relative to the semantic unarmed punch range `X`, cadence is 180 ms every 750 ms beyond `5X`, ramps to 50 ms every 200 ms at `X`, and becomes a constant tone inside `X`. Material period reductions trigger the next chirp immediately. Constant-mode entry remains `X`, with an exit-only `1.1X` hysteresis threshold to suppress collision-boundary flicker. Directly pointing at an admitted hostile produces a separate fixed 660 Hz centered alignment tone, including while unarmed. Friendly, neutral, dying, dead, falling-knocked-out, knocked-out, hidden, untargetable, imperceptibly cloaked, offscreen, and occluded characters do not produce positive feedback.
- **Acceptance:** In Holo Training 4, each ordinary hostile automatically enters and leaves an oscillator slot as visibility and combat state change; pointing at any admitted hostile starts the alignment tone without requiring a weapon. Approach slowly through `5X` and `X`, confirm a smooth faster/shorter ramp followed by a click-free constant spatial tone, then verify whether that transition supports reliable punches without claiming a hit when aim is off target. Verify stationary, moving, fighting, occluded, dying, and knocked-out states, including immediate silence for a body on the floor, session completion/abort, menus, pause, and repeated sessions. Repeat later in a mission containing friendly or neutral characters and with several simultaneous hostiles, then stress all ten slots and overflow ordering.
- **Evidence:** Correlate `targeting/combat_candidate`, `scope_gate`, `observation`, `presence_play`, `aim_acquisition`, `aim_loss`, `alignment_start`, `alignment_update`, `alignment_stop`, and `telemetry` records with perceived targets and audio ownership.
- **Risks:** A body-midpoint line-of-sight ray may reject a character visible only at an extremity; the finalized attack-query hit mitigates this only while aimed. Ten five-Hz voices intentionally create dense output and need masking/overload tuning; candidates beyond the fixed capacity wait behind stable existing assignments. Combat aim quality remains unavailable, so the tone does not yet communicate head or other high-value-zone accuracy.
- **Explicit non-goals:** No aim automation, through-wall disclosure, spoken identity, friendly/neutral cue vocabulary, head-quality pitch mapping, multiplayer policy, or automatic support for vehicles and other non-character threats.

### Weapon-function state cue slice

- **Status:** Implemented; runtime and blind-user acceptance pending.
- **Goal:** Make the existing visual primary/secondary function indicator available without speech latency or controller-specific assumptions.
- **Behavior:** Observe the engine's final effective function after weapon processing. A transition to primary produces one centered 1000 Hz, 35 ms beep; secondary produces two, separated by 30 ms. Initial state, stage changes, and weapon changes are silent baselines. The dedicated fixed oscillator lane remains independent of beacon, hazard, aiming, and combat voices.
- **Acceptance:** With several weapons that have persistent and temporary alternate functions, use R1/right bumper and at least one other supported function-selection path. Confirm exactly two beeps only when secondary becomes effective and exactly one only when primary becomes effective; invalid or unavailable toggles, equipping weapons, stage entry, pause, and menus must not create false output. Check rapid toggles, firing during transitions, dual wielding, and audio overlap. Multiplayer remains unvalidated.
- **Evidence:** Correlate `weapon_function/state_change` with player, stage, weapon, previous/current state, beep count, and tone parameters. Advanced performance logs expose the pattern sequence and requested pulse count; long sessions must show bounded mixer work and no allocation or game-channel growth.

## Milestone 10 — Broader interaction scanner

### Prioritized non-hostile-character beacon slice

- **Status:** Engineering implementation builds; runtime and blind-user acceptance are pending.
- **Goal:** Make nearby people who are not presently hostile spatially discoverable without mixing them with the hostile combat cue vocabulary.
- **Behavior:** F7 independently toggles up to three automatically refreshed friendly, neutral, or native blue-sight protected character beacons in any single-player gameplay stage. Each retained person emits two rapid positioned 440 Hz chirps on the shared staggered beacon timeline. Protected nonlethal targets also admit the valid-aim alignment tone but never receive hostile enemy-presence cadence. Existing relationship, life/action, hidden, untargetable, cloak/IR, room, range, and shooting-blocker line-of-sight state govern eligibility.
- **Door visibility refinement:** F6 door candidates must be rendered in the active camera view at scan and playback time. Native sibling leaves retain one canonical doorway identity and at most one rendered positioned source, so paired leaves cannot consume multiple result or schedule slots.
- **Acceptance:** Test friendly and neutral people, scripted allegiance changes, nearby hostiles, death/knockout, cloak with and without IR perception, occlusion, movement, more than three candidates, stage transitions, menus, pause, cutscenes, death, and repeated sessions. Confirm F7 changes only this category and all enabled categories remain serialized.
- **Evidence:** Correlate `beacon/candidate`, `scan_result`, `schedule_target`, `pulse`, and `category_state` records with team, action, visibility, identity, position, distance, pan, and two-pulse output.
- **Risks:** A neutral relationship can change through scripts after a refresh; two 440 Hz chirps require masking tests against one-chirp doors. A midpoint line-of-sight ray may exclude a partly visible person. The three-target cap prioritizes stable nearby identities rather than exhaustive awareness.
- **Explicit non-goals:** No spoken identity, through-wall disclosure, hostility prediction, dialogue availability claim, multiplayer policy, or automatic interaction.

Engineering extension: door and non-hostile-character beacon queries now follow the active CamSpy perspective and return to Joanna automatically. Remote people must also be rendered in the CamSpy viewport, after an acceptance log showed room-connected but visually absent characters passing the broader Joanna scanner policy. The same observer boundary leaves body-actionable CI object/pickup categories paused during remote viewing. Runtime and blind-user transition revalidation remain pending.

- **Goal:** Generalize the Carrington Institute beacon proof into a deliberate, knowledge-safe query for a broader set of nearby actionable things.
- **User-visible result:** A scan summarizes eligible doors, pickups, terminals, characters, and mission objects by direction/distance; results can be stepped or repeated, while positioned beacons remain available for supported categories.
- **Systems:** Milestone 5 beacon core, prop/object types, generalized interaction eligibility, rooms/visibility, localized object names, spatial formatter, and permanent action bindings.
- **Acceptance:** In a controlled area, results are stable, ordered, range-limited, knowledge-safe, and actionable; opened/collected/destroyed state updates; overlapping objects and empty scans recover cleanly; a blind tester locates and activates chosen objects.
- **Logging/evidence:** Query origin and position, all considered props with semantic/raw IDs and pointers, eligibility/filter reasons, exact ordering/distance, selected result, and movement context.
- **Risks and unknowns:** Engine object names may be absent or visual-only, while visibility, knowledge, and actionability can differ.
- **Explicit non-goals:** No global enemy radar, hidden-object disclosure, or automatic interaction.

## Milestone 11 — Navigation prototype

Implementation status: a general single-player virtual-cane slice is implemented as an orientation experiment, but Milestone 11 remains open. F4 cycles off, a two-second seven-angle sweep, and a one-second seven-angle sweep. Each live player-sized movement-cylinder probe reports the nearest background/object/door/path-blocking collision with a fixed-pitch spatial chirp while excluding characters and drop-offs. Route guidance, landmarks, deviation/recovery semantics, controller/settings UI, runtime validation, and the required blind-user iterations remain pending.

The implemented cane now uses the active player-or-CamSpy observer pose and restarts a partial sweep when the visible perspective changes. CamSpy collision dimensions and rooms replace Joanna's only while its camera mode is effective. This engineering behavior still requires runtime transition, collision, and performance evidence.

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
