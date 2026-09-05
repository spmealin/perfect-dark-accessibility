#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_RESULT_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_RESULT_H

/* Value-only contract: no engine pointers, queries, clocks, or audio calls. */
enum accessibilitycaneevidence {
	ACCESSIBILITY_CANE_UNKNOWN,
	ACCESSIBILITY_CANE_CLEAR,
	ACCESSIBILITY_CANE_BLOCKED,
};

enum accessibilitycanereason {
	ACCESSIBILITY_CANE_REASON_QUERY_ERROR = 1 << 0,
	ACCESSIBILITY_CANE_REASON_RISE_UNTESTED = 1 << 1,
	ACCESSIBILITY_CANE_REASON_CLEARANCE_ERROR = 1 << 2,
	ACCESSIBILITY_CANE_REASON_CLEARANCE_BLOCKED = 1 << 3,
	ACCESSIBILITY_CANE_REASON_CLEARANCE_INCOMPLETE = 1 << 4,
	ACCESSIBILITY_CANE_REASON_CURRENT_GRADE = 1 << 5,
	ACCESSIBILITY_CANE_REASON_PLATEAU = 1 << 6,
	ACCESSIBILITY_CANE_REASON_SHORT_RUNWAY = 1 << 7,
	ACCESSIBILITY_CANE_REASON_DROP_AT_BARRIER = 1 << 8,
	ACCESSIBILITY_CANE_REASON_CROUCH_MERGE = 1 << 9,
};

enum accessibilitycanecue {
	ACCESSIBILITY_CANE_CUE_NONE,
	ACCESSIBILITY_CANE_CUE_BARRIER,
	ACCESSIBILITY_CANE_CUE_TERRAIN,
	ACCESSIBILITY_CANE_CUE_DROP,
	ACCESSIBILITY_CANE_CUE_CROUCH,
	ACCESSIBILITY_CANE_CUE_LADDER,
};

enum accessibilitycanesource {
	ACCESSIBILITY_CANE_SOURCE_NONE,
	ACCESSIBILITY_CANE_SOURCE_BARRIER,
	ACCESSIBILITY_CANE_SOURCE_TERRAIN,
};

struct accessibilitycaneobservation {
	/* CLEAR means this barrier query returned no collision, not a safe route. */
	enum accessibilitycaneevidence barrier;
	float barrierdistance;
	int terrainfound;
	int terrain; /* existing contour: +1 rise, -1 descent, -2 drop */
	float terraindistance;
	float terrainheight;
	int drop;
	int traversalapplicable;
	int clearancequeries;
	enum accessibilitycaneevidence lastclearance;
	float clearancedistance;
	int gradecontinuous;
	int plateau;
	float radius;
	float minimumrunway;
	float dropbarrierclearance;
	int crouchfound;
	int ladder;
};

struct accessibilitycaneresult {
	struct accessibilitycaneobservation observation;
	/* Evidence at sampled raised positions; never claims a connected route. */
	enum accessibilitycaneevidence riseclearance;
	unsigned int reasons;
	int legacyblockedrise;
	int shortdeadend;
	int gradesafe;
	int dropbarriersuppressed;
	int terraincedes;
	int crouchmerge;
	float runway; /* legacy contact-distance difference, not proven free travel */
	float dropbarriergap;
	enum accessibilitycanecue cue;
	enum accessibilitycanesource source;
};

void accessibilityCaneEvaluateResult(
		const struct accessibilitycaneobservation *observation,
		struct accessibilitycaneresult *result);

#endif
