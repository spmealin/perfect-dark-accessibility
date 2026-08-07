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
#include "accessibility/accessibility_combat_radar.h"
#include "accessibility/accessibility_hazard.h"
#include "accessibility/accessibility_hill.h"
#include "accessibility/accessibility_hud.h"
#include "accessibility/accessibility_incident.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_marker.h"
#include "accessibility/accessibility_menu.h"
#include "accessibility/accessibility_speech.h"
#include "accessibility/accessibility_status.h"
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
static s32 g_AccessibilityInteractableScannerActiveConfig = 1;
static s32 g_AccessibilityDoorScannerActiveConfig = 1;
static s32 g_AccessibilityPickupScannerActiveConfig = 1;
static s32 g_AccessibilityNonHostileScannerActiveConfig = 1;
static s32 g_AccessibilityIrScannerAudioEnabledConfig = 1;
static s32 g_AccessibilityNonHostileBeaconsEnabledConfig = 1;
static s32 g_AccessibilityRTrackerAudioEnabledConfig = 1;
static s32 g_AccessibilityTargetingFeedbackEnabledConfig = 1;
static s32 g_AccessibilityWeaponChangeAnnouncementsEnabledConfig = 1;
static s32 g_AccessibilityWeaponFunctionCuesEnabledConfig = 1;
static s32 g_AccessibilityXrayScannerAudioEnabledConfig = 1;
static s32 g_AccessibilityAudibleMarkersEnabledConfig = 1;
static s32 g_AccessibilityCombatRadarAudioEnabledConfig = 1;
static s32 g_AccessibilityCombatRadarContactAlertsConfig = 1;
static s32 g_AccessibilityKingOfTheHillBeaconEnabledConfig = 1;
static s32 g_AccessibilityPlayerStatusEnabledConfig = 1;
static s32 g_AccessibilityVirtualCaneModeConfig = 1;
static f32 g_AccessibilityVirtualCaneReachConfig = 900.0f;
static f32 g_AccessibilityVirtualCaneFullVolumeDistanceConfig = 112.5f;
static f32 g_AccessibilityVirtualCaneFadeDistanceConfig = 750.0f;
static f32 g_AccessibilityVirtualCaneMaximumAudibleDistanceConfig = 975.0f;
static f32 g_AccessibilityVirtualCaneNearFrequencyConfig = 600.0f;
static f32 g_AccessibilityVirtualCaneFarFrequencyConfig = 300.0f;
static f32 g_AccessibilityVirtualCaneTerrainReachConfig = 450.0f;
static f32 g_AccessibilityVirtualCaneTerrainHeightThresholdConfig = 12.0f;
static f32 g_AccessibilityVirtualCaneDropHeightThresholdConfig = 80.0f;
static f32 g_AccessibilityVirtualCaneVolumeConfig = 0.184f;
static f32 g_AccessibilityEnemyFullVolumeDistanceConfig = 4000.0f;
static f32 g_AccessibilityEnemyFadeDistanceConfig = 5500.0f;
static f32 g_AccessibilityEnemyMaximumDistanceConfig = 6000.0f;
static f32 g_AccessibilityEnemyScopedFullVolumeDistanceConfig = 6000.0f;
static f32 g_AccessibilityEnemyScopedFadeDistanceConfig = 9000.0f;
static f32 g_AccessibilityEnemyScopedMaximumDistanceConfig = 12000.0f;
static f32 g_AccessibilityEnemyVolumeConfig = 0.25f;
static f32 g_AccessibilityEnemyFrequencyConfig = 900.0f;
static f32 g_AccessibilityMarkerRangeConfig = 1200.0f;
static f32 g_AccessibilityMarkerVolumeConfig = 1.0f;
static f32 g_AccessibilityCombatRadarMediumDistanceConfig = 2000.0f;
static f32 g_AccessibilityCombatRadarCloseDistanceConfig = 750.0f;
static f32 g_AccessibilityCombatRadarVolumeConfig = 0.20f;
static s32 g_AccessibilityInitialized = 0;
static s32 g_AccessibilityEnabled = 0;
static s32 g_AccessibilityCombatRadarContactAlerts = 1;
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

void accessibilityGetVirtualCanePitch(f32 *nearfrequency,
		f32 *farfrequency)
{
	f32 effectiveNear = accessibilityValidatedFloat(
			g_AccessibilityVirtualCaneNearFrequencyConfig,
			600.0f, 20.0f, 4000.0f);
	f32 effectiveFar = accessibilityValidatedFloat(
			g_AccessibilityVirtualCaneFarFrequencyConfig,
			300.0f, 20.0f, 4000.0f);

	if (effectiveNear < effectiveFar) {
		f32 swap = effectiveNear;

		effectiveNear = effectiveFar;
		effectiveFar = swap;
	}

	if (nearfrequency) {
		*nearfrequency = effectiveNear;
	}
	if (farfrequency) {
		*farfrequency = effectiveFar;
	}
}

void accessibilityGetVirtualCaneTerrainTuning(f32 *reach,
		f32 *heightthreshold, f32 *dropheightthreshold)
{
	if (reach) {
		*reach = accessibilityValidatedFloat(
				g_AccessibilityVirtualCaneTerrainReachConfig,
				450.0f, 50.0f, 2000.0f);
	}
	if (heightthreshold) {
		*heightthreshold = accessibilityValidatedFloat(
				g_AccessibilityVirtualCaneTerrainHeightThresholdConfig,
				12.0f, 1.0f, 100.0f);
	}
	if (dropheightthreshold) {
		*dropheightthreshold = accessibilityValidatedFloat(
				g_AccessibilityVirtualCaneDropHeightThresholdConfig,
				80.0f, 30.0f, 500.0f);
	}
}

f32 accessibilityGetVirtualCaneVolume(void)
{
	return accessibilityValidatedFloat(
			g_AccessibilityVirtualCaneVolumeConfig,
			0.184f, 0.0f, 0.4f);
}

void accessibilityGetEnemyTuning(f32 *fulldistance, f32 *fadedistance,
		f32 *silentdistance, f32 *volume)
{
	f32 effectiveFull = accessibilityValidatedFloat(
			g_AccessibilityEnemyFullVolumeDistanceConfig,
			4000.0f, 0.0f, 6000.0f);
	f32 effectiveFade = accessibilityValidatedFloat(
			g_AccessibilityEnemyFadeDistanceConfig,
			5500.0f, 0.0f, 6000.0f);
	f32 effectiveSilent = accessibilityValidatedFloat(
			g_AccessibilityEnemyMaximumDistanceConfig,
			6000.0f, 1.0f, 6000.0f);
	f32 effectiveVolume = accessibilityValidatedFloat(
			g_AccessibilityEnemyVolumeConfig, 0.25f, 0.0f, 0.4f);

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

void accessibilityGetEnemyScopedTuning(f32 *fulldistance, f32 *fadedistance,
		f32 *silentdistance)
{
	f32 effectiveFull = accessibilityValidatedFloat(
			g_AccessibilityEnemyScopedFullVolumeDistanceConfig,
			6000.0f, 0.0f, 20000.0f);
	f32 effectiveFade = accessibilityValidatedFloat(
			g_AccessibilityEnemyScopedFadeDistanceConfig,
			9000.0f, 0.0f, 20000.0f);
	f32 effectiveSilent = accessibilityValidatedFloat(
			g_AccessibilityEnemyScopedMaximumDistanceConfig,
			12000.0f, 1.0f, 20000.0f);

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
}

f32 accessibilityGetEnemyFrequency(void)
{
	return accessibilityValidatedFloat(
			g_AccessibilityEnemyFrequencyConfig,
			900.0f, 100.0f, 4000.0f);
}

void accessibilityGetMarkerTuning(f32 *range, f32 *volume)
{
	if (range) {
		*range = accessibilityValidatedFloat(
				g_AccessibilityMarkerRangeConfig,
				1200.0f, 100.0f, 10000.0f);
	}
	if (volume) {
		*volume = accessibilityValidatedFloat(
				g_AccessibilityMarkerVolumeConfig,
				1.0f, 0.0f, 4.0f);
	}
}

void accessibilityGetCombatRadarTuning(f32 *mediumdistance,
		f32 *closedistance, f32 *volume)
{
	f32 effectiveMedium = accessibilityValidatedFloat(
			g_AccessibilityCombatRadarMediumDistanceConfig,
			2000.0f, 1.0f, 20000.0f);
	f32 effectiveClose = accessibilityValidatedFloat(
			g_AccessibilityCombatRadarCloseDistanceConfig,
			750.0f, 1.0f, 20000.0f);

	if (effectiveClose >= effectiveMedium) {
		effectiveMedium = 2000.0f;
		effectiveClose = 750.0f;
	}

	if (mediumdistance) {
		*mediumdistance = effectiveMedium;
	}
	if (closedistance) {
		*closedistance = effectiveClose;
	}
	if (volume) {
		*volume = accessibilityValidatedFloat(
				g_AccessibilityCombatRadarVolumeConfig,
				0.20f, 0.0f, 0.4f);
	}
}

void accessibilityInit(void)
{
	f32 caneReach;
	f32 caneFull;
	f32 caneFade;
	f32 caneMaximum;
	f32 caneNearFrequency;
	f32 caneFarFrequency;
	f32 caneTerrainReach;
	f32 caneTerrainHeightThreshold;
	f32 caneDropHeightThreshold;
	f32 caneVolume;
	f32 enemyFull;
	f32 enemyFade;
	f32 enemyMaximum;
	f32 enemyScopedFull;
	f32 enemyScopedFade;
	f32 enemyScopedMaximum;
	f32 enemyVolume;
	f32 enemyFrequency;
	f32 markerRange;
	f32 markerVolume;
	f32 combatRadarMedium;
	f32 combatRadarClose;
	f32 combatRadarVolume;

	if (g_AccessibilityInitialized) {
		return;
	}

	g_AccessibilityInitialized = 1;
	g_AccessibilityEnabled = g_AccessibilityEnabledConfig;
	g_AccessibilityCombatRadarContactAlerts
			= g_AccessibilityCombatRadarContactAlertsConfig;
	g_AccessibilityStartTimeUs = sysGetMicroseconds();
	accessibilityBeaconReset("init", false);
	accessibilityBeaconRestoreConfiguredState();
	accessibilityCaneReset("init");
	accessibilityCombatRadarReset("init");
	accessibilityHazardReset("init");
	accessibilityHillReset("init");
	accessibilityHudReset("init");
	accessibilityIncidentReset("init");
	accessibilityMarkerReset("init");
	accessibilityStatusReset("init");
	accessibilityTargetingReset("init");
	accessibilityTrackerReset("init");
	accessibilityWeaponFunctionReset("init");

	if (!g_AccessibilityEnabled) {
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
		if (g_AccessibilityLoggingEnabledConfig && accessibilityLogInit()) {
			accessibilityLogEvent("lifecycle", "session_start",
					"enabled=0 logging=1 speech=0 menu_narration=0 hud_messages=0 player_status=0 environmental_hazards=0 interactable_beacons=0 ir_scanner_audio=0 non_hostile_beacons=0 rtracker_audio=0 combat_radar_audio=0 combat_radar_contact_alerts=0 king_of_the_hill_beacon=0 targeting_feedback=0 weapon_change_announcements=0 weapon_function_cues=0 xray_scanner_audio=0 virtual_cane_mode=0 incident_capture=0 incident_key=Shift+F2 incident_history_seconds=15 performance_diagnostics=1 performance_interval_us=1000000 speech_test=0 path=%s diagnostic_control=accessibility_disabled",
					fsFullPath(ACCESSIBILITY_LOG_PATH));
		}
#endif
		return;
	}

	accessibilityGetVirtualCaneTuning(&caneReach, &caneFull,
			&caneFade, &caneMaximum);
	accessibilityGetVirtualCanePitch(&caneNearFrequency,
			&caneFarFrequency);
	accessibilityGetVirtualCaneTerrainTuning(&caneTerrainReach,
			&caneTerrainHeightThreshold, &caneDropHeightThreshold);
	caneVolume = accessibilityGetVirtualCaneVolume();
	accessibilityGetEnemyTuning(&enemyFull, &enemyFade,
			&enemyMaximum, &enemyVolume);
	accessibilityGetEnemyScopedTuning(&enemyScopedFull, &enemyScopedFade,
			&enemyScopedMaximum);
	enemyFrequency = accessibilityGetEnemyFrequency();
	accessibilityGetMarkerTuning(&markerRange, &markerVolume);
	accessibilityGetCombatRadarTuning(&combatRadarMedium,
			&combatRadarClose, &combatRadarVolume);

	sysLogPrintf(LOG_NOTE, "accessibility: enabled (logging %s, speech %s, HUD messages %s, player status %s, environmental hazards %s, interactable beacons %s, audible markers %s, IR Scanner audio %s, non-hostile beacons %s, R-Tracker audio %s, combat radar audio %s, combat radar alerts %s, King of the Hill beacon %s, targeting feedback %s, weapon change announcements %s, weapon function cues %s, X-Ray Scanner audio %s, virtual cane mode %d, cane reach %.1f, cane pitch %.1f-%.1f Hz, cane volume %.3f, terrain reach %.1f, drop threshold %.1f, enemy maximum %.1f, scoped enemy maximum %.1f, enemy frequency %.1f Hz, enemy volume %.3f, marker range %.1f, marker volume %.3f, radar medium %.1f, radar close %.1f, radar volume %.3f)",
			g_AccessibilityLoggingEnabledConfig ? "enabled" : "disabled",
			g_AccessibilitySpeechEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityHudMessagesEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityPlayerStatusEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityEnvironmentalHazardsEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityInteractableBeaconsEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityAudibleMarkersEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityIrScannerAudioEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityNonHostileBeaconsEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityRTrackerAudioEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityCombatRadarAudioEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityCombatRadarContactAlerts ? "enabled" : "disabled",
			g_AccessibilityKingOfTheHillBeaconEnabledConfig
					? "enabled" : "disabled",
			g_AccessibilityTargetingFeedbackEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityWeaponChangeAnnouncementsEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityWeaponFunctionCuesEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityXrayScannerAudioEnabledConfig ? "enabled" : "disabled",
			g_AccessibilityVirtualCaneModeConfig, caneReach,
			caneFarFrequency, caneNearFrequency,
			caneVolume, caneTerrainReach, caneDropHeightThreshold,
			enemyMaximum, enemyScopedMaximum, enemyFrequency, enemyVolume,
			markerRange, markerVolume,
			combatRadarMedium, combatRadarClose, combatRadarVolume);

	if (g_AccessibilityLoggingEnabledConfig && accessibilityLogInit()) {
		accessibilityLogEvent("lifecycle", "session_start",
				"enabled=%d logging=%d speech=%d menu_narration=%d hud_messages=%d player_status=%d environmental_hazards=%d interactable_beacons=%d audible_markers=%d ir_scanner_audio=%d non_hostile_beacons=%d rtracker_audio=%d combat_radar_audio=%d combat_radar_contact_alerts=%d combat_radar_medium_distance=%.3f combat_radar_close_distance=%.3f combat_radar_volume=%.4f king_of_the_hill_beacon=%d targeting_feedback=%d weapon_change_announcements=%d weapon_function_cues=%d xray_scanner_audio=%d virtual_cane_mode=%d cane_reach=%.3f cane_full_distance=%.3f cane_fade_distance=%.3f cane_maximum_audible_distance=%.3f cane_near_frequency_hz=%.3f cane_far_frequency_hz=%.3f cane_volume=%.4f cane_terrain_reach=%.3f cane_terrain_height_threshold=%.3f cane_drop_height_threshold=%.3f enemy_full_distance=%.3f enemy_fade_distance=%.3f enemy_maximum_distance=%.3f enemy_scoped_full_distance=%.3f enemy_scoped_fade_distance=%.3f enemy_scoped_maximum_distance=%.3f enemy_frequency_hz=%.3f enemy_volume=%.4f marker_range=%.3f marker_volume=%.4f marker_line_of_sight=1 marker_keys=F9,F10,F11,F12 marker_voices=4 incident_capture=1 incident_key=Shift+F2 incident_history_seconds=15 performance_diagnostics=%d performance_interval_us=%d speech_test=%d path=%s",
				g_AccessibilityEnabledConfig,
				g_AccessibilityLoggingEnabledConfig,
				g_AccessibilitySpeechEnabledConfig,
				g_AccessibilityMenuNarrationEnabledConfig,
				g_AccessibilityHudMessagesEnabledConfig,
				g_AccessibilityPlayerStatusEnabledConfig,
				g_AccessibilityEnvironmentalHazardsEnabledConfig,
				g_AccessibilityInteractableBeaconsEnabledConfig,
				g_AccessibilityAudibleMarkersEnabledConfig,
				g_AccessibilityIrScannerAudioEnabledConfig,
				g_AccessibilityNonHostileBeaconsEnabledConfig,
				g_AccessibilityRTrackerAudioEnabledConfig,
				g_AccessibilityCombatRadarAudioEnabledConfig,
				g_AccessibilityCombatRadarContactAlerts,
				combatRadarMedium, combatRadarClose, combatRadarVolume,
				g_AccessibilityKingOfTheHillBeaconEnabledConfig,
				g_AccessibilityTargetingFeedbackEnabledConfig,
				g_AccessibilityWeaponChangeAnnouncementsEnabledConfig,
				g_AccessibilityWeaponFunctionCuesEnabledConfig,
				g_AccessibilityXrayScannerAudioEnabledConfig,
				g_AccessibilityVirtualCaneModeConfig,
				caneReach, caneFull, caneFade, caneMaximum,
				caneNearFrequency, caneFarFrequency,
				caneVolume, caneTerrainReach, caneTerrainHeightThreshold,
				caneDropHeightThreshold,
				enemyFull, enemyFade, enemyMaximum,
				enemyScopedFull, enemyScopedFade, enemyScopedMaximum,
				enemyFrequency, enemyVolume,
				markerRange, markerVolume,
				ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS,
				ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS ? 1000000 : 0,
				sysArgCheck("--accessibility-speech-test"),
				fsFullPath(ACCESSIBILITY_LOG_PATH));
		accessibilityLogEvent("lifecycle", "scanner_start_state",
				"interactable_saved=%d door_saved=%d pickup_saved=%d non_hostile_saved=%d interactable_effective=%d door_effective=%d pickup_effective=%d non_hostile_effective=%d persistence=pd.ini",
				accessibilityGetScannerActive(
					ACCESSIBILITY_SCANNER_INTERACTABLE),
				accessibilityGetScannerActive(ACCESSIBILITY_SCANNER_DOOR),
				accessibilityGetScannerActive(ACCESSIBILITY_SCANNER_PICKUP),
				accessibilityGetScannerActive(
					ACCESSIBILITY_SCANNER_NON_HOSTILE),
				g_AccessibilityInteractableBeaconsEnabledConfig
					&& accessibilityGetScannerActive(
						ACCESSIBILITY_SCANNER_INTERACTABLE),
				g_AccessibilityInteractableBeaconsEnabledConfig
					&& accessibilityGetScannerActive(ACCESSIBILITY_SCANNER_DOOR),
				g_AccessibilityInteractableBeaconsEnabledConfig
					&& accessibilityGetScannerActive(ACCESSIBILITY_SCANNER_PICKUP),
				g_AccessibilityNonHostileBeaconsEnabledConfig
					&& accessibilityGetScannerActive(
						ACCESSIBILITY_SCANNER_NON_HOSTILE));
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
	accessibilityBeaconReset("shutdown", false);
	accessibilityCaneReset("shutdown");
	accessibilityCombatRadarReset("shutdown");
	accessibilityHazardReset("shutdown");
	accessibilityHillReset("shutdown");
	accessibilityHudReset("shutdown");
	accessibilityIncidentReset("shutdown");
	accessibilityMarkerReset("shutdown");
	accessibilityStatusReset("shutdown");
	accessibilityTargetingReset("shutdown");
	accessibilityTrackerReset("shutdown");
	accessibilityWeaponFunctionReset("shutdown");
	accessibilityMenuReset();
	accessibilityAnnouncementReset();
	accessibilitySpeechShutdown();
	accessibilityPerformanceShutdown();

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

s32 accessibilityIsWeaponChangeAnnouncementsEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityWeaponChangeAnnouncementsEnabledConfig;
}

s32 accessibilityIsXrayScannerAudioEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityXrayScannerAudioEnabledConfig;
}

s32 accessibilityIsAudibleMarkersEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityAudibleMarkersEnabledConfig;
}

s32 accessibilityIsCombatRadarAudioEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityCombatRadarAudioEnabledConfig;
}

s32 accessibilityIsKingOfTheHillBeaconEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityKingOfTheHillBeaconEnabledConfig;
}

s32 accessibilityIsPlayerStatusEnabled(void)
{
	return g_AccessibilityEnabled
			&& g_AccessibilityPlayerStatusEnabledConfig;
}

s32 accessibilityGetCombatRadarContactAlerts(void)
{
	return g_AccessibilityCombatRadarContactAlerts;
}

void accessibilitySetCombatRadarContactAlerts(s32 enabled)
{
	g_AccessibilityCombatRadarContactAlerts = enabled != 0;
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

s32 accessibilityGetScannerActive(s32 scanner)
{
	switch (scanner) {
	case ACCESSIBILITY_SCANNER_INTERACTABLE:
		return g_AccessibilityInteractableScannerActiveConfig;
	case ACCESSIBILITY_SCANNER_DOOR:
		return g_AccessibilityDoorScannerActiveConfig;
	case ACCESSIBILITY_SCANNER_PICKUP:
		return g_AccessibilityPickupScannerActiveConfig;
	case ACCESSIBILITY_SCANNER_NON_HOSTILE:
		return g_AccessibilityNonHostileScannerActiveConfig;
	default:
		return false;
	}
}

void accessibilitySetScannerActive(s32 scanner, s32 active)
{
	s32 value = active != 0;

	switch (scanner) {
	case ACCESSIBILITY_SCANNER_INTERACTABLE:
		g_AccessibilityInteractableScannerActiveConfig = value;
		break;
	case ACCESSIBILITY_SCANNER_DOOR:
		g_AccessibilityDoorScannerActiveConfig = value;
		break;
	case ACCESSIBILITY_SCANNER_PICKUP:
		g_AccessibilityPickupScannerActiveConfig = value;
		break;
	case ACCESSIBILITY_SCANNER_NON_HOSTILE:
		g_AccessibilityNonHostileScannerActiveConfig = value;
		break;
	}
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
	configRegisterInt("Accessibility.InteractableScannerActive",
			&g_AccessibilityInteractableScannerActiveConfig, 0, 1);
	configRegisterInt("Accessibility.DoorScannerActive",
			&g_AccessibilityDoorScannerActiveConfig, 0, 1);
	configRegisterInt("Accessibility.PickupScannerActive",
			&g_AccessibilityPickupScannerActiveConfig, 0, 1);
	configRegisterInt("Accessibility.NonHostileScannerActive",
			&g_AccessibilityNonHostileScannerActiveConfig, 0, 1);
	configRegisterInt("Accessibility.IRScannerAudio", &g_AccessibilityIrScannerAudioEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.NonHostileBeacons", &g_AccessibilityNonHostileBeaconsEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.RTrackerAudio", &g_AccessibilityRTrackerAudioEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.TargetingFeedback", &g_AccessibilityTargetingFeedbackEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.WeaponChangeAnnouncements", &g_AccessibilityWeaponChangeAnnouncementsEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.WeaponFunctionCues", &g_AccessibilityWeaponFunctionCuesEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.XRayScannerAudio", &g_AccessibilityXrayScannerAudioEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.AudibleMarkers", &g_AccessibilityAudibleMarkersEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.CombatRadarAudio",
			&g_AccessibilityCombatRadarAudioEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.CombatRadarContactAlerts",
			&g_AccessibilityCombatRadarContactAlertsConfig, 0, 1);
	configRegisterInt("Accessibility.KingOfTheHillBeacon",
			&g_AccessibilityKingOfTheHillBeaconEnabledConfig, 0, 1);
	configRegisterInt("Accessibility.PlayerStatus",
			&g_AccessibilityPlayerStatusEnabledConfig, 0, 1);
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
	configRegisterFloat("Accessibility.VirtualCaneNearFrequency",
			&g_AccessibilityVirtualCaneNearFrequencyConfig, 20.0f, 4000.0f);
	configRegisterFloat("Accessibility.VirtualCaneFarFrequency",
			&g_AccessibilityVirtualCaneFarFrequencyConfig, 20.0f, 4000.0f);
	configRegisterFloat("Accessibility.VirtualCaneTerrainReach",
			&g_AccessibilityVirtualCaneTerrainReachConfig, 50.0f, 2000.0f);
	configRegisterFloat("Accessibility.VirtualCaneTerrainHeightThreshold",
			&g_AccessibilityVirtualCaneTerrainHeightThresholdConfig,
			1.0f, 100.0f);
	configRegisterFloat("Accessibility.VirtualCaneDropHeightThreshold",
			&g_AccessibilityVirtualCaneDropHeightThresholdConfig,
			30.0f, 500.0f);
	configRegisterFloat("Accessibility.VirtualCaneVolume",
			&g_AccessibilityVirtualCaneVolumeConfig, 0.0f, 0.4f);
	configRegisterFloat("Accessibility.EnemyFullVolumeDistance",
			&g_AccessibilityEnemyFullVolumeDistanceConfig, 0.0f, 6000.0f);
	configRegisterFloat("Accessibility.EnemyFadeDistance",
			&g_AccessibilityEnemyFadeDistanceConfig, 0.0f, 6000.0f);
	configRegisterFloat("Accessibility.EnemyMaximumDistance",
			&g_AccessibilityEnemyMaximumDistanceConfig, 1.0f, 6000.0f);
	configRegisterFloat("Accessibility.EnemyScopedFullVolumeDistance",
			&g_AccessibilityEnemyScopedFullVolumeDistanceConfig,
			0.0f, 20000.0f);
	configRegisterFloat("Accessibility.EnemyScopedFadeDistance",
			&g_AccessibilityEnemyScopedFadeDistanceConfig, 0.0f, 20000.0f);
	configRegisterFloat("Accessibility.EnemyScopedMaximumDistance",
			&g_AccessibilityEnemyScopedMaximumDistanceConfig,
			1.0f, 20000.0f);
	configRegisterFloat("Accessibility.EnemyVolume",
			&g_AccessibilityEnemyVolumeConfig, 0.0f, 0.4f);
	configRegisterFloat("Accessibility.EnemyFrequency",
			&g_AccessibilityEnemyFrequencyConfig, 100.0f, 4000.0f);
	configRegisterFloat("Accessibility.MarkerRange",
			&g_AccessibilityMarkerRangeConfig, 100.0f, 10000.0f);
	configRegisterFloat("Accessibility.MarkerVolume",
			&g_AccessibilityMarkerVolumeConfig, 0.0f, 4.0f);
	configRegisterFloat("Accessibility.CombatRadarMediumDistance",
			&g_AccessibilityCombatRadarMediumDistanceConfig, 1.0f, 20000.0f);
	configRegisterFloat("Accessibility.CombatRadarCloseDistance",
			&g_AccessibilityCombatRadarCloseDistanceConfig, 1.0f, 20000.0f);
	configRegisterFloat("Accessibility.CombatRadarVolume",
			&g_AccessibilityCombatRadarVolumeConfig, 0.0f, 0.4f);
}
