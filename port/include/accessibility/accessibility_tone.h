#ifndef _IN_PORT_ACCESSIBILITY_ACCESSIBILITY_TONE_H
#define _IN_PORT_ACCESSIBILITY_ACCESSIBILITY_TONE_H

#include <PR/ultratypes.h>

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
struct accessibilitytonediagnostics {
	s32 toneenabled;
	s32 chirpenabled;
	s32 chirpsequence;
	s32 hazardenabled;
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
void accessibilityToneStopChirp(void);
void accessibilityToneSetHazard(s32 enabled, f32 frequencyhz, f32 volume, f32 pan);
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
void accessibilityToneGetDiagnostics(struct accessibilitytonediagnostics *diagnostics);
#endif
const s16 *accessibilityToneMix(const s16 *input, u32 len);

#endif
