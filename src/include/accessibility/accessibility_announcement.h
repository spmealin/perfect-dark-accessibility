#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_ANNOUNCEMENT_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_ANNOUNCEMENT_H

#include <PR/ultratypes.h>

enum accessibility_announcement_reason {
	ACCESSIBILITY_ANNOUNCEMENT_DIALOG,
	ACCESSIBILITY_ANNOUNCEMENT_FOCUS,
	ACCESSIBILITY_ANNOUNCEMENT_VALUE,
	ACCESSIBILITY_ANNOUNCEMENT_REPEAT,
};

s32 accessibilityAnnouncementReplaceMenu(const char *text,
		enum accessibility_announcement_reason reason);
void accessibilityAnnouncementCancel(void);
void accessibilityAnnouncementReset(void);

#endif
