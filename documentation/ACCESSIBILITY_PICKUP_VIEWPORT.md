# Pickup beacon viewport requirement

Pickup (F8) beacons now require a finite projected model bound intersecting
the current viewport, in addition to existing pickup eligibility, range and
line-of-sight rules. This is checked during scanning, again before playback,
and in incident candidate explanations. Unlike ordinary interactables, pickup
viewport eligibility is not bypassed by an X-Ray exposure flag.

The existing pre-render projection cache now includes eligible pickup props,
including weapons and collectable objects that have no activation semantics.
Failing or stale projection evidence is silent. No renderer hook, new setting,
shortcut, game-pickup behavior or audio pattern changes.

The core changes are in `src/accessibility/accessibility_beacon.c`; the
visibility fixture checks category policy in both diagnostic modes. All 30
regression tests passed. Owner acceptance is pending: turn away from pickups,
look above/below them, then bring any part into view and verify the chirps.
The next experimental 3D-audio change is separate from this visibility fix.
