#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_PATH_BLOCKER_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_PATH_BLOCKER_H

#include "types.h"

bool accessibilityPathBlockerIsBreakable(const struct prop *prop);
bool accessibilityPathBlockerCanTakeGunfire(const struct prop *prop);
bool accessibilityPathBlockerCanTakeExplosion(const struct prop *prop);

#endif
