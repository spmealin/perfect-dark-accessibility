# Open-relative mouse radial selection

## Behavior

The primary local player's captured mouse acts as an eight-direction stick
while a gameplay radial is open. The opening input frame establishes the
neutral anchor; later relative motion is accumulated without clipping, with
existing absolute Mouse Speed X/Y and per-player Radial Menu Speed scaling.
Only output is scaled to native stick range, preserving direction. Returning
to the native 20-unit axis dead zone immediately clears selection after mouse
use; the native 15-sample return timer is not imposed on mouse neutral.
Release applies the current nonempty directional slot, or does nothing at
center. No click, absolute cursor target or automatic weapon choice is added.

Opening-frame deltas are deliberately discarded because they precede the
anchor. Close, reopen, or mouse-capture loss discards the anchor. Changing
radial pages does not move it. Function and order pages share the behavior.
Controller/D-pad-only input retains its existing delay and selection logic;
mixed input still combines the virtual mouse stick with native input, so
neutral mouse does not cancel a held controller/D-pad direction. Mouse input
remains limited to primary player 0, matching the existing PC implementation.

## Implementation boundaries

- `port/src/radialmouse.c` and `port/include/radialmouse.h`: fixed-size
  main-thread state and pure displacement/output math. No allocation, SDL,
  logging, speech, game object or frame-rate-dependent motion.
- `src/game/activemenu.c`: reset exactly when the menu opens.
- `src/game/activemenutick.c`: integrate once per input frame, reuse across
  sampled stick processing, immediate mouse neutral, settle selection before
  release. Existing accessibility focus and weapon speech hooks are unchanged.
- `src/include/types.h`: remove the replaced PC-only mouse accumulator fields.

These changes intentionally modify an existing PC mouse-input behavior at the
owner's request; they are not a fix to native N64/controller gameplay. They do
not depend on accessibility being enabled and introduce no new configuration.
Existing English option labels are reused, with no new user-facing strings.

## Verification and owner test

The `radial_mouse` fixture checks the opening anchor, all eight directions,
neutral, repeated samples, overshoot/reverse without anchor drift, proportional
output saturation, sensitivity, invalid inputs and per-player reset isolation.
It runs with performance diagnostics both off and on. It tests the port math,
not a physical mouse, complete native tick loop or screen-reader usability.
All 30 suite tests passed. The MinGW64 Windows x64 `ntsc-final`
RelWithDebInfo executable rebuilt successfully with performance diagnostics
off; accessibility, logging and menu narration remain enabled in the local INI.

Test with the usual mouse capture:

1. Hold Q or middle mouse; no weapon should be selected initially.
2. Move in each direction. Focus should follow without clicking.
3. Move far out, reverse to your starting point, and release. Your weapon
   should remain unchanged, including if you return and release rapidly.
4. Move to a populated slot and release; it should equip that weapon.
5. Reopen at a different physical position and repeat. Try the function page
   and controller-only selection as regression checks.

No game was launched by the agent and no new session/log acceptance evidence
was generated. Existing `active_menu/snapshot_changed` focus records can be
used to inspect the owner test. Rollback the radial-mouse diff only; aiming
and enemy-beep changes were committed separately as `b9ac61d82`.
