#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <PR/ultratypes.h>
#include "bss.h"
#include "constants.h"
#include "system.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_hud.h"
#include "accessibility/accessibility_log.h"

#define ACCESSIBILITY_HUD_TEXT_MAX 400

struct accessibilityrespawnstate {
	s32 active;
	s32 seconds;
};

static struct accessibilityrespawnstate
		g_AccessibilityRespawnStates[MAX_PLAYERS];
static u32 g_AccessibilityHudObservationId;

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

void accessibilityHudMessageObserved(const char *text, s32 type, u32 flags,
		s32 playernum, s32 channelnum)
{
	char normalized[ACCESSIBILITY_HUD_TEXT_MAX];
	u32 id = g_AccessibilityHudObservationId++;

	if (!accessibilityIsEnabled()) {
		return;
	}

	accessibilityLogEvent("hud", "message_observed",
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

void accessibilityHudRespawnCountdownObserve(s32 visible, const char *prompt,
		s32 seconds, s32 playernum)
{
	struct accessibilityrespawnstate *state;
	char normalized[ACCESSIBILITY_HUD_TEXT_MAX];
	char utterance[ACCESSIBILITY_HUD_TEXT_MAX];
	s32 initial;

	if (playernum < 0 || playernum >= MAX_PLAYERS) {
		return;
	}

	state = &g_AccessibilityRespawnStates[playernum];

	if (!accessibilityIsHudMessagesEnabled() || !visible) {
		if (state->active) {
			accessibilityLogEvent("respawn_countdown", "hidden",
					"player=%d previous_seconds=%d reason=%s",
					playernum, state->seconds,
					visible ? "feature_disabled" : "not_rendered");
		}

		state->active = false;
		state->seconds = -1;
		return;
	}

	if (seconds < 0) {
		seconds = 0;
	}

	initial = !state->active;

	if (!initial && (seconds <= 0 || state->seconds == seconds)) {
		return;
	}

	accessibilityHudNormalizeText(normalized, sizeof(normalized), prompt);
	utterance[0] = '\0';

	if (initial && normalized[0] && seconds > 0) {
		snprintf(utterance, sizeof(utterance), "%s, %d",
				normalized, seconds);
	} else if (initial && normalized[0]) {
		snprintf(utterance, sizeof(utterance), "%s", normalized);
	} else if (seconds > 0) {
		snprintf(utterance, sizeof(utterance), "%d", seconds);
	}

	state->active = true;
	state->seconds = seconds;

	if (utterance[0]) {
		accessibilityAnnouncementRespawnCountdown(utterance, playernum,
				seconds, initial);
		accessibilityLogEvent("respawn_countdown", "announced",
				"player=%d seconds=%d initial=%d text=%s",
				playernum, seconds, initial, utterance);
	}
}

void accessibilityHudReset(const char *reason)
{
	memset(g_AccessibilityRespawnStates, 0,
			sizeof(g_AccessibilityRespawnStates));
	g_AccessibilityHudObservationId = 0;
	accessibilityLogEvent("respawn_countdown", "reset", "reason=%s",
			reason ? reason : "unspecified");
}
