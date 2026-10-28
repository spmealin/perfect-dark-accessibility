#include <string.h>
#include "accessibility/accessibility_cane_decision.h"

static void accessibilityCaneDecisionBarrier(
		struct accessibilitycaneobservation *working,
		const struct accessibilitycanepathresult *path,
		struct accessibilitycanedecisionstep *step,
		struct accessibilitycaneresult *result, int crouchfound)
{
	step->distance = path->stopdistance;
	step->preservecontact = working->barrier == ACCESSIBILITY_CANE_BLOCKED
			&& working->barrierdistance > 0.0f
			&& working->barrierdistance <= step->distance + working->radius;
	if (step->preservecontact) step->distance = working->barrierdistance;
	working->barrier = ACCESSIBILITY_CANE_BLOCKED;
	working->barrierdistance = step->distance;
	working->drop = 0;
	working->terrain = 0;
	working->terrainfound = 0;
	working->crouchfound = crouchfound;
	accessibilityCaneEvaluateResult(working, result);
}

void accessibilityCaneResolveDecision(
		const struct accessibilitycaneresult *legacy,
		const struct accessibilitycanepathresult *path,
		int laddergeometry, float minimumrunway, float minimumcontinuation,
		float dropheight, struct accessibilitycanedecision *decision)
{
	struct accessibilitycaneobservation working = legacy->observation;
	const int legacyterrainfound = working.terrainfound;
	const int crouchfound = working.crouchfound;
	struct accessibilitycaneresult *result = &decision->result;
	struct accessibilitycanedecisionstep *step = &decision->primary;

	memset(decision, 0, sizeof(*decision));
	*result = *legacy;

	/* Preserve established precedence: validate a selected drop first. Even an
	 * uncertain drop validation leaves terrain fallback alone. */
	if (result->cue == ACCESSIBILITY_CANE_CUE_DROP) {
		decision->dropvalidation = accessibilityCaneValidateDrop(path,
				working.terraindistance, minimumrunway);
		step->distance = path->cuedistance;
		switch (decision->dropvalidation) {
		case ACCESSIBILITY_CANE_DROP_CONFIRMED_EDGE:
			step->action = ACCESSIBILITY_CANE_DECISION_DROP_EDGE;
			working.terraindistance = step->distance;
			result->observation.terraindistance = step->distance;
			break;
		case ACCESSIBILITY_CANE_DROP_BARRIER_FIRST:
			step->action = ACCESSIBILITY_CANE_DECISION_DROP_BARRIER;
			step->distance = path->stopdistance;
			working.drop = 0;
			working.terrain = 0;
			working.barrier = ACCESSIBILITY_CANE_BLOCKED;
			working.barrierdistance = step->distance;
			result->observation.barrier = working.barrier;
			result->observation.barrierdistance = step->distance;
			result->cue = ACCESSIBILITY_CANE_CUE_BARRIER;
			result->source = ACCESSIBILITY_CANE_SOURCE_BARRIER;
			break;
		case ACCESSIBILITY_CANE_DROP_CONNECTED_DESCENT:
			step->action = ACCESSIBILITY_CANE_DECISION_DESCENT;
			working.drop = 0;
			working.terrain = -1;
			working.terrainheight = path->finaldelta;
			working.terraindistance = step->distance;
			result->observation.drop = working.drop;
			result->observation.terrain = working.terrain;
			result->observation.terrainheight = working.terrainheight;
			result->observation.terraindistance = step->distance;
			result->cue = ACCESSIBILITY_CANE_CUE_TERRAIN;
			result->source = ACCESSIBILITY_CANE_SOURCE_TERRAIN;
			break;
		default:
			break;
		}
	}

	if (decision->dropvalidation == ACCESSIBILITY_CANE_DROP_NOT_RUN
			&& result->cue != ACCESSIBILITY_CANE_CUE_LADDER
			&& (result->cue != ACCESSIBILITY_CANE_CUE_CROUCH
				|| path->cue == ACCESSIBILITY_CANE_CUE_TERRAIN)
			&& !laddergeometry
			&& (legacyterrainfound || path->cue == ACCESSIBILITY_CANE_CUE_TERRAIN)) {
		decision->terrainvalidation = accessibilityCaneValidateTerrain(path, minimumrunway);
		step->distance = path->cuedistance;
		switch (decision->terrainvalidation) {
		case ACCESSIBILITY_CANE_TERRAIN_CONFIRMED:
			step->action = ACCESSIBILITY_CANE_DECISION_TERRAIN;
			working.drop = 0;
			working.terrain = path->direction;
			working.terrainheight = path->cuedelta;
			working.terraindistance = step->distance;
			working.traversalapplicable = 1;
			working.lastclearance = ACCESSIBILITY_CANE_CLEAR;
			working.clearancequeries = 1;
			working.clearancedistance = path->reached;
			working.terrainfound = 1;
			working.crouchfound = crouchfound;
			accessibilityCaneEvaluateResult(&working, result);
			result->cue = ACCESSIBILITY_CANE_CUE_TERRAIN;
			result->source = ACCESSIBILITY_CANE_SOURCE_TERRAIN;
			result->terraincedes = 0;
			result->shortdeadend = 0;
			break;
		case ACCESSIBILITY_CANE_TERRAIN_BARRIER_FIRST:
			step->action = ACCESSIBILITY_CANE_DECISION_BARRIER;
			accessibilityCaneDecisionBarrier(&working, path, step, result, crouchfound);
			break;
		case ACCESSIBILITY_CANE_TERRAIN_CONNECTED_FLAT:
			if (legacyterrainfound) {
				step->action = ACCESSIBILITY_CANE_DECISION_FLAT;
				working.drop = 0;
				working.terrain = 0;
				working.terrainfound = 0;
				working.crouchfound = crouchfound;
				accessibilityCaneEvaluateResult(&working, result);
			}
			break;
		case ACCESSIBILITY_CANE_TERRAIN_EDGE:
			step->action = ACCESSIBILITY_CANE_DECISION_EDGE;
			working.drop = 1;
			working.terrain = -2;
			working.terrainheight = -dropheight;
			working.terraindistance = step->distance;
			working.terrainfound = 1;
			working.crouchfound = 0;
			accessibilityCaneEvaluateResult(&working, result);
			break;
		default:
			break;
		}
	}

	/* A ladder remains more specific than a low passage. A completed dead end
	 * is negative evidence; incomplete paths retain the established fallback. */
	step = &decision->crouch;
	if (path->cue == ACCESSIBILITY_CANE_CUE_CROUCH
			&& result->cue != ACCESSIBILITY_CANE_CUE_LADDER && !laddergeometry) {
		decision->crouchvalidation = accessibilityCaneValidateCrouch(path, minimumcontinuation);
		if (decision->crouchvalidation == ACCESSIBILITY_CANE_CROUCH_DEAD_END) {
			step->action = ACCESSIBILITY_CANE_DECISION_CROUCH_DEAD_END;
			accessibilityCaneDecisionBarrier(&working, path, step, result, 0);
		} else if (decision->crouchvalidation == ACCESSIBILITY_CANE_CROUCH_CONFIRMED) {
			step->action = ACCESSIBILITY_CANE_DECISION_CROUCH;
			step->distance = path->cuedistance + working.radius;
			if (step->distance > path->reached) step->distance = path->reached;
			if (step->distance < 0.0f) step->distance = 0.0f;
			working.barrier = ACCESSIBILITY_CANE_BLOCKED;
			working.barrierdistance = step->distance;
			working.drop = 0;
			working.terrain = 0;
			working.terrainfound = 0;
			working.crouchfound = 1;
			accessibilityCaneEvaluateResult(&working, result);
		}
	}
	decision->barrierdistance = working.barrierdistance;
}
