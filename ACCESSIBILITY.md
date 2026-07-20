# Accessibility project charter

## Purpose

This work aims to make the Perfect Dark PC port meaningfully playable by blind and low-vision players, starting with nonvisual access to menus and essential game state and progressing through small, testable gameplay slices.

The current branch contains the Milestone 2 accessibility coordinator/logger, the Milestone 3 Tolk/NVDA Windows speech backend, the completed Milestone 4 menu-agnostic focus-narration slice, and an engineering-complete Milestone 5 Carrington Institute beacon prototype awaiting runtime and blind-user acceptance. Accessibility, comprehensive logging, speech, and menu narration default to enabled for testing. Milestone 4 observes final menu focus, resolves all currently used focusable control families, exposes semantic text for custom-rendered rows such as `New Agent...`, supports one-time semantic dialog summaries for important non-focusable content, replaces stale menu speech, and provides provisional F5 repeat/F6 cancel commands. Its MinGW64 build passed, and the project owner completed blind-user acceptance testing and accepted the spoken-menu behavior. The firing-range training-information dialog supplies its localized weapon name, challenge values, and weapon description through that summary path. The separate CI `Weapons Available` laptop supplies the focused weapon's localized manufacturer, primary and secondary functions, and marquee description as part of each list announcement. The Milestone 5 prototype can spatially beacon nearby CI interactable objects and doors; broader gameplay accessibility and navigation are not implemented.

### Carrington Institute beacon prototype

`Accessibility.InteractableBeacons` defaults to `1` for project-owner blind-user acceptance testing. It remains configurable in `pd.ini`. Enabling the setting does not start a sound automatically; use F5 and F6 during Carrington Institute gameplay.

- F5 independently toggles the nearest interactable-object beacon.
- F6 independently toggles the nearest door beacon.
- Neither, either, or both categories can be active. When both are active, their pulses are staggered and each follows its own nearest eligible prop.
- While a menu is open, F5 retains its menu-narration repeat action and F6 retains its speech-cancel action; gameplay beacons are reset and do not process those presses.
- Interactable objects use a positioned `SFX_MENU_FOCUS` pulse; doors use positioned `SFX_MENU_SUBFOCUS`.
- The implementation is limited to single-player Carrington Institute training, a 1,200-unit radius, and the two named categories.
- Menus, pause, cutscenes, death, unsupported player counts, stage changes, invalid props, and audio allocation failures stop the beacon.
- The current stereo positional system primarily conveys left/right and distance. Front/rear and vertical usefulness require runtime and blind-user evidence and are not yet claimed.

Perfect Dark is a fast first-person game whose original interface communicates heavily through graphics, spatial audio, motion, timing, and implicit world knowledge. A useful accessibility layer must expose the engine's own semantic state and provide ways to act on it. Merely reading every piece of on-screen text would leave core tasks inaccessible.

## Initial users and scope

The primary users for the first milestones are:

- blind players who use speech and audio cues;
- low-vision players who benefit from explicit, repeatable state announcements;
- screen-reader users who need predictable focus, labels, values, and interruption behaviour;
- testers and developers who need detailed event evidence.

Initial work focuses on Windows because a practical speech path is required for the first end-to-end proof. The semantic core should remain platform-independent so other backends can be added without rewriting game hooks.

The first useful slice is startup through the main menus. Later slices cover HUD messages and objectives, player status, targeting, interaction scanning, navigation experiments, and one bounded training or campaign segment.

## Not in the initial phase

- Claiming that the full campaign, multiplayer, or every menu is accessible.
- Computer vision, OCR, or pixel scraping as the primary interface to known engine state.
- Automated aiming, invulnerability, enemy AI changes, or mission simplification by default.
- Distributing ROMs, extracted assets, generated game data, or copyrighted text/audio.
- Replacing the existing audio, input, menu, localization, or configuration systems.
- Committing to one Windows speech technology before a small backend experiment measures availability, latency, cancellation, deployment, and failure behaviour.
- Treating a successful build or a sighted developer walkthrough as proof of accessibility.

## Design principles

1. **Semantics before pixels.** Read a resolved menu label or objective state, not screen coordinates or glyphs.
2. **Nonvisual interaction, not spoken screenshots.** A user needs reliable focus, commands, state, orientation, and recovery paths—not a stream of visual descriptions.
3. **Small upstream hooks.** Established systems publish narrow facts; accessibility modules decide what, when, and how to announce.
4. **Platform separation.** Gameplay code emits platform-neutral events. A backend owns Windows speech integration.
5. **User control.** Speech, logging, verbosity, categories, repeat commands, and cue volume need explicit settings and useful defaults.
6. **Calm output.** Priorities, replacement, deduplication, throttling, and expiry prevent speech from becoming another barrier.
7. **Deterministic recovery.** Users must be able to repeat current focus/state, learn context, silence speech, and return to a known state.
8. **Preserve game behaviour.** With accessibility disabled, hooks should be inert and game logic should remain unchanged.
9. **Evidence over confidence.** Separate build, runtime, speech-output, task-completion, and independent-user evidence.
10. **Blind testing early.** Short tests after each user-visible increment are more valuable than a large untested feature dump.
11. **Diagnostic-first logging.** Keep logging local and configurable, but enable comprehensive evidence by default during acceptance testing and record all feature-relevant state that can help explain accessibility behaviour.
12. **Legal boundaries.** The project consumes a user-supplied supported ROM and never ships protected game content.

## Capability areas

### Menus and settings

Announce dialog context, focused control, role, value, availability, relevant hints, and explicitly provided summaries for important non-focusable content. Support repeat, speech cancellation, and predictable handling of lists, sliders, checkboxes, dropdowns, scrollable briefing text, and dynamic labels.

### Messages, dialogue, and objectives

Expose accepted HUD messages and subtitle text without duplicating messages that the HUD rejects. Announce objective state changes with priority and allow the current objective list and briefing to be queried.

### Player status and inventory

Offer deliberate queries for health, shield, weapon, firing mode, ammunition, and inventory. Reserve unsolicited announcements for important changes and thresholds.

### Targeting and interaction

Describe the current valid target and nearby actionable objects from semantic game objects. Convey friendly/hostile/unknown status only when the game can determine it. Avoid leaking hidden information.

### Navigation and orientation

Experiment with headings, landmark scans, route cues, distance bands, room connectivity, and recovery commands. Navigation must respect what the player could reasonably know and must be evaluated in one controlled space before broad use.

### Presentation and low vision

Later work may add scalable text, contrast controls, reduced motion, cue balancing, and other visual options. These should share semantic settings where appropriate without coupling them to speech availability.

## Development and playtest loop

Each increment should follow this loop:

1. Identify one user task and the semantic state required to complete it.
2. Trace the owning engine systems and record uncertainty.
3. Add the smallest event or query boundary and structured log evidence.
4. Test build, startup, backend failure, output ordering, and regression behaviour.
5. Run a short blind or screen-reader-dependent task with a defined start and success condition.
6. Correlate tester observations with the session log, fix the highest-impact barrier, and repeat.
7. Expand scope only when the bounded task is reliable and recoverable.

Development logs may contain resolved text, names, paths, arguments, positions, input events, identifiers, pointers, and detailed game state when useful. They must never contain ROM contents, extracted copyrighted assets, passwords, authentication tokens, or unrelated operating-system secrets, and they are never uploaded automatically. Testers should be told that the diagnostic log is comprehensive before choosing whether to share it.

## Glossary

**Announcement**

A semantic text event offered to speech output, with priority, category, context, and queue policy. It is not necessarily spoken if disabled, superseded, deduplicated, or expired.

**Earcon**

A short non-speech sound with a documented meaning, such as target acquired or route correction. Earcons complement rather than replace queryable speech.

**Speech backend**

A platform-facing implementation that accepts normalized announcements and controls a speech technology. It must report availability and support safe initialization, cancellation, and shutdown.

**Accessibility hook**

A small call placed at an established semantic boundary so the accessibility subsystem can observe an event. Hooks must not contain speech policy or platform API calls.

**Scanner**

A user-triggered query that summarizes relevant nearby or directional entities from game state, subject to range, visibility/knowledge, priority, and rate limits.

**Navigation cue**

A speech or non-speech indication that helps the player orient, follow a route, or recover from deviation without taking control away.

**Playtest log**

A development-focused sequence of accessibility events and decisions used to correlate a tester's experience with detailed internal state. It is enabled by default for the current acceptance-testing phase, remains configurable, is separate from the general engine log, and may intentionally contain raw diagnostics.
