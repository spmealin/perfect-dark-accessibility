#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "accessibility/accessibility_cane_result.h"

static struct accessibilitycaneobservation base(void)
{
	struct accessibilitycaneobservation o = {0};
	o.barrier = ACCESSIBILITY_CANE_BLOCKED;
	o.barrierdistance = 400.0f;
	o.radius = 30.0f;
	o.minimumrunway = 120.0f;
	o.dropbarrierclearance = 30.0f;
	return o;
}

static void fixtures(void)
{
	struct accessibilitycaneobservation o = base();
	struct accessibilitycaneresult r;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.cue == ACCESSIBILITY_CANE_CUE_BARRIER);
	assert(r.source == ACCESSIBILITY_CANE_SOURCE_BARRIER);
	o.barrier = ACCESSIBILITY_CANE_CLEAR;
	o.barrierdistance = -1.0f;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.cue == ACCESSIBILITY_CANE_CUE_NONE);
	o.barrier = ACCESSIBILITY_CANE_UNKNOWN;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.cue == ACCESSIBILITY_CANE_CUE_NONE);
	assert(r.reasons == ACCESSIBILITY_CANE_REASON_QUERY_ERROR);

	/* Session 1788562366, dump 2, sweep 1460, direction -15 degrees. */
	o = base();
	o.barrierdistance = 424.64f;
	o.terrainfound = 1;
	o.terrain = 1;
	o.terraindistance = 423.64f;
	o.terrainheight = 26.94f;
	o.traversalapplicable = 1;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.riseclearance == ACCESSIBILITY_CANE_UNKNOWN);
	assert(r.reasons & ACCESSIBILITY_CANE_REASON_RISE_UNTESTED);
	assert(!(r.reasons & ACCESSIBILITY_CANE_REASON_CLEARANCE_BLOCKED));
	assert(r.shortdeadend && r.legacyblockedrise);
	assert(r.runway == 1.0f && r.cue == ACCESSIBILITY_CANE_CUE_BARRIER);

	/* Actual rejected, failed, partial and accepted clearance are distinct. */
	o.terraindistance = 100.0f;
	o.clearancequeries = 1;
	o.clearancedistance = 100.0f;
	o.lastclearance = ACCESSIBILITY_CANE_BLOCKED;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.riseclearance == ACCESSIBILITY_CANE_BLOCKED);
	assert(r.source == ACCESSIBILITY_CANE_SOURCE_TERRAIN);
	o.lastclearance = ACCESSIBILITY_CANE_UNKNOWN;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.riseclearance == ACCESSIBILITY_CANE_UNKNOWN);
	assert(r.reasons & ACCESSIBILITY_CANE_REASON_CLEARANCE_ERROR);
	o.lastclearance = ACCESSIBILITY_CANE_CLEAR;
	o.clearancedistance = 60.0f;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.reasons & ACCESSIBILITY_CANE_REASON_CLEARANCE_INCOMPLETE);
	o.clearancedistance = 100.0f;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.riseclearance == ACCESSIBILITY_CANE_CLEAR);
	assert(r.cue == ACCESSIBILITY_CANE_CUE_TERRAIN);
	o.plateau = 1;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.cue == ACCESSIBILITY_CANE_CUE_BARRIER);
	o.plateau = 0;
	o.gradecontinuous = 1;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.reasons & ACCESSIBILITY_CANE_REASON_CURRENT_GRADE);

	/* An accessible edge wins over a farther wall, but not a wall at the edge. */
	o = base();
	o.terrainfound = 1;
	o.terrain = -2;
	o.drop = 1;
	o.terraindistance = 100.0f;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.cue == ACCESSIBILITY_CANE_CUE_DROP);
	o.barrierdistance = 125.0f;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.cue == ACCESSIBILITY_CANE_CUE_BARRIER);
	assert(r.reasons & ACCESSIBILITY_CANE_REASON_DROP_AT_BARRIER);

	/* A low passage at a small rise remains a crouch cue; ladders take priority. */
	o = base();
	o.terrainfound = 1;
	o.terrain = 1;
	o.terraindistance = 390.0f;
	o.crouchfound = 1;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.crouchmerge && r.cue == ACCESSIBILITY_CANE_CUE_CROUCH);
	o.ladder = 1;
	accessibilityCaneEvaluateResult(&o, &r);
	assert(r.cue == ACCESSIBILITY_CANE_CUE_LADDER);

	/* Native ladder faces must offer a predominantly horizontal normal. */
	assert(accessibilityCaneLadderNormalIsClimbable(24522.0f, 0.0f, 0.0f));
	assert(accessibilityCaneLadderNormalIsClimbable(1.0f, 0.0f, 0.0f));
	assert(accessibilityCaneLadderNormalIsClimbable(-10.0f, 2.0f, 20.0f));
	assert(!accessibilityCaneLadderNormalIsClimbable(0.0f, 62600.0f, 0.0f));
	assert(!accessibilityCaneLadderNormalIsClimbable(1.0f, 1.0f, 0.0f));
	assert(!accessibilityCaneLadderNormalIsClimbable(0.0f, 0.0f, 0.0f));
	puts("Cane result fixtures passed.");
}

/* Read value-only observations exported from existing cane logs; no ROM needed. */
static int replay(void)
{
	struct accessibilitycaneobservation o;
	struct accessibilitycaneresult r;
	int barrier, clearance, cue, source, deferred, blocked, fields;
	int count = 0;
	int failures = 0;
	for (;;) {
		memset(&o, 0, sizeof(o));
		fields = scanf("%d %f %d %d %f %f %d %d %d %d %f %d %d %f %f %f %d %d %d %d %d %d",
				&barrier, &o.barrierdistance, &o.terrainfound, &o.terrain,
				&o.terraindistance, &o.terrainheight, &o.drop, &o.traversalapplicable,
				&o.clearancequeries, &clearance, &o.clearancedistance,
				&o.gradecontinuous, &o.plateau, &o.radius, &o.minimumrunway,
				&o.dropbarrierclearance, &o.crouchfound, &o.ladder,
				&cue, &source, &deferred, &blocked);
		if (fields == EOF) break;
		if (fields != 22) return 2;
		o.barrier = (enum accessibilitycaneevidence)barrier;
		o.lastclearance = (enum accessibilitycaneevidence)clearance;
		accessibilityCaneEvaluateResult(&o, &r);
		count++;
		if ((int)r.cue != cue || (int)r.source != source
				|| r.terraincedes != deferred || r.legacyblockedrise != blocked) {
			fprintf(stderr, "Replay mismatch %d: cue %d/%d source %d/%d deferred %d/%d blocked %d/%d\n",
					count, r.cue, cue, r.source, source, r.terraincedes, deferred,
					r.legacyblockedrise, blocked);
			failures++;
		}
	}
	printf("Replayed %d cane observations: %d mismatches.\n", count, failures);
	return failures || count == 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--replay") == 0) return replay();
	fixtures();
	return 0;
}
