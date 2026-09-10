#include <math.h>
#include <string.h>
#include "accessibility/accessibility_cane_path.h"

enum accessibilitycanedropvalidation accessibilityCaneValidateDrop(
		const struct accessibilitycanepathresult *result,
		float legacydropdistance, float minimumrunway)
{
	if (result->stop == ACCESSIBILITY_CANE_PATH_EDGE
			&& result->cue == ACCESSIBILITY_CANE_CUE_DROP) {
		return ACCESSIBILITY_CANE_DROP_CONFIRMED_EDGE;
	}
	/* The legacy vertical probe can look beyond a real connected slope and
	 * report the later wall as a drop. Preserve the nearer traversable terrain
	 * when it has the same sustained-runway proof required for an ordinary
	 * path cue. */
	if (result->cue == ACCESSIBILITY_CANE_CUE_TERRAIN
			&& result->direction < 0
			&& accessibilityCaneValidateTerrain(result, minimumrunway)
					== ACCESSIBILITY_CANE_TERRAIN_CONFIRMED) {
		return ACCESSIBILITY_CANE_DROP_CONNECTED_DESCENT;
	}
	if (result->stop == ACCESSIBILITY_CANE_PATH_WALL
			&& result->stopdistance > 0
			&& result->stopdistance <= legacydropdistance) {
		return ACCESSIBILITY_CANE_DROP_BARRIER_FIRST;
	}
	return ACCESSIBILITY_CANE_DROP_FALLBACK;
}

enum accessibilitycaneterrainvalidation accessibilityCaneValidateTerrain(
		const struct accessibilitycanepathresult *result,
		float minimumrunway)
{
	float runway;

	if (!isfinite(minimumrunway) || minimumrunway < 0) {
		return ACCESSIBILITY_CANE_TERRAIN_FALLBACK;
	}
	if (result->stop == ACCESSIBILITY_CANE_PATH_EDGE
			&& result->cue == ACCESSIBILITY_CANE_CUE_DROP) {
		return ACCESSIBILITY_CANE_TERRAIN_EDGE;
	}
	if (result->stop == ACCESSIBILITY_CANE_PATH_RANGE) {
		if (result->cue == ACCESSIBILITY_CANE_CUE_TERRAIN) {
			return ACCESSIBILITY_CANE_TERRAIN_CONFIRMED;
		}
		if (result->cue == ACCESSIBILITY_CANE_CUE_NONE) {
			return ACCESSIBILITY_CANE_TERRAIN_CONNECTED_FLAT;
		}
		return ACCESSIBILITY_CANE_TERRAIN_FALLBACK;
	}
	if (result->stop == ACCESSIBILITY_CANE_PATH_WALL) {
		if (result->cue == ACCESSIBILITY_CANE_CUE_TERRAIN) {
			runway = result->reached - result->cuedistance;
			return runway + 0.01f >= minimumrunway
					? ACCESSIBILITY_CANE_TERRAIN_CONFIRMED
					: ACCESSIBILITY_CANE_TERRAIN_BARRIER_FIRST;
		}
		if (result->cue == ACCESSIBILITY_CANE_CUE_NONE
				|| result->cue == ACCESSIBILITY_CANE_CUE_BARRIER) {
			return ACCESSIBILITY_CANE_TERRAIN_BARRIER_FIRST;
		}
	}
	return ACCESSIBILITY_CANE_TERRAIN_FALLBACK;
}

enum accessibilitycanecrouchvalidation accessibilityCaneValidateCrouch(
		const struct accessibilitycanepathresult *result,
		float minimumcontinuation)
{
	float continuation;

	if (!isfinite(minimumcontinuation) || minimumcontinuation < 0
			|| result->cue != ACCESSIBILITY_CANE_CUE_CROUCH
			|| result->count < 2
			|| result->requiredheight <= 0
			|| result->requiredheight >= result->nodes[0].height - 1.0f) {
		return ACCESSIBILITY_CANE_CROUCH_FALLBACK;
	}
	if (result->stop != ACCESSIBILITY_CANE_PATH_RANGE
			&& result->stop != ACCESSIBILITY_CANE_PATH_WALL) {
		return ACCESSIBILITY_CANE_CROUCH_FALLBACK;
	}

	continuation = result->reached - result->cuedistance;
	if (continuation + 0.01f < minimumcontinuation) {
		return ACCESSIBILITY_CANE_CROUCH_SHORT;
	}
	if (result->stop == ACCESSIBILITY_CANE_PATH_WALL
			&& result->standingcontinuation + 0.01f < minimumcontinuation) {
		return ACCESSIBILITY_CANE_CROUCH_DEAD_END;
	}
	return ACCESSIBILITY_CANE_CROUCH_CONFIRMED;
}

void accessibilityCaneBuildTerrainPhrase(
		const struct accessibilitycanepathresult *result, float minimumdelta,
		float terminalwalldistance, struct accessibilitycanepathphrase *phrase)
{
	int indices[ACCESSIBILITY_CANE_PATH_PHRASE_STEPS];
	int candidatecount;
	int floorcount;
	int haswall;
	int flatstart;
	int i;

	if (!phrase) return;
	memset(phrase, 0, sizeof(*phrase));
	if (!result || !isfinite(minimumdelta) || minimumdelta < 0.0f
			|| result->cue != ACCESSIBILITY_CANE_CUE_TERRAIN
			|| result->count < 2) return;

	haswall = isfinite(terminalwalldistance) && terminalwalldistance > 0.0f;
	candidatecount = result->count;
	floorcount = ACCESSIBILITY_CANE_PATH_PHRASE_STEPS - (haswall ? 1 : 0);
	if (candidatecount < floorcount) floorcount = candidatecount;

	for (i = 0; i < floorcount; i++) {
		indices[i] = floorcount == 1 ? result->count - 1
				: i * (candidatecount - 1) / (floorcount - 1);
	}

	/* Preserve a real landing at the end of a long downsampled profile. The
	 * final two atoms then hold the same absolute pitch rather than making the
	 * last stair look as though it continues into the terminal wall. */
	flatstart = result->count - 1;
	while (flatstart > 1 && fabsf(result->nodes[flatstart].floor.ground
			- result->nodes[flatstart - 1].floor.ground) <= minimumdelta) {
		flatstart--;
	}
	if (floorcount >= 2 && flatstart < result->count - 1
			&& (floorcount < 3 || indices[floorcount - 3] < flatstart)) {
		indices[floorcount - 2] = flatstart;
		indices[floorcount - 1] = result->count - 1;
	}

	for (i = 0; i < floorcount; i++) {
		int index = indices[i];
		phrase->steps[phrase->count].distance = result->nodes[index].distance;
		phrase->steps[phrase->count].elevation
				= result->nodes[index].floor.ground
				- result->nodes[0].floor.ground;
		phrase->count++;
	}
	phrase->floorcount = phrase->count;
	if (haswall && phrase->count < ACCESSIBILITY_CANE_PATH_PHRASE_STEPS) {
		phrase->steps[phrase->count].distance = terminalwalldistance;
		phrase->steps[phrase->count].elevation
				= phrase->count > 0
						? phrase->steps[phrase->count - 1].elevation : 0.0f;
		phrase->count++;
	}
}

static int accessibilityCanePathIsDrop(const struct accessibilitycanepathfloor *floor,
		const struct accessibilitycanepathfloor *previous, float threshold)
{
	/* Point support locates the physical edge even while the footprint overlaps it. */
	return !floor->supported || !floor->pointsupported
			|| (previous->pointsupported
					&& floor->pointground < previous->pointground - threshold);
}

void accessibilityCaneTracePath(const struct accessibilitycanepathinput *input,
		const struct accessibilitycanepathqueries *queries,
		struct accessibilitycanepathresult *result)
{
	struct accessibilitycanepathnode next;
	float step;
	float minimumstep;
	float baseground;
	float height;
	float standingrunstart = -1.0f;
	int reductions = 0;
	memset(result, 0, sizeof(*result));
	result->stop = ACCESSIBILITY_CANE_PATH_UNCERTAIN;
	if (!isfinite(input->reach) || !isfinite(input->radius)
			|| !isfinite(input->height) || !isfinite(input->dropthreshold)
			|| !isfinite(input->maxrise) || !isfinite(input->terrainthreshold)
			|| input->reach <= 0 || input->radius <= 0 || input->height <= 0
			|| input->dropthreshold <= 0 || input->maxrise <= 0
			|| input->terrainthreshold <= 0) return;
	height = input->height;
	step = input->radius;
	minimumstep = input->radius * 0.125f;
	result->nodes[0].height = height;
	if (!queries->floor(queries->context, NULL, 0, &result->nodes[0].floor)
			|| !result->nodes[0].floor.supported) return;
	result->count = 1;
	baseground = result->nodes[0].floor.ground;
	result->requiredheight = height;
	while (result->count < ACCESSIBILITY_CANE_PATH_NODES) {
		struct accessibilitycanepathnode *previous = &result->nodes[result->count - 1];
		enum accessibilitycaneevidence clearance;
		int i;
		float delta;
		memset(&next, 0, sizeof(next));
		next.distance = previous->distance + step;
		if (next.distance > input->reach) next.distance = input->reach;
		next.height = height;
		if (!queries->floor(queries->context, previous, next.distance, &next.floor)) return;

		if (accessibilityCanePathIsDrop(&next.floor, &previous->floor, input->dropthreshold)) {
			struct accessibilitycanepathnode safe = *previous;
			float unsafe = next.distance;
			/* Refine against the last connected floor, not the player's original height.
			 * Advance safe only after clearance succeeds; never infer an edge through a wall. */
			for (i = 0; i < ACCESSIBILITY_CANE_PATH_REFINEMENTS; i++) {
				struct accessibilitycanepathnode middle = {0};
				middle.distance = (safe.distance + unsafe) * 0.5f;
				middle.height = height;
				if (!queries->floor(queries->context, &safe, middle.distance, &middle.floor)) return;
				result->refinements++;
				if (accessibilityCanePathIsDrop(&middle.floor, &safe.floor, input->dropthreshold)) {
					unsafe = middle.distance;
				} else {
					/* A high shelf is not a proven safe bracket. */
					if (middle.floor.ground - safe.floor.ground > input->maxrise) return;
					clearance = queries->move(queries->context, &safe, &middle, height);
					if (clearance != ACCESSIBILITY_CANE_CLEAR) {
						if (clearance == ACCESSIBILITY_CANE_BLOCKED) {
							result->stop = ACCESSIBILITY_CANE_PATH_WALL;
							result->cue = ACCESSIBILITY_CANE_CUE_BARRIER;
							result->cuedistance = middle.distance;
							result->stopdistance = middle.distance;
						}
						return;
					}
					safe = middle;
				}
			}
			/* A multi-step descent encountered during refinement is followed normally. */
			if (next.floor.supported && next.floor.pointsupported
					&& !accessibilityCanePathIsDrop(&next.floor, &safe.floor, input->dropthreshold)) {
				if (safe.distance <= previous->distance) return;
				next = safe;
			} else {
				struct accessibilitycanepathnode edge = safe;
				edge.distance = unsafe;
				clearance = queries->move(queries->context, &safe, &edge, height);
				if (clearance != ACCESSIBILITY_CANE_CLEAR) {
					if (clearance == ACCESSIBILITY_CANE_BLOCKED) {
						result->stop = ACCESSIBILITY_CANE_PATH_WALL;
						result->cue = ACCESSIBILITY_CANE_CUE_BARRIER;
						result->cuedistance = unsafe;
						result->stopdistance = unsafe;
					}
					return;
				}
				result->stop = ACCESSIBILITY_CANE_PATH_EDGE;
				result->cue = ACCESSIBILITY_CANE_CUE_DROP;
				result->direction = -1;
				result->cuedistance = (safe.distance + unsafe) * 0.5f;
				result->stopdistance = result->cuedistance;
				result->edgewidth = unsafe - safe.distance;
				result->reached = safe.distance;
				return;
			}
		}

		delta = next.floor.ground - previous->floor.ground;
		if (delta > input->maxrise) {
			if (step > minimumstep && reductions < ACCESSIBILITY_CANE_PATH_REFINEMENTS) {
				step *= 0.5f;
				reductions++;
				result->refinements++;
				continue;
			}
			return; /* Insufficient proof that a high landing is reachable. */
		}
		clearance = queries->move(queries->context, previous, &next, height);
		for (i = 0; clearance == ACCESSIBILITY_CANE_BLOCKED && i < 2; i++) {
			float lower = input->lowerheights[i];
			if (lower <= 0 || lower >= height - 1.0f) continue;
			clearance = queries->move(queries->context, previous, &next, lower);
			if (clearance == ACCESSIBILITY_CANE_CLEAR) {
				height = lower;
				result->requiredheight = height;
				result->cue = ACCESSIBILITY_CANE_CUE_CROUCH;
				result->cuedistance = previous->distance;
			}
		}
		if (clearance != ACCESSIBILITY_CANE_CLEAR) {
			if (clearance == ACCESSIBILITY_CANE_BLOCKED) {
				result->stop = ACCESSIBILITY_CANE_PATH_WALL;
				/* Retain a prior verified terrain/stance transition as a separate fact. */
				if (result->cue == ACCESSIBILITY_CANE_CUE_NONE) {
					result->cue = ACCESSIBILITY_CANE_CUE_BARRIER;
					result->cuedistance = next.distance;
				}
				result->stopdistance = next.distance;
			}
			return;
		}
		next.height = height;
		result->nodes[result->count++] = next;
		result->reached = next.distance;
		result->finaldelta = next.floor.ground - baseground;
		if (height < input->height - 1.0f && queries->clearance) {
			enum accessibilitycaneevidence standing = queries->clearance(
					queries->context, &next, input->height);
			if (standing == ACCESSIBILITY_CANE_CLEAR) {
				float standingcontinuation;
				if (standingrunstart < 0.0f) standingrunstart = next.distance;
				standingcontinuation = next.distance - standingrunstart;
				if (standingcontinuation > result->standingcontinuation) {
					result->standingrecoverydistance = standingrunstart;
					result->standingcontinuation = standingcontinuation;
				}
			} else {
				standingrunstart = -1.0f;
			}
		}
		if (result->cue == ACCESSIBILITY_CANE_CUE_NONE
				&& fabsf(result->finaldelta) >= input->terrainthreshold) {
			result->cue = ACCESSIBILITY_CANE_CUE_TERRAIN;
			result->direction = result->finaldelta > 0 ? 1 : -1;
			result->cuedistance = next.distance;
			result->cuedelta = result->finaldelta;
		}
		if (next.distance >= input->reach) {
			result->stop = ACCESSIBILITY_CANE_PATH_RANGE;
			result->stopdistance = next.distance;
			return;
		}
		step = input->radius;
		reductions = 0;
	}
	result->stop = ACCESSIBILITY_CANE_PATH_CAPACITY;
	result->stopdistance = result->reached;
}
