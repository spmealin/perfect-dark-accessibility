# Close-in precision aiming experiment

Date: 2026-10-24. Status: automated fixtures passed; the project owner reported
that precision guidance is working better and requested committing/pushing it.
This is positive blind-user acceptance of the first mapping experiment, not
confirmation of every weapon, target type or zoom configuration.

## Goal and bounded first step

The owner reports precision guidance sounding centered/level while the visible
crosshairs still miss the enemy. The existing screen-normalized linear pan and
wide vertical dead zone can become acoustically neutral well before acquisition.
This first experiment strengthens the audio mapping without changing what is
targetable, enlarging a target, moving aim, or adding collision queries.

- Use a square-root near-center curve for horizontal pan and vertical pitch.
- Reduce the vertical level band tenfold, from 0.025 to 0.0025 viewport
  half-heights. Apply the same small jitter band to horizontal direction.
- Outside the band, retain at least 0.16 horizontal pan and a 0.14 vertical
  pitch exponent. The existing saturation thresholds and pitch endpoints remain.
- Preserve the reference-first/altered-second beep, 100 ms duration and 160 ms
  period, configured enemy carrier/volume and scope-aware viewport inputs.
- Snap the desired precision frequency when entering precision guidance,
  changing above/below direction or entering level. Keep 0.65 smoothing while
  moving within the same vertical direction. Mixer interpolation is unchanged.
- Keep ordinary enemy proximity/elevation and the solid/interrupted targeting
  tones unchanged. Near-but-unconfirmed remains pulsed guidance, not a new lock.

At 1080p the vertical band is approximately 1.35 pixels each way instead of
13.5. A 20-pixel horizontal offset on a 1920-wide viewport maps to roughly
0.31 pan instead of 0.036. These are mapping examples, not physical monitor
inches or a claim about how small an offset a player will distinguish.

Target-size-relative scaling, new acquisition patterns and target/anchor
selection changes are intentionally deferred to keep this listening comparison
small and attributable. A centered guide can still be unconfirmed, and the
existing two-observation character lock-loss grace is unchanged.

## Files and engine boundaries

`src/accessibility/accessibility_targeting.c` owns the mapping, direction-aware
smoothing and rate-limited diagnostics. The test includes that core to exercise
private helpers without exposing test-only APIs. Documentation describes the
experiment in the player guide, feature reference and architecture.

No established engine file, game rule, public contract, input binding, INI key,
ROM/extracted asset, native channel or procedural voice is changed. The work
adds constant-time arithmetic only for existing precision playback, and one
small diagnostic timestamp per existing combat voice. No dynamic storage or
new geometry/refinement query is introduced. This is not a measured FPS claim.

## Automated verification

After configuring, run in MinGW64:

```sh
cc @build/CMakeFiles/pd.dir/includes_C.rsp \
  -DPLATFORM_64BIT=1 -DVERSION=2 -DPAL=0 -D_LANGUAGE_C=1 \
  -DACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=0 -std=c11 -O2 -fwhole-program \
  tools/tests/accessibility_precision_mapping_test.c \
  -o build/accessibility_precision_mapping_test.exe -lm
build/accessibility_precision_mapping_test.exe
```

Fixtures test both directions, dead-zone boundaries, small misses, monotonicity,
clamping, non-finite inputs, uniform resolution scaling and immediate sign/level
transitions. They also execute the unchanged alignment formatter and verify
660/825/528 Hz hostile body/head/arm locks, the protected-arm interrupted pitch,
and the penetrable-glass flag. Guidance helpers cannot start alignment audio.
These do not simulate native firing, establish listening usability, or measure
live game/render frame times.

## Owner listening tests

Use your existing settings; targeting is already enabled. No new toggle is needed.

1. Aim near a stationary nearby guard. Move horizontally from each side until
   acquiring the continuous lock. Can you still hear which way to move during
   the last small correction? Does the stronger curve make you overshoot?
2. Move above/below that guard. Confirm a higher second half still means aim up
   and a lower half means aim down. Does passing through alignment make an
   understandable transition, without a broad misleading level band?
3. Repeat against a distant guard/sniper with and without zoom. Note any case
   where guidance sounds centered but no confirmed lock appears. Target-size
   scaling may be needed if small distant enemies remain ambiguous.
4. Repeat on a turret or camera. They use the same precision mapping, but their
   existing acquisition/visibility and exact-lock rules must remain unchanged.
5. Verify successful hostile locks and head/arm differences still sound familiar,
   then release aim and check ordinary enemy proximity/elevation sounds.

Capture Shift+F2 if guidance still falsely suggests alignment, changes direction
unexpectedly, or encourages repeated overshoot. Report whether the misleading
sound was the pulsed guide or the separate continuous lock, plus capture number,
weapon, zoom state and target type. Prefer a capture immediately before acquiring
the real lock and another just after it for direct comparison.

## Diagnostic and handoff evidence

`targeting/precision_mapping` reports raw normalized horizontal/vertical errors,
the dead zone/minimums, shaped pan, base/desired/smoothed pitch, elevation zone,
anchor source, matching alignment activity and existing aim-loss grace frames.
Records occur on entry/direction changes and approximately once per second.
Existing `precision_refinement`, `combat_slot_cadence`, `alignment_start/update`
and Shift+F2 incident records remain available. No per-frame logging is added.

Build target: Windows x86-64, ntsc-final, RelWithDebInfo, with advanced performance
diagnostics off. The current developer INI has accessibility, logging, speech,
targeting and all scanner categories enabled. Existing session `1792699737`
confirms their effective state at startup; it predates this mapping and is not
runtime acceptance evidence. Its log is preserved rather than overwritten by
an automated game launch. The owner's subsequent report, "That's working better,"
accepts the overall improvement; no specific test matrix, session/capture ID or
frame-time measurement was supplied. Independent-user testing and detailed
jitter/overshoot, weapon, distance and zoom coverage remain open.

## Rollback and risks

Revert only this experiment's core mapping and documentation/tests, or use the
published v0.92 executable for comparison. No settings/save migration is needed.
The small jitter band and minimum directional contrast can expose animation or
anchor changes as stronger direction changes. Continue checking jitter,
overshoot, masking and distant targets during broader testing. Do not tune collision tolerance to make
an ambiguous sound appear correct; any later target-relative mapping should
remain separate from exact hit/lock eligibility.
