#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "accessibility/accessibility_cane_path.h"

struct fixture {
	float slope;
	float stair;
	float edge;
	float wall;
	float ceiling;
	float base;
	int calls;
	int failafter;
};

static int floorquery(void *context, const struct accessibilitycanepathnode *from,
		float distance, struct accessibilitycanepathfloor *floor)
{
	struct fixture *f = context;
	(void)from;
	if (++f->calls > f->failafter) return 0;
	memset(floor, 0, sizeof(*floor));
	floor->supported = floor->pointsupported = distance < f->edge;
	floor->ground = f->base + f->slope * distance + floorf(distance / 30) * f->stair;
	floor->pointground = floor->ground;
	floor->platform = 123;
	return 1;
}

static enum accessibilitycaneevidence movequery(void *context,
		const struct accessibilitycanepathnode *from,
		const struct accessibilitycanepathnode *to, float height)
{
	struct fixture *f = context;
	assert(to->distance >= from->distance);
	if (++f->calls > f->failafter) return ACCESSIBILITY_CANE_UNKNOWN;
	return to->distance >= f->wall || (to->distance >= 60 && height > f->ceiling)
			? ACCESSIBILITY_CANE_BLOCKED : ACCESSIBILITY_CANE_CLEAR;
}

int main(void)
{
	struct accessibilitycanepathinput input = {180, 30, 160, {115, 80}, 30, 12, 80};
	struct fixture f = {0, 0, 10000, 10000, 10000, 0, 0, 10000};
	struct accessibilitycanepathqueries queries = {&f, floorquery, movequery};
	struct accessibilitycanepathresult result;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_RANGE && result.count == 7);
	assert(result.cue == ACCESSIBILITY_CANE_CUE_NONE);
	assert(accessibilityCaneValidateDrop(&result, 90) == ACCESSIBILITY_CANE_DROP_FALLBACK);
	f.stair = -17;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_RANGE);
	assert(result.cue == ACCESSIBILITY_CANE_CUE_TERRAIN && result.direction == -1);
	assert(result.finaldelta == -102);
	assert(accessibilityCaneValidateDrop(&result, 90)
			== ACCESSIBILITY_CANE_DROP_CONNECTED_DESCENT);
	f.stair = 17;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_RANGE && result.direction == 1);
	f.stair = 0;
	f.edge = 77;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_EDGE);
	assert(fabsf(result.cuedistance - 77) < 1 && result.edgewidth < 1);
	assert(accessibilityCaneValidateDrop(&result, 90)
			== ACCESSIBILITY_CANE_DROP_CONFIRMED_EDGE);
	f.edge = 10000;
	f.wall = 70;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_WALL);
	assert(accessibilityCaneValidateDrop(&result, 100)
			== ACCESSIBILITY_CANE_DROP_BARRIER_FIRST);
	assert(accessibilityCaneValidateDrop(&result, 80)
			== ACCESSIBILITY_CANE_DROP_FALLBACK); /* Same unresolved segment. */
	f.stair = -17;
	f.wall = 150;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_WALL && result.reached == 120);
	assert(accessibilityCaneValidateDrop(&result, 90)
			== ACCESSIBILITY_CANE_DROP_CONNECTED_DESCENT);
	f.wall = 10000;
	f.edge = 10000;
	f.stair = 17;
	f.ceiling = 100;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_RANGE);
	assert(result.cue == ACCESSIBILITY_CANE_CUE_CROUCH && result.requiredheight == 80);
	input.height = 80;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.cue == ACCESSIBILITY_CANE_CUE_TERRAIN);
	f.stair = 0;
	f.base = 400;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.finaldelta == 0 && result.nodes[0].floor.platform == 123);
	f.base = -100;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.finaldelta == 0); /* New elevator position is a fresh reference. */
	f.calls = 0;
	f.failafter = 4;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_UNCERTAIN);
	f.failafter = 10000;
	input.reach = 5000;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_CAPACITY && result.count == 32);
	f.edge = 0;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_UNCERTAIN && result.count == 0);
	puts("cane path: all fixtures passed");
	return 0;
}
