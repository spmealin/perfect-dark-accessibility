#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_COMBAT_RADAR_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_COMBAT_RADAR_H

#include <PR/ultratypes.h>

#define ACCESSIBILITY_COMBAT_RADAR_OBJECTIVE_GAIN_SCALE 0.80f

struct coord;
struct prop;

void accessibilityCombatRadarCaptureBegin(s32 available);
void accessibilityCombatRadarCaptureSetOwnMarker(s32 ownmarker);
void accessibilityCombatRadarCaptureDot(struct prop *prop,
		const struct coord *relative, u32 colour1, u32 colour2,
		s32 swapcolours, s32 yindicators);
void accessibilityCombatRadarCaptureEnd(void);
s32 accessibilityCombatRadarGetScenarioObjective(s32 scenario, const struct prop *token,
		struct coord *position);
s32 accessibilityCombatRadarObjectiveDurationMs(f32 distance);
s32 accessibilityCombatRadarIsManualObjectivePlaying(void);
void accessibilityCombatRadarTick(void);
void accessibilityCombatRadarReset(const char *reason);

#endif
