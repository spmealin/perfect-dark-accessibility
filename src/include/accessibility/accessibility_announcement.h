#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_ANNOUNCEMENT_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_ANNOUNCEMENT_H

#include <PR/ultratypes.h>

#define ACCESSIBILITY_ANNOUNCEMENT_TEXT_MAX 12288

enum accessibility_announcement_reason {
	ACCESSIBILITY_ANNOUNCEMENT_DIALOG,
	ACCESSIBILITY_ANNOUNCEMENT_FOCUS,
	ACCESSIBILITY_ANNOUNCEMENT_VALUE,
	ACCESSIBILITY_ANNOUNCEMENT_REPEAT,
};

s32 accessibilityAnnouncementReplaceMenu(const char *text,
		enum accessibility_announcement_reason reason);
s32 accessibilityAnnouncementQueueHud(const char *text, s32 type, u32 flags,
		s32 playernum, s32 channelnum, u32 id);
s32 accessibilityAnnouncementWeaponChange(const char *text,
		const char *source, s32 playernum, s32 interrupt);
s32 accessibilityAnnouncementWeaponFunction(const char *text,
		s32 playernum);
s32 accessibilityAnnouncementRespawnCountdown(const char *text,
		s32 playernum, s32 seconds, s32 initial);
s32 accessibilityAnnouncementStatus(const char *text, const char *source,
		s32 playernum, s32 interrupt);
void accessibilityAnnouncementCancel(void);
void accessibilityAnnouncementReset(void);

#endif
