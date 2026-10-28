# Shared object-surface visibility

## Scope

The F5/F6 scanner now calls the existing
`accessibilityVisibilityHasObjectSurfaceLineOfSight` implementation instead of
maintaining a duplicate surface sampler, transform, camera offset, and four
tolerance constants in `accessibility_beacon.c`. No established engine file or
hook changes. The public function signature is unchanged.

This is intended to preserve behavior, not broaden visibility:

- Origin first, then the nearest model-box face center and four inset points.
- Same transforms, 0.35 inset factor and 0.25-unit camera pull.
- At most six ordinary LOS queries. Embedded-object opt-in adds at most five
  retries: 8 units, or 24 for native render-post-background monitors.
- Embedded retries still require current on-screen render evidence and the
  absence of `OBJFLAG2_INTERACTCHECKLOS`. F5 opts in; F6 does not.
- The scanner retains its independent viewport/cache, category, X-Ray, range,
  door canonicalization and visible-leaf selection rules. This does not merge
  combat's different screen/geometry sampling algorithm into object scanning.
- Door-sibling perimeter exclusion remains in the door adapter, with restoration
  only for perimeters it disabled. The shared LOS query retains its own target
  perimeter handling and the existing visual collision mask/glass semantics.

No settings, shortcuts, speech, audio voices, allocations, or additional collision
queries are introduced. Existing LOS diagnostic sample IDs and counts remain
unchanged.

## Verification and limits

`tools/tests/accessibility_visibility_test.c` first passed against both separate
implementations, then passed unchanged after consolidation. Fixtures cover all
six nearest-face choices, all ordinary success points and total occlusion,
embedded-policy combinations and retry success points, rotation/nonuniform
scale, absent model/bounds, optional counters, pre-disabled target perimeters,
and multi-leaf doors with every initial enabled/disabled combination. They also
check the exact collision masks, sample IDs, query counts, restoration and bounds.
The unified suite runs this in both diagnostics modes (18 tests total).

All 18 tests passed before and after consolidation on 2026-10-27. The standard
Windows x64 `ntsc-final` RelWithDebInfo MinGW64 build also completed, with
performance diagnostics off. Existing upstream compiler warnings were not
changed. The development configuration retains accessibility, logging, targeting
and all four scanner selections enabled; no user settings were edited.

These tests use synthetic collision responses. They do not prove real level
occlusion, glass behavior, viewport projection or player audibility. No runtime
session or blind-user acceptance is claimed by the refactor; existing logs are
historical evidence only. For an optional in-game check, approach a wall-mounted
computer and a partly visible multipart door; confirm unchanged discovery and
that opaque walls still block sound. Capture Shift+F2 if behavior differs and
report the session/capture identifiers and beacon LOS samples/query counts.

Build integration is the standard MinGW64 Windows x64 `ntsc-final` configuration.
Rollback is a revert of this refactor commit; no save/config migration is needed.
