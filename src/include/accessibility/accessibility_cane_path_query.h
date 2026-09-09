#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_PATH_QUERY_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_PATH_QUERY_H

#include "accessibility/accessibility_cane_path.h"
#include "accessibility/accessibility_observer.h"

struct accessibilitycanepathdiagnostic {
	struct accessibilitycanepathresult path;
	u64 elapsedus;
	s32 queries;
	s32 floorqueries;
	s32 movequeries;
	s32 errors;
	s32 budget; /* 1 query cap, 2 elapsed-time cap */
	f32 lastdistance;
	f32 lastground;
	f32 lastpointground;
	s32 lastclearance;
	uintptr_t blocker;
};

/* Caller excludes the observer perimeter. Never moves a live player or plays audio. */
void accessibilityCaneQueryPath(const struct accessibilityobserver *observer,
		const struct coord *direction, f32 reach, f32 terrainthreshold,
		f32 dropthreshold, struct accessibilitycanepathdiagnostic *result);
#endif
