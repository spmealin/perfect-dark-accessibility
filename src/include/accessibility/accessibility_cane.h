#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_H

#include <PR/ultratypes.h>
#include "types.h"

void accessibilityCaneTick(void);
void accessibilityCaneReset(const char *reason);
void accessibilityCaneObserveHoverbikeMove(struct coord *requestedvelocity,
		s32 result);

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
/* Per-window counters; tick includes queries, scheduling, audio commands and logs. */
struct accessibilitycaneprofile {
	u64 tickcalls;
	u64 tickus;
	u64 tickmaxus;
	u64 querycalls;
	u64 observationus;
	u64 evaluationus;
	u64 publishus;
	u64 logus;
	u64 logmaxus;
	u64 unknownrises;
	u64 blockedrises;
	u64 clearrises;
	u64 queryretries;
	u64 queryerrors;
	u64 pathcalls;
	u64 pathus;
	u64 pathmaxus;
	u64 pathqueries;
	u64 pathretries;
	u64 pathbudgets;
	u64 patherrors;
	u64 pathdifferences;
	u64 corridorcalls;
	u64 corridorus;
	u64 corridormaxus;
	u64 corridorqueries;
	u64 corridorretries;
	u64 corridorbudgets;
	u64 corridorerrors;
	u64 corridorbounded;
	u64 corridorguided;
	u64 corridorbroad;
	u64 corridoruncertain;
	u64 corridorsuppressed;
};

void accessibilityCaneTakeProfile(struct accessibilitycaneprofile *profile);

struct accessibilitycanediagnostics {
	u64 queries;
	u64 hits;
	u64 misses;
	u64 skipped;
	u64 sweeps;
	u64 missedcycles;
	u64 querytotalus;
	u64 querymaxus;
};

void accessibilityCaneGetDiagnostics(
		struct accessibilitycanediagnostics *diagnostics);
#endif

#endif
