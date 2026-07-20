#include <stdlib.h>
#include <string.h>
#include <PR/ultratypes.h>
#include "system.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_speech.h"

static char *g_AccessibilityCurrentMenuText;

static const char *accessibilityAnnouncementReasonName(enum accessibility_announcement_reason reason)
{
	switch (reason) {
	case ACCESSIBILITY_ANNOUNCEMENT_DIALOG:
		return "dialog";
	case ACCESSIBILITY_ANNOUNCEMENT_FOCUS:
		return "focus";
	case ACCESSIBILITY_ANNOUNCEMENT_VALUE:
		return "value";
	case ACCESSIBILITY_ANNOUNCEMENT_REPEAT:
		return "repeat";
	}

	return "invalid";
}

s32 accessibilityAnnouncementReplaceMenu(const char *text,
		enum accessibility_announcement_reason reason)
{
	char *copy;
	s32 accepted;
	u64 started;
	u64 elapsed;

	if (!text || !text[0]) {
		accessibilityLogEvent("announcement", "suppressed",
				"group=menu reason=%s decision=empty",
				accessibilityAnnouncementReasonName(reason));
		return 0;
	}

	copy = malloc(strlen(text) + 1);

	if (!copy) {
		accessibilityLogEvent("announcement", "suppressed",
				"group=menu reason=%s decision=allocation_failed bytes=%u",
				accessibilityAnnouncementReasonName(reason), (u32)strlen(text) + 1);
		return 0;
	}

	strcpy(copy, text);
	free(g_AccessibilityCurrentMenuText);
	g_AccessibilityCurrentMenuText = copy;
	started = sysGetMicroseconds();
	accepted = accessibilitySpeechOutput(copy, 1);
	elapsed = sysGetMicroseconds() - started;

	accessibilityLogEvent("announcement", "output_result",
			"group=menu reason=%s interrupt=1 accepted=%d available=%d elapsed_us=%llu text=%s",
			accessibilityAnnouncementReasonName(reason), accepted,
			accessibilitySpeechIsAvailable(), (unsigned long long)elapsed, copy);

	return accepted;
}

void accessibilityAnnouncementCancel(void)
{
	u64 started = sysGetMicroseconds();
	s32 cancelled = accessibilitySpeechCancel();
	u64 elapsed = sysGetMicroseconds() - started;

	accessibilityLogEvent("announcement", "cancel_result",
			"group=menu cancelled=%d available=%d elapsed_us=%llu retained_text=%s",
			cancelled, accessibilitySpeechIsAvailable(),
			(unsigned long long)elapsed,
			g_AccessibilityCurrentMenuText ? g_AccessibilityCurrentMenuText : "");
}

void accessibilityAnnouncementReset(void)
{
	free(g_AccessibilityCurrentMenuText);
	g_AccessibilityCurrentMenuText = NULL;
}
