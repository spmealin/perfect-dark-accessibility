#ifndef PD_RADIALMOUSE_H
#define PD_RADIALMOUSE_H

/* Main-thread, fixed per-player state. Deltas are relative locked-mouse motion
 * scaled by the existing mouse/radial sensitivity settings. */
void radialMouseReset(int player);
void radialMouseUpdate(int player, float dx, float dy, float speed);
/* Returns whether the mouse has moved since open, even after returning neutral.
 * Output uses native stick coordinates (positive Y is up). */
int radialMouseGetStick(int player, int *x, int *y);

#endif
