# Virtual cane acceptance evidence

This file preserves value-only conclusions from local blind-user playtests. It
does not contain raw accessibility logs, ROM data, extracted assets, or player
save data. Capture numbers are meaningful only with their recorded session ID.

## Session 1789056273 — 2026-09-10

Test context:

- Windows x86-64, `ntsc-final`, `RelWithDebInfo`, accessibility branch.
- The log-reported build identifier was `d45ec570c`.
- Fast virtual cane mode was used with the nine-ray fan at
  `-80, -45, -25, -10, 0, 10, 25, 45, 80` degrees.
- Each conclusion below was checked against the three complete structured cane
  sweeps immediately preceding the named Shift+F2 capture.

### Accepted checks

#### CANE-01: flat floor ending at a wall — capture 1

The tester reported that the result sounded correct. Three stable sweeps
classified the center and inner rays as ordinary barriers at approximately
298–349 units. The wider rays described the surrounding room rather than
changing the center route classification.

Result: accepted for this location.

#### CANE-02: open drop — captures 2 and 3

The tester reported that both distances sounded correct. At medium distance,
the center ray placed the edge at approximately 385 units and adjacent rays at
approximately 391–425 units. Close to the edge, the center and adjacent rays
placed it at approximately 51–72 units. All relevant rays consistently
classified the transition as an open drop, with a roughly 600-unit floor-height
loss.

Result: accepted at medium and close range for this location.

#### CANE-03: railing or wall before a drop — capture 4

The tester reported that the result sounded correct. Across three stable
sweeps, the center and most fan rays stopped at the protective barrier within
approximately 30–60 units. The far-right peripheral ray could see a genuine
drop approximately 303–317 units away, but it did not replace the central wall
classification.

Result: accepted for this protected ledge.

#### CANE-04: narrow inaccessible gap — capture 5

The tester reported that the result sounded correct. The center and right-side
rays stopped at nearby barriers, approximately 30–90 units away, rather than
presenting the lower or rising geometry beyond them as a reachable route. The
far-left peripheral ray independently described visible rising terrain.

This differs from CANE-03 in what it guards against: CANE-03 verifies that a
physical railing or wall takes precedence over a real open ledge; CANE-04
verifies that floor probes cannot advertise lower floor or empty space behind
a blocking surface as a traversable drop. They can sound similar to the player
because the correct central result in both cases is a wall.

Result: accepted for this inaccessible gap.

### Open check

#### CANE-05: flat floor, stairs down, landing, then wall — capture 6

The tester heard the stairs but could not clearly perceive the lower landing.
The result was stable across three sweeps, so this is a presentation problem
rather than sweep-to-sweep classification jitter.

The center ray recorded level floor from 0–180 units, descending samples from
210–330 units, and then a terminal wall near 638–639 units. It contained only
one final lower-floor sample before the wall. The inner-left ray recorded level
floor from 0–180 units, a descent from 210–390 units, and repeated the final
lower elevation at 390 and 450 units before a wall near 600 units. Thus at least
one ray contains a real landing, but the center contour does not sustain it and
the mixed sweep does not communicate it clearly enough.

Result: not accepted. The current capture is sufficient to diagnose the issue,
so CANE-05 was removed from the active data-collection checklist. Add a focused
retest after a cue or contour-duration adjustment is ready.

## Session 1789057518 — 2026-09-10

Test context:

- Windows x86-64, `ntsc-final`, `RelWithDebInfo`, accessibility branch.
- The log-reported build identifier was `d45ec570c`.
- Fast virtual cane mode used the nine-ray fan at
  `-80, -45, -25, -10, 0, 10, 25, 45, 80` degrees.
- Each conclusion was checked against the three complete structured cane
  sweeps immediately preceding its Shift+F2 capture.

### Accepted or explained checks

#### CANE-09: ramp up to open space — captures 4 and 5

On approach, the center traces remained level for roughly 210 units before
beginning a stable rise. Halfway up, the inner rays described a continuing
rise ahead, including center traces that rose continuously across the full
450-unit terrain-query range. The tester therefore continued to hear rising
terrain while on the ramp.

This is expected cane feedback, not the removed current-slope status cue. The
status cue reported the surface under the player independently of the fan; the
remaining contour reports the traversable surface still ahead. A player
halfway up a ramp should continue to hear that its forward path rises.

Result: accepted for this location; no separate current-slope cue was present.

#### CANE-10: ramp down to open space — captures 6 and 7

At the start, the inner traces remained level for roughly 270 units and then
descended. Halfway down, the inner traces described the continuing descent;
one ray settled onto a short lower flat near the end. The tester also heard an
extra sound after the contour. The structured results confirm that it was a
terminal wall: the center trace ended at a barrier near 390 units, with other
nearby rays also reporting barriers.

Result: accepted for the captured geometry. The extra sound was the real
terminal-wall component, not a duplicate slope cue.

### Open or inconclusive checks

#### CANE-06: stairs up to an open landing — capture 1

The tester clearly heard the ascent but could not clearly hear the open space
above it. The center trace rose continuously over all of its stored floor
samples, from 0 through 240 units, and did not contain a held upper landing.
It then appended a very distant terminal wall near 913 units. Adjacent rays
also described rising terrain, nearby side barriers, and one distant crouch
candidate.

Result: not accepted. The current data is sufficient to show that the missing
landing is in the sampled/published contour, not merely tester uncertainty.
Revisit landing retention or longer-range terrain sampling before retesting.

#### CANE-07: stairs up to a nearby wall — capture 2

The tester clearly heard the ascent but not the wall. The relevant inner-left
trace held level through approximately 270 units, rose by about 40 units over
the next 60 units, and recorded a terminal wall after that rise. Every other
ray also reported a barrier, so wall detection was stable; the failure is that
the terminal-wall component was not perceptually clear after the contour.

Result: not accepted. Preserve this as a terminal-wall differentiation and
timing problem rather than a collision-query failure.

#### CANE-08: stairs down to open space — capture 3

The center traces clearly recorded a descent of approximately 150 units and
one trace held the lower landing across several samples before a wall. The
tester noted that the location actually contained a wall with a traversable
door rather than unobstructed open space. The capture therefore proves that
the descent and lower landing were sampled, but it is not a clean fixture for
the requested open-space case.

Result: inconclusive for CANE-08. Use a genuinely open lower landing for a
future retest; treat doorway discoverability separately under CANE-20.

#### CANE-12: small traversable step — capture 8

The tester reported many sounds but did not perceive the route as blocked.
Three stable sweeps classified the center and two adjacent rays as an edge
only about 14–15 units away, while the remaining rays emitted a mixture of
terrain, distant crouch, and ordinary barrier results. The immediate edge was
supported by a prop rather than ordinary background floor.

Result: safety intent partially accepted because it did not sound impassable,
but presentation is too busy. The capture is sufficient to investigate
small-step/prop-edge suppression or consolidation before a focused retest.

## Session 1789058773 — 2026-09-10

Test context:

- Windows x86-64, `ntsc-final`, `RelWithDebInfo`, accessibility branch.
- The log-reported build identifier was `d45ec570c`.
- Fast virtual cane mode used the nine-ray fan at
  `-80, -45, -25, -10, 0, 10, 25, 45, 80` degrees.
- Each conclusion was checked against the three complete structured cane
  sweeps immediately preceding its Shift+F2 capture unless noted otherwise.
- Capture 11 was an unassigned extra hoverbike capture. It matched capture 10,
  except that Shift+F2 occurred partway through the final sweep; it was not
  used as evidence for a separate checklist item.

### Accepted checks

#### CANE-13: genuine crouch passage — captures 1, 2, and 3

Standing produced stable crouch-passage cues on the three center rays at
approximately 74–78 units. In crouch and full-squat stances, the same view
produced ordinary wall results instead of repeating the crouch instruction.
The tester reported that all three stances sounded correct.

Result: accepted for this passage and all three walking stances.

#### CANE-14: shallow recess that goes nowhere — capture 4

All nine rays consistently resolved as ordinary barriers, with no crouch cue.
The tester reported that the result sounded correct.

Result: accepted for this dead-end recess.

#### CANE-15: ladder and adjacent wall — captures 5 and 6

Facing the ladder directly produced the ladder sweep on the three center rays
at approximately 190–207 units. With the ladder at the far left, only the
`-80` and `-45` degree rays retained the ladder classification; the remaining
rays reported ordinary walls. The tester reported that both views sounded
correct.

Result: accepted for ladder localization and neighboring-wall separation.

#### CANE-17: carrying a crate — capture 14

The sweep explicitly recorded a nonzero ignored grabbed-prop identity. All
reported obstacles were beyond the carried crate, ranging from approximately
180–500 units, with one open peripheral ray. The tester reported normal route
feedback while carrying it.

Result: accepted; the held crate did not occlude the cane.

#### CANE-19: hoverbike — captures 8, 9, and 10

All captures used the vehicle-relative cane path. Open travel contained several
misses and distant obstacles; facing a wall produced a compact wall fan at
approximately 103–227 units; travelling alongside a left wall produced close
left-side hits and open center/right rays. No walking-only terrain, crouch, or
ladder semantic leaked into any of the three views. The tester reported that
all views sounded correct.

Result: accepted for these open, head-on-wall, and wall-alongside situations.

#### CANE-20: doorway or side opening — capture 13

The center-left ray missed all geometry, establishing open space ahead, while
the far-right ray traced a descending side ramp of roughly 96 units. The other
rays described the surrounding walls at distinct distances. The tester clearly
heard the open route and localized the ramp on the right.

Result: accepted for peripheral opening and side-route discoverability.

### Accepted with a retained edge case

#### CANE-16: elevator — captures 15 through 19

Before movement, during travel, and after arrival, every ray reported ordinary
barriers; elevator motion alone produced no terrain or drop cue. At the
threshold after travel, the center rays still reported distant walls, but the
`+25` degree ray found a 335.54-unit drop only 31.25 units away. Its later
barrier was approximately 340 units away. The tester reported correct elevator
behavior except for this threshold cue and noted that it might represent real
level geometry.

Result: elevator-motion behavior accepted. The single threshold drop cannot be
classified as false from this capture alone; retain it as a location-specific
edge case if it is reproduced from both sides of the stationary threshold.

### Partially accepted check

#### CANE-18: CamSpy perspective — capture 7

The capture and all nearby history samples identify the CamSpy as the remote
observer, and the sweep origin follows its position rather than Joanna. The
tester reported that the wall, doorway, and drop feedback appeared to work.
However, the final three sweeps consistently returned query errors on two
center rays while neighboring rays continued to report terrain and barriers.

Result: perspective switching accepted, but the center-ray errors are a real
reliability issue. Preserve this capture for a focused CamSpy query-failure
investigation before declaring the full geometry test complete.

### Open check

#### CANE-11: ramp or incline ending at a wall — capture 12

The tester heard many elevation changes but no recognizable wall, making the
actual exit ramp difficult to distinguish from dead ends. Seven consecutive
rays described a shallow descent of approximately 21 units, and six of those
contours also contained terminal walls. The far-left ray reported an ordinary
wall and the far-right ray described the descent without a terminal wall.
Results were stable across all three sweeps.

Result: not accepted. This is another terminal-wall differentiation problem,
made worse by nearly the entire fan emitting similar shallow terrain contours.
The collision data contains the walls; the audio presentation does not make
them distinct enough from the viable ramp.

### Incidental performance evidence

This session was not marked as the formal CANE-23 stress test, so these values
are diagnostic context rather than acceptance. Across 1,319 one-second windows,
the connected cane path ran 5,199 times and consumed 168,342 microseconds in
total, approximately 32.4 microseconds per call. The largest single call was
1,016 microseconds, below the 2,000-microsecond time gate. There were no query-
budget stops. Twenty-one query errors occurred; the CamSpy capture establishes
that at least some were remote-observer center-ray failures and should be
investigated with CANE-18. No dedicated slow-frame/gate-trip event was emitted.

## Session 1789060751 — 2026-09-10

Test context:

- Windows x86-64, `ntsc-final`, `RelWithDebInfo`, accessibility branch.
- The log-reported build identifier was `d45ec570c`.
- Fast mode used a 60-tick cycle and Slow mode used a 120-tick cycle.
- No sweep reported a missed cycle.

### Accepted checks

#### CANE-21: inside and outside corners — captures 1, 2, and 3

With an inside corner and a wall on the left, the stable fan progressed from a
near left wall at approximately 83 units through center distances of roughly
261–323 units to right-side distances of approximately 465–688 units. Facing
the inside corner at about 45 degrees produced a compact, stable nine-ray wall
shape at approximately 191–264 units. At the outside corner, the center ray
missed while adjacent rays reached through the opening for approximately
520–957 units and the peripheral rays retained the nearby wall edges. The final
two sweeps were stable; an earlier `-25` degree sample changed as the view
crossed the edge. The tester reported that all three situations sounded
correct.

Result: accepted for inside-corner distance progression and outside-corner
opening detection.

#### CANE-22: Slow versus Fast — captures 4 and 5

Both captures used the exact same player position. Camera direction drifted
only slightly, by less than 0.006 per horizontal look-vector component. The
nine corresponding obstacle distances remained within approximately 2.5 units
between modes. Fast sweeps used 60 ticks and Slow sweeps used 120 ticks, with
the same left-to-right order and no missed cycles. A partial Fast sweep created
while changing modes was excluded; the two following Slow sweeps were complete
and stable. The tester reported that the geometry sounded unchanged.

Result: accepted; the mode changes timing without changing sampled geometry.

### Incidental performance evidence

This short session was not the formal CANE-23 stress test. Across 202 one-second
windows, the connected path layer ran 855 times and consumed 20,063
microseconds in total, approximately 23.5 microseconds per call. The maximum
single call was 171 microseconds, with no budget stops or query errors.
