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

#endif
