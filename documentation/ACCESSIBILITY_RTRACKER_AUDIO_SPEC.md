# R-Tracker nonvisual interface specification

## Status and scope

This specification defines an initial nonvisual equivalent for the Perfect Dark
R-Tracker HUD. It covers the semantic markers drawn by
`radarRenderRTrackedProps`: yellow mission/training objects, red tracked
characters, and blue secret-item markers when the native R-Tracker cheat is
active.

The first implementation supports one local player. Multiplayer and cooperative
radar composition remain unvalidated. It does not sonify ordinary multiplayer
players, simulants, buddies, or scenario-specific radar additions because those
are not R-Tracker semantic markers.

## Native behavior that remains authoritative

- The interface is active only while `DEVICE_RTRACKER` is active and not
  inhibited for the current player.
- Yellow objects are eligible whenever `OBJFLAG3_RTRACKED_YELLOW` is set.
- Blue objects are eligible only when `OBJFLAG3_RTRACKED_BLUE` is set and the
  native R-Tracker cheat is active.
- Characters are eligible only while `chr->rtracked` is set, they are not in
  `ACT_DIE` or `ACT_DEAD`, and they are not cloaked.
- Eligibility does not require rendering, line of sight, room connectivity, or
  an unobstructed path.
- Horizontal distance is capped at 4,000 world units for feedback, matching the
  visual radar's 16-pixel radius at 250 units per pixel.
- Native target removal remains authoritative. Accessibility does not infer
  objective completion or reveal names, routes, or hidden classifications.

The native radar and accessibility adapter must call one shared classification
function so their eligibility rules cannot drift.

## State announcements

When the R-Tracker becomes active, speak `R-Tracker on`. When it becomes
inactive, speak `R-Tracker off`.

If it remains active with no eligible markers after a six-logical-tick settling
delay, speak `No tracked targets` once. The delay permits setup scripts to
publish their target immediately after device activation. Do not repeat the
empty-state announcement during the same activation. If a target later appears
or disappears, its audio begins or ends without speech.

These initial accessibility-only phrases are centralized in the R-Tracker
adapter. A future localization pass should assign language IDs and translations
before upstreaming; gameplay hooks must not contain copies of the English text.

## Simultaneous target audio

Ten dedicated, preallocated mixer slots are reserved for R-Tracker markers.
The audited base-game maximum is eight simultaneous markers, including blue
cheat markers. Each semantic identity retains its slot until it becomes
ineligible. New identities fill empty slots in active-prop order. The adapter
logs overflow and suppresses excess markers rather than allocating or stealing
another accessibility feature's lane.

All occupied slots feed one serialized round-robin oscillator lane. Only one
contact phrase can sound at a time, so contacts cannot drift into overlap after
an initially staggered start.

### Category

- Yellow mission/training object: 1450 Hz.
- Red tracked character: 1200 Hz.
- Blue cheat item: 1800 Hz.

Category changes update the assigned voice without changing its identity or
phase.

### Bearing and front/back

Stereo pan uses the engine's established property-sound pan calculation and is
normalized around `AL_PAN_CENTER`. This keeps the R-Tracker's left/right
orientation identical to beacons, hazards, combat cues, and ordinary positioned
game sounds. Pan is refreshed each logical tick and interpolated within audio
buffers.

The horizontal forward dot product distinguishes front from rear. Rear chirps
receive a light 30 Hz amplitude modulation. A +/-0.1 dot-product hysteresis
prevents the timbre from flickering as a marker crosses the player's side.

### Distance

Horizontal distance controls cadence continuously:

```text
clamped = min(horizontal_distance, 4000)
distance_fraction = clamped / 4000
period_ms = 150 + 1250 * distance_fraction^2
volume = 0.35 + 0.65 * (1 - distance_fraction)^2
```

Thus a marker at the player requests approximately 150 ms and a marker at or
beyond the radar edge requests 1400 ms. Gain reaches full tracker volume at the
player and falls to 35 percent at the radar edge. Each slot retains its own due
time. When more than one contact is due, the scheduler advances through them in
round-robin order. A 120 ms event floor leaves room for the longest vertical
phrase and guarantees that phrases do not overlap; contention can delay a due
contact but cannot replace it or make a distant contact inherit a near one's
cadence.

### Relative height

The native visual threshold is 250 world units above or below the player.
Accessibility represents it as:

- above: two 35 ms chirps, first at 90% and second at 110% of category pitch;
- level: one 45 ms chirp at category pitch;
- below: two 35 ms chirps, first at 110% and second at 90%.

The double-chirp gap is 25 ms. Existing above/below state is retained until it
crosses 225 units toward level; level changes only beyond 275 units. This
hysteresis prevents positional jitter without materially changing the native
classification boundary.

## Lifecycle and suppression

Stop all R-Tracker voices on device deactivation, feature disable, pause, menu,
cutscene, player death, unsupported player count, stage teardown, or shutdown.
Temporary suppression does not announce `R-Tracker off` while the native device
remains active. Resuming rebuilds slots from current semantic state.

`Accessibility.RTrackerAudio` defaults to `1` for blind-user acceptance testing
and remains subordinate to `Accessibility.Enabled`. Speech announcements also
require the configured speech backend.

## Logging

Log:

- device activation/deactivation and announcement results;
- scope entry/loss and suppression reason;
- each candidate's pointer, prop number, prop type, category, position, raw and
  clamped distance, height delta, pan, forward dot, rear state, period, and
  frequency;
- slot assignment, update, release, and overflow;
- aggregate scan count, candidate count, occupied voices, and scan duration
  when performance diagnostics are compiled in.

Logs must not change eligibility or speech.

## Performance constraints

- No runtime allocation.
- No game property-sound channel.
- One traversal of `g_Vars.activeprops` per logical tick only while active.
- Ten fixed mixer voices and fixed atomic control fields.
- Mixer work is skipped when no accessibility voice is active.
- Advanced performance diagnostics report active R-Tracker voices alongside
  existing oscillator lanes.

Investigate sustained frame regression, scan time above 0.5 ms, an individual
scan above 2 ms, mixer clipping, stale slots after stage transitions, or any
growth in memory or sound-channel counts.

## Acceptance procedure

1. In CI device training, activate the R-Tracker. Confirm `R-Tracker on`, then a
   single yellow marker for the IR Scanner.
2. Turn in place and verify left/right pan and the clean-front/modulated-rear
   distinction. Approach and retreat to verify the nonlinear 1400-to-150 ms
   cadence and increasing proximity volume.
3. Change floors or use controlled vertical positions to verify rising,
   single, and falling patterns near the native +/-250-unit threshold.
4. Collect the IR Scanner and confirm its voice stops immediately. Deactivate
   the device and confirm `R-Tracker off`.
5. Activate the device where no marker exists and confirm one delayed
   `No tracked targets` announcement with no repetition.
6. In Skedar Ruins, confirm all three yellow pillars sound in round-robin order
   without overlap and each disappears when its target amplifier is placed.
7. On Attack Ship, confirm simultaneous tracked characters and yellow objects,
   and confirm dead or cloaked tracked characters are removed under native
   rules.
8. With the R-Tracker cheat, verify blue secret-item markers use the blue
   category pitch and coexist with yellow/red markers.
9. Exercise pause, menus, death, restart, stage exit, feature disable, and
   repeated sessions. Confirm no stale audio or speech and correlate each
   perceived marker with `rtracker` logs.

Compilation and engineering checks do not complete accessibility acceptance.
Record independent blind-user task evidence in `ACCESSIBILITY_TESTING.md`.
