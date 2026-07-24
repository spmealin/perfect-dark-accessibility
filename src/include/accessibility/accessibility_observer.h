#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_OBSERVER_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_OBSERVER_H

#include <ultra64.h>
#include "types.h"

struct accessibilityobserver {
	struct prop *prop;
	struct coord origin;
	struct coord camera;
	struct coord look;
	RoomNum room;
	f32 ground;
	f32 radius;
	f32 ymin;
	f32 ymax;
	s32 isremote;
};

s32 accessibilityObserverGet(struct accessibilityobserver *observer);

#endif
