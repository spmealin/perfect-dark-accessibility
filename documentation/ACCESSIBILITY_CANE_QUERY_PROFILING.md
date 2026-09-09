# Structured cane results and profiling

Status: structured results and a bounded connected floor-profile query are
implemented. Since 2026-09-09, the connected result validates only legacy drop
candidates before audio. The existing policy and query cadence remain in use
for every other cue. With performance diagnostics ON, the path query also runs
in shadow mode for non-drop walking samples to retain comparison coverage.

## Contract and boundaries

`src/include/accessibility/accessibility_cane_result.h` defines value-only
observations and results. `src/accessibility/accessibility_cane_result.c`
evaluates them without engine pointers, allocation, collision calls, timing,
logging, or audio. The existing cane adapter acquires geometry, submits an
observation, conditionally checks crouch clearance, and consumes the final
result's cue and source. Raw point-floor probes and live observer details stay
in the existing fixed per-direction record.

Barrier evidence and raised-position clearance use `unknown`, `clear`, and
`blocked`. A raised candidate with zero clearance queries is unknown. A native
query error or a clearance check that stops before the candidate is also
unknown. Only an actual collision establishes blocked clearance. Clear evidence
means the tested positions passed; it does **not** establish a continuous route.

The legacy audible fallback still uses a barrier chirp for an unverified rise.
The old `blocked_rise` field remains for comparison and must not be interpreted
as proof of collision; consult `rise_clearance` and `decision_reasons` instead.
Likewise, `runway` remains the legacy difference between terrain-sample and
collision-contact distances, not proven player travel distance.

Every `cane/sweep` sample adds `evidence_version:1`, `barrier_evidence`,
`barrier_distance`, `rise_clearance`, `decision_reasons`, `selected_cue`, and
`selected_source`. Cue values are 0 none, 1 barrier, 2 terrain, 3 drop, 4 crouch,
5 ladder. Source values are 0 none, 1 barrier, 2 terrain. Pending/skipped slots
retain unknown evidence and no selection.

Reason bits are: 0x001 barrier-query error; 0x002 rise untested; 0x004 clearance
error; 0x008 clearance collision; 0x010 clearance incomplete; 0x020 current-grade
continuation; 0x040 traversable plateau; 0x080 short runway; 0x100 drop adjacent
to barrier; 0x200 crouch/terrain merge. Reasons may be combined.

## Profiling

Enable the existing CMake flag `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON` through
MinGW64. It remains OFF by default in source, and is ON in the acceptance build
prepared for this change. No new INI key or shortcut is required. Existing
accessibility and logging settings remain effective.

One `performance/cane_window` record is emitted per performance window:

- `tick_calls`, `tick_total_us`, `tick_max_us`: entire cane ticks, including
  scheduling, query preparation, decisions, command publication and sweep logs.
- `query_calls`, `observation_total_us`, `evaluation_total_us`: existing
  observation work versus value-only result evaluation. These are components
  of tick time, not additional costs to add to tick time.
- `publish_total_us`: selected-output handling and synchronous tone command
  publication, excluding audio-buffer synthesis.
- `sweep_log_total_us`, `sweep_log_max_us`: formatting and submitting sweep logs.
  Stage/reset logs may occur outside the tick, so this is independently measured.
- `rise_unknown`, `rise_blocked`, `rise_clear`, `query_errors`: evidence counters.
- `result_policy=legacy_with_validated_drops
  query_mode=existing_plus_connected_drop`: identifies the audible
  query/decision path. Shadow cost is separately reported below; full tick and
  adapter-group timings include it.

Samples also include `evaluation_us` and `publish_us`. Existing `query_us`
continues to cover the observation/evaluation interval before publication.
All new timers and window counters compile out with diagnostics OFF. All
window maxima reset each window, unlike the older lifetime `cane_query_max_us`.

`performance/gameplay_accessibility_window` times the existing post-`lvTick`
adapter group from beacons through incident capture. It includes cane ticks
and synchronous logging in that group, and reports calls, total/max microseconds,
and percentage of the window's wall time. It excludes accessibility hooks
inside game ticks/rendering (including some targeting/menu work), the audio
mixer, and the performance observer itself. It is **not** total accessibility
CPU cost and is not an FPS-loss percentage. Never add cane time to this inclusive
group time. Correlate both with existing frame, targeting, and graphics windows.

## Evidence and next test

Session `1788562366`, sweep 1460 at dump 2, detected a 26.94-unit rise at 423.64
units, with a collision at 424.64. Zero clearance tests ran. The structured
fixture requires unknown clearance plus short-runway/untested reasons while
preserving the existing barrier sound.

The standalone tests require no ROM or game launch. From MinGW64 at repo root:

```sh
gcc -std=c99 -Wall -Wextra -Werror -Isrc/include tools/tests/accessibility_cane_result_test.c src/accessibility/accessibility_cane_result.c -o build/cane_result_test.exe
build/cane_result_test.exe
```

Then from PowerShell:

```powershell
./tools/tests/replay_cane_results.ps1
```

The replay exports observations from the local log to the test process and
compares cue, source, suppression and legacy blocked-rise decisions. No log or
ROM data is committed. The captured session replayed 14,718 samples with zero
mismatches; focused fixtures cover unknown/error/blocked/clear evidence, current
grade, plateau, drops, crouch merge and ladder priority.

That session's original positive query timings averaged 16.327 microseconds,
with p95 27, p99 42, and maximum 583 microseconds. These exclude old sweep-log
and audio costs and are not measurements of this build.

For the next gameplay run, revisit dump 2 and test stairs, short ledges, ramps
ending at walls, crouch passages and lift transitions. Compare Slow/Fast/Off on
the same route. Check new per-window mean and maximum tick/group costs against
frame gaps, query counts, skipped samples and actual sounds. An individual
query over 2 ms or average query over 0.5 ms remains an investigation trigger;
also investigate logging/tick spikes even if collision work is cheap. A capped
60 FPS reading alone does not establish negligible CPU overhead.

Gameplay, speech, and blind-user acceptance of this refactor remain pending.
The shadow prototype below compares connected support/clearance observations
before adopting new sound policy. Do not treat successful replay as validation
of geometry that the audible sensors never queried.

Rollback: disable diagnostics with the CMake flag and rebuild to remove timers;
set the existing `Accessibility.VirtualCaneMode=0` to silence the cane. Revert
the structured-results commit to restore the former integrated classifier.

## Connected floor-profile query and narrow audible adoption

`accessibility_cane_path.c` is a portable, allocation-free bounded evaluator
with injected floor and movement queries. `accessibility_cane_path_query.c`
adapts it to native collision APIs. No additional INI switch, key, earcon,
speech, gameplay hook, or player movement is introduced. With diagnostics OFF,
it runs only when the legacy classifier selects a drop. Diagnostics ON runs it
for every scheduled walking sample and enables comparison logs and timers.

The prototype samples forward at one player-radius spacing, relative to the
last connected floor rather than the original player's height. It combines
footprint support with point support to distinguish stairs descending through
more than the drop threshold in total from a single abrupt edge. Five bounded
bisections refine an edge. Each accepted step also passes current-stance
clearance; failed clearance may retry native-height approximations for crouch
and squat. Vertical movement tests the swept body envelope, and horizontal
movement uses the existing collision helpers and destination-volume test.
High rises above the current cylinder's foot clearance trigger finer sampling
or an uncertain result, not an invented traversable route.

The current physical floor is a fresh reference on every query. Supporting
lift/prop identity and floor heights are retained in the trace, so elevator
motion can be compared without treating motion since the previous sweep as a
drop. Walking stances are covered. CamSpy, riding and grabbed-object modes are
explicitly unsupported; their existing audible cane result is retained as the
conservative fallback. Observer perimeter and slope state are restored on
every exit. Collision scratch is main-thread-only, as in the existing adapter.

Limits per scheduled direction are 32 stored nodes, five refinements per
candidate, 192 reserved query operations, and an elapsed-time gate of 2,000 us.
A room-resolution batch (portal traversal plus entered-room expansion) counts
as one operation. Floor and collision calls each count separately. Time is
checked before operations; a single native call can exceed the gate, so this
is not a hard real-time deadline. No catch-up or additional audio is scheduled.
Budget/error/capacity termination does not establish clear space beyond the
last proven node. Fixed storage avoids allocation churn.

`cane/path_shadow` remains the separate diagnostic aggregate per sweep, joined to `cane/sweep`
by session and `id`, then by sample slot. Each slot records proposed and legacy
cue, final audible cue, validation outcome, stop, direction, cue/stop distances,
reached distance, final floor delta, edge
bracket width, required height, counts, budget, errors and elapsed `us`.
Nodes are `(distance, footprint ground, point ground, room, flags, supporting
prop pointer, body height)`. Last queried distance/floors/clearance and blocker
pointer retain evidence from a failed attempt. Pointer zero can mean background
geometry; it does not prove clear space.

Stop values: 0 not run, 1 reached range, 2 wall, 3 edge, 4 uncertain, 5 node
capacity, 6 unsupported context. Clearance uses the existing evidence enum:
0 unknown, 1 clear, 2 blocked. Budget is 0 none, 1 query cap, 2 time gate.
Cue numbers match the existing structured result. A proposed terrain/crouch
cue can coexist with a later wall stop: these are separate observations, not
a claim that the entire corridor is open.

Only an existing drop candidate consumes the path result audibly:

- a refined path edge preserves the drop contour at the refined edge;
- a wall whose sampled stop is no farther than the legacy edge replaces the
  drop with the ordinary barrier cue at that wall;
- a connected descending profile that reaches the full range, or proves a
  point beyond the legacy edge before a later wall, replaces the drop with the
  ordinary downward-terrain contour;
- unsupported, uncertain, capacity-limited, budget-limited, conflicting, and
  query-error outcomes preserve the legacy drop.

This does not adopt the path evaluator's ordinary barrier, ascent, crouch, or
ladder policy. `cane/sweep` records `drop_validation`, original distance and
height. The outcomes are `confirmed_edge`, `barrier_first`,
`connected_descent`, `fallback`, and `not_run`.

`performance/cane_path_window` reports shadow calls, total/max microseconds,
query operations, budget stops, errors, and cue differences. These costs are
already included in full cane ticks and the inclusive adapter-group timing;
do not add them again. `query_us` and observation timing remain legacy-only.
Sweep logging timing now includes formatting/submitting both aggregate records.
Differences are diagnostic, not automatic regressions: the sampled reach and
the experimental policy differ from the production barrier policy.

Standalone fixtures (no ROM required), from MinGW64:

```sh
gcc -std=c99 -Wall -Wextra -Werror -Isrc/include tools/tests/accessibility_cane_path_test.c src/accessibility/accessibility_cane_path.c -o build/cane_path_test.exe
build/cane_path_test.exe
```

Fixtures cover flat floor, cumulative stair ascent/descent, refined drops,
walls before drops, stairs beneath a lower passage, already-low stance,
changing lift reference heights, query failure and node limits. Synthetic
fixtures do not validate the native geometry adapter. Radius-spaced support
can miss very narrow gaps between samples; unusual step flags, overlapping
floors, thin ceilings, small-Jo cheats and moving support boundaries still
need engine-backed testing. This is not the game's complete movement solver.

Pre-adoption session `1788739906` supplied 6,209 completed native shadow calls
and 375 legacy drop decisions: 138 confirmed edges, 87 paths stopped at a
barrier, and 150 followed connected descending terrain. Average shadow cost was
21.48 us, maximum 196 us, with no budget stops and one error among 332,817
reserved operations. This supports the narrow trial; it is not post-adoption
acceptance. The ntsc-final x86_64 RelWithDebInfo MinGW64 build uses diagnostics
ON; pure fixtures and diagnostics-OFF syntax checks pass. Existing configuration
has accessibility/logging/speech enabled and Fast cane mode, terrain reach
450, rise threshold 12 and drop threshold 80. Confirm effective session-start
values in the tester's next run; native geometry and performance acceptance
remain pending.

Post-adoption session `1788970498` supplied the first sustained runtime evidence
at genuine large edges. Near the opening of DataDyne Central: Defection, the
connected query repeatedly confirmed the 600-unit drops while the player
approached from roughly 400 units to within single-digit units of an edge. For
the center ray in the first observed sweep, the legacy estimate was 383.20
units and the connected refinement was 384.84 units. Across the full session,
201 ray decisions confirmed an edge, 35 found a barrier first, 24 proved a
connected descent, and five conservatively fell back. The 6,723 connected calls
used 384,266 bounded operations in 148,058 us total (22.02 us mean, 1,066 us
maximum), with no budget stops or query errors. No call crossed the documented
2 ms investigation threshold. This supports retaining the narrow connected
drop validator without changing its thresholds; it does not yet accept the
ordinary terrain policy.

Next test: repeat stairs up/down, an incline ending at a wall, low passages in
all three stances, and stationary elevator rides. Capture
Shift+F2 at ambiguous places. Compare shadow reached distance/stop/support
identity and unknown/budget counts with the audible record. Compare the same
route in Fast, Slow and Off to assess added cost. Investigate average shadow
query over 0.5 ms, any over 2 ms, frequent budget stops, log/tick spikes or new
frame gaps. Disabling the CMake flag removes broad shadow comparison and its
timers/logs, but deliberately retains connected validation of actual drop
candidates. Set `Accessibility.VirtualCaneMode=0` to silence the feature, or
revert the connected-drop adoption to restore the prior drop policy.
