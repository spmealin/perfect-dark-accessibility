#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_PATH_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_PATH_H

#include <stdint.h>
#include "accessibility/accessibility_cane_result.h"

#define ACCESSIBILITY_CANE_PATH_NODES 32
#define ACCESSIBILITY_CANE_PATH_REFINEMENTS 5
#define ACCESSIBILITY_CANE_PATH_QUERY_LIMIT 192
#define ACCESSIBILITY_CANE_PATH_TIME_LIMIT_US 2000

enum accessibilitycanepathstop {
	ACCESSIBILITY_CANE_PATH_NOT_RUN,
	ACCESSIBILITY_CANE_PATH_RANGE,
	ACCESSIBILITY_CANE_PATH_WALL,
	ACCESSIBILITY_CANE_PATH_EDGE,
	ACCESSIBILITY_CANE_PATH_UNCERTAIN,
	ACCESSIBILITY_CANE_PATH_CAPACITY,
	ACCESSIBILITY_CANE_PATH_UNSUPPORTED,
};

enum accessibilitycanedropvalidation {
	ACCESSIBILITY_CANE_DROP_NOT_RUN,
	ACCESSIBILITY_CANE_DROP_CONFIRMED_EDGE,
	ACCESSIBILITY_CANE_DROP_BARRIER_FIRST,
	ACCESSIBILITY_CANE_DROP_CONNECTED_DESCENT,
	ACCESSIBILITY_CANE_DROP_FALLBACK,
};

struct accessibilitycanepathfloor {
	int supported;
	int pointsupported;
	float ground;
	float pointground;
	int room;
	unsigned int flags;
	uintptr_t platform;
	short rooms[8];
};

struct accessibilitycanepathnode {
	float distance;
	struct accessibilitycanepathfloor floor;
	float height;
};

struct accessibilitycanepathinput {
	float reach;
	float radius;
	float height;
	float lowerheights[2];
	float maxrise;
	float terrainthreshold;
	float dropthreshold;
};

/* Floor callbacks distinguish a completed no-support query from an error.
 * Movement checks must cover the segment, vertical adjustment and destination.
 * UNKNOWN includes an adapter budget expiration; it never means open space. */
struct accessibilitycanepathqueries {
	void *context;
	int (*floor)(void *context, const struct accessibilitycanepathnode *from,
			float distance, struct accessibilitycanepathfloor *floor);
	enum accessibilitycaneevidence (*move)(void *context,
			const struct accessibilitycanepathnode *from,
			const struct accessibilitycanepathnode *to, float height);
};

struct accessibilitycanepathresult {
	enum accessibilitycanepathstop stop;
	enum accessibilitycanecue cue;
	int direction;
	float cuedistance;
	float stopdistance;
	float reached;
	float finaldelta;
	float edgewidth;
	float requiredheight;
	int count;
	int refinements;
	struct accessibilitycanepathnode nodes[ACCESSIBILITY_CANE_PATH_NODES];
};

void accessibilityCaneTracePath(const struct accessibilitycanepathinput *input,
		const struct accessibilitycanepathqueries *queries,
		struct accessibilitycanepathresult *result);
enum accessibilitycanedropvalidation accessibilityCaneValidateDrop(
		const struct accessibilitycanepathresult *result,
		float legacydropdistance);

#endif
