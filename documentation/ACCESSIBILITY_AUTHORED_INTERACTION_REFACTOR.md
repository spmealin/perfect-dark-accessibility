# Authored interaction classification

## Scope

The generic object scanner's only remaining mission-specific action rule was
Battle Shrine's pushable bridge-puzzle rock. Its `STAGE_SKEDARRUINS` / tag `0x4c`
mapping now lives in the existing `accessibility_interaction.c` semantic module
as a stage/tag/prop-type/action-kind registry entry. The new
`accessibilityInteractionGetKind` query returns native or pushable action kind.
It resolves the current tag mapping rather than caching pointers or recognizing
model names. The scanner no longer names a mission or resolves a setup tag.

This is a behavior-preserving refactor, not a new catch-all movable-object scan:

- The confirmed rock remains on F5 with the existing crossing-sweep/low-tail
  movable cue, diagnostic kind/reason strings and scheduling.
- A pushable action remains distinct from normal activation. Its authored
  eligibility still bypasses `OBJFLAG_CANNOT_ACTIVATE` and native interaction
  eligibility, after the existing lifecycle and landmark-ownership gates.
- The registry classifies identity only. Scanner lifecycle, range, projection,
  viewport, line-of-sight, X-Ray, feature-state and category gates stay in place.
- Projection capture, ordinary scanning and incident diagnostics use the same
  semantic query instead of separate stage/tag predicates.
- The rock does not gain or consume an authored-landmark voice. Its pressure
  pad still uses the existing placement/completion test, silences while the rock
  holds the bridge open, and returns if the rock moves away. The rock's F5 role
  is not tied to that pad's completion or position.
- Pelagic II's existing X-Ray switch-state classification stays in the same
  semantic module and is unchanged.

No native game hook, gameplay physics, aim eligibility, configuration, speech,
keyboard shortcut, cue sound, audio channel or collision query changes. The
registry has one bounded entry and allocates nothing. Future confirmed authored
actions can add semantic entries without putting level checks back in discovery.

## Verification

The interaction fixture compares all 216 combinations of six stage values,
four prop types, three current object identities and three live tag mappings
with the old scanner predicate. Additional checks cover null/unavailable tags,
native-activation bypass, invalid backlinks, inactivity, destruction, deleting/
gone/invisible flags, landmark ownership priority, movement-independent role,
and dispatch of the unchanged movable cue. The real landmark fixture confirms
that the rock is not independently owned as a sounding mission landmark.

The existing 40,320 shuffled Pelagic switch assignments and audio/pulse tests
remain in the suite. A new CTest source-contract check prevents direct mission
stage constants and setup-tag lookups from returning to the generic scanner.
The unified suite now has 24 tests, with each compiled fixture run in both
diagnostics modes. These are synthetic checks, not new live geometry, audible
or blind-user acceptance. No new game/log session is claimed.

All 24 tests passed on 2026-10-27, and the standard MinGW64 Windows x64
`ntsc-final` RelWithDebInfo game build completed successfully with diagnostics
off. Existing upstream compiler warnings were left unchanged. Accessibility,
logging, targeting and all four scanner selections remain enabled in the local
configuration; no settings were edited.

For an optional gameplay check, find the Battle Shrine rock using F5, push it
onto the pressure pad and then away. The rock cue should stay distinct from
the pad, and pad completion behavior should be unchanged. Report differences
with Shift+F2 session/capture identifiers and beacon kind, eligibility reason,
projection/LOS and pulse-pattern fields.

Build integration uses MinGW64 Windows x64 `ntsc-final` RelWithDebInfo with
performance diagnostics off. Rollback is a revert of this refactor commit;
there is no save or configuration migration.
