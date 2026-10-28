#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_LOG_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_LOG_H

#include <PR/ultratypes.h>

#define ACCESSIBILITY_LOG_PATH "$S/accessibility.log"

s32 accessibilityLogInit(void);
void accessibilityLogShutdown(void);
s32 accessibilityLogIsOpen(void);

// This logger is main-thread-only until synchronization is added.
void accessibilityLogEvent(const char *category, const char *event, const char *fmt, ...)
		__attribute__((format(printf, 3, 4)));
void accessibilityLogEventMessage(const char *category, const char *event,
		const char *message);

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
/* Main-thread, fixed-storage window counters. Explicit flush time is separate
 * from serialization/write time, which includes implicit stdio buffer drains. */
struct accessibilitylogdiagnostics {
	u64 events, formatcalls, formatus, formatmaxus, writeus, writemaxus;
	u64 flushes, flushus, flushmaxus;
	u64 allocationattempts, allocations, allocatedbytes, frees, allocationfailures;
	u64 iofailures;
	char worstwritecategory[64], worstwriteevent[64];
};
void accessibilityLogGetDiagnostics(struct accessibilitylogdiagnostics *diagnostics);
void accessibilityLogReportPerformance(const char *reason);
#endif

#endif
