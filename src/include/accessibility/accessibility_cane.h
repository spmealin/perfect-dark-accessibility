#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_H

#include <PR/ultratypes.h>

void accessibilityCaneTick(void);
void accessibilityCaneReset(const char *reason);

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
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
