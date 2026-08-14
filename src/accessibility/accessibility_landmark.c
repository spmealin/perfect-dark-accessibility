#include <math.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/lv.h"
#include "game/objectives.h"
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

struct accessibilitylandmarkspec {
	s32 stage;
	s32 tag;
	s32 proptype;
	const char *name;
};

struct accessibilitylandmarkstate {
	struct defaultobj *obj;
	s32 audible;
	s32 inrange;
	s32 lineofsight;
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
	{ STAGE_RESCUE, 0x18, PROPTYPE_DOOR, "crate_placement_marker" },
};

static struct accessibilitylandmarkstate
		g_AccessibilityLandmarkStates[ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT];
static s32 g_AccessibilityLandmarkSuppressed;
static s32 g_AccessibilityLandmarkNextLogTick;
static s32 g_AccessibilityLandmarkStage = -1;

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
	if (!obj || !obj->prop || objGetTagNum(obj) != spec->tag) {
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

	*reason = "eligible";
	return true;
}

static s32 accessibilityLandmarkHasLineOfSight(
		const struct accessibilityobserver *observer,
		struct defaultobj *obj)
{
	struct coord from = observer->camera;
	struct coord to = obj->prop->pos;
	RoomNum fromrooms[2];

	if (observer->room <= 0 || obj->prop->rooms[0] <= 0) {
		return false;
	}

	fromrooms[0] = observer->room;
	fromrooms[1] = -1;

	return accessibilityVisibilityHasVisualLineOfSight(
			&from, fromrooms, &to, obj->prop->rooms, obj->prop);
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
	f32 range;
	f32 mastervolume;
	s32 slot = 0;
	s32 specindex;
	s32 audiblecount = 0;

	accessibilityGetMarkerTuning(&range, &mastervolume);

	for (specindex = 0;
			specindex < ARRAYCOUNT(g_AccessibilityLandmarkSpecs)
					&& slot < ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT;
			specindex++) {
		const struct accessibilitylandmarkspec *spec
				= &g_AccessibilityLandmarkSpecs[specindex];
		struct accessibilitylandmarkstate *state;
		struct defaultobj *obj;
		const char *reason = "stage_mismatch";
		f32 limit;
		s32 inrange;
		s32 lineofsight;
		s32 pan;
		f32 normalizedpan;
		s32 restart;

		if (spec->stage != g_Vars.stagenum) {
			continue;
		}

		state = &g_AccessibilityLandmarkStates[slot];
		obj = objFindByTagId(spec->tag);

		if (!accessibilityLandmarkObjectEligible(spec, obj, &reason)) {
			if (state->obj || state->audible) {
				accessibilityLogEvent("landmark", "state",
						"slot=%d name=%s state=inactive reason=%s tick=%d stage=%d",
						slot, spec->name, reason, g_Vars.lvframe60,
						g_Vars.stagenum);
			}
			accessibilityToneSetLandmarkSlot(
					slot, false, 0.0f, 0.0f, false);
			memset(state, 0, sizeof(*state));
			slot++;
			continue;
		}

		if (state->obj != obj) {
			memset(state, 0, sizeof(*state));
			state->obj = obj;
		}

		state->distance = accessibilityLandmarkDistance(
				&observer->camera, &obj->prop->pos);
		limit = state->inrange ? range + ACCESSIBILITY_LANDMARK_HYSTERESIS
				: range;
		inrange = state->distance <= limit;
		lineofsight = inrange
				&& accessibilityLandmarkHasLineOfSight(observer, obj);
		state->gain = lineofsight
				? accessibilityLandmarkDistanceGain(state->distance, range)
						* mastervolume
				: 0.0f;
		pan = psCalculatePan(&obj->prop->pos,
				ACCESSIBILITY_LANDMARK_INNER_DISTANCE, range, range,
				state->distance, false, NULL);
		normalizedpan = ((f32)pan - (f32)AL_PAN_CENTER)
				/ (f32)AL_PAN_CENTER;
		restart = (!state->audible && state->gain > 0.0f);

		if (state->inrange != inrange || state->lineofsight != lineofsight) {
			accessibilityLogEvent("landmark", "visibility",
					"slot=%d name=%s tick=%d stage=%d tag=%d prop=%p propnum=%d in_range=%d line_of_sight=%d distance=%.3f observer_remote=%d",
					slot, spec->name, g_Vars.lvframe60, g_Vars.stagenum,
					spec->tag, (void *)obj->prop,
					(s32)(obj->prop - g_Vars.props), inrange, lineofsight,
					state->distance, observer->isremote);
		}

		state->inrange = inrange;
		state->lineofsight = lineofsight;
		state->audible = state->gain > 0.0f;
		state->pan = normalizedpan;
		audiblecount += state->audible;
		accessibilityToneSetLandmarkSlot(slot, state->audible,
				state->gain, normalizedpan, restart);
		slot++;
	}

	while (slot < ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT) {
		accessibilityToneSetLandmarkSlot(slot, false, 0.0f, 0.0f, false);
		memset(&g_AccessibilityLandmarkStates[slot], 0,
				sizeof(g_AccessibilityLandmarkStates[slot]));
		slot++;
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
