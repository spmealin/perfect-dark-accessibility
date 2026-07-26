#include <math.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/lv.h"
#include "game/propsnd.h"
#include "lib/collision.h"
#include "lib/vars.h"
#include "system.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_marker.h"
#include "accessibility/accessibility_observer.h"
#include "accessibility/accessibility_tone.h"
#ifndef PLATFORM_N64
#include "input.h"
#endif

#define ACCESSIBILITY_MARKER_COUNT 4
#define ACCESSIBILITY_MARKER_INNER_DISTANCE 100.0f
#define ACCESSIBILITY_MARKER_HYSTERESIS 75.0f
#define ACCESSIBILITY_MARKER_LOG_TICKS TICKS(60)

struct accessibilitymarker {
	s32 active;
	struct coord position;
	RoomNum rooms[2];
	s32 stage;
	u32 placedtick;
	u32 revision;
	s32 audible;
	s32 inrange;
	s32 lineofsight;
	f32 distance;
	f32 gain;
	f32 pan;
	u64 queryus;
};

static struct accessibilitymarker
		g_AccessibilityMarkers[ACCESSIBILITY_MARKER_COUNT];
static s32 g_AccessibilityMarkerSuppressed;
static s32 g_AccessibilityMarkerNextLogTick;
static uintptr_t g_AccessibilityMarkerObserverProp;
static s32 g_AccessibilityMarkerObserverRemote;

static const char *accessibilityMarkerScopeReason(void)
{
	if (!accessibilityIsAudibleMarkersEnabled()) {
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

	if (g_Vars.currentplayer->isdead) {
		return "player_dead";
	}

	return NULL;
}

static s32 accessibilityMarkerAnyActive(void)
{
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_MARKER_COUNT; slot++) {
		if (g_AccessibilityMarkers[slot].active) {
			return true;
		}
	}

	return false;
}

static void accessibilityMarkerStopVoices(void)
{
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_MARKER_COUNT; slot++) {
		accessibilityToneSetMarkerSlot(slot, false, 0.0f, 0.0f, false);
		g_AccessibilityMarkers[slot].audible = false;
		g_AccessibilityMarkers[slot].lineofsight = false;
	}
}

static f32 accessibilityMarkerDistance(const struct coord *from,
		const struct coord *to)
{
	f32 x = to->x - from->x;
	f32 y = to->y - from->y;
	f32 z = to->z - from->z;

	return sqrtf(x * x + y * y + z * z);
}

static f32 accessibilityMarkerDistanceGain(f32 distance, f32 range)
{
	f32 progress;

	if (distance <= ACCESSIBILITY_MARKER_INNER_DISTANCE) {
		return 1.0f;
	}

	if (distance >= range || range <= ACCESSIBILITY_MARKER_INNER_DISTANCE) {
		return 0.0f;
	}

	progress = (range - distance)
			/ (range - ACCESSIBILITY_MARKER_INNER_DISTANCE);
	return progress * progress;
}

static s32 accessibilityMarkerHasLineOfSight(
		const struct accessibilityobserver *observer,
		const struct accessibilitymarker *marker)
{
	struct coord from = observer->camera;
	struct coord to = marker->position;
	RoomNum fromrooms[2];
	RoomNum torooms[2];

	if (observer->room <= 0 || marker->rooms[0] <= 0) {
		return false;
	}

	fromrooms[0] = observer->room;
	fromrooms[1] = -1;
	torooms[0] = marker->rooms[0];
	torooms[1] = -1;

	return cdTestLos05(&from, fromrooms, &to, torooms,
			CDTYPE_DOORS | CDTYPE_BG,
			GEOFLAG_WALL | GEOFLAG_BLOCK_SIGHT | GEOFLAG_BLOCK_SHOOT);
}

static void accessibilityMarkerPlace(s32 slot,
		const struct accessibilityobserver *observer)
{
	struct accessibilitymarker *marker = &g_AccessibilityMarkers[slot];
	s32 moved = marker->active;
	struct coord oldposition = marker->position;

	marker->active = true;
	marker->position = observer->camera;
	marker->rooms[0] = observer->room;
	marker->rooms[1] = -1;
	marker->stage = g_Vars.stagenum;
	marker->placedtick = g_Vars.lvframe60;
	marker->revision++;
	marker->audible = false;
	marker->lineofsight = true;

	accessibilityLogEvent("marker", "command",
			"action=%s slot=%d tick=%d stage=%d revision=%u remote=%d observer=%p old_position=%.3f,%.3f,%.3f position=%.3f,%.3f,%.3f room=%d",
			moved ? "move" : "place", slot + 1, g_Vars.lvframe60,
			g_Vars.stagenum, marker->revision, observer->isremote,
			(void *)observer->prop, oldposition.x, oldposition.y, oldposition.z,
			marker->position.x, marker->position.y, marker->position.z,
			marker->rooms[0]);
}

static void accessibilityMarkerRemove(s32 slot)
{
	struct accessibilitymarker *marker = &g_AccessibilityMarkers[slot];

	if (!marker->active) {
		accessibilityLogEvent("marker", "command",
				"action=ignored slot=%d reason=empty tick=%d stage=%d",
				slot + 1, g_Vars.lvframe60, g_Vars.stagenum);
		return;
	}

	accessibilityLogEvent("marker", "command",
			"action=remove slot=%d tick=%d stage=%d revision=%u position=%.3f,%.3f,%.3f room=%d",
			slot + 1, g_Vars.lvframe60, g_Vars.stagenum,
			marker->revision + 1, marker->position.x, marker->position.y,
			marker->position.z, marker->rooms[0]);

	marker->active = false;
	marker->audible = false;
	marker->lineofsight = false;
	marker->revision++;
	accessibilityToneSetMarkerSlot(slot, false, 0.0f, 0.0f, false);
	accessibilityTonePlayMarkerRemoval(slot);
}

static void accessibilityMarkerHandleInput(
		const struct accessibilityobserver *observer)
{
#ifndef PLATFORM_N64
	static const s32 keys[ACCESSIBILITY_MARKER_COUNT] = {
		VK_F9, VK_F10, VK_F11, VK_F12,
	};
	u32 modifiers = inputGetKeyModState();
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_MARKER_COUNT; slot++) {
		if (!inputKeyJustPressed(keys[slot])) {
			continue;
		}

		if (modifiers & (KM_ALT | KM_CTRL)) {
			accessibilityLogEvent("marker", "command",
					"action=ignored slot=%d reason=modified_shortcut modifiers=0x%x tick=%d",
					slot + 1, modifiers, g_Vars.lvframe60);
		} else if (modifiers & KM_SHIFT) {
			accessibilityMarkerRemove(slot);
		} else {
			accessibilityMarkerPlace(slot, observer);
		}
	}
#endif
}

static void accessibilityMarkerUpdateAudio(
		const struct accessibilityobserver *observer)
{
	f32 range;
	f32 mastervolume;
	s32 activecount = 0;
	s32 audiblemask = 0;
	s32 slot;

	accessibilityGetMarkerTuning(&range, &mastervolume);

	for (slot = 0; slot < ACCESSIBILITY_MARKER_COUNT; slot++) {
		struct accessibilitymarker *marker = &g_AccessibilityMarkers[slot];
		f32 distance;
		f32 limit;
		f32 gain;
		s32 lineofsight;
		s32 inrange;
		s32 pan;
		f32 normalizedpan;
		s32 restart;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
		u64 querystart;
#endif

		if (!marker->active || marker->stage != g_Vars.stagenum) {
			accessibilityToneSetMarkerSlot(slot, false, 0.0f, 0.0f, false);
			marker->audible = false;
			continue;
		}

		activecount++;
		distance = accessibilityMarkerDistance(&observer->camera,
				&marker->position);
		limit = marker->inrange ? range + ACCESSIBILITY_MARKER_HYSTERESIS
				: range;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
		querystart = sysGetMicroseconds();
#endif
		inrange = distance <= limit;
		lineofsight = inrange
				&& accessibilityMarkerHasLineOfSight(observer, marker);
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
		marker->queryus = distance <= limit
				? sysGetMicroseconds() - querystart : 0;
#else
		marker->queryus = 0;
#endif
		gain = lineofsight
				? accessibilityMarkerDistanceGain(distance, range)
						* mastervolume
				: 0.0f;
		pan = psCalculatePan(&marker->position,
				ACCESSIBILITY_MARKER_INNER_DISTANCE, range, range,
				distance, false, NULL);
		normalizedpan = ((f32)pan - (f32)AL_PAN_CENTER)
				/ (f32)AL_PAN_CENTER;
		restart = !marker->audible && gain > 0.0f;

		if (marker->lineofsight != lineofsight) {
			accessibilityLogEvent("marker", "line_of_sight",
					"slot=%d tick=%d stage=%d state=%s remote=%d distance=%.3f observer=%.3f,%.3f,%.3f marker=%.3f,%.3f,%.3f observer_room=%d marker_room=%d",
					slot + 1, g_Vars.lvframe60, g_Vars.stagenum,
					lineofsight ? "clear" : "blocked",
					observer->isremote, distance,
					observer->camera.x, observer->camera.y,
					observer->camera.z, marker->position.x,
					marker->position.y, marker->position.z,
					observer->room, marker->rooms[0]);
		}

		marker->inrange = inrange;
		marker->lineofsight = lineofsight;
		marker->audible = gain > 0.0f;
		marker->distance = distance;
		marker->gain = gain;
		marker->pan = normalizedpan;
		if (marker->audible) {
			audiblemask |= 1 << slot;
		}
		accessibilityToneSetMarkerSlot(slot, marker->audible,
				gain, normalizedpan, restart);
	}

	if (activecount > 0 && g_Vars.lvframe60 >= g_AccessibilityMarkerNextLogTick) {
		accessibilityLogEvent("marker", "summary",
				"tick=%d stage=%d active=%d audible_mask=0x%x remote=%d observer=%p position=%.3f,%.3f,%.3f room=%d range=%.3f master_volume=%.4f",
				g_Vars.lvframe60, g_Vars.stagenum, activecount,
				audiblemask, observer->isremote, (void *)observer->prop,
				observer->camera.x, observer->camera.y, observer->camera.z,
				observer->room, range, mastervolume);
		for (slot = 0; slot < ACCESSIBILITY_MARKER_COUNT; slot++) {
			struct accessibilitymarker *marker
					= &g_AccessibilityMarkers[slot];

			if (marker->active) {
				accessibilityLogEvent("marker", "state",
						"tick=%d stage=%d slot=%d revision=%u position=%.3f,%.3f,%.3f room=%d distance=%.3f in_range=%d line_of_sight=%d audible=%d gain=%.5f pan=%.5f query_us=%llu",
						g_Vars.lvframe60, g_Vars.stagenum, slot + 1,
						marker->revision, marker->position.x,
						marker->position.y, marker->position.z,
						marker->rooms[0], marker->distance, marker->inrange,
						marker->lineofsight, marker->audible,
						marker->gain, marker->pan,
						(unsigned long long)marker->queryus);
			}
		}
		g_AccessibilityMarkerNextLogTick
				= g_Vars.lvframe60 + ACCESSIBILITY_MARKER_LOG_TICKS;
	}
}

void accessibilityMarkerTick(void)
{
	struct accessibilityobserver observer;
	const char *reason = accessibilityMarkerScopeReason();

	if (reason) {
		if (strcmp(reason, "feature_disabled") == 0) {
			if (accessibilityMarkerAnyActive()) {
				accessibilityMarkerReset(reason);
			}
			g_AccessibilityMarkerSuppressed = false;
		} else if (accessibilityMarkerAnyActive()
				&& !g_AccessibilityMarkerSuppressed) {
			accessibilityMarkerStopVoices();
			g_AccessibilityMarkerSuppressed = true;
			accessibilityLogEvent("marker", "scope",
					"state=suspended reason=%s tick=%d stage=%d",
					reason, g_Vars.lvframe60, g_Vars.stagenum);
		}
		return;
	}

	if (!accessibilityObserverGet(&observer)) {
		if (accessibilityMarkerAnyActive()
				&& !g_AccessibilityMarkerSuppressed) {
			accessibilityMarkerStopVoices();
			g_AccessibilityMarkerSuppressed = true;
			accessibilityLogEvent("marker", "scope",
					"state=suspended reason=observer_unavailable tick=%d stage=%d",
					g_Vars.lvframe60, g_Vars.stagenum);
		}
		return;
	}

	if (g_AccessibilityMarkerObserverProp
			&& (g_AccessibilityMarkerObserverProp != (uintptr_t)observer.prop
				|| g_AccessibilityMarkerObserverRemote != observer.isremote)) {
		accessibilityLogEvent("marker", "observer_change",
				"tick=%d stage=%d observer=%p remote=%d",
				g_Vars.lvframe60, g_Vars.stagenum,
				(void *)observer.prop, observer.isremote);
	}
	g_AccessibilityMarkerObserverProp = (uintptr_t)observer.prop;
	g_AccessibilityMarkerObserverRemote = observer.isremote;

	if (g_AccessibilityMarkerSuppressed) {
		g_AccessibilityMarkerSuppressed = false;
		accessibilityLogEvent("marker", "scope",
				"state=resumed reason=gameplay_eligible tick=%d stage=%d remote=%d",
				g_Vars.lvframe60, g_Vars.stagenum, observer.isremote);
	}

	accessibilityMarkerHandleInput(&observer);
	accessibilityMarkerUpdateAudio(&observer);
}

void accessibilityMarkerReset(const char *reason)
{
	s32 activecount = 0;
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_MARKER_COUNT; slot++) {
		activecount += g_AccessibilityMarkers[slot].active != 0;
	}

	accessibilityToneStopMarkers();
	memset(g_AccessibilityMarkers, 0, sizeof(g_AccessibilityMarkers));
	g_AccessibilityMarkerSuppressed = false;
	g_AccessibilityMarkerNextLogTick = 0;
	g_AccessibilityMarkerObserverProp = 0;
	g_AccessibilityMarkerObserverRemote = false;

	if (activecount > 0) {
		accessibilityLogEvent("marker", "reset",
				"reason=%s active_cleared=%d stage=%d tick=%d",
				reason ? reason : "reset", activecount,
				g_Vars.stagenum, g_Vars.lvframe60);
	}
}
