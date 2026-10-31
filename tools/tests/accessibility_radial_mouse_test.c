/* Port input math only; no game, SDL device or screen reader is launched. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "radialmouse.h"

static void stick(int player, int active, int expectedx, int expectedy)
{
	int x, y;
	assert(radialMouseGetStick(player, &x, &y) == active);
	assert(x == expectedx && y == expectedy);
}

int main(void)
{
	int dx, dy, x, y;
	for (dx = -1; dx <= 1; dx++) {
		for (dy = -1; dy <= 1; dy++) {
			radialMouseReset(0);
			radialMouseUpdate(0, 999, 999, 1); /* Opening frame anchors here. */
			stick(0, 0, 0, 0);
			radialMouseUpdate(0, dx * 30, dy * 30, 1);
			stick(0, dx != 0 || dy != 0, dx * 30, -dy * 30);
			/* Held still preserves direction for all controller samples. */
			stick(0, dx != 0 || dy != 0, dx * 30, -dy * 30);
			radialMouseUpdate(0, -dx * 30, -dy * 30, 1);
			stick(0, dx != 0 || dy != 0, 0, 0);
		}
	}
	radialMouseReset(0);
	radialMouseUpdate(0, 0, 0, 1);
	radialMouseUpdate(0, 1000, 500, 1);
	stick(0, 1, 127, -63); /* Direction survives output saturation. */
	radialMouseUpdate(0, -1000, -500, 1);
	stick(0, 1, 0, 0); /* Unlike clipped accumulation, exact anchor survives. */
	radialMouseUpdate(0, .25f, -.25f, 4);
	stick(0, 1, 1, 1);
	radialMouseUpdate(0, -.25f, .25f, 4);
	stick(0, 1, 0, 0);
	radialMouseUpdate(0, NAN, INFINITY, 1);
	radialMouseUpdate(0, 100, 100, 0);
	radialMouseUpdate(0, 100, 100, -1);
	stick(0, 1, 0, 0);
	radialMouseReset(1);
	radialMouseUpdate(1, 0, 0, 1);
	radialMouseUpdate(1, -40, 30, 1);
	stick(1, 1, -40, -30);
	stick(0, 1, 0, 0);
	radialMouseReset(1);
	stick(1, 0, 0, 0);
	radialMouseReset(-1);
	radialMouseUpdate(99, 10, 10, 1);
	assert(!radialMouseGetStick(99, &x, &y) && x == 0 && y == 0);
	puts("Radial mouse: anchor, eight directions, overshoot/return, held samples, sensitivity and reset passed.");
	return 0;
}
