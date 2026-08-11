#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_HUD_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_HUD_H

#include <PR/ultratypes.h>

void accessibilityHudMessageObserved(const char *text, s32 type, u32 flags,
		s32 playernum, s32 channelnum);
void accessibilityHudRespawnCountdownObserve(s32 visible, const char *prompt,
		s32 seconds, s32 playernum);
void accessibilityHudReset(const char *reason);

#endif
