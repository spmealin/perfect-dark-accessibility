#include <math.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/lv.h"
#include "game/objectives.h"
#include "game/pad.h"
#include "game/propobj.h"
#include "game/propsnd.h"
#include "lib/vars.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_landmark.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_observer.h"
#include "accessibility/accessibility_tone.h"
#include "accessibility/accessibility_visibility.h"

#define ACCESSIBILITY_LANDMARK_INNER_DISTANCE 100.0f
#define ACCESSIBILITY_LANDMARK_HYSTERESIS 75.0f
#define ACCESSIBILITY_LANDMARK_LOG_TICKS TICKS(60)
#define ACCESSIBILITY_LANDMARK_START_SPACING TICKS(15)
#define ACCESSIBILITY_LANDMARK_SKEDAR_PUZZLE_ROCK_TAG 0x4c
#define ACCESSIBILITY_LANDMARK_SKEDAR_PAD_RANGE 500.0f
#define ACCESSIBILITY_LANDMARK_SKEDAR_PAD_VERTICAL_RANGE 200.0f

struct accessibilitylandmarkspec {
	s32 stage;
	s32 tag;
	s32 proptype;
	s32 objective;
	u32 completionflag;
	s32 requirevulnerable;
	s32 pad;
	s32 completiontag;
	const char *name;
};

struct accessibilitylandmarkstate {
	const struct accessibilitylandmarkspec *spec;
	struct defaultobj *obj;
	s32 audible;
	s32 inrange;
	s32 lineofsight;
	s32 startpending;
	s32 starttick;
	s32 lossample;
	s32 losqueries;
	f32 distance;
	f32 gain;
	f32 pan;
};

/*
 * Authored landmarks are semantic navigation destinations, not interactable
 * objects. Registry entries may name setup tags from any stage; up to four
 * entries in the current stage receive independent preallocated voices.
 */
static const struct accessibilitylandmarkspec g_AccessibilityLandmarkSpecs[] = {
	{ STAGE_RESCUE, 0x18, PROPTYPE_DOOR, -1, 0, false, -1, -1,
		"crate_placement_marker" },
	{ STAGE_AIRBASE, 0x04, PROPTYPE_OBJ, 1, 0, false, -1, -1,
		"suitcase_deposit_conveyor" },
	{ STAGE_ATTACKSHIP, 0x04, PROPTYPE_OBJ, 0, 0, false, -1, -1,
		"shield_console_1" },
	{ STAGE_ATTACKSHIP, 0x05, PROPTYPE_OBJ, 0, 0, false, -1, -1,
		"shield_console_2" },
	{ STAGE_ATTACKSHIP, 0x06, PROPTYPE_OBJ, 0, 0, false, -1, -1,
		"shield_console_3" },
	{ STAGE_SKEDARRUINS, -1, -1, -1, 0, false, PAD_SHO_00D4,
		ACCESSIBILITY_LANDMARK_SKEDAR_PUZZLE_ROCK_TAG,
		"puzzle_rock_pressure_pad" },
	{ STAGE_SKEDARRUINS, 0x13, PROPTYPE_OBJ, 4, 0, true, -1, -1,
		"king_spike_middle_left" },
	{ STAGE_SKEDARRUINS, 0x14, PROPTYPE_OBJ, 4, 0, true, -1, -1,
		"king_spike_middle_right" },
	{ STAGE_SKEDARRUINS, 0x15, PROPTYPE_OBJ, 4, 0, true, -1, -1,
		"king_spike_bottom_left" },
	{ STAGE_SKEDARRUINS, 0x16, PROPTYPE_OBJ, 4, 0, true, -1, -1,
		"king_spike_bottom_right" },
	{ STAGE_SKEDARRUINS, 0x17, PROPTYPE_OBJ, 4, 0, true, -1, -1,
		"king_spike_top" },
};

static struct accessibilitylandmarkstate
		g_AccessibilityLandmarkStates[ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT];
static s32 g_AccessibilityLandmarkSuppressed;
static s32 g_AccessibilityLandmarkNextLogTick;
static s32 g_AccessibilityLandmarkStage = -1;

static const struct accessibilitylandmarkspec *accessibilityLandmarkFindSpec(
		struct prop *prop)
{
	s32 i;

	if (!prop || !prop->obj || prop->obj->prop != prop) {
		return NULL;
	}

	for (i = 0; i < ARRAYCOUNT(g_AccessibilityLandmarkSpecs); i++) {
		const struct accessibilitylandmarkspec *spec
				= &g_AccessibilityLandmarkSpecs[i];

		if (spec->stage == g_Vars.stagenum && spec->pad < 0
				&& spec->proptype == prop->type
				&& objFindByTagId(spec->tag) == prop->obj) {
			return spec;
		}
	}

	return NULL;
}

s32 accessibilityLandmarkOwnsProp(struct prop *prop)
{
	return accessibilityLandmarkFindSpec(prop) != NULL;
}

static const char *accessibilityLandmarkScopeReason(void)
{
	if (!accessibilityIsAuthoredLandmarksEnabled()) {
		return "feature_disabled";
	}

	if (PLAYERCOUNT() != 1) {
		return "unsupported_player_count";
	}

	if (!g_Vars.currentplayer || !g_Vars.currentplayer->prop) {
		return "player_unavailable";
	}

	if (g_MenuData.count > 0) {
		return "menu_open";
	}

	if (lvIsPaused()) {
		return "paused";
	}

	if (g_Vars.in_cutscene || g_Vars.tickmode == TICKMODE_CUTSCENE) {
		return "cutscene";
	}

	if (g_Vars.tickmode != TICKMODE_NORMAL) {
		return "non_gameplay_tickmode";
	}

	if (g_Vars.currentplayer->isdead) {
		return "player_dead";
	}

	return NULL;
}

static f32 accessibilityLandmarkDistance(const struct coord *from,
		const struct coord *to)
{
	f32 x = to->x - from->x;
	f32 y = to->y - from->y;
	f32 z = to->z - from->z;

	return sqrtf(x * x + y * y + z * z);
}

static f32 accessibilityLandmarkDistanceGain(f32 distance, f32 range)
{
	f32 progress;

	if (distance <= ACCESSIBILITY_LANDMARK_INNER_DISTANCE) {
		return 1.0f;
	}

	if (distance >= range || range <= ACCESSIBILITY_LANDMARK_INNER_DISTANCE) {
		return 0.0f;
	}

	progress = (range - distance)
			/ (range - ACCESSIBILITY_LANDMARK_INNER_DISTANCE);
	return progress * progress;
}

static s32 accessibilityLandmarkObjectEligible(
		const struct accessibilitylandmarkspec *spec,
		struct defaultobj *obj, const char **reason)
{
	if (spec->completionflag && (g_StageFlags & spec->completionflag)) {
		*reason = "completion_flag_set";
		return false;
	}

	if (spec->objective >= 0) {
		if (spec->objective >= objectiveGetCount()
				|| !(objectiveGetDifficultyBits(spec->objective)
						& (1 << lvGetDifficulty()))) {
			*reason = "objective_not_available";
			return false;
		}

		if (objectiveCheck(spec->objective) != OBJECTIVE_INCOMPLETE) {
			*reason = "objective_not_incomplete";
			return false;
		}
	}

	/* Compare against the engine's current tag mapping, not objGetTagNum.
	 * Skedar Ruins remaps destination tags 1-3 onto randomly chosen pillars;
	 * those objects retain their original source tags as aliases. */
	if (!obj || !obj->prop || objFindByTagId(spec->tag) != obj) {
		*reason = "tagged_object_unavailable";
		return false;
	}

	if (obj->prop->type != spec->proptype) {
		*reason = "prop_type_mismatch";
		return false;
	}

	if (!obj->prop->active
			|| (obj->hidden & (OBJHFLAG_DELETING | OBJHFLAG_GONE))) {
		*reason = "object_inactive_or_hidden";
		return false;
	}

	if (!objIsHealthy(obj)) {
		*reason = "object_destroyed";
		return false;
	}

	if (obj->flags2 & OBJFLAG2_INVISIBLE) {
		*reason = "object_invisible";
		return false;
	}

	if (spec->requirevulnerable && (obj->flags & OBJFLAG_INVINCIBLE)) {
		*reason = "object_invincible";
		return false;
	}

	*reason = "eligible";
	return true;
}

static s32 accessibilityLandmarkPadEligible(
		const struct accessibilitylandmarkspec *spec, struct coord *position,
		RoomNum *rooms, const char **reason)
{
	struct defaultobj *completionobj;
	struct pad pad;
	f32 xdiff;
	f32 ydiff;
	f32 zdiff;

	if (spec->pad < 0) {
		*reason = "pad_not_configured";
		return false;
	}

	completionobj = objFindByTagId(spec->completiontag);

	if (!completionobj || !completionobj->prop) {
		*reason = "completion_object_unavailable";
		return false;
	}

	padUnpack(spec->pad, PADFIELD_POS | PADFIELD_ROOM, &pad);

	if (pad.room <= 0) {
		*reason = "pad_room_unavailable";
		return false;
	}

	/* Mirror setupsho.c's if_object_distance_to_pad_lt(..., 50, ...).
	 * That command encodes 50 as a 500-unit X/Z box and uses a fixed
	 * 200-unit vertical tolerance. Reevaluate every tick so moving the rock
	 * away restores the landmark along with the retracting bridge. */
	xdiff = completionobj->prop->pos.x - pad.pos.x;
	ydiff = completionobj->prop->pos.y - pad.pos.y;
	zdiff = completionobj->prop->pos.z - pad.pos.z;

	if (xdiff < ACCESSIBILITY_LANDMARK_SKEDAR_PAD_RANGE
			&& xdiff > -ACCESSIBILITY_LANDMARK_SKEDAR_PAD_RANGE
			&& ydiff < ACCESSIBILITY_LANDMARK_SKEDAR_PAD_VERTICAL_RANGE
			&& ydiff > -ACCESSIBILITY_LANDMARK_SKEDAR_PAD_VERTICAL_RANGE
			&& zdiff < ACCESSIBILITY_LANDMARK_SKEDAR_PAD_RANGE
			&& zdiff > -ACCESSIBILITY_LANDMARK_SKEDAR_PAD_RANGE) {
		*reason = "completion_object_on_pad";
		return false;
	}

	*position = pad.pos;
	rooms[0] = pad.room;
	rooms[1] = -1;
	*reason = "eligible_pad";
	return true;
}

static s32 accessibilityLandmarkHasLineOfSight(
		const struct accessibilityobserver *observer,
		struct defaultobj *obj, struct coord *position, RoomNum *targetrooms,
		s32 *sample, s32 *queries)
{
	struct coord from = observer->camera;
	RoomNum fromrooms[2];

	if (observer->room <= 0 || !position || !targetrooms
			|| targetrooms[0] <= 0) {
		return false;
	}

	fromrooms[0] = observer->room;
	fromrooms[1] = -1;

	if (obj && obj->prop) {
		return accessibilityVisibilityHasObjectSurfaceLineOfSight(
				&from, fromrooms, obj->prop, true, sample, queries);
	}

	*sample = 0;
	*queries = 1;
	return accessibilityVisibilityHasVisualLineOfSight(
			&from, fromrooms, position, targetrooms, NULL);
}

static void accessibilityLandmarkStopVoices(void)
{
	s32 slot;

	accessibilityToneStopLandmarks();

	for (slot = 0; slot < ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT; slot++) {
		g_AccessibilityLandmarkStates[slot].audible = false;
		g_AccessibilityLandmarkStates[slot].lineofsight = false;
	}
}

static void accessibilityLandmarkUpdate(
		const struct accessibilityobserver *observer)
{
	struct accessibilitylandmarkcandidate {
		const struct accessibilitylandmarkspec *spec;
		struct defaultobj *obj;
		struct coord position;
		RoomNum rooms[2];
		s32 slot;
	};
	struct accessibilitylandmarkcandidate candidates[
			ARRAYCOUNT(g_AccessibilityLandmarkSpecs)];
	s32 claimed[ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT] = { 0 };
	f32 range;
	f32 mastervolume;
	s32 candidatecount = 0;
	s32 candidateindex;
	s32 slot;
	s32 specindex;
	s32 audiblecount = 0;

	accessibilityGetMarkerTuning(&range, &mastervolume);

	for (specindex = 0;
			specindex < ARRAYCOUNT(g_AccessibilityLandmarkSpecs);
			specindex++) {
		const struct accessibilitylandmarkspec *spec
				= &g_AccessibilityLandmarkSpecs[specindex];
		struct defaultobj *obj;
		const char *reason = "stage_mismatch";

		if (spec->stage != g_Vars.stagenum) {
			continue;
		}

		obj = spec->pad >= 0 ? NULL : objFindByTagId(spec->tag);

		if ((spec->pad >= 0
				&& accessibilityLandmarkPadEligible(spec,
						&candidates[candidatecount].position,
						candidates[candidatecount].rooms, &reason))
				|| (spec->pad < 0
					&& accessibilityLandmarkObjectEligible(spec, obj, &reason))) {
			candidates[candidatecount].spec = spec;
			candidates[candidatecount].obj = obj;
			if (obj) {
				candidates[candidatecount].position = obj->prop->pos;
				candidates[candidatecount].rooms[0] = obj->prop->rooms[0];
				candidates[candidatecount].rooms[1] = -1;
			}
			candidates[candidatecount].slot = -1;
			candidatecount++;
		} else {
			for (slot = 0;
					slot < ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT;
					slot++) {
				if (g_AccessibilityLandmarkStates[slot].spec == spec) {
					accessibilityLogEvent("landmark", "state",
							"slot=%d name=%s state=inactive reason=%s tick=%d stage=%d",
							slot, spec->name, reason, g_Vars.lvframe60,
							g_Vars.stagenum);
					break;
				}
			}
		}
	}

	/* Retain existing slot identities so destroying or hiding one landmark does
	 * not restart and reshuffle every remaining positioned voice. */
	for (candidateindex = 0; candidateindex < candidatecount;
			candidateindex++) {
		for (slot = 0; slot < ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT; slot++) {
			if (!claimed[slot]
					&& g_AccessibilityLandmarkStates[slot].spec
							== candidates[candidateindex].spec
					&& g_AccessibilityLandmarkStates[slot].obj
							== candidates[candidateindex].obj) {
				candidates[candidateindex].slot = slot;
				claimed[slot] = true;
				break;
			}
		}
	}

	/* Allocate only eligible entries. Inactive entries in the same stage must
	 * not consume one of the four preallocated voices. */
	for (candidateindex = 0; candidateindex < candidatecount;
			candidateindex++) {
		if (candidates[candidateindex].slot >= 0) {
			continue;
		}

		for (slot = 0; slot < ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT; slot++) {
			if (!claimed[slot]) {
				candidates[candidateindex].slot = slot;
				claimed[slot] = true;
				break;
			}
		}

		if (candidates[candidateindex].slot < 0) {
			accessibilityLogEvent("landmark", "capacity",
					"name=%s tag=%d pad=%d eligible=%d capacity=%d tick=%d stage=%d",
					candidates[candidateindex].spec->name,
					candidates[candidateindex].spec->tag,
					candidates[candidateindex].spec->pad, candidatecount,
					ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT,
					g_Vars.lvframe60, g_Vars.stagenum);
		}
	}

	for (slot = 0; slot < ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT; slot++) {
		const struct accessibilitylandmarkspec *spec = NULL;
		struct accessibilitylandmarkstate *state
				= &g_AccessibilityLandmarkStates[slot];
		struct defaultobj *obj = NULL;
		struct coord targetpos;
		RoomNum targetrooms[2] = { -1, -1 };
		f32 limit;
		s32 inrange;
		s32 lineofsight;
		s32 waslineofsight;
		s32 pan;
		f32 normalizedpan;
		s32 restart;

		for (candidateindex = 0; candidateindex < candidatecount;
				candidateindex++) {
			if (candidates[candidateindex].slot == slot) {
				spec = candidates[candidateindex].spec;
				obj = candidates[candidateindex].obj;
				targetpos = candidates[candidateindex].position;
				targetrooms[0] = candidates[candidateindex].rooms[0];
				break;
			}
		}

		if (!spec) {
			accessibilityToneSetLandmarkSlot(
					slot, false, 0.0f, 0.0f, false);
			memset(state, 0, sizeof(*state));
			continue;
		}

		if (state->spec != spec || state->obj != obj) {
			memset(state, 0, sizeof(*state));
			state->spec = spec;
			state->obj = obj;
		}

		state->distance = accessibilityLandmarkDistance(
				&observer->camera, &targetpos);
		limit = state->inrange ? range + ACCESSIBILITY_LANDMARK_HYSTERESIS
				: range;
		inrange = state->distance <= limit;
		state->lossample = 0;
		state->losqueries = 0;
		lineofsight = inrange
				&& accessibilityLandmarkHasLineOfSight(observer, obj,
						&targetpos, targetrooms,
						&state->lossample, &state->losqueries);
		waslineofsight = state->lineofsight;

		if (lineofsight && !waslineofsight) {
			state->startpending = slot > 0;
			state->starttick = g_Vars.lvframe60
					+ slot * ACCESSIBILITY_LANDMARK_START_SPACING;
		} else if (!lineofsight) {
			state->startpending = false;
		}

		if (state->startpending
				&& g_Vars.lvframe60 >= state->starttick) {
			state->startpending = false;
		}

		state->gain = lineofsight && !state->startpending
				? accessibilityLandmarkDistanceGain(state->distance, range)
						* mastervolume
				: 0.0f;
		pan = psCalculatePan(&targetpos,
				ACCESSIBILITY_LANDMARK_INNER_DISTANCE, range, range,
				state->distance, false, NULL);
		normalizedpan = ((f32)pan - (f32)AL_PAN_CENTER)
				/ (f32)AL_PAN_CENTER;
		restart = (!state->audible && state->gain > 0.0f);

		if (state->inrange != inrange || state->lineofsight != lineofsight) {
			accessibilityLogEvent("landmark", "visibility",
					"slot=%d name=%s tick=%d stage=%d tag=%d pad=%d prop=%p propnum=%d position=%.3f,%.3f,%.3f in_range=%d line_of_sight=%d los_sample=%d los_queries=%d start_pending=%d start_tick=%d distance=%.3f observer_remote=%d",
					slot, spec->name, g_Vars.lvframe60, g_Vars.stagenum,
					spec->tag, spec->pad, obj ? (void *)obj->prop : NULL,
					obj ? (s32)(obj->prop - g_Vars.props) : -1,
					targetpos.x, targetpos.y, targetpos.z, inrange, lineofsight,
					state->lossample, state->losqueries,
					state->startpending, state->starttick, state->distance,
					observer->isremote);
		}

		state->inrange = inrange;
		state->lineofsight = lineofsight;
		state->audible = state->gain > 0.0f;
		state->pan = normalizedpan;
		audiblecount += state->audible;
		accessibilityToneSetLandmarkSlot(slot, state->audible,
				state->gain, normalizedpan, restart);
	}

	if (audiblecount > 0
			&& g_Vars.lvframe60 >= g_AccessibilityLandmarkNextLogTick) {
		accessibilityLogEvent("landmark", "summary",
				"tick=%d stage=%d audible=%d capacity=%d range=%.3f master_volume=%.4f remote=%d observer=%p",
				g_Vars.lvframe60, g_Vars.stagenum, audiblecount,
				ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT, range, mastervolume,
				observer->isremote, (void *)observer->prop);
		g_AccessibilityLandmarkNextLogTick
				= g_Vars.lvframe60 + ACCESSIBILITY_LANDMARK_LOG_TICKS;
	}
}

void accessibilityLandmarkTick(void)
{
	struct accessibilityobserver observer;
	const char *reason = accessibilityLandmarkScopeReason();

	if (reason) {
		if (!g_AccessibilityLandmarkSuppressed) {
			accessibilityLandmarkStopVoices();
			g_AccessibilityLandmarkSuppressed = true;
			accessibilityLogEvent("landmark", "scope",
					"state=suspended reason=%s tick=%d stage=%d",
					reason, g_Vars.lvframe60, g_Vars.stagenum);
		}
		return;
	}

	if (!accessibilityObserverGet(&observer)) {
		if (!g_AccessibilityLandmarkSuppressed) {
			accessibilityLandmarkStopVoices();
			g_AccessibilityLandmarkSuppressed = true;
			accessibilityLogEvent("landmark", "scope",
					"state=suspended reason=observer_unavailable tick=%d stage=%d",
					g_Vars.lvframe60, g_Vars.stagenum);
		}
		return;
	}

	if (g_AccessibilityLandmarkStage != g_Vars.stagenum) {
		accessibilityLandmarkReset("stage_changed");
		g_AccessibilityLandmarkStage = g_Vars.stagenum;
	}

	if (g_AccessibilityLandmarkSuppressed) {
		g_AccessibilityLandmarkSuppressed = false;
		accessibilityLogEvent("landmark", "scope",
				"state=resumed reason=gameplay_eligible tick=%d stage=%d remote=%d",
				g_Vars.lvframe60, g_Vars.stagenum, observer.isremote);
	}

	accessibilityLandmarkUpdate(&observer);
}

void accessibilityLandmarkReset(const char *reason)
{
	s32 active = 0;
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT; slot++) {
		active += g_AccessibilityLandmarkStates[slot].audible != 0;
	}

	accessibilityToneStopLandmarks();
	memset(g_AccessibilityLandmarkStates, 0,
			sizeof(g_AccessibilityLandmarkStates));
	g_AccessibilityLandmarkSuppressed = false;
	g_AccessibilityLandmarkNextLogTick = 0;
	g_AccessibilityLandmarkStage = -1;

	if (active > 0) {
		accessibilityLogEvent("landmark", "reset",
				"reason=%s active_cleared=%d stage=%d tick=%d",
				reason ? reason : "reset", active,
				g_Vars.stagenum, g_Vars.lvframe60);
	}
}
