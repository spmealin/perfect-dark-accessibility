#include <stddef.h>
#include <PR/ultratypes.h>
#include "constants.h"
#include "system.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_hud.h"
#include "accessibility/accessibility_log.h"

#define ACCESSIBILITY_HUD_TEXT_MAX 400

static void accessibilityHudNormalizeText(char *dst, size_t dstlen, const char *src)
{
	size_t out = 0;
	s32 pending_space = false;
	const unsigned char *ptr = (const unsigned char *)src;

	if (!dstlen) {
		return;
	}

	dst[0] = '\0';

	if (!ptr) {
		return;
	}

	while (*ptr && out + 1 < dstlen) {
		if (*ptr <= 0x20 || *ptr == 0x7f) {
			pending_space = out > 0;
			ptr++;
			continue;
		}

		if (pending_space && out + 1 < dstlen) {
			dst[out++] = ' ';
			pending_space = false;
		}

		dst[out++] = (char)*ptr++;
	}

	dst[out] = '\0';
}

void accessibilityHudMessageAccepted(const char *text, s32 type, u32 flags,
		s32 playernum, s32 channelnum, u32 id)
{
	char normalized[ACCESSIBILITY_HUD_TEXT_MAX];

	if (!accessibilityIsEnabled()) {
		return;
	}

	accessibilityLogEvent("hud", "message_accepted",
			"id=%u player=%d type=%d flags=0x%08x channel=%d text=%s",
			id, playernum, type, flags, channelnum, text ? text : "");

	if (!accessibilityIsHudMessagesEnabled()) {
		accessibilityLogEvent("hud", "speech_suppressed",
				"id=%u player=%d type=%d reason=feature_disabled text=%s",
				id, playernum, type, text ? text : "");
		return;
	}

	if (type == HUDMSGTYPE_INGAMESUBTITLE
			|| type == HUDMSGTYPE_CUTSCENESUBTITLE) {
		accessibilityLogEvent("hud", "speech_suppressed",
				"id=%u player=%d type=%d reason=subtitle text=%s",
				id, playernum, type, text ? text : "");
		return;
	}

	accessibilityHudNormalizeText(normalized, sizeof(normalized), text);

	if (!normalized[0]) {
		accessibilityLogEvent("hud", "speech_suppressed",
				"id=%u player=%d type=%d reason=empty_after_normalization text=%s",
				id, playernum, type, text ? text : "");
		return;
	}

	accessibilityAnnouncementQueueHud(normalized, type, flags, playernum,
			channelnum, id);
}
