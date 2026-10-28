#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_DECISION_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_DECISION_H

#include "accessibility/accessibility_cane_path.h"

/* Value-only reconciliation. No engine pointers, geometry queries or audio. */
enum accessibilitycanedecisionaction {
	ACCESSIBILITY_CANE_DECISION_KEEP,
	ACCESSIBILITY_CANE_DECISION_DROP_EDGE,
	ACCESSIBILITY_CANE_DECISION_DROP_BARRIER,
	ACCESSIBILITY_CANE_DECISION_DESCENT,
	ACCESSIBILITY_CANE_DECISION_TERRAIN,
	ACCESSIBILITY_CANE_DECISION_BARRIER,
	ACCESSIBILITY_CANE_DECISION_FLAT,
	ACCESSIBILITY_CANE_DECISION_EDGE,
	ACCESSIBILITY_CANE_DECISION_CROUCH_DEAD_END,
	ACCESSIBILITY_CANE_DECISION_CROUCH,
};

struct accessibilitycanedecisionstep {
	enum accessibilitycanedecisionaction action;
	float distance;
	int preservecontact;
};

struct accessibilitycanedecision {
	struct accessibilitycaneresult result;
	struct accessibilitycanedecisionstep primary;
	struct accessibilitycanedecisionstep crouch;
	enum accessibilitycanedropvalidation dropvalidation;
	enum accessibilitycaneterrainvalidation terrainvalidation;
	enum accessibilitycanecrouchvalidation crouchvalidation;
	float barrierdistance;
};

void accessibilityCaneResolveDecision(
		const struct accessibilitycaneresult *legacy,
		const struct accessibilitycanepathresult *path,
		int laddergeometry, float minimumrunway, float minimumcontinuation,
		float dropheight, struct accessibilitycanedecision *decision);

#endif
