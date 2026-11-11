#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_HILL_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_HILL_H

#include <PR/ultratypes.h>

struct prop;

s32 accessibilityObjectiveHasVisibleMarker(s32 scenario, const struct prop *token);
void accessibilityHillTick(void);
void accessibilityHillReset(const char *reason);

#endif
