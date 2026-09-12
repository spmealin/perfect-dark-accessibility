#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_LANDMARK_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_LANDMARK_H

struct prop;

s32 accessibilityLandmarkOwnsProp(struct prop *prop);
s32 accessibilityLandmarkIsCompletedProp(struct prop *prop);
void accessibilityLandmarkTick(void);
void accessibilityLandmarkReset(const char *reason);

#endif
