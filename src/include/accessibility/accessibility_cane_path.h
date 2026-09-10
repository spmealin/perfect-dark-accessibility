#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_PATH_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_CANE_PATH_H

#include <stdint.h>
#include "accessibility/accessibility_cane_result.h"

#define ACCESSIBILITY_CANE_PATH_NODES 32
#define ACCESSIBILITY_CANE_PATH_REFINEMENTS 5
#define ACCESSIBILITY_CANE_PATH_QUERY_LIMIT 192
#define ACCESSIBILITY_CANE_PATH_TIME_LIMIT_US 2000
#define ACCESSIBILITY_CANE_PATH_PHRASE_STEPS 10

enum accessibilitycanepathphrasestep {
	ACCESSIBILITY_CANE_PATH_PHRASE_NONE,
	ACCESSIBILITY_CANE_PATH_PHRASE_LEVEL,
	ACCESSIBILITY_CANE_PATH_PHRASE_UP,
	ACCESSIBILITY_CANE_PATH_PHRASE_DOWN,
	ACCESSIBILITY_CANE_PATH_PHRASE_WALL,
};

struct accessibilitycanepathphrase {
	int count;
	unsigned int bits;
	struct {
		float distance;
		float elevation;
	} steps[ACCESSIBILITY_CANE_PATH_PHRASE_STEPS];
};

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

enum accessibilitycaneterrainvalidation {
	ACCESSIBILITY_CANE_TERRAIN_NOT_RUN,
	ACCESSIBILITY_CANE_TERRAIN_CONFIRMED,
	ACCESSIBILITY_CANE_TERRAIN_BARRIER_FIRST,
	ACCESSIBILITY_CANE_TERRAIN_CONNECTED_FLAT,
	ACCESSIBILITY_CANE_TERRAIN_EDGE,
	ACCESSIBILITY_CANE_TERRAIN_FALLBACK,
};

enum accessibilitycanecrouchvalidation {
	ACCESSIBILITY_CANE_CROUCH_NOT_RUN,
	ACCESSIBILITY_CANE_CROUCH_CONFIRMED,
	ACCESSIBILITY_CANE_CROUCH_SHORT,
	ACCESSIBILITY_CANE_CROUCH_DEAD_END,
	ACCESSIBILITY_CANE_CROUCH_FALLBACK,
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
	enum accessibilitycaneevidence (*clearance)(void *context,
			const struct accessibilitycanepathnode *at, float height);
};

struct accessibilitycanepathresult {
	enum accessibilitycanepathstop stop;
	enum accessibilitycanecue cue;
	int direction;
	float cuedistance;
	float cuedelta;
	float stopdistance;
	float reached;
	float finaldelta;
	float edgewidth;
	float requiredheight;
	float standingrecoverydistance;
	float standingcontinuation;
	int count;
	int refinements;
	struct accessibilitycanepathnode nodes[ACCESSIBILITY_CANE_PATH_NODES];
};

void accessibilityCaneTracePath(const struct accessibilitycanepathinput *input,
		const struct accessibilitycanepathqueries *queries,
		struct accessibilitycanepathresult *result);
enum accessibilitycanedropvalidation accessibilityCaneValidateDrop(
		const struct accessibilitycanepathresult *result,
		float legacydropdistance, float minimumrunway);
enum accessibilitycaneterrainvalidation accessibilityCaneValidateTerrain(
		const struct accessibilitycanepathresult *result,
		float minimumrunway);
enum accessibilitycanecrouchvalidation accessibilityCaneValidateCrouch(
		const struct accessibilitycanepathresult *result,
		float minimumcontinuation);
void accessibilityCaneBuildTerrainPhrase(
		const struct accessibilitycanepathresult *result, float minimumdelta,
		float terminalwalldistance, struct accessibilitycanepathphrase *phrase);

#endif
