#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_H

#include <PR/ultratypes.h>

void accessibilityInit(void);
void accessibilityShutdown(void);
s32 accessibilityIsEnabled(void);
s32 accessibilityIsHudMessagesEnabled(void);
s32 accessibilityIsInteractableBeaconsEnabled(void);
s32 accessibilityIsMenuNarrationEnabled(void);
s32 accessibilityIsTargetingFeedbackEnabled(void);

#endif
