#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_INCIDENT_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_INCIDENT_H

#include <ultra64.h>
#include "types.h"

void accessibilityIncidentTick(void);
void accessibilityIncidentReset(const char *reason);
void accessibilityIncidentRecordHoverbikeMove(struct coord *requestedvelocity,
		f32 angledelta, s32 result, struct prop *obstacle);

#endif
