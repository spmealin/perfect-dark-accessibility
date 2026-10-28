/* Differential policy/materialization regression, no live geometry or audio.
 * The reference is the frozen pre-refactor adapter, never a runtime fallback. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/accessibility/accessibility_cane_result.c"
#include "../../src/accessibility/accessibility_cane_path.c"
#include "../../src/accessibility/accessibility_cane_decision.c"
#include "../../src/accessibility/accessibility_cane.c"
#include "fixtures/cane_decision_reference.inc"

struct g_vars g_Vars;
u64 sysGetMicroseconds(void) { return 0; }

int main(void)
{
	const s32 results[] = { CDRESULT_ERROR, CDRESULT_NOCOLLISION, CDRESULT_COLLISION };
	const s32 terrains[] = { -2, -1, 0, 1 };
	struct accessibilitycanesample seed, reference, actual;
	struct accessibilityobserver observer = {0};
	struct prop blocker = {0}, original = {0};
	struct player player = {0};
	s32 r, t, flags, ladder, stop, cue, contact, runway, size, count = 0;
	u32 actions = 0, crouchactions = 0;
	observer.origin = (struct coord){ .x = 10, .y = 20, .z = 30 };
	observer.camera = (struct coord){ .x = 10, .y = 90, .z = 30 };
	blocker.type = PROPTYPE_OBJ;
	original.type = PROPTYPE_DOOR;
	player.vv_manground = 123;
	for (size = 0; size < 2; size++)
	for (r = 0; r < ARRAYCOUNT(results); r++)
	for (t = 0; t < ARRAYCOUNT(terrains); t++)
	for (flags = 0; flags < 16; flags++)
	for (ladder = 0; ladder < 3; ladder++)
	for (stop = ACCESSIBILITY_CANE_PATH_NOT_RUN; stop <= ACCESSIBILITY_CANE_PATH_UNSUPPORTED; stop++)
	for (cue = ACCESSIBILITY_CANE_CUE_NONE; cue <= ACCESSIBILITY_CANE_CUE_LADDER; cue++)
	for (contact = 0; contact < 3; contact++)
	for (runway = 0; runway < 3; runway++) {
		struct accessibilitycanepathresult *path;
		struct accessibilitycanedecision decision;
		f32 rd, ad;
		s32 rr = results[r], ar = results[r];
		memset(&seed, 0, sizeof(seed));
		seed.radius = size ? 60 : 30;
		seed.result = results[r];
		seed.terrain = terrains[t];
		seed.drop = terrains[t] == -2;
		seed.terraindistance = 100;
		seed.terrainheight = terrains[t] * 20;
		seed.dropheightthreshold = 80;
		seed.terraintraversaltested = 1;
		seed.terrainclearancequeries = contact;
		seed.terrainclearanceresult = results[r];
		seed.terrainclearancedistance = contact ? 200 : 0;
		seed.gradecontinuous = !!(flags & 4);
		seed.terrainplateau = !!(flags & 8);
		seed.geoflags = ladder == 1 ? (flags & 4)
				? GEOFLAG_LADDER_PLAYERONLY : GEOFLAG_LADDER : 0;
		seed.ladderprobehit = ladder == 2;
		seed.observervehicle = !!(flags & 8);
		seed.direction = (struct coord){ .x = .6f, .y = 0, .z = .8f };
		seed.audiosource = (struct coord){ .x = 51, .y = 52, .z = 53 };
		seed.rawhit = (struct coord){ .x = 81, .y = 82, .z = 83 };
		seed.normal = seed.edge1 = seed.edge2 = seed.direction;
		seed.obstacle = &original;
		seed.obstacletype = original.type;
		seed.crouchpassage = !!(flags & 2);
		seed.crouchpass = 1;
		seed.crouchresult = CDRESULT_NOCOLLISION;
		seed.pathdiagnostic.blocker = (uintptr_t)&blocker;
		path = &seed.pathdiagnostic.path;
		path->stop = stop;
		path->cue = cue;
		path->direction = terrains[t] > 0 ? 1 : -1;
		path->cuedistance = 100;
		path->cuedelta = path->direction * 40;
		path->finaldelta = path->direction * 90;
		path->reached = path->stopdistance = 100 + seed.radius * (runway + 1);
		if (runway == 0 && stop == ACCESSIBILITY_CANE_PATH_WALL) {
			path->reached = path->stopdistance = 80;
		}
		path->requiredheight = 60;
		path->nodes[0].height = 180;
		path->standingcontinuation = runway == 2 ? 1000 : 0;
		path->count = stop == ACCESSIBILITY_CANE_PATH_NOT_RUN ? 0 : 2;
		rd = ad = contact == 0 ? -1 : contact == 1 ? 80 : 400;
		accessibilityCaneEvaluateSample(&seed, rd, !!(flags & 1), !!(flags & 2));
		seed.legacycue = seed.evaluation.cue;
		reference = actual = seed;
		g_Vars.currentplayer = count & 1 ? &player : NULL;
		referenceCaneApplyDropValidation(&reference, &observer, &rd, &rr);
		referenceCaneApplyTerrainValidation(&reference, &observer, &rd, &rr,
				!!(flags & 1), !!(flags & 2));
		referenceCaneApplyCrouchValidation(&reference, &observer, &rd, &rr);
		accessibilityCaneResolveDecision(&seed.evaluation, path, ladder == 1,
				seed.radius * 2, seed.radius * 2, 80, &decision);
		actions |= 1u << decision.primary.action;
		crouchactions |= 1u << decision.crouch.action;
		accessibilityCaneResolveSample(&actual, &observer, &ad, &ar);
		if (memcmp(&reference, &actual, sizeof(actual)) || rd != ad || rr != ar) {
			const unsigned char *a = (const unsigned char *)&reference;
			const unsigned char *b = (const unsigned char *)&actual;
			size_t offset = 0;
			while (offset < sizeof(actual) && a[offset] == b[offset]) offset++;
			fprintf(stderr, "Cane decision mismatch #%d r=%d t=%d flags=%d ladder=%d stop=%d cue=%d contact=%d runway=%d size=%d byte=%zu distance=%f/%f result=%d/%d cue=%d/%d\n",
					count, r, t, flags, ladder, stop, cue, contact, runway, size,
					offset, rd, ad, rr, ar, reference.evaluation.cue, actual.evaluation.cue);
			return 1;
		}
		count++;
	}
	assert((actions & ((1u << (ACCESSIBILITY_CANE_DECISION_EDGE + 1)) - 1))
			== ((1u << (ACCESSIBILITY_CANE_DECISION_EDGE + 1)) - 1));
	assert(crouchactions & (1u << ACCESSIBILITY_CANE_DECISION_CROUCH));
	assert(crouchactions & (1u << ACCESSIBILITY_CANE_DECISION_CROUCH_DEAD_END));
	printf("Cane decision: %d differential fixtures matched all sample fields, contacts, cues and diagnostics.\n", count);
	return 0;
}
