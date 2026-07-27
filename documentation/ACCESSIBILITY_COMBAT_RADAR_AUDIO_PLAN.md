# Combat Simulator audio radar implementation plan

## Status and design goal

This document specifies the nonvisual interface implemented for the radar
shown during Combat Simulator gameplay. The prototype is implemented, but it
is not an acceptance claim; blind-user playtesting is still required.

The design has two complementary operations:

- Pressing F3 performs an immediate audible pulse of everything the native
  radar currently draws, except the current player's own marker.
- Pressing Shift+F3 toggles automatic enemy-contact alerts. Alerts report a new
  enemy contact and inward crossings into medium and close range.

The interface must remain useful during fast combat without becoming another
continuous background scanner. Manual pulses are complete snapshots.
Automatic output is event-driven and silent while contact state is unchanged.

The initial implementation supports one local human player in a normal Combat
Simulator match. Split-screen speech/audio ownership and cooperative or anti
radar behavior remain outside this slice.

The implemented prototype uses the planned 16-marker and 16-contact bounds,
a 48-entry fixed priority queue, and one dedicated procedural mixer voice.
The larger queue preserves a launch cue plus a full 16-marker manual pulse
while automatic events are pending. All documented configuration keys and
defaults are registered; no runtime allocation or game sound channel is used.

## Confirmed native behavior

The following observations come from `src/game/radar.c` and the multiplayer
scenario callbacks:

- The normal multiplayer radar is not rendered when `MPOPTION_NORADAR` is set.
- It is also hidden when the current player's `MPDISPLAYOPTION_RADAR` display
  option is clear, while a player menu is open, or while the player is dead.
- `MPOPTION_NOPLAYERONRADAR` removes player and simulant markers.
- Living, uncloaked human players and simulants are normally drawn.
- In team games, marker color represents team. In non-team games, every other
  combatant is an opponent.
- Scenario callbacks may replace a character marker or draw additional
  objectives such as cases, terminals, an uplink, a designated victim, or the
  King of the Hill location.
- Horizontal radar position uses the target's relative X/Z coordinates.
- One radar pixel represents 250 world units. Horizontal distance is clamped
  at the 16-pixel outer radius, equivalent to 4,000 world units.
- When vertical indicators are enabled, a marker more than 250 units above the
  player is drawn as an up triangle and one more than 250 units below is drawn
  as a down triangle.
- Radar eligibility does not require line of sight, viewport visibility, room
  connectivity, or an unobstructed path.

Accessibility must not independently reconstruct a broader target set. If the
native radar does not draw a marker, neither manual pulse nor automatic alert
may expose it.

## Input contract

### F3: manual radar pulse

F3 is not currently named or consumed by the PC input or accessibility code.
SDL scancodes place it at value 60. Add `VK_F3 = 60` to the consecutive
function-key region of `port/include/input.h`.

During supported gameplay, an unmodified F3 pressed edge requests one snapshot
of the most recently completed native radar frame. Holding F3 must not repeat
the scan.

The pulse:

- works whether automatic contact alerts are enabled or disabled;
- includes every native radar marker except the current player's marker;
- includes enemies, allies, and scenario/objective markers;
- never adds a marker based only on world state;
- serializes markers so two radar sounds do not begin at the same instant; and
- does not change game targeting, radar settings, or contact-alert state.

If the native radar is unavailable, F3 plays one short centered unavailable
earcon. If the radar is available but contains no other markers, it plays a
distinct short centered empty-scan earcon. These responses are necessary
because silence cannot distinguish an unrecognized key, a hidden radar, and a
valid empty radar. Final earcon frequencies should be tuned in acceptance
testing and must not reuse the enabled/disabled confirmation patterns.

### Shift+F3: automatic contact-alert toggle

Either Shift key combined with the F3 pressed edge toggles automatic enemy
alerts. The modified chord must not also request a manual pulse.

Use the established accessibility toggle confirmation:

- enabled: one normal beep followed by one higher beep;
- disabled: one normal beep followed by one lower beep.

The toggle remains available during supported gameplay even if the current
match has disabled its radar, allowing the player to establish their preferred
state before a later match. Menus retain their own input context and neither
F3 action runs while a menu, pause screen, cutscene, or end screen owns input.

The runtime toggle is initialized from
`Accessibility.CombatRadarContactAlerts`, defaults on for blind-user
acceptance testing, survives temporary presentation suppression and match
transitions, and is not written back to `pd.ini` merely because the key was
pressed. Contact identities and range bands reset at match/stage boundaries.

## Native radar frame capture

### Capture the final visual decision

The safest equality boundary is the actual radar draw path:

1. At the start of `radarRender`, publish whether the native radar will render
   for the current player.
2. While that render is active, observe each successful `radarDrawDot` call,
   including calls made by scenario callbacks.
3. Record the actual relative coordinate, source prop when present, colors,
   color-swap mode, vertical-indicator result, and a monotonically assigned
   draw index.
4. Exclude the current player's marker from accessibility output.
5. Finalize the fixed snapshot when `radarRender` completes.
6. Every early-return path must publish an unavailable frame so stale markers
   cannot remain actionable.

Do not play audio directly from `radarDrawDot`. The render hook only fills
caller-owned fixed state. The accessibility adapter classifies, schedules, and
publishes audio after a complete snapshot is available.

A helper such as `radarIsVisibleForCurrentPlayer` should centralize the early
visibility rules if that can be done without changing native behavior. Both
the renderer and accessibility capture must consume the same result.

### Identity and classification

Use stable semantic identity wherever possible:

- Human player or simulant: prop identity plus the player/character index.
- Object-backed scenario marker: prop identity plus scenario/category.
- Location-only marker such as the hill: scenario category plus stable
  scenario slot, not a transient stack address.

Classification for the first slice:

- `enemy`: another combatant on an opposing team, or any other combatant in a
  non-team match;
- `ally`: another combatant on the current player's team;
- `objective`: a scenario callback marker that is not an ordinary combatant;
- `other`: a native marker that cannot yet be assigned a narrower semantic
  category.

Automatic alerts consume only `enemy`. Manual F3 consumes all four categories.
Unknown markers remain audible in a manual scan rather than being discarded,
but logs must identify them for later semantic refinement.

The renderer's final inclusion remains authoritative even if the adapter's
relationship classification is uncertain. Classification may change the
timbre, never eligibility.

## Capacity and ownership

Reserve a fixed array of 16 captured markers. The audited ordinary maximum is
four human players plus eight simulants, with a small allowance for
scenario-specific additions. If more than 16 dots are drawn:

- retain enemy markers first, followed by objectives, allies, and other
  markers;
- preserve stable identities within each priority class;
- log the total, retained count, and category overflow; and
- never allocate, grow a container, or steal another accessibility subsystem's
  audio channel.

Reserve one dedicated procedural radar voice and a fixed event queue of at
least 32 scheduled pings. A manual scan and automatic alerts share this queue,
which guarantees serialized starts. Automatic close alerts take priority over
medium, new-contact, and manual events, but must not permanently starve a
player-requested scan.

The audio callback reads only fixed atomic parameters. Sorting, identity
matching, threshold state, and event construction remain on the main thread.

## Manual pulse presentation

### Sweep timing and ordering

F3 first plays a subtle centered launch tick that establishes the start of the
snapshot. Schedule retained markers clockwise from straight ahead over an
initial 800 ms revolution.

The target's relative bearing determines its ideal offset in that window.
Markers whose ideal starts would overlap are separated by a minimum start gap
of 45 ms, extending the scan slightly when necessary. This preserves one
marker at a time while retaining approximate angular order.

The scan uses the captured snapshot. Moving after pressing F3 does not reorder
or reposition an already scheduled pulse. Pressing F3 again cancels any
unplayed manual events and replaces them with a fresh snapshot; it must not
cancel a close-range automatic alert already sounding.

### Bearing

Stereo pan uses the same established property-sound pan calculation used by
other spatial accessibility cues. A light amplitude modulation distinguishes
markers behind the player, matching the learned R-Tracker convention.

The clockwise timing is supplemental. Pan and rear modulation remain the
primary bearing cues, particularly when only one marker exists.

### Distance

Map actual horizontal distance to a continuous carrier before applying the
native 4,000-unit clamp:

```text
clamped = min(horizontal_distance, 4000)
proximity = 1 - clamped / 4000
frequency_hz = 650 + 750 * proximity
```

This yields approximately 650 Hz at the radar edge and 1,400 Hz at contact.
Higher remains consistently nearer, matching the virtual cane's distance
direction. Markers beyond 4,000 units use the same 650 Hz outer-edge value
because the visual radar also collapses them onto the edge.

Do not attenuate manual radar pings by world distance. The visual dot remains
equally available at the radar edge; pitch communicates the clamped distance.
Use one conservative configurable radar master volume.

### Height

Mirror the native vertical threshold exactly:

- more than 250 units above: short rising double ping;
- below -250 units: short falling double ping;
- otherwise: one steady ping.

Do not introduce finer height information that the visual radar does not
provide.

### Category timbre

Carrier pitch is reserved for distance, so category should use timbre:

- enemy: bright carrier with a quiet second harmonic;
- ally: pure sine with a softer envelope;
- objective: carrier plus a short delayed octave component;
- other: pure sine with the neutral envelope.

These are prototype choices. They require masking tests against the existing
enemy presence, targeting, R-Tracker, door/person, pickup, cane, and security
camera cues. Do not use category frequencies that reverse or obscure the
distance mapping.

## Automatic enemy-contact alerts

### Configurable thresholds

Initial defaults:

```ini
Accessibility.CombatRadarAudio=1
Accessibility.CombatRadarContactAlerts=1
Accessibility.CombatRadarMediumDistance=2000
Accessibility.CombatRadarCloseDistance=750
Accessibility.CombatRadarVolume=0.20
```

Validation rules:

- both distances must be finite and positive;
- close must be lower than medium;
- invalid or reversed values fall back to documented defaults;
- volume uses the same bounded validation pattern as other procedural cues.

The feature master controls manual and automatic radar audio. The contact-alert
setting controls only automatic events; F3 remains available while contact
alerts are off.

### Contact state machine

Each retained enemy identity has one of these distance bands:

```text
Far -> Medium -> Close
```

Events are generated only on inward transitions:

- absent to present: new-contact alert;
- Far to Medium: medium alert;
- Medium or Far to Close: close alert.

If a newly appearing contact is already inside Medium or Close, emit only the
highest applicable alert. Do not play new, medium, and close events in
sequence.

Enabling alerts, beginning a match, returning from death, or restoring a
temporarily hidden radar establishes a silent baseline from the first complete
frame. This prevents every existing opponent from being announced as new.
Manual F3 is available whenever the player wants that baseline described.

A contact that genuinely disappears for at least 30 logical ticks becomes
absent. Death, cloak, disconnection, or native scenario suppression can
therefore make a later return a new contact. A one-frame render miss must not.
Time spent while the radar is unavailable does not age the disappearance
grace.

### Hysteresis and cooldown

Range alerts rearm only after outward movement beyond a separate threshold:

```text
medium enter: 2000
medium rearm: 2300
close enter: 750
close rearm: 900
```

Derive rearm values as validated defaults or expose them later only if
acceptance testing shows a need. Do not add unnecessary initial settings.

Each identity and threshold also has a three-second minimum repeat cooldown.
Hysteresis is the primary anti-chatter mechanism; cooldown protects against
teleports, noisy coordinates, and repeated scenario replacement.

Crossing outward is silent. Once both the rearm distance and cooldown have
been satisfied, a later inward crossing may alert again. This communicates a
combatant retreating and approaching again without claiming to infer intent or
whether they are actually stalking the player.

### Alert sound

An automatic alert plays one positioned radar ping using the same bearing,
rear, height, distance-pitch, and enemy timbre as a manual snapshot. The
current pitch therefore communicates whether a new contact is far, medium, or
close without adding three unrelated patterns.

Close alerts receive scheduler priority and a slightly stronger envelope, not
a different carrier mapping. Medium alerts use normal gain. New-contact gain
depends only on its current band.

Automatic alerts are one-shot events. They do not create a looping voice, do
not change the existing line-of-sight enemy scanner, and are not suppressed
merely because that enemy is also visible or targetable. The event communicates
a radar range transition; the continuous scanner communicates immediate
combat presence.

## Lifecycle and suppression

Manual pulse and automatic output are allowed only when:

- accessibility and Combat Radar Audio are enabled;
- exactly one local player is active;
- a normal Combat Simulator match is running;
- the native radar completed an available frame for the current player;
- gameplay, rather than a menu, pause screen, cutscene, death overlay, or end
  screen, owns presentation; and
- the player is alive.

Temporary suppression:

- stops the radar voice and clears queued manual sounds;
- preserves the Shift+F3 alert-toggle state;
- freezes contact disappearance timers; and
- silently re-baselines contacts when the native radar returns.

Stage stop, match teardown, accessibility disablement, and shutdown clear
captured markers, identities, thresholds, scheduled events, and mixer state.
They preserve no live prop pointers beyond the current fixed snapshot.

## Logging

Logs should be comprehensive but transition-driven:

- `combat_radar/frame`: availability changes, player, scenario, native option
  flags, total dots, retained dots, and overflow counts;
- `combat_radar/marker`: on manual capture or identity/category change, record
  identity, prop/type, category, team relationship, position, horizontal and
  vertical distance, bearing, native colors, height class, and retention
  decision;
- `combat_radar/command`: F3 pulse, Shift+F3 toggle, modifier state, accepted or
  suppressed reason, snapshot count, and prior/new alert state;
- `combat_radar/contact`: identity admission/removal, silent baseline, previous
  and new bands, thresholds, hysteresis/rearm state, cooldown, and reason;
- `combat_radar/event`: manual/new/medium/close/empty/unavailable event,
  priority, scheduled offset, frequency, gain, pan, rear flag, height pattern,
  and queue result;
- `combat_radar/reset`: lifecycle reason and counts cleared;
- bounded aggregate telemetry for captured frames, commands, contact
  transitions, emitted/dropped events, maximum queue use, and overflow.

Do not log every unchanged marker every rendered frame. Advanced performance
diagnostics may include radar capture and scheduler counters in their existing
fixed frame window.

## Implementation sequence

1. Add `VK_F3` and verify no default game binding or debug command consumes
   scancode 60.
2. Add configuration gates and validated tuning getters in
   `src/accessibility/accessibility.c` and its public header.
3. Create `src/accessibility/accessibility_combat_radar.c` and the matching
   header for fixed snapshots, identity state, input handling, scheduling,
   logging, and lifecycle.
4. Register the new core source in `CMakeLists.txt`.
5. Add a narrow begin/capture/end contract around `radarRender` and
   `radarDrawDot`. Keep native render decisions and output unchanged.
6. Add one dedicated fixed radar voice and atomic publication API under
   `port/src/accessibility/accessibility_tone.c`.
7. Tick input and event scheduling from the established post-`lvTick`
   accessibility gameplay boundary.
8. Reset the adapter and mixer lane before stage memory is disabled.
9. Add effective settings and bounded counters to session-start and optional
   performance logs.
10. Update `ACCESSIBILITY.md`, `ACCESSIBILITY_ARCHITECTURE.md`,
    `ACCESSIBILITY_ROADMAP.md`, and `ACCESSIBILITY_TESTING.md` with the final
    implemented behavior and hook ledger.
11. Build the default `ntsc-final` executable in MinGW64, inspect warnings,
    run static diff checks, then perform runtime and blind-user acceptance.

## Static verification

- `git diff --check` passes.
- F3 is named exactly once in the virtual-key enum and no existing command
  consumes it.
- Shift+F3 cannot fall through to the unmodified F3 action.
- No heap allocation, native property-sound channel, live stage pointer, or
  unbounded queue is introduced.
- Every `radarRender` early return publishes unavailable state.
- The native renderer and all scenario callbacks remain behaviorally
  unchanged.
- The feature compiles with performance diagnostics both off and on.
- Existing R-Tracker, IR/X-Ray, combat, beacon, cane, marker, hazard, targeting,
  toggle, and weapon-function lanes retain independent ownership.

## Runtime test matrix

### Native parity

- Test Radar enabled, `No Radar`, display-option Radar off, and
  `No Player on Radar`.
- Test free-for-all and team matches with human and simulant opponents.
- Test living, dead, respawning, cloaked, and uncloaked combatants.
- Compare every F3 sound with the exact native dots visible in the same frame.
- Confirm no line-of-sight, viewport, room, or path restriction is added.
- Verify the 4,000-unit edge clamp and +/-250-unit height thresholds.
- Exercise Combat, Hold the Briefcase, Hacker Central, Pop a Cap, King of the
  Hill, and Capture the Case scenario additions.

### Manual pulse

- Press and hold F3; only one scan occurs.
- Press F3 rapidly; the newest snapshot replaces unplayed manual events.
- Test zero, one, and the maximum supported number of markers.
- Verify clockwise order, minimum separation, pan, rear modulation,
  distance-pitch direction, height patterns, and category timbres.
- Confirm current-player markers are never sounded.
- Confirm empty and unavailable responses are distinct.
- Confirm F3 works with automatic alerts off.

### Automatic alerts

- Enable and disable with both left and right Shift; no manual pulse leaks
  through the chord.
- On enable and match entry, existing enemies form a silent baseline.
- Spawn, respawn, cloak/uncloak, and remove enemies at Far, Medium, and Close
  distances.
- Move an enemy and the player across both thresholds in both directions.
- Confirm only inward crossings sound.
- Hover around 2,000 and 750 units to verify hysteresis and cooldown.
- Cross directly from Far to Close and admit a new already-close enemy; each
  produces one close event, not stacked events.
- Pause, open menus, die, respawn, end the match, restart, change arenas, and
  disable accessibility; no stale event or mass new-contact alert may occur.

### Masking, stability, and performance

- Combine radar output with the enemy scanner, fine aim, weapon fire, music,
  speech, virtual cane, R-Tracker-compatible devices where possible, beacons,
  markers, and toggle earcons.
- Confirm close alerts remain audible without masking the targeting tone or
  producing clipped output.
- Run dense eight-simulant matches for at least 20 minutes with repeated F3
  pulses, deaths, cloaking, threshold crossings, menus, and match restarts.
- Inspect fixed capacity, maximum queue depth, dropped-event policy, frame
  timing, mixer cost, memory totals, and reset counters.
- Any sustained frame regression, queue growth, allocation, stale identity,
  repeated boundary chatter, or audio choppiness blocks acceptance.

## Acceptance criteria

The slice is ready for project-owner testing when:

- F3 reliably describes exactly the markers on the active native radar;
- Shift+F3 changes only automatic enemy-contact alerts and has clear earcon
  confirmation;
- new, medium, and close events are timely and non-repetitive;
- distance, bearing, rear position, and native above/below state are learnable;
- no output occurs when the visual radar would reveal nothing;
- the system remains bounded and stable in dense long-running matches; and
- the documentation, configuration defaults, hook ledger, logs, and built
  acceptance executable agree.

It should not be described as complete until blind-user testing establishes
that the manual pulse can form a useful spatial snapshot and the automatic
threshold alerts improve awareness without becoming distracting.
