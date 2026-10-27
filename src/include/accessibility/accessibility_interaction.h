#ifndef _IN_ACCESSIBILITY_INTERACTION_H
#define _IN_ACCESSIBILITY_INTERACTION_H

#include <PR/ultratypes.h>

struct prop;
enum accessibilityinteractioncue {
	ACCESSIBILITY_INTERACTION_DEFAULT,
	ACCESSIBILITY_INTERACTION_CORRECT,
	ACCESSIBILITY_INTERACTION_RESET,
	ACCESSIBILITY_INTERACTION_COMPLETED,
	ACCESSIBILITY_INTERACTION_PENDING,
};

/* Semantic visual state only; the scanner still owns all eligibility gates. */
s32 accessibilityInteractionGetCue(struct prop *prop);

#endif
