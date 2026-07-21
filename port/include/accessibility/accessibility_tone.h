#ifndef _IN_PORT_ACCESSIBILITY_ACCESSIBILITY_TONE_H
#define _IN_PORT_ACCESSIBILITY_ACCESSIBILITY_TONE_H

#include <PR/ultratypes.h>

void accessibilityToneSet(s32 enabled, f32 frequencyhz);
void accessibilityTonePlayChirp(f32 frequencyhz, f32 volume, f32 pan);
void accessibilityToneStopChirp(void);
const s16 *accessibilityToneMix(const s16 *input, u32 len);

#endif
