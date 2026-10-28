# Cane evidence and decision separation

## Scope

This is a behavior-preserving technical-debt pass, not a new cane design.
`accessibility_cane_decision.c` and its value-only header now own the precedence
previously spread across the engine-facing drop, terrain and crouch correction
functions. It accepts the preliminary result and connected-path evidence and
returns a final evaluation, validation statuses, effective barrier distance,
and explicit materialization steps. It has no engine pointers, queries, clocks,
allocation, globals or audio calls.

`accessibility_cane.c` remains responsible for gathering observations, applying
returned positions/contacts, observer-relative heights, and publishing audio and
diagnostics. Its former three correction entry points and repeated engine-side
cue overrides are removed. Diagnostic field copying has one helper shared by
preliminary and final evaluation. No established game hook is added or removed.

The resolver retains the original precedence:

1. Reconcile a selected legacy drop with the connected edge, nearer barrier or
   validated descent. An uncertain validation retains the existing fallback.
2. Where applicable, use connected terrain, flat floor, edge or barrier
   evidence, preserving explicit ladder exclusions and verified-runway rules.
3. Resolve a connected crouch passage or completed crouch dead end, with ladder
   precedence and continuation requirements unchanged.

These stages are now private value-only policy, not adapter mutations that
subsequent engine-facing correction functions must inspect. Explicit steps let
the adapter retain exact collision contacts where the prior policy retained
them, and synthesize a path-step contact only where it did so previously.

## Deliberate limits

Preliminary evaluation still decides whether the existing optional crouch
probe is needed. Removing it would change query cost and observations, so it
is not part of this pass. Legacy geometric sampling remains the fallback for
uncertain/unsupported connected queries. Corridor grouping, multi-ray route
selection, terrain contours, boundary sequences and drop-edge audio scheduling
are unchanged and remain separate from single-ray reconciliation. No thresholds,
ray angles, settings, speech strings or audio characteristics change.

Existing diagnostic semantics are preserved, including compatibility fields
that retain preliminary reasons during certain direct connected overrides.
The new resolver does not silently reinterpret those historical log fields.
With advanced diagnostics enabled, its value-only reconciliation and application
are timed as one evaluation interval rather than separate re-evaluation calls.
This is not a measured FPS improvement: no new geometry calls are introduced,
but runtime performance and usability still need normal gameplay observation.

## Verification

`tools/tests/accessibility_cane_decision_test.c` compares the real new resolver
and adapter against a frozen copy of the old correction functions from
`07b091ec5`, stored under `tools/tests/fixtures/` for tests only. It checks
435,456 combinations per diagnostics mode: unknown/clear/blocked collision,
rise/descent/drop/flat evidence, current-grade and plateau flags, crouch evidence,
ladder geometry and probes, every connected stop/cue, two player radii, contact
placement and short/exact/long runway evidence. It compares every byte of the
initialized output sample, final distance and collision result, including cue,
position, contact metadata, fallback and diagnostic fields. All primary actions
and both crouch actions must be exercised. Current-player and observer-ground
crouch-height branches are both checked. The test clock is deterministic; actual
elapsed timings are not compared.

All 20 tests in the unified suite passed on 2026-10-27 with diagnostics off and
on, including the new differential fixture. These are synthetic policy checks,
not new collision-engine, audible, FPS or independent blind-user acceptance.
No new runtime session is claimed. Existing logs are historical only.

For an optional gameplay regression pass, use familiar stairs/ramp landings,
the known CI crouch passage, a ladder, a real drop and an elevator. They should
sound and behave as before, including uncertain-path fallbacks. Report any
difference with Shift+F2 session/capture identifiers; existing `cane/sweep`
legacy cue, selected cue/source, path, validation and corridor fields remain
available for comparison.

Build integration uses standard MinGW64 Windows x64 `ntsc-final` RelWithDebInfo,
with advanced performance diagnostics off; that build completed successfully.
Existing upstream compiler warnings were left unchanged. The local accessibility,
logging, targeting and scanner selections remain enabled; settings were not edited.
Reverting this refactor commit
restores the previous runtime correction path; there is no save/config migration.
