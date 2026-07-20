#include <inttypes.h>
#include <PR/ultratypes.h>
#include "platform.h"
#include "config.h"
#include "fs.h"
#include "system.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_beacon.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_menu.h"
#include "accessibility/accessibility_speech.h"

static s32 g_AccessibilityEnabledConfig = 1;
static s32 g_AccessibilityLoggingEnabledConfig = 1;
static s32 g_AccessibilitySpeechEnabledConfig = 1;
static s32 g_AccessibilityMenuNarrationEnabledConfig = 1;
static s32 g_AccessibilityInteractableBeaconsEnabledConfig = 1;
static s32 g_AccessibilityInitialized = 0;
static s32 g_AccessibilityEnabled = 0;
static s32 g_AccessibilityShutdownComplete = 0;
static u64 g_AccessibilityStartTimeUs = 0;

void accessibilityInit(void)
{
	if (g_AccessibilityInitialized) {
		return;
	}

	g_AccessibilityInitialized = 1;
	g_AccessibilityEnabled = g_AccessibilityEnabledConfig;
	g_AccessibilityStartTimeUs = sysGetMicroseconds();
	accessibilityBeaconReset("init");

	if (!g_AccessibilityEnabled) {
		return;
	}

	sysLogPrintf(LOG_NOTE, "accessibility: enabled (logging %s, speech %s, interactable beacons %s)",
			g_AccessibilityLoggingEnabledConfig ? "enabled" : "disabled",
			g_AccessibilitySpeechEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityInteractableBeaconsEnabledConfig ? "enabled" : "disabled");

	if (g_AccessibilityLoggingEnabledConfig && accessibilityLogInit()) {
		accessibilityLogEvent("lifecycle", "session_start",
				"enabled=%d logging=%d speech=%d menu_narration=%d interactable_beacons=%d speech_test=%d path=%s",
				g_AccessibilityEnabledConfig,
				g_AccessibilityLoggingEnabledConfig,
				g_AccessibilitySpeechEnabledConfig,
				g_AccessibilityMenuNarrationEnabledConfig,
				g_AccessibilityInteractableBeaconsEnabledConfig,
				sysArgCheck("--accessibility-speech-test"),
				fsFullPath(ACCESSIBILITY_LOG_PATH));
	}

	if (g_AccessibilitySpeechEnabledConfig) {
		accessibilitySpeechInit();

		if (sysArgCheck("--accessibility-speech-test")) {
			if (!accessibilitySpeechIsAvailable()) {
				accessibilityLogEvent("speech", "output_result",
						"accepted=0 reason=test_backend_unavailable");
			} else {
				accessibilitySpeechOutput("Perfect Dark accessibility speech test.", 1);
			}
		}
	} else if (sysArgCheck("--accessibility-speech-test")) {
		accessibilityLogEvent("speech", "output_result",
				"accepted=0 reason=test_speech_disabled");
	}
}

void accessibilityShutdown(void)
{
	u64 elapsed;

	if (!g_AccessibilityInitialized || g_AccessibilityShutdownComplete) {
		return;
	}

	g_AccessibilityShutdownComplete = 1;
	accessibilityBeaconReset("shutdown");
	accessibilityMenuReset();
	accessibilityAnnouncementReset();
	accessibilitySpeechShutdown();

	if (accessibilityLogIsOpen()) {
		elapsed = sysGetMicroseconds() - g_AccessibilityStartTimeUs;

		accessibilityLogEvent("lifecycle", "session_stop",
				"enabled=%d logging=%d elapsed_us=%" PRIu64,
				g_AccessibilityEnabledConfig,
				g_AccessibilityLoggingEnabledConfig,
				(uint64_t)elapsed);
	}

	accessibilityLogShutdown();
	g_AccessibilityEnabled = 0;
}

s32 accessibilityIsEnabled(void)
{
	return g_AccessibilityEnabled;
}

s32 accessibilityIsMenuNarrationEnabled(void)
{
	return g_AccessibilityEnabled && g_AccessibilityMenuNarrationEnabledConfig;
}

s32 accessibilityIsInteractableBeaconsEnabled(void)
{
	return g_AccessibilityEnabled && g_AccessibilityInteractableBeaconsEnabledConfig;
}

PD_CONSTRUCTOR static void accessibilityConfigInit(void)
{
	configRegisterInt("Accessibility.Enabled", &g_AccessibilityEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.LoggingEnabled", &g_AccessibilityLoggingEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.SpeechEnabled", &g_AccessibilitySpeechEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.MenuNarration", &g_AccessibilityMenuNarrationEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.InteractableBeacons", &g_AccessibilityInteractableBeaconsEnabledConfig, 0, 1);
}
