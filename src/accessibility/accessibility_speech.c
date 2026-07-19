#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <PR/ultratypes.h>
#include "system.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_speech.h"
#include "accessibility/accessibility_speech_backend.h"

static s32 g_AccessibilitySpeechInitialized = 0;
static s32 g_AccessibilitySpeechShutdownComplete = 0;
static s32 g_AccessibilitySpeechAvailable = 0;

s32 accessibilitySpeechInit(void)
{
	u64 started;

	if (g_AccessibilitySpeechInitialized) {
		return g_AccessibilitySpeechAvailable;
	}

	g_AccessibilitySpeechInitialized = 1;
	started = sysGetMicroseconds();
	accessibilityLogEvent("speech", "backend_init_start", "platform_backend_init=1");

	accessibilitySpeechBackendInit();
	g_AccessibilitySpeechAvailable = accessibilitySpeechBackendIsAvailable();

	accessibilityLogEvent("speech", "backend_init_stop",
			"available=%d backend=%s elapsed_us=%" PRIu64,
			g_AccessibilitySpeechAvailable,
			accessibilitySpeechBackendGetName(),
			(uint64_t)(sysGetMicroseconds() - started));

	return g_AccessibilitySpeechAvailable;
}

void accessibilitySpeechShutdown(void)
{
	u64 started;

	if (!g_AccessibilitySpeechInitialized || g_AccessibilitySpeechShutdownComplete) {
		return;
	}

	g_AccessibilitySpeechShutdownComplete = 1;
	started = sysGetMicroseconds();
	accessibilityLogEvent("speech", "backend_shutdown_start",
			"available=%d backend=%s",
			g_AccessibilitySpeechAvailable,
			accessibilitySpeechBackendGetName());

	accessibilitySpeechBackendShutdown();
	g_AccessibilitySpeechAvailable = 0;

	accessibilityLogEvent("speech", "backend_shutdown_stop",
			"elapsed_us=%" PRIu64,
			(uint64_t)(sysGetMicroseconds() - started));
}

s32 accessibilitySpeechIsAvailable(void)
{
	return g_AccessibilitySpeechAvailable;
}

const char *accessibilitySpeechGetBackendName(void)
{
	if (!g_AccessibilitySpeechAvailable) {
		return "";
	}

	return accessibilitySpeechBackendGetName();
}

s32 accessibilitySpeechOutput(const char *text, s32 interrupt)
{
	u64 started;
	s32 result;

	if (!g_AccessibilitySpeechInitialized || !g_AccessibilitySpeechAvailable
			|| !text || !text[0]) {
		accessibilityLogEvent("speech", "output_result",
				"accepted=0 reason=%s initialized=%d available=%d text=%s",
				!text ? "null_text" : !text[0] ? "empty_text" : "unavailable",
				g_AccessibilitySpeechInitialized,
				g_AccessibilitySpeechAvailable,
				text ? text : "");
		return 0;
	}

	accessibilityLogEvent("speech", "output_request",
			"backend=%s interrupt=%d utf8_bytes=%llu text=%s",
			accessibilitySpeechBackendGetName(), interrupt != 0,
			(unsigned long long)strlen(text), text);
	started = sysGetMicroseconds();
	result = accessibilitySpeechBackendOutput(text, interrupt != 0);
	accessibilityLogEvent("speech", "output_result",
			"accepted=%d backend=%s interrupt=%d elapsed_us=%" PRIu64 " text=%s",
			result, accessibilitySpeechBackendGetName(), interrupt != 0,
			(uint64_t)(sysGetMicroseconds() - started), text);

	return result;
}

s32 accessibilitySpeechCancel(void)
{
	u64 started;
	s32 result;

	if (!g_AccessibilitySpeechInitialized || !g_AccessibilitySpeechAvailable) {
		accessibilityLogEvent("speech", "cancel_result",
				"cancelled=0 reason=unavailable initialized=%d available=%d",
				g_AccessibilitySpeechInitialized, g_AccessibilitySpeechAvailable);
		return 0;
	}

	accessibilityLogEvent("speech", "cancel_request", "backend=%s",
			accessibilitySpeechBackendGetName());
	started = sysGetMicroseconds();
	result = accessibilitySpeechBackendCancel();
	accessibilityLogEvent("speech", "cancel_result",
			"cancelled=%d backend=%s elapsed_us=%" PRIu64,
			result, accessibilitySpeechBackendGetName(),
			(uint64_t)(sysGetMicroseconds() - started));

	return result;
}
