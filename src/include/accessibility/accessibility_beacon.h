#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_BEACON_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_BEACON_H

#include <PR/ultratypes.h>

void accessibilityBeaconTick(void);
void accessibilityBeaconReset(const char *reason, s32 preservecategories);
void accessibilityBeaconDumpDiagnostics(u64 captureid);

#endif
