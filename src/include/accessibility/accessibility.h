#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_H

#include <PR/ultratypes.h>

enum accessibilityscanner {
	ACCESSIBILITY_SCANNER_INTERACTABLE = 0,
	ACCESSIBILITY_SCANNER_DOOR = 1,
	ACCESSIBILITY_SCANNER_PICKUP = 2,
	ACCESSIBILITY_SCANNER_NON_HOSTILE = 3,
	ACCESSIBILITY_SCANNER_COUNT = 4,
};

void accessibilityInit(void);
void accessibilityShutdown(void);
void accessibilityPerformanceTick(void);
void accessibilityPerformanceShutdown(void);
s32 accessibilityIsEnabled(void);
s32 accessibilityIsEnvironmentalHazardsEnabled(void);
s32 accessibilityIsHudMessagesEnabled(void);
s32 accessibilityIsInteractableBeaconsEnabled(void);
s32 accessibilityIsIrScannerAudioEnabled(void);
s32 accessibilityIsNonHostileBeaconsEnabled(void);
s32 accessibilityIsMenuNarrationEnabled(void);
s32 accessibilityIsRTrackerAudioEnabled(void);
s32 accessibilityIsTargetingFeedbackEnabled(void);
f32 accessibilityGetTargetingVolume(void);
s32 accessibilityIsWeaponChangeAnnouncementsEnabled(void);
s32 accessibilityIsWeaponFunctionCuesEnabled(void);
s32 accessibilityIsStanceCuesEnabled(void);
s32 accessibilityIsXrayScannerAudioEnabled(void);
s32 accessibilityIsAudibleMarkersEnabled(void);
s32 accessibilityIsAuthoredLandmarksEnabled(void);
s32 accessibilityIsCombatRadarAudioEnabled(void);
s32 accessibilityIsKingOfTheHillBeaconEnabled(void);
s32 accessibilityIsPlayerStatusEnabled(void);
s32 accessibilityGetCombatRadarContactAlerts(void);
void accessibilitySetCombatRadarContactAlerts(s32 enabled);
void accessibilityGetCombatRadarTuning(f32 *mediumdistance,
		f32 *closedistance, f32 *volume);
s32 accessibilityGetVirtualCaneMode(void);
void accessibilitySetVirtualCaneMode(s32 mode);
s32 accessibilityGetScannerActive(s32 scanner);
void accessibilitySetScannerActive(s32 scanner, s32 active);
void accessibilityGetVirtualCaneTuning(f32 *reach, f32 *fulldistance,
		f32 *fadedistance, f32 *silentdistance);
void accessibilityGetVirtualCanePitch(f32 *nearfrequency,
		f32 *farfrequency);
void accessibilityGetVirtualCaneTerrainTuning(f32 *reach,
		f32 *heightthreshold, f32 *dropheightthreshold);
f32 accessibilityGetVirtualCaneVolume(void);
void accessibilityGetEnemyTuning(f32 *fulldistance, f32 *fadedistance,
		f32 *silentdistance, f32 *volume);
void accessibilityGetEnemyScopedTuning(f32 *fulldistance, f32 *fadedistance,
		f32 *silentdistance);
f32 accessibilityGetEnemyFrequency(void);
void accessibilityGetMarkerTuning(f32 *range, f32 *volume);

#endif
