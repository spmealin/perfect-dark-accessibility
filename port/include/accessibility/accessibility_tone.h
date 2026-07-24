#ifndef _IN_PORT_ACCESSIBILITY_ACCESSIBILITY_TONE_H
#define _IN_PORT_ACCESSIBILITY_ACCESSIBILITY_TONE_H

#include <PR/ultratypes.h>

#define ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT 10
#define ACCESSIBILITY_TONE_CANE_SLOT_COUNT 7
#define ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT 10

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
void accessibilityTonePlayChirp(f32 frequencyhz, f32 volume, f32 pan);
void accessibilityTonePlayChirpPattern(f32 frequencyhz, f32 volume, f32 pan,
		s32 pulses);
void accessibilityToneStopChirp(void);
void accessibilityTonePlayToggleConfirmation(s32 enabled);
void accessibilityTonePlayWeaponFunction(s32 secondary);
void accessibilityToneStopWeaponFunction(void);
void accessibilityToneSetHazard(s32 enabled, f32 frequencyhz, f32 volume, f32 pan);
void accessibilityToneSetCombatSlot(s32 slot, s32 enabled, f32 frequencyhz,
		f32 volume, f32 pan, s32 periodms, s32 durationms, s32 continuous,
		s32 restart, s32 triggernow);
void accessibilityToneStopCombat(void);
void accessibilityToneSetTrackerSlot(s32 slot, s32 enabled, f32 frequencyhz,
		f32 volume, f32 pan, s32 periodms, s32 height, s32 rear,
		s32 restart);
void accessibilityToneStopTracker(void);
void accessibilityTonePlayCaneSlot(s32 slot, f32 startfrequencyhz,
		f32 endfrequencyhz, f32 volume, f32 pan, s32 durationms);
void accessibilityToneStopCane(void);
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
void accessibilityToneGetDiagnostics(struct accessibilitytonediagnostics *diagnostics);
#endif
const s16 *accessibilityToneMix(const s16 *input, u32 len);

#endif
