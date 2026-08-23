#ifndef _IN_PORT_ACCESSIBILITY_ACCESSIBILITY_TONE_H
#define _IN_PORT_ACCESSIBILITY_ACCESSIBILITY_TONE_H

#include <PR/ultratypes.h>

#define ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT 10
#define ACCESSIBILITY_TONE_CANE_SLOT_COUNT 10
#define ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT 10
#define ACCESSIBILITY_TONE_MARKER_SLOT_COUNT 4
#define ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT 4
#define ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT 3
#define ACCESSIBILITY_TONE_DOOR_SLOT_COUNT 3

enum accessibilitytonecombatcontour {
	ACCESSIBILITY_TONE_COMBAT_CONTOUR_LINEAR,
	ACCESSIBILITY_TONE_COMBAT_CONTOUR_BASE_THEN_END,
};

enum accessibilitytonecanepattern {
	ACCESSIBILITY_TONE_CANE_PATTERN_CONTOUR,
	ACCESSIBILITY_TONE_CANE_PATTERN_CROUCH_DOUBLE,
	ACCESSIBILITY_TONE_CANE_PATTERN_LADDER_TRIPLE,
};

enum accessibilitytoneradarkind {
	ACCESSIBILITY_TONE_RADAR_ENEMY,
	ACCESSIBILITY_TONE_RADAR_ALLY,
	ACCESSIBILITY_TONE_RADAR_OBJECTIVE,
	ACCESSIBILITY_TONE_RADAR_OTHER,
	ACCESSIBILITY_TONE_RADAR_LAUNCH,
	ACCESSIBILITY_TONE_RADAR_EMPTY,
	ACCESSIBILITY_TONE_RADAR_UNAVAILABLE,
};

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
struct accessibilitytonediagnostics {
	s32 toneenabled;
	s32 chirpenabled;
	s32 chirpsequence;
	s32 weaponfunctionsequence;
	s32 weaponfunctionpulses;
	s32 hazardenabled;
	s32 combatenabledslots;
	s32 trackerenabledslots;
	s32 friendlyenabledslots;
	s32 doorenabledslots;
	s32 radarenabled;
	s32 radarsequence;
	s32 hillenabled;
	s32 markerenabledslots;
	s32 landmarkenabledslots;
	s32 canerequestedmask;
	s32 caneactivemask;
	s32 canecommands;
	s32 canetonesstarted;
	s32 canestops;
	s32 hazardfrequencymillihz;
	s32 hazardvolumemillionths;
	s32 hazardpanmillionths;
	s32 mixcalls;
	s32 passthroughcalls;
	s32 activecalls;
	s32 mixedframes;
};
#endif

void accessibilityToneSet(s32 enabled, f32 frequencyhz);
void accessibilityToneSetAlignment(s32 enabled, f32 frequencyhz,
		s32 interrupted);
void accessibilityTonePlayChirp(f32 frequencyhz, f32 volume, f32 pan);
void accessibilityTonePlayChirpPattern(f32 frequencyhz, f32 volume, f32 pan,
		s32 pulses, f32 gain);
void accessibilityToneStopChirp(void);
void accessibilityTonePlayTargetPresence(f32 frequencyhz,
		f32 volume, f32 pan);
void accessibilityToneStopTargetPresence(void);
void accessibilityTonePlayThreatAlert(f32 volume, f32 pan);
void accessibilityToneStopThreatAlert(void);
void accessibilityTonePlayToggleConfirmation(s32 enabled);
void accessibilityTonePlayCaneModeConfirmation(s32 mode);
void accessibilityTonePlayWeaponFunction(s32 secondary);
void accessibilityToneStopWeaponFunction(void);
void accessibilityToneSetHazard(s32 enabled, f32 frequencyhz, f32 volume, f32 pan);
void accessibilityToneSetCombatSlot(s32 slot, s32 enabled,
		f32 startfrequencyhz, f32 endfrequencyhz, f32 volume, f32 pan,
		s32 periodms, s32 durationms, s32 frequencycontour, s32 continuous,
		s32 restart, s32 triggernow);
void accessibilityToneStopCombat(void);
void accessibilityToneSetTrackerSlot(s32 slot, s32 enabled, f32 frequencyhz,
		f32 volume, f32 pan, s32 periodms, s32 height, s32 rear,
		s32 restart);
void accessibilityToneStopTracker(void);
void accessibilityToneSetFriendlySlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 pulsethird, s32 restart);
void accessibilityToneStopFriendly(void);
void accessibilityToneSetDoorSlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 restart);
void accessibilityToneStopDoors(void);
void accessibilityTonePlayRadarPing(f32 frequencyhz, f32 volume, f32 pan,
		s32 height, s32 rear, s32 kind);
void accessibilityToneStopRadar(void);
void accessibilityToneSetHillBeacon(s32 enabled, f32 volume, f32 pan,
		s32 rear, s32 identitychirp, s32 scoring, s32 restart);
void accessibilityToneStopHillBeacon(void);
void accessibilityTonePlayCaneSlot(s32 slot, f32 startfrequencyhz,
		f32 endfrequencyhz, f32 volume, f32 pan, s32 durationms,
		s32 pattern);
void accessibilityToneStopCane(void);
void accessibilityToneSetMarkerSlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 restart);
void accessibilityTonePlayMarkerRemoval(s32 slot);
void accessibilityToneStopMarkers(void);
void accessibilityToneSetLandmarkSlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 restart);
void accessibilityToneStopLandmarks(void);
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
void accessibilityToneGetDiagnostics(struct accessibilitytonediagnostics *diagnostics);
#endif
const s16 *accessibilityToneMix(const s16 *input, u32 len);

#endif
