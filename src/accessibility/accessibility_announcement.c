#include <string.h>
#include <PR/ultratypes.h>
#include "system.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_speech.h"

static char g_AccessibilityCurrentMenuText[ACCESSIBILITY_ANNOUNCEMENT_TEXT_MAX];
static s32 g_AccessibilityHasCurrentMenuText;

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
	size_t length;
	s32 accepted;
	u64 started;
	u64 elapsed;

	if (!text || !text[0]) {
		accessibilityLogEvent("announcement", "suppressed",
				"group=menu reason=%s decision=empty",
				accessibilityAnnouncementReasonName(reason));
		return 0;
	}

	length = strlen(text);

	if (length >= sizeof(g_AccessibilityCurrentMenuText)) {
		accessibilityLogEvent("announcement", "suppressed",
				"group=menu reason=%s decision=text_too_long bytes=%u capacity=%u",
				accessibilityAnnouncementReasonName(reason), (u32)length + 1,
				(u32)sizeof(g_AccessibilityCurrentMenuText));
		return 0;
	}

	memcpy(g_AccessibilityCurrentMenuText, text, length + 1);
	g_AccessibilityHasCurrentMenuText = 1;
	started = sysGetMicroseconds();
	accepted = accessibilitySpeechOutput(g_AccessibilityCurrentMenuText, 1);
	elapsed = sysGetMicroseconds() - started;

	accessibilityLogEvent("announcement", "output_result",
			"group=menu reason=%s interrupt=1 accepted=%d available=%d elapsed_us=%llu text=%s",
			accessibilityAnnouncementReasonName(reason), accepted,
			accessibilitySpeechIsAvailable(), (unsigned long long)elapsed,
			g_AccessibilityCurrentMenuText);

	return accepted;
}

s32 accessibilityAnnouncementQueueHud(const char *text, s32 type, u32 flags,
		s32 playernum, s32 channelnum, u32 id)
{
	s32 accepted;
	u64 started;
	u64 elapsed;

	if (!text || !text[0]) {
		accessibilityLogEvent("announcement", "suppressed",
				"group=hud decision=empty id=%u player=%d type=%d",
				id, playernum, type);
		return 0;
	}

	started = sysGetMicroseconds();
	accepted = accessibilitySpeechOutput(text, 0);
	elapsed = sysGetMicroseconds() - started;
	accessibilityLogEvent("announcement", "output_result",
			"group=hud priority=normal interrupt=0 accepted=%d available=%d elapsed_us=%llu id=%u player=%d type=%d flags=0x%08x channel=%d text=%s",
			accepted, accessibilitySpeechIsAvailable(),
			(unsigned long long)elapsed, id, playernum, type, flags,
			channelnum, text);

	return accepted;
}

s32 accessibilityAnnouncementWeaponChange(const char *text,
		const char *source, s32 playernum, s32 interrupt)
{
	s32 accepted;
	u64 started;
	u64 elapsed;

	if (!text || !text[0]) {
		accessibilityLogEvent("announcement", "suppressed",
				"group=weapon_change decision=empty source=%s player=%d",
				source ? source : "unknown", playernum);
		return 0;
	}

	started = sysGetMicroseconds();
	accepted = accessibilitySpeechOutput(text, interrupt != 0);
	elapsed = sysGetMicroseconds() - started;
	accessibilityLogEvent("announcement", "output_result",
			"group=weapon_change priority=normal interrupt=%d accepted=%d available=%d elapsed_us=%llu source=%s player=%d text=%s",
			interrupt != 0, accepted, accessibilitySpeechIsAvailable(),
			(unsigned long long)elapsed, source ? source : "unknown",
			playernum, text);

	return accepted;
}

s32 accessibilityAnnouncementWeaponFunction(const char *text, s32 playernum)
{
	s32 accepted;
	u64 started;
	u64 elapsed;

	if (!text || !text[0]) {
		accessibilityLogEvent("announcement", "suppressed",
				"group=weapon_function decision=empty player=%d", playernum);
		return 0;
	}

	started = sysGetMicroseconds();
	accepted = accessibilitySpeechOutput(text, 0);
	elapsed = sysGetMicroseconds() - started;
	accessibilityLogEvent("announcement", "output_result",
			"group=weapon_function priority=normal interrupt=0 accepted=%d available=%d elapsed_us=%llu player=%d text=%s",
			accepted, accessibilitySpeechIsAvailable(),
			(unsigned long long)elapsed, playernum, text);

	return accepted;
}

s32 accessibilityAnnouncementStatus(const char *text, const char *source,
		s32 playernum, s32 interrupt)
{
	s32 accepted;
	u64 started;
	u64 elapsed;

	if (!text || !text[0]) {
		accessibilityLogEvent("announcement", "suppressed",
				"group=status decision=empty source=%s player=%d",
				source ? source : "unknown", playernum);
		return 0;
	}

	started = sysGetMicroseconds();
	accepted = accessibilitySpeechOutput(text, interrupt != 0);
	elapsed = sysGetMicroseconds() - started;
	accessibilityLogEvent("announcement", "output_result",
			"group=status priority=normal interrupt=%d accepted=%d available=%d elapsed_us=%llu source=%s player=%d text=%s",
			interrupt != 0, accepted, accessibilitySpeechIsAvailable(),
			(unsigned long long)elapsed, source ? source : "unknown",
			playernum, text);

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
			g_AccessibilityHasCurrentMenuText
				? g_AccessibilityCurrentMenuText : "");
}

void accessibilityAnnouncementReset(void)
{
	g_AccessibilityCurrentMenuText[0] = '\0';
	g_AccessibilityHasCurrentMenuText = 0;
}
