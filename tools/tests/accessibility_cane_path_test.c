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
	float ceilingend;
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
	return to->distance >= f->wall || (to->distance >= 60
			&& to->distance < f->ceilingend && height > f->ceiling)
			? ACCESSIBILITY_CANE_BLOCKED : ACCESSIBILITY_CANE_CLEAR;
}

static enum accessibilitycaneevidence clearancequery(void *context,
		const struct accessibilitycanepathnode *at, float height)
{
	struct fixture *f = context;
	if (++f->calls > f->failafter) return ACCESSIBILITY_CANE_UNKNOWN;
	return at->distance >= f->wall || (at->distance >= 60
			&& at->distance < f->ceilingend && height > f->ceiling)
			? ACCESSIBILITY_CANE_BLOCKED : ACCESSIBILITY_CANE_CLEAR;
}

int main(void)
{
	struct accessibilitycanepathinput input = {180, 30, 160, {115, 80}, 30, 12, 80};
	struct fixture f = {
		.edge = 10000, .wall = 10000, .ceiling = 10000,
		.ceilingend = 10000, .failafter = 10000,
	};
	struct accessibilitycanepathqueries queries = {
		.context = &f, .floor = floorquery, .move = movequery,
		.clearance = clearancequery,
	};
	struct accessibilitycanepathresult result;
	struct accessibilitycanepathphrase phrase;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_RANGE && result.count == 7);
	assert(result.cue == ACCESSIBILITY_CANE_CUE_NONE);
	assert(accessibilityCaneValidateDrop(&result, 90, 60)
			== ACCESSIBILITY_CANE_DROP_FALLBACK);
	assert(accessibilityCaneValidateTerrain(&result, 120)
			== ACCESSIBILITY_CANE_TERRAIN_CONNECTED_FLAT);
	f.stair = -17;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_RANGE);
	assert(result.cue == ACCESSIBILITY_CANE_CUE_TERRAIN && result.direction == -1);
	assert(result.cuedelta == -17);
	assert(result.finaldelta == -102);
	assert(accessibilityCaneValidateDrop(&result, 90, 60)
			== ACCESSIBILITY_CANE_DROP_CONNECTED_DESCENT);
	assert(accessibilityCaneValidateTerrain(&result, 120)
			== ACCESSIBILITY_CANE_TERRAIN_CONFIRMED);
	f.stair = 17;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_RANGE && result.direction == 1);
	f.stair = 0;
	f.edge = 77;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_EDGE);
	assert(fabsf(result.cuedistance - 77) < 1 && result.edgewidth < 1);
	assert(accessibilityCaneValidateDrop(&result, 90, 60)
			== ACCESSIBILITY_CANE_DROP_CONFIRMED_EDGE);
	assert(accessibilityCaneValidateTerrain(&result, 120)
			== ACCESSIBILITY_CANE_TERRAIN_EDGE);
	f.edge = 10000;
	f.wall = 70;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_WALL);
	assert(accessibilityCaneValidateDrop(&result, 100, 60)
			== ACCESSIBILITY_CANE_DROP_BARRIER_FIRST);
	assert(accessibilityCaneValidateDrop(&result, 80, 60)
			== ACCESSIBILITY_CANE_DROP_FALLBACK); /* Same unresolved segment. */
	assert(accessibilityCaneValidateTerrain(&result, 120)
			== ACCESSIBILITY_CANE_TERRAIN_BARRIER_FIRST);
	f.stair = -17;
	f.wall = 150;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_WALL && result.reached == 120);
	assert(accessibilityCaneValidateTerrain(&result, 60)
			== ACCESSIBILITY_CANE_TERRAIN_CONFIRMED);
	assert(accessibilityCaneValidateDrop(&result, 200, 60)
			== ACCESSIBILITY_CANE_DROP_CONNECTED_DESCENT);
	assert(accessibilityCaneValidateDrop(&result, 90, 120)
			== ACCESSIBILITY_CANE_DROP_FALLBACK);
	assert(accessibilityCaneValidateTerrain(&result, 120)
			== ACCESSIBILITY_CANE_TERRAIN_BARRIER_FIRST);
	f.wall = 180;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_WALL && result.reached == 150);
	/* Sustained connected descent wins even when a farther wall lies before
	 * the legacy probe's reported drop position. */
	assert(accessibilityCaneValidateDrop(&result, 200, 120)
			== ACCESSIBILITY_CANE_DROP_CONNECTED_DESCENT);
	assert(accessibilityCaneValidateTerrain(&result, 120)
			== ACCESSIBILITY_CANE_TERRAIN_CONFIRMED);
	f.wall = 10000;
	f.edge = 10000;
	f.stair = 17;
	f.ceiling = 100;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_RANGE);
	assert(result.cue == ACCESSIBILITY_CANE_CUE_CROUCH && result.requiredheight == 80);
	assert(accessibilityCaneValidateTerrain(&result, 120)
			== ACCESSIBILITY_CANE_TERRAIN_FALLBACK);
	assert(accessibilityCaneValidateCrouch(&result, 60)
			== ACCESSIBILITY_CANE_CROUCH_CONFIRMED);
	assert(accessibilityCaneValidateCrouch(&result, 180)
			== ACCESSIBILITY_CANE_CROUCH_SHORT);
	f.wall = 90;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.cue == ACCESSIBILITY_CANE_CUE_CROUCH
			&& result.stop == ACCESSIBILITY_CANE_PATH_WALL);
	assert(accessibilityCaneValidateCrouch(&result, 60)
			== ACCESSIBILITY_CANE_CROUCH_SHORT);
	f.wall = 120;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(accessibilityCaneValidateCrouch(&result, 60)
			== ACCESSIBILITY_CANE_CROUCH_DEAD_END);
	/* A bounded low obstruction is a passage only after the route recovers
	 * standing clearance and proves continuation beyond that recovery. */
	f.wall = 180;
	f.ceilingend = 90;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.standingrecoverydistance == 90);
	assert(result.standingcontinuation == 60);
	assert(accessibilityCaneValidateCrouch(&result, 60)
			== ACCESSIBILITY_CANE_CROUCH_CONFIRMED);
	f.ceilingend = 10000;
	memset(&result, 0, sizeof(result));
	result.cue = ACCESSIBILITY_CANE_CUE_TERRAIN;
	result.stop = ACCESSIBILITY_CANE_PATH_WALL;
	result.cuedistance = 30;
	result.count = 4;
	result.nodes[0].distance = 0;
	result.nodes[1].distance = 30;
	result.nodes[1].floor.ground = 10;
	result.nodes[2].distance = 60;
	result.nodes[2].floor.ground = 20;
	result.nodes[3].distance = 90;
	result.nodes[3].floor.ground = 20.5f;
	accessibilityCaneBuildTerrainPhrase(&result, 1.0f, 120.0f, &phrase);
	assert(phrase.count == 5);
	assert(phrase.floorcount == 4);
	assert(phrase.steps[0].distance == 0 && phrase.steps[0].elevation == 0);
	assert(phrase.steps[1].distance == 30 && phrase.steps[1].elevation == 10);
	assert(phrase.steps[2].distance == 60 && phrase.steps[2].elevation == 20);
	assert(phrase.steps[3].distance == 90 && phrase.steps[3].elevation == 20.5f);
	assert(phrase.steps[4].distance == 120);
	result.nodes[1].floor.ground = -10;
	result.nodes[2].floor.ground = -20;
	result.nodes[3].floor.ground = -20.5f;
	accessibilityCaneBuildTerrainPhrase(&result, 1.0f, 120.0f, &phrase);
	assert(phrase.count == 5 && phrase.floorcount == 4);
	assert(phrase.steps[0].elevation == 0);
	assert(phrase.steps[1].elevation == -10);
	assert(phrase.steps[2].elevation == -20);
	assert(phrase.steps[3].elevation == -20.5f);
	assert(phrase.steps[4].distance == 120);
	/* Long profiles are evenly reduced to the fixed mixer capacity and still
	 * reserve their final atom for the terminal wall. */
	memset(&result, 0, sizeof(result));
	result.cue = ACCESSIBILITY_CANE_CUE_TERRAIN;
	result.stop = ACCESSIBILITY_CANE_PATH_WALL;
	result.cuedistance = 30;
	result.count = 10;
	for (int i = 0; i < result.count; i++) {
		result.nodes[i].distance = i * 30;
		result.nodes[i].floor.ground = i * 10;
	}
	accessibilityCaneBuildTerrainPhrase(&result, 1.0f, 300.0f, &phrase);
	assert(phrase.count == ACCESSIBILITY_CANE_PATH_PHRASE_STEPS);
	assert(phrase.floorcount == ACCESSIBILITY_CANE_PATH_PHRASE_STEPS - 1);
	assert(phrase.steps[phrase.count - 1].distance == 300);
	/* Flat, descending stairs, landing, then a farther wall. Absolute
	 * elevations preserve both level regions and the proportional descent. */
	memset(&result, 0, sizeof(result));
	result.cue = ACCESSIBILITY_CANE_CUE_TERRAIN;
	result.stop = ACCESSIBILITY_CANE_PATH_RANGE;
	result.count = 16;
	for (int i = 0; i < result.count; i++) {
		result.nodes[i].distance = i * 30;
		result.nodes[i].floor.ground = i < 8 ? 0
				: (i < 14 ? -(i - 7) * 17.0f : -102.0f);
	}
	accessibilityCaneBuildTerrainPhrase(&result, 1.0f, 648.0f, &phrase);
	assert(phrase.count == ACCESSIBILITY_CANE_PATH_PHRASE_STEPS);
	assert(phrase.floorcount == ACCESSIBILITY_CANE_PATH_PHRASE_STEPS - 1);
	assert(phrase.steps[0].elevation == 0);
	assert(phrase.steps[1].elevation == 0);
	assert(phrase.steps[phrase.count - 3].elevation == -102.0f);
	assert(phrase.steps[phrase.count - 2].elevation == -102.0f);
	assert(phrase.steps[phrase.count - 1].distance == 648);
	f.wall = 10000;
	input.height = 80;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.cue == ACCESSIBILITY_CANE_CUE_TERRAIN);
	assert(accessibilityCaneValidateCrouch(&result, 60)
			== ACCESSIBILITY_CANE_CROUCH_FALLBACK);
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
	assert(accessibilityCaneValidateTerrain(&result, 120)
			== ACCESSIBILITY_CANE_TERRAIN_FALLBACK);
	f.failafter = 10000;
	input.reach = 5000;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_CAPACITY && result.count == 32);
	assert(result.cue == ACCESSIBILITY_CANE_CUE_NONE);
	assert(accessibilityCaneValidateTerrain(&result, 120)
			== ACCESSIBILITY_CANE_TERRAIN_CONNECTED_FLAT);
	f.stair = -17;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_CAPACITY && result.count == 32);
	assert(result.cue == ACCESSIBILITY_CANE_CUE_TERRAIN);
	assert(accessibilityCaneValidateTerrain(&result, 120)
			== ACCESSIBILITY_CANE_TERRAIN_CONFIRMED);
	f.stair = 0;
	f.edge = 0;
	accessibilityCaneTracePath(&input, &queries, &result);
	assert(result.stop == ACCESSIBILITY_CANE_PATH_UNCERTAIN && result.count == 0);
	puts("cane path: all fixtures passed");
	return 0;
}
