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
	s32 clearancequeries;
	s32 retries;
	s32 errors;
	s32 budget; /* 1 query cap, 2 elapsed-time cap */
	f32 lastdistance;
	f32 lastground;
	f32 lastpointground;
	s32 lastclearance;
	uintptr_t blocker;
};

#define ACCESSIBILITY_CANE_CORRIDOR_CLASSIFICATION_STATIONS 3
#define ACCESSIBILITY_CANE_CORRIDOR_STATIONS 4
#define ACCESSIBILITY_CANE_CORRIDOR_REFINEMENTS 3

enum accessibilitycanecorridorclassification {
	ACCESSIBILITY_CANE_CORRIDOR_NOT_RUN,
	ACCESSIBILITY_CANE_CORRIDOR_BOUNDED,
	ACCESSIBILITY_CANE_CORRIDOR_GUIDED,
	ACCESSIBILITY_CANE_CORRIDOR_BROAD,
	ACCESSIBILITY_CANE_CORRIDOR_UNCERTAIN,
};

enum accessibilitycanecorridorsideevidence {
	ACCESSIBILITY_CANE_CORRIDOR_SIDE_UNKNOWN,
	ACCESSIBILITY_CANE_CORRIDOR_SIDE_BOUNDARY,
	ACCESSIBILITY_CANE_CORRIDOR_SIDE_OPEN,
};

enum accessibilitycanecorridorsidekind {
	ACCESSIBILITY_CANE_CORRIDOR_KIND_UNKNOWN,
	ACCESSIBILITY_CANE_CORRIDOR_KIND_NONE,
	ACCESSIBILITY_CANE_CORRIDOR_KIND_BARRIER,
	ACCESSIBILITY_CANE_CORRIDOR_KIND_EDGE,
	ACCESSIBILITY_CANE_CORRIDOR_KIND_TERRAIN,
};

struct accessibilitycanecorridorside {
	enum accessibilitycanecorridorsideevidence evidence;
	enum accessibilitycanecorridorsidekind kind;
	f32 distance;
	struct coord point;
};

struct accessibilitycanecorridorstation {
	f32 pathdistance;
	s32 posttransition;
	struct coord center;
	struct accessibilitycanecorridorside left;
	struct accessibilitycanecorridorside right;
};

struct accessibilitycanecorridordiagnostic {
	enum accessibilitycanecorridorclassification classification;
	s32 stationcount;
	s32 boundedstations;
	s32 leftguidedstations;
	s32 rightguidedstations;
	s32 broadstations;
	s32 uncertainstations;
	f32 maxwidth;
	u64 elapsedus;
	s32 queries;
	s32 floorqueries;
	s32 movequeries;
	s32 retries;
	s32 errors;
	s32 budget;
	struct accessibilitycanecorridorstation
			stations[ACCESSIBILITY_CANE_CORRIDOR_STATIONS];
};

/* Caller excludes the observer perimeter. Never moves a live player or plays audio. */
void accessibilityCaneQueryPath(const struct accessibilityobserver *observer,
		const struct coord *direction, f32 reach, f32 terrainthreshold,
		f32 dropthreshold, struct accessibilitycanepathdiagnostic *result);
/* Tests the independent lateral boundaries of a verified terrain path. A
 * GUIDED result has at least one positively located hard boundary with
 * continuation opposite it, or one paired boundary station. BROAD requires
 * positive traversable evidence on both sides at two stations and no hard
 * station. Query failure is always UNCERTAIN, never open space. */
void accessibilityCaneQueryCorridor(const struct accessibilityobserver *observer,
		const struct coord *direction,
		const struct accessibilitycanepathresult *path, f32 maxwidth,
		struct accessibilitycanecorridordiagnostic *result);
#endif
