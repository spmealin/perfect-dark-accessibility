#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_VISIBILITY_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_VISIBILITY_H

#include "types.h"

bool accessibilityVisibilityHasVisualLineOfSight(
		struct coord *viewpos, RoomNum *viewrooms,
		struct coord *targetpos, RoomNum *targetrooms,
		struct prop *targetprop);
/* Main-thread geometric LOS only: callers retain category, viewport, device
 * and door-sibling policy. Origin first, then the nearest model face and four
 * inset points. Embedded retries require explicit opt-in plus rendered/native
 * interaction eligibility. Optional counters preserve sample order and budget. */
bool accessibilityVisibilityHasObjectSurfaceLineOfSight(
		struct coord *viewpos, RoomNum *viewrooms,
		struct prop *targetprop, bool allowembedded,
		s32 *sample, s32 *queries);
bool accessibilityVisibilityIsXrayExposed(struct prop *prop);
bool accessibilityVisibilityIsFarsightExposed(struct prop *prop);

#endif
