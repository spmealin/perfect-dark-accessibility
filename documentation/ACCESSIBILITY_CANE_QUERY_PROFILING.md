# Structured cane results and profiling

Status: first migration stage implemented on 2026-09-04. Connected,
floor-following corridor queries are still proposed. This stage preserves the
existing collision queries, probe count, nine-angle cadence, and audible policy.

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
- `result_policy=legacy query_mode=existing_samples`: explicitly identifies the
  migration stage. This does not measure the proposed corridor query algorithm.

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
The next implementation stage may add bounded connected support/clearance
observations behind the same contract and compare decisions before adopting
new sound policy. Do not treat successful replay as validation of geometry
that the current sensors never queried.

Rollback: disable diagnostics with the CMake flag and rebuild to remove timers;
set the existing `Accessibility.VirtualCaneMode=0` to silence the cane. Revert
the structured-results commit to restore the former integrated classifier.
