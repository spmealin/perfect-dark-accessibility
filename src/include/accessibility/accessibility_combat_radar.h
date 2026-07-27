#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_COMBAT_RADAR_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_COMBAT_RADAR_H

#include <PR/ultratypes.h>

struct coord;
struct prop;

void accessibilityCombatRadarCaptureBegin(s32 available);
void accessibilityCombatRadarCaptureSetOwnMarker(s32 ownmarker);
void accessibilityCombatRadarCaptureDot(struct prop *prop,
		const struct coord *relative, u32 colour1, u32 colour2,
		s32 swapcolours, s32 yindicators);
void accessibilityCombatRadarCaptureEnd(void);
void accessibilityCombatRadarTick(void);
void accessibilityCombatRadarReset(const char *reason);

#endif
