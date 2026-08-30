#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_VISIBILITY_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_VISIBILITY_H

#include "types.h"

bool accessibilityVisibilityHasVisualLineOfSight(
		struct coord *viewpos, RoomNum *viewrooms,
		struct coord *targetpos, RoomNum *targetrooms,
		struct prop *targetprop);
bool accessibilityVisibilityIsXrayExposed(struct prop *prop);
bool accessibilityVisibilityIsFarsightExposed(struct prop *prop);

#endif
