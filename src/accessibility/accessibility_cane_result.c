#include <string.h>
#include "accessibility/accessibility_cane_result.h"

void accessibilityCaneEvaluateResult(
		const struct accessibilitycaneobservation *observation,
		struct accessibilitycaneresult *result)
{
	int traversable = 0;
	int nearbarrier;

	memset(result, 0, sizeof(*result));
	result->observation = *observation;
	result->runway = -1.0f;

	if (observation->barrier == ACCESSIBILITY_CANE_UNKNOWN) {
		result->reasons |= ACCESSIBILITY_CANE_REASON_QUERY_ERROR;
		return;
	}

	if (observation->terrainfound && observation->terrain > 0
			&& observation->traversalapplicable) {
		if (observation->clearancequeries == 0) {
			result->reasons |= ACCESSIBILITY_CANE_REASON_RISE_UNTESTED;
		} else if (observation->lastclearance == ACCESSIBILITY_CANE_BLOCKED) {
			result->riseclearance = ACCESSIBILITY_CANE_BLOCKED;
			result->reasons |= ACCESSIBILITY_CANE_REASON_CLEARANCE_BLOCKED;
		} else if (observation->lastclearance == ACCESSIBILITY_CANE_UNKNOWN) {
			result->reasons |= ACCESSIBILITY_CANE_REASON_CLEARANCE_ERROR;
		} else if (observation->clearancedistance + 0.01f
				< observation->terraindistance) {
			result->reasons |= ACCESSIBILITY_CANE_REASON_CLEARANCE_INCOMPLETE;
		} else {
			result->riseclearance = ACCESSIBILITY_CANE_CLEAR;
			traversable = 1;
		}
		/* Preserve the accepted output policy; unknown is not collision evidence. */
		result->legacyblockedrise = !traversable;
	}

	if (observation->terrainfound) {
		if (!observation->drop && observation->barrierdistance > 0.0f) {
			result->runway = observation->barrierdistance
					- observation->terraindistance;
			if (result->runway < 0.0f) {
				result->runway = 0.0f;
			}
			result->shortdeadend = result->runway < observation->minimumrunway;
			if (result->shortdeadend) {
				result->reasons |= ACCESSIBILITY_CANE_REASON_SHORT_RUNWAY;
			}
		}
		result->gradesafe = observation->gradecontinuous && !observation->plateau
				&& (observation->terrain < 0 || traversable);
		if (result->gradesafe) {
			result->reasons |= ACCESSIBILITY_CANE_REASON_CURRENT_GRADE;
		}
		if (traversable && observation->plateau) {
			result->reasons |= ACCESSIBILITY_CANE_REASON_PLATEAU;
		}
		result->terraincedes = result->gradesafe
				|| (traversable && observation->plateau) || result->shortdeadend;
		if (observation->drop
				&& observation->barrierdistance > observation->terraindistance) {
			result->dropbarriergap = observation->barrierdistance
					- observation->terraindistance;
			result->dropbarriersuppressed = result->dropbarriergap
					<= observation->dropbarrierclearance;
			if (result->dropbarriersuppressed) {
				result->reasons |= ACCESSIBILITY_CANE_REASON_DROP_AT_BARRIER;
				result->terraincedes = 1;
			}
		}
	}

	nearbarrier = !observation->terrainfound || result->terraincedes
			|| observation->terraindistance + observation->radius
					>= observation->barrierdistance;
	result->crouchmerge = observation->barrier == ACCESSIBILITY_CANE_BLOCKED
			&& !observation->drop && nearbarrier && observation->crouchfound
			&& observation->terrainfound
			&& observation->terraindistance < observation->barrierdistance;
	if (result->crouchmerge) {
		result->reasons |= ACCESSIBILITY_CANE_REASON_CROUCH_MERGE;
	}

	if (observation->terrainfound && !result->terraincedes
			&& (observation->barrierdistance < 0.0f
					|| observation->terraindistance < observation->barrierdistance)
			&& !result->crouchmerge) {
		result->source = ACCESSIBILITY_CANE_SOURCE_TERRAIN;
		result->cue = result->legacyblockedrise ? ACCESSIBILITY_CANE_CUE_BARRIER
				: observation->drop ? ACCESSIBILITY_CANE_CUE_DROP
				: ACCESSIBILITY_CANE_CUE_TERRAIN;
	} else if (observation->barrier == ACCESSIBILITY_CANE_BLOCKED) {
		result->source = ACCESSIBILITY_CANE_SOURCE_BARRIER;
		result->cue = observation->ladder ? ACCESSIBILITY_CANE_CUE_LADDER
				: observation->crouchfound ? ACCESSIBILITY_CANE_CUE_CROUCH
				: ACCESSIBILITY_CANE_CUE_BARRIER;
	}
}
