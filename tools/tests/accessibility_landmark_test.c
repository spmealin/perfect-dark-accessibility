/* Registry/eligibility regression test, not a gameplay or audibility test.
 * Include the adapter to exercise its private policy without exporting test APIs.
 * Compile with -O2 -fwhole-program to discard unused game tick code.
 * From MinGW64, after configuring the game:
 * cc @build/CMakeFiles/pd.dir/includes_C.rsp -DPLATFORM_64BIT=1 -DVERSION=2
 *   -DPAL=0 -D_LANGUAGE_C=1 -std=c11 -O2 -fwhole-program
 *   tools/tests/accessibility_landmark_test.c -o build/accessibility_landmark_test.exe
 */
#include <assert.h>
#include <stdio.h>
#include "../../src/accessibility/accessibility_landmark.c"

struct g_vars g_Vars;
u32 g_StageFlags;
static struct defaultobj pillars[5];
static struct prop props[5];
static s32 healthy = true;
static struct defaultobj puzzlerock;
static struct prop puzzlerockprop;

struct defaultobj *objFindByTagId(s32 tag)
{
	if (tag == 0x4c) return &puzzlerock;
	if (tag >= 0x2a && tag <= 0x2e) {
		return &pillars[tag - 0x2a];
	}
	/* Simulate objective aliases shuffled onto different physical pillars. */
	return tag >= 1 && tag <= 3 ? &pillars[5 - tag] : NULL;
}

bool objIsHealthy(struct defaultobj *obj)
{
	(void)obj;
	return healthy;
}

s32 objectiveGetCount(void) { return 0; }
u32 objectiveGetDifficultyBits(s32 index) { (void)index; return 0; }
s32 objectiveCheck(s32 index) { (void)index; return OBJECTIVE_COMPLETE; }
s32 lvGetDifficulty(void) { return 0; }

int main(void)
{
	const char *reason;
	s32 i;
	s32 count = 0;
	s32 stageentries = 0;

	g_Vars.stagenum = STAGE_SKEDARRUINS;
	/* All amplifiers placed: physical-pillar cues must remain eligible. */
	g_StageFlags = 0x00000700;
	for (i = 0; i < 5; i++) {
		pillars[i].prop = &props[i];
		props[i].obj = &pillars[i];
		props[i].type = PROPTYPE_OBJ;
		props[i].active = true;
		pillars[i].flags = OBJFLAG_INVINCIBLE;
	}
	for (i = 0; i < ARRAYCOUNT(g_AccessibilityLandmarkSpecs); i++) {
		const struct accessibilitylandmarkspec *spec = &g_AccessibilityLandmarkSpecs[i];
		struct defaultobj *obj;

		if (spec->stage == STAGE_SKEDARRUINS) stageentries++;
		if (spec->toneprofile != ACCESSIBILITY_TONE_LANDMARK_PILLAR) continue;
		assert(spec->stage == STAGE_SKEDARRUINS);
		assert(spec->tag == 0x2a + count);
		assert(spec->objective == -1 && spec->completionflag == 0);
		assert(!spec->requirevulnerable && spec->pad == -1);
		obj = objFindByTagId(spec->tag);
		assert(accessibilityLandmarkFindSpec(obj->prop) == spec);
		assert(accessibilityLandmarkObjectEligible(spec, obj, &reason));
		healthy = false;
		assert(!accessibilityLandmarkObjectEligible(spec, obj, &reason));
		assert(strcmp(reason, "object_destroyed") == 0);
		healthy = true;
		obj->hidden = OBJHFLAG_GONE;
		assert(!accessibilityLandmarkObjectEligible(spec, obj, &reason));
		obj->hidden = 0;
		count++;
	}
	assert(count == 5);
	puzzlerockprop.type = PROPTYPE_OBJ;
	puzzlerockprop.obj = &puzzlerock;
	puzzlerock.prop = &puzzlerockprop;
	/* The rock is scanner-owned, not an independently sounding landmark.
	 * Its pressure pad retains its own completion-tag rule and voice. */
	assert(!accessibilityLandmarkOwnsProp(&puzzlerockprop));
	assert(stageentries <= ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT);
	puts("Pillar registry, shuffled aliases, completion independence, destruction and capacity: PASS");
	return 0;
}
