#include <inttypes.h>
#include <math.h>
#include <PR/ultratypes.h>
#include "platform.h"
#include "config.h"
#include "fs.h"
#include "system.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_beacon.h"
#include "accessibility/accessibility_cane.h"
#include "accessibility/accessibility_hazard.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_menu.h"
#include "accessibility/accessibility_speech.h"
#include "accessibility/accessibility_targeting.h"
#include "accessibility/accessibility_tracker.h"
#include "accessibility/accessibility_weapon.h"

static s32 g_AccessibilityEnabledConfig = 1;
static s32 g_AccessibilityLoggingEnabledConfig = 1;
static s32 g_AccessibilitySpeechEnabledConfig = 1;
static s32 g_AccessibilityMenuNarrationEnabledConfig = 1;
static s32 g_AccessibilityHudMessagesEnabledConfig = 1;
static s32 g_AccessibilityEnvironmentalHazardsEnabledConfig = 1;
static s32 g_AccessibilityInteractableBeaconsEnabledConfig = 1;
static s32 g_AccessibilityIrScannerAudioEnabledConfig = 1;
static s32 g_AccessibilityNonHostileBeaconsEnabledConfig = 1;
static s32 g_AccessibilityRTrackerAudioEnabledConfig = 1;
static s32 g_AccessibilityTargetingFeedbackEnabledConfig = 1;
static s32 g_AccessibilityWeaponFunctionCuesEnabledConfig = 1;
static s32 g_AccessibilityXrayScannerAudioEnabledConfig = 1;
static s32 g_AccessibilityVirtualCaneModeConfig = 1;
static f32 g_AccessibilityVirtualCaneReachConfig = 900.0f;
static f32 g_AccessibilityVirtualCaneFullVolumeDistanceConfig = 112.5f;
static f32 g_AccessibilityVirtualCaneFadeDistanceConfig = 750.0f;
static f32 g_AccessibilityVirtualCaneMaximumAudibleDistanceConfig = 975.0f;
static f32 g_AccessibilityEnemyFullVolumeDistanceConfig = 600.0f;
static f32 g_AccessibilityEnemyFadeDistanceConfig = 3500.0f;
static f32 g_AccessibilityEnemyMaximumDistanceConfig = 4000.0f;
static f32 g_AccessibilityEnemyVolumeConfig = 0.14f;
static s32 g_AccessibilityInitialized = 0;
static s32 g_AccessibilityEnabled = 0;
static s32 g_AccessibilityShutdownComplete = 0;
static u64 g_AccessibilityStartTimeUs = 0;

static f32 accessibilityValidatedFloat(f32 value, f32 fallback,
		f32 minimum, f32 maximum)
{
	if (!isfinite(value)) {
		return fallback;
	}

	if (value < minimum) {
		return minimum;
	}

	if (value > maximum) {
		return maximum;
	}

	return value;
}

void accessibilityGetVirtualCaneTuning(f32 *reach, f32 *fulldistance,
		f32 *fadedistance, f32 *silentdistance)
{
	f32 effectiveReach = accessibilityValidatedFloat(
			g_AccessibilityVirtualCaneReachConfig, 900.0f, 100.0f, 5000.0f);
	f32 effectiveFull = accessibilityValidatedFloat(
			g_AccessibilityVirtualCaneFullVolumeDistanceConfig,
			112.5f, 0.0f, 10000.0f);
	f32 effectiveFade = accessibilityValidatedFloat(
			g_AccessibilityVirtualCaneFadeDistanceConfig,
			750.0f, 0.0f, 10000.0f);
	f32 effectiveSilent = accessibilityValidatedFloat(
			g_AccessibilityVirtualCaneMaximumAudibleDistanceConfig,
			975.0f, 1.0f, 10000.0f);

	if (effectiveFade < effectiveFull) {
		effectiveFade = effectiveFull;
	}

	if (effectiveSilent < effectiveFade) {
		effectiveSilent = effectiveFade;
	}

	if (effectiveSilent < effectiveReach) {
		effectiveSilent = effectiveReach;
	}

	if (reach) {
		*reach = effectiveReach;
	}
	if (fulldistance) {
		*fulldistance = effectiveFull;
	}
	if (fadedistance) {
		*fadedistance = effectiveFade;
	}
	if (silentdistance) {
		*silentdistance = effectiveSilent;
	}
}

void accessibilityGetEnemyTuning(f32 *fulldistance, f32 *fadedistance,
		f32 *silentdistance, f32 *volume)
{
	f32 effectiveFull = accessibilityValidatedFloat(
			g_AccessibilityEnemyFullVolumeDistanceConfig,
			600.0f, 0.0f, 20000.0f);
	f32 effectiveFade = accessibilityValidatedFloat(
			g_AccessibilityEnemyFadeDistanceConfig,
			3500.0f, 0.0f, 20000.0f);
	f32 effectiveSilent = accessibilityValidatedFloat(
			g_AccessibilityEnemyMaximumDistanceConfig,
			4000.0f, 1.0f, 20000.0f);
	f32 effectiveVolume = accessibilityValidatedFloat(
			g_AccessibilityEnemyVolumeConfig, 0.14f, 0.0f, 0.25f);

	if (effectiveFade < effectiveFull) {
		effectiveFade = effectiveFull;
	}

	if (effectiveSilent < effectiveFade) {
		effectiveSilent = effectiveFade;
	}

	if (fulldistance) {
		*fulldistance = effectiveFull;
	}
	if (fadedistance) {
		*fadedistance = effectiveFade;
	}
	if (silentdistance) {
		*silentdistance = effectiveSilent;
	}
	if (volume) {
		*volume = effectiveVolume;
	}
}

void accessibilityInit(void)
{
	f32 caneReach;
	f32 caneFull;
	f32 caneFade;
	f32 caneMaximum;
	f32 enemyFull;
	f32 enemyFade;
	f32 enemyMaximum;
	f32 enemyVolume;

	if (g_AccessibilityInitialized) {
		return;
	}

	g_AccessibilityInitialized = 1;
	g_AccessibilityEnabled = g_AccessibilityEnabledConfig;
	g_AccessibilityStartTimeUs = sysGetMicroseconds();
	accessibilityBeaconReset("init");
	accessibilityCaneReset("init");
	accessibilityHazardReset("init");
	accessibilityTargetingReset("init");
	accessibilityTrackerReset("init");
	accessibilityWeaponFunctionReset("init");

	if (!g_AccessibilityEnabled) {
		return;
	}

	accessibilityGetVirtualCaneTuning(&caneReach, &caneFull,
			&caneFade, &caneMaximum);
	accessibilityGetEnemyTuning(&enemyFull, &enemyFade,
			&enemyMaximum, &enemyVolume);

	sysLogPrintf(LOG_NOTE, "accessibility: enabled (logging %s, speech %s, HUD messages %s, environmental hazards %s, interactable beacons %s, IR Scanner audio %s, non-hostile beacons %s, R-Tracker audio %s, targeting feedback %s, weapon function cues %s, X-Ray Scanner audio %s, virtual cane mode %d, cane reach %.1f, enemy maximum %.1f, enemy volume %.3f)",
			g_AccessibilityLoggingEnabledConfig ? "enabled" : "disabled",
			g_AccessibilitySpeechEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityHudMessagesEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityEnvironmentalHazardsEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityInteractableBeaconsEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityIrScannerAudioEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityNonHostileBeaconsEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityRTrackerAudioEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityTargetingFeedbackEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityWeaponFunctionCuesEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityXrayScannerAudioEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityVirtualCaneModeConfig, caneReach,
			enemyMaximum, enemyVolume);

	if (g_AccessibilityLoggingEnabledConfig && accessibilityLogInit()) {
		accessibilityLogEvent("lifecycle", "session_start",
				"enabled=%d logging=%d speech=%d menu_narration=%d hud_messages=%d environmental_hazards=%d interactable_beacons=%d ir_scanner_audio=%d non_hostile_beacons=%d rtracker_audio=%d targeting_feedback=%d weapon_function_cues=%d xray_scanner_audio=%d virtual_cane_mode=%d cane_reach=%.3f cane_full_distance=%.3f cane_fade_distance=%.3f cane_maximum_audible_distance=%.3f enemy_full_distance=%.3f enemy_fade_distance=%.3f enemy_maximum_distance=%.3f enemy_volume=%.4f performance_diagnostics=%d performance_interval_us=%d speech_test=%d path=%s",
				g_AccessibilityEnabledConfig,
				g_AccessibilityLoggingEnabledConfig,
				g_AccessibilitySpeechEnabledConfig,
				g_AccessibilityMenuNarrationEnabledConfig,
				g_AccessibilityHudMessagesEnabledConfig,
				g_AccessibilityEnvironmentalHazardsEnabledConfig,
				g_AccessibilityInteractableBeaconsEnabledConfig,
				g_AccessibilityIrScannerAudioEnabledConfig,
				g_AccessibilityNonHostileBeaconsEnabledConfig,
				g_AccessibilityRTrackerAudioEnabledConfig,
				g_AccessibilityTargetingFeedbackEnabledConfig,
				g_AccessibilityWeaponFunctionCuesEnabledConfig,
				g_AccessibilityXrayScannerAudioEnabledConfig,
				g_AccessibilityVirtualCaneModeConfig,
				caneReach, caneFull, caneFade, caneMaximum,
				enemyFull, enemyFade, enemyMaximum, enemyVolume,
				ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS,
				ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS ? 1000000 : 0,
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
	accessibilityCaneReset("shutdown");
	accessibilityHazardReset("shutdown");
	accessibilityTargetingReset("shutdown");
	accessibilityTrackerReset("shutdown");
	accessibilityWeaponFunctionReset("shutdown");
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

s32 accessibilityIsHudMessagesEnabled(void)
{
	return g_AccessibilityEnabled && g_AccessibilityHudMessagesEnabledConfig;
}

s32 accessibilityIsEnvironmentalHazardsEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityEnvironmentalHazardsEnabledConfig;
}

s32 accessibilityIsInteractableBeaconsEnabled(void)
{
	return g_AccessibilityEnabled && g_AccessibilityInteractableBeaconsEnabledConfig;
}

s32 accessibilityIsIrScannerAudioEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityIrScannerAudioEnabledConfig;
}

s32 accessibilityIsNonHostileBeaconsEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityNonHostileBeaconsEnabledConfig;
}

s32 accessibilityIsRTrackerAudioEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityRTrackerAudioEnabledConfig;
}

s32 accessibilityIsTargetingFeedbackEnabled(void)
{
	return g_AccessibilityEnabled && g_AccessibilityTargetingFeedbackEnabledConfig;
}

s32 accessibilityIsWeaponFunctionCuesEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityWeaponFunctionCuesEnabledConfig;
}

s32 accessibilityIsXrayScannerAudioEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityXrayScannerAudioEnabledConfig;
}

s32 accessibilityGetVirtualCaneMode(void)
{
	if (!g_AccessibilityEnabled) {
		return 0;
	}

	return g_AccessibilityVirtualCaneModeConfig;
}

void accessibilitySetVirtualCaneMode(s32 mode)
{
	if (mode < 0) {
		mode = 0;
	} else if (mode > 2) {
		mode = 2;
	}

	g_AccessibilityVirtualCaneModeConfig = mode;
}

PD_CONSTRUCTOR static void accessibilityConfigInit(void)
{
	configRegisterInt("Accessibility.Enabled", &g_AccessibilityEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.LoggingEnabled", &g_AccessibilityLoggingEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.SpeechEnabled", &g_AccessibilitySpeechEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.MenuNarration", &g_AccessibilityMenuNarrationEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.HudMessages", &g_AccessibilityHudMessagesEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.EnvironmentalHazards", &g_AccessibilityEnvironmentalHazardsEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.InteractableBeacons", &g_AccessibilityInteractableBeaconsEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.IRScannerAudio", &g_AccessibilityIrScannerAudioEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.NonHostileBeacons", &g_AccessibilityNonHostileBeaconsEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.RTrackerAudio", &g_AccessibilityRTrackerAudioEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.TargetingFeedback", &g_AccessibilityTargetingFeedbackEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.WeaponFunctionCues", &g_AccessibilityWeaponFunctionCuesEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.XRayScannerAudio", &g_AccessibilityXrayScannerAudioEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.VirtualCaneMode", &g_AccessibilityVirtualCaneModeConfig, 0, 2);
	configRegisterFloat("Accessibility.VirtualCaneReach",
			&g_AccessibilityVirtualCaneReachConfig, 100.0f, 5000.0f);
	configRegisterFloat("Accessibility.VirtualCaneFullVolumeDistance",
			&g_AccessibilityVirtualCaneFullVolumeDistanceConfig, 0.0f, 10000.0f);
	configRegisterFloat("Accessibility.VirtualCaneFadeDistance",
			&g_AccessibilityVirtualCaneFadeDistanceConfig, 0.0f, 10000.0f);
	configRegisterFloat("Accessibility.VirtualCaneMaximumAudibleDistance",
			&g_AccessibilityVirtualCaneMaximumAudibleDistanceConfig,
			1.0f, 10000.0f);
	configRegisterFloat("Accessibility.EnemyFullVolumeDistance",
			&g_AccessibilityEnemyFullVolumeDistanceConfig, 0.0f, 20000.0f);
	configRegisterFloat("Accessibility.EnemyFadeDistance",
			&g_AccessibilityEnemyFadeDistanceConfig, 0.0f, 20000.0f);
	configRegisterFloat("Accessibility.EnemyMaximumDistance",
			&g_AccessibilityEnemyMaximumDistanceConfig, 1.0f, 20000.0f);
	configRegisterFloat("Accessibility.EnemyVolume",
			&g_AccessibilityEnemyVolumeConfig, 0.0f, 0.25f);
}
