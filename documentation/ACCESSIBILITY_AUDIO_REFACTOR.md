# Accessibility audio ownership and rendering cleanup

## Ownership

The private `port/src/accessibility/accessibility_tone_ownership.c` module owns
main-thread authorization. A three-state machine replaces scattered checks of
two booleans: gameplay commands allowed, preview transaction commands allowed,
or held preview rejecting gameplay commands. Existing Begin/Commit/Commands/Stop
public APIs retain their behavior; stopping is idempotent and releases ownership.
The preview clear routine continues using the normal guarded setters while
holding an authorized transaction. The audio callback never reads this state.

Every public start/update/stop command uses the same authorization predicate.
The CTest source-contract check verifies this for all mutators in the public
header, excluding only preview lifecycle and read-only diagnostics. A future
mutator missing its gate therefore fails the suite rather than relying solely
on a reviewer remembering the rule.

There are no new settings, strings, shortcuts, voices, dynamic allocations,
audio-thread locks or game hooks. Existing atomic publication, sample deadlines,
waveforms, phase/envelope rules and final clipping are unchanged.

## Verification

Before modifying the backend, the new synthetic PCM fixture recorded fingerprint
`d9543198abbfbb3b` from `d24eb4830`. It runs the existing glossary/menu/PCM checks
and all 93 sound entries with nonzero game audio, variable buffer sizes, and
conflicting gameplay start/stop commands. After the ownership change, both
diagnostic variants match that fingerprint exactly. Explicit ownership tests
cover unauthorized commit, command-window updates, held previews, repeated
begin/replay and repeated stop. Existing glossary tests cover menu focus/close,
feature disable, tail clearing and sample-clock expiry without menu ticks.

All 23 tests passed on 2026-10-27. The fingerprint is specific to this tested
MinGW64 math/toolchain environment; unexpected differences on another toolchain
must be investigated, not silently accepted by regenerating it. These tests are
not concurrent-thread race proofs, FPS benchmarks, listening tests or independent
blind-user acceptance. No game session is launched or claimed by this test pass.

## Next mechanical step

Separate command publication from audio-owned phase/envelope/mixing state into
private implementation units, preserving the same public API and exact sample
operation order. Keep the PCM fixture as a guard; do not introduce dynamic voice
management or change the live sound design as part of that split.

Rollback is a revert of the refactor commits. No save or configuration migration
is needed, and no user choices are changed.
