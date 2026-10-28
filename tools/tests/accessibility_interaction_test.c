/* Synthetic semantic/pulse regression, not an in-game visibility/listening test.
 * Build with the configured include response file, -O2 -fwhole-program. */
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "../../src/accessibility/accessibility_interaction.c"
#include "../../src/accessibility/accessibility_beacon.c"

struct g_vars g_Vars;
static struct multimonitorobj monitors[9];
static struct prop props[9];
static s32 permutation[8];
static s32 xray = 1;
static s32 healthy = 1;
static s32 playable = 1;
static s32 tonecalls;
static f32 tonefrequency;
static struct defaultobj rock, otherrock;
static struct defaultobj *rockmapping;
static struct prop rockprop;
static s32 landmarkowned, movablecalls;

struct defaultobj *objFindByTagId(s32 tag)
{
	if (tag == 0x4c) return rockmapping;
	return tag >= 8 && tag <= 15 ? &monitors[permutation[tag - 8]].base : NULL;
}
bool accessibilityVisibilityIsXrayExposed(struct prop *prop)
{ return xray && (prop->flags & PROPFLAG_ONANYSCREENPREVTICK); }
bool tvscreenIsShowingImage(const struct tvscreen *screen, s32 image)
{ return screen && (uintptr_t)screen->cmdlist == (uintptr_t)image; }
bool objIsHealthy(struct defaultobj *obj) { return healthy; }
bool objIsPotentiallyInteractable(struct prop *prop) { return playable; }
bool accessibilityLandmarkOwnsProp(struct prop *prop) { return landmarkowned; }
u32 propobjGetCiTagId(struct prop *prop) { return 0; }
void accessibilityLogEvent(const char *a, const char *b, const char *fmt, ...) { }
s32 psCalculateVolumeFromDistance(f32 d, f32 a, f32 b, f32 c, s32 full)
{ return full; }
s32 psCalculatePan(struct coord *p, f32 a, f32 b, f32 c, f32 d,
		bool flag, struct pschannel *channel) { return AL_PAN_CENTER; }
void accessibilityTonePlayChirpPattern(f32 frequency, f32 volume, f32 pan,
		s32 pulses, f32 gain) { tonecalls++; tonefrequency = frequency; }
void accessibilityTonePlayMovableLandmark(f32 volume, f32 pan)
{ assert(volume == 1 && pan == 0); movablecalls++; }

static void image(s32 slot, s32 num)
{ monitors[slot].screens[0].cmdlist = (u32 *)(uintptr_t)num; }

static s32 eligible(s32 slot)
{
	u32 tag = 0;
	const char *reason = NULL;
	return accessibilityBeaconObjectEligible(&props[slot], &tag, &reason);
}

static void authoredObjectCases(void)
{
	const s32 stages[] = { -1, STAGE_CITRAINING, STAGE_PELAGIC,
		STAGE_SKEDARRUINS, STAGE_RESCUE, STAGE_AIRBASE };
	const s32 types[] = { PROPTYPE_OBJ, PROPTYPE_WEAPON, PROPTYPE_DOOR, PROPTYPE_CHR };
	struct defaultobj *objects[] = { NULL, &rock, &otherrock };
	struct accessibilitybeaconresult result = {0};
	const char *reason;
	u32 tag;
	s32 stage, type, mapping, object;
	assert(accessibilityInteractionGetKind(NULL) == ACCESSIBILITY_INTERACTION_NATIVE);
	for (stage = 0; stage < ARRAYCOUNT(stages); stage++)
	for (type = 0; type < ARRAYCOUNT(types); type++)
	for (mapping = 0; mapping < ARRAYCOUNT(objects); mapping++)
	for (object = 0; object < ARRAYCOUNT(objects); object++) {
		s32 legacy;
		g_Vars.stagenum = stages[stage];
		rockprop.type = types[type];
		rockprop.obj = objects[object];
		rockmapping = objects[mapping];
		/* Frozen original scanner classification; no lifecycle policy here. */
		legacy = g_Vars.stagenum == STAGE_SKEDARRUINS
				&& rockprop.type == PROPTYPE_OBJ && rockprop.obj
				&& objFindByTagId(0x4c) == rockprop.obj;
		assert((accessibilityInteractionGetKind(&rockprop)
				== ACCESSIBILITY_INTERACTION_PUSHABLE) == legacy);
	}
	g_Vars.stagenum = STAGE_SKEDARRUINS;
	memset(&rock, 0, sizeof(rock));
	memset(&rockprop, 0, sizeof(rockprop));
	rockprop.type = PROPTYPE_OBJ;
	rockprop.active = 1;
	rockprop.obj = rockmapping = &rock;
	rock.prop = &rockprop;
	/* Pushing is not native activation, and must not require an activate flag. */
	rock.flags = OBJFLAG_CANNOT_ACTIVATE | OBJFLAG_DEACTIVATED;
	playable = 0;
	assert(accessibilityBeaconObjectEligible(&rockprop, &tag, &reason));
	assert(tag == 0 && !strcmp(reason, "movable_puzzle_object"));
	landmarkowned = 1;
	assert(!accessibilityBeaconObjectEligible(&rockprop, &tag, &reason));
	assert(!strcmp(reason, "authored_landmark_owned"));
	landmarkowned = 0;
	healthy = 0; assert(!accessibilityBeaconObjectEligible(&rockprop, &tag, &reason)); healthy = 1;
	rockprop.active = 0; assert(!accessibilityBeaconObjectEligible(&rockprop, &tag, &reason)); rockprop.active = 1;
	rock.hidden = OBJHFLAG_GONE; assert(!accessibilityBeaconObjectEligible(&rockprop, &tag, &reason));
	rock.hidden = OBJHFLAG_DELETING; assert(!accessibilityBeaconObjectEligible(&rockprop, &tag, &reason));
	rock.hidden = 0;
	rock.flags2 = OBJFLAG2_INVISIBLE; assert(!accessibilityBeaconObjectEligible(&rockprop, &tag, &reason));
	rock.flags2 = 0;
	rock.prop = NULL; assert(!accessibilityBeaconObjectEligible(&rockprop, &tag, &reason)); rock.prop = &rockprop;
	/* Moving onto/away from the pad cannot change the rock's scanner role. */
	rockprop.pos.x = 9999;
	assert(accessibilityInteractionGetKind(&rockprop) == ACCESSIBILITY_INTERACTION_PUSHABLE);
	result.category = ACCESSIBILITY_BEACON_CATEGORY_OBJECT;
	result.kind = ACCESSIBILITY_BEACON_KIND_MOVABLE_PUZZLE;
	accessibilityBeaconPulse(result.category, &result, &rockprop);
	assert(movablecalls == 1 && tonecalls == 3);
	rockmapping = NULL;
	assert(accessibilityInteractionGetKind(&rockprop) == ACCESSIBILITY_INTERACTION_NATIVE);
	assert(!accessibilityBeaconObjectEligible(&rockprop, &tag, &reason));
	playable = 1;
}

static s32 nextPermutation(void)
{
	s32 i = 6, j = 7, temp, left, right;
	while (i >= 0 && permutation[i] >= permutation[i + 1]) i--;
	if (i < 0) return 0;
	while (permutation[j] <= permutation[i]) j--;
	temp = permutation[i]; permutation[i] = permutation[j]; permutation[j] = temp;
	for (left = i + 1, right = 7; left < right; left++, right--) {
		temp = permutation[left]; permutation[left] = permutation[right]; permutation[right] = temp;
	}
	return 1;
}

int main(void)
{
	s32 i, count = 0;
	struct accessibilitybeaconresult result = {0};
	g_Vars.stagenum = STAGE_PELAGIC;
	for (i = 0; i < 9; i++) {
		props[i].type = PROPTYPE_OBJ;
		props[i].obj = &monitors[i].base;
		props[i].active = 1;
		props[i].flags = PROPFLAG_ONANYSCREENPREVTICK;
		monitors[i].base.type = OBJTYPE_MULTIMONITOR;
		monitors[i].base.prop = &props[i];
		if (i < 8) permutation[i] = i;
	}
	do {
		for (i = 0; i < 8; i++) image(permutation[i], i < 4 ? TVCMDLIST_12 : TVCMDLIST_13);
		for (i = 0; i < 8; i++) {
			s32 slot = permutation[i];
			assert(accessibilityInteractionGetCue(&props[slot]) == (i < 4
					? ACCESSIBILITY_INTERACTION_RESET : ACCESSIBILITY_INTERACTION_CORRECT));
			assert(eligible(slot));
			if (i >= 4) {
				image(slot, TVCMDLIST_14);
				assert(accessibilityInteractionGetCue(&props[slot]) == ACCESSIBILITY_INTERACTION_COMPLETED);
				assert(!eligible(slot));
				image(slot, TVCMDLIST_13); /* Actual reset, not a stale activation flag. */
				assert(eligible(slot));
			} else {
				image(slot, TVCMDLIST_0F); /* Wrong switch flashes during reset. */
				assert(accessibilityInteractionGetCue(&props[slot]) == ACCESSIBILITY_INTERACTION_RESET);
			}
		}
		count++;
	} while (nextPermutation());
	assert(count == 40320);
	/* No color information outside the scoped X-Ray puzzle. */
	image(8, TVCMDLIST_12);
	assert(accessibilityInteractionGetCue(&props[8]) == ACCESSIBILITY_INTERACTION_DEFAULT);
	xray = 0;
	assert(accessibilityInteractionGetCue(&props[0]) == ACCESSIBILITY_INTERACTION_DEFAULT);
	xray = 1; props[0].flags = 0;
	assert(accessibilityInteractionGetCue(&props[0]) == ACCESSIBILITY_INTERACTION_DEFAULT);
	props[0].flags = PROPFLAG_ONANYSCREENPREVTICK;
	g_Vars.stagenum = STAGE_CITRAINING;
	assert(accessibilityInteractionGetCue(&props[0]) == ACCESSIBILITY_INTERACTION_DEFAULT);
	g_Vars.stagenum = STAGE_PELAGIC;
	monitors[0].base.type = OBJTYPE_BASIC;
	assert(accessibilityInteractionGetCue(&props[0]) == ACCESSIBILITY_INTERACTION_DEFAULT);
	monitors[0].base.type = OBJTYPE_MULTIMONITOR;
	image(0, TVCMDLIST_01);
	assert(accessibilityInteractionGetCue(&props[0]) == ACCESSIBILITY_INTERACTION_PENDING);
	assert(!eligible(0));
	image(0, TVCMDLIST_13);
	props[0].active = 0; assert(!eligible(0)); props[0].active = 1;
	healthy = 0; assert(!eligible(0)); healthy = 1;
	playable = 0; assert(!eligible(0)); playable = 1;
	monitors[0].base.flags |= OBJFLAG_CANNOT_ACTIVATE; assert(!eligible(0));
	monitors[0].base.flags &= ~OBJFLAG_CANNOT_ACTIVATE;
	monitors[0].base.flags2 |= OBJFLAG2_INVISIBLE; assert(!eligible(0));
	monitors[0].base.flags2 &= ~OBJFLAG2_INVISIBLE;
	/* Playback rechecks the current state even with cached membership. */
	result.category = ACCESSIBILITY_BEACON_CATEGORY_OBJECT;
	result.kind = ACCESSIBILITY_BEACON_KIND_OBJECT;
	image(0, TVCMDLIST_12);
	accessibilityBeaconPulse(result.category, &result, &props[0]);
	assert(tonecalls == 1 && tonefrequency == 660);
	image(0, TVCMDLIST_13);
	accessibilityBeaconPulse(result.category, &result, &props[0]);
	assert(tonecalls == 2 && tonefrequency == 880);
	image(0, TVCMDLIST_14);
	accessibilityBeaconPulse(result.category, &result, &props[0]);
	assert(tonecalls == 2);
	image(0, TVCMDLIST_13);
	accessibilityBeaconPulse(result.category, &result, &props[0]);
	assert(tonecalls == 3 && tonefrequency == 880);
	authoredObjectCases();
	printf("Interaction cues: %d shuffled assignments, states, scope, stale pulses and authored-action registry checks passed.\n", count);
	return 0;
}
