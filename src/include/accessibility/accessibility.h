#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_H

#include <PR/ultratypes.h>

void accessibilityInit(void);
void accessibilityShutdown(void);
void accessibilityPerformanceTick(void);
s32 accessibilityIsEnabled(void);
s32 accessibilityIsEnvironmentalHazardsEnabled(void);
s32 accessibilityIsHudMessagesEnabled(void);
s32 accessibilityIsInteractableBeaconsEnabled(void);
s32 accessibilityIsIrScannerAudioEnabled(void);
s32 accessibilityIsNonHostileBeaconsEnabled(void);
s32 accessibilityIsMenuNarrationEnabled(void);
s32 accessibilityIsRTrackerAudioEnabled(void);
s32 accessibilityIsTargetingFeedbackEnabled(void);
s32 accessibilityIsWeaponFunctionCuesEnabled(void);
s32 accessibilityIsXrayScannerAudioEnabled(void);
s32 accessibilityGetVirtualCaneMode(void);
void accessibilitySetVirtualCaneMode(s32 mode);

#endif
