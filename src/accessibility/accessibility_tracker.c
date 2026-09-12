#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/lv.h"
#include "game/propobj.h"
#include "game/propsnd.h"
#include "game/radar.h"
#include "system.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_landmark.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_tone.h"
#include "accessibility/accessibility_tracker.h"

#define ACCESSIBILITY_TRACKER_SLOT_COUNT ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT
#define ACCESSIBILITY_TRACKER_RADAR_DISTANCE 4000.0f
#define ACCESSIBILITY_TRACKER_NEAR_PERIOD_MS 200
#define ACCESSIBILITY_TRACKER_FAR_PERIOD_MS 1200
#define ACCESSIBILITY_TRACKER_HEIGHT_THRESHOLD 250.0f
#define ACCESSIBILITY_TRACKER_HEIGHT_HYSTERESIS 25.0f
#define ACCESSIBILITY_TRACKER_REAR_HYSTERESIS 0.1f
#define ACCESSIBILITY_TRACKER_EMPTY_DELAY_TICKS 6
#define ACCESSIBILITY_TRACKER_VOLUME 0.8f

enum accessibilitytrackerheight {
	ACCESSIBILITY_TRACKER_HEIGHT_LEVEL,
	ACCESSIBILITY_TRACKER_HEIGHT_ABOVE,
	ACCESSIBILITY_TRACKER_HEIGHT_BELOW,
};

enum accessibilitytrackersource {
	ACCESSIBILITY_TRACKER_SOURCE_NONE,
	ACCESSIBILITY_TRACKER_SOURCE_RTRACKER,
	ACCESSIBILITY_TRACKER_SOURCE_INFRARED,
};

#define ACCESSIBILITY_TRACKER_CATEGORY_INFRARED 4

struct accessibilitytrackercandidate {
	uintptr_t identity;
	struct prop *prop;
	s32 propnum;
	s32 category;
	struct coord position;
	f32 distance;
	f32 clampeddistance;
	f32 sourcedistance;
	f32 heightdelta;
	s32 nativepan;
	f32 pan;
	f32 forwarddot;
	f32 frequencyhz;
	s32 periodms;
};

struct accessibilitytrackerslot {
	uintptr_t identity;
	s32 propnum;
	s32 category;
	s32 height;
	s32 rear;
};

static struct accessibilitytrackercandidate
		g_AccessibilityTrackerCandidates[ACCESSIBILITY_TRACKER_SLOT_COUNT];
static struct accessibilitytrackerslot
		g_AccessibilityTrackerSlots[ACCESSIBILITY_TRACKER_SLOT_COUNT];
static s32 g_AccessibilityTrackerCandidateCount;
static s32 g_AccessibilityTrackerDeviceActive;
static s32 g_AccessibilityTrackerInfraredActive;
static s32 g_AccessibilityTrackerXrayActive;
static s32 g_AccessibilityTrackerSource;
static s32 g_AccessibilityTrackerAudioSuppressed;
static s32 g_AccessibilityTrackerEmptyPending;
static s32 g_AccessibilityTrackerEmptyDeadline;
static s32 g_AccessibilityTrackerLastStage = -1;
static const char *g_AccessibilityTrackerLastScopeReason;
static u64 g_AccessibilityTrackerScanCount;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
static u64 g_AccessibilityTrackerScanCurrentUs;
static u64 g_AccessibilityTrackerScanTotalUs;
static u64 g_AccessibilityTrackerScanMaxUs;
#endif

static s32 accessibilityTrackerPropNum(const struct prop *prop)
{
	uintptr_t address;
	uintptr_t first;
	uintptr_t end;

	if (!prop || !g_Vars.props || g_Vars.maxprops <= 0) {
		return -1;
	}

	address = (uintptr_t)prop;
	first = (uintptr_t)g_Vars.props;
	end = first + sizeof(struct prop) * (uintptr_t)g_Vars.maxprops;

	if (address < first || address >= end
			|| (address - first) % sizeof(struct prop) != 0) {
		return -1;
	}

	return (s32)((address - first) / sizeof(struct prop));
}

static const char *accessibilityTrackerCategoryName(s32 category)
{
	switch (category) {
	case RADAR_TRACKED_YELLOW:
		return "yellow_object";
	case RADAR_TRACKED_BLUE:
		return "blue_cheat_object";
	case RADAR_TRACKED_CHARACTER:
		return "red_character";
	case ACCESSIBILITY_TRACKER_CATEGORY_INFRARED:
		return "infrared_highlight";
	}

	return "none";
}

static f32 accessibilityTrackerCategoryFrequency(s32 category)
{
	switch (category) {
	case RADAR_TRACKED_CHARACTER:
		return 520.0f;
	case RADAR_TRACKED_BLUE:
		return 1000.0f;
	case RADAR_TRACKED_YELLOW:
	case ACCESSIBILITY_TRACKER_CATEGORY_INFRARED:
	default:
		return 700.0f;
	}
}

static const char *accessibilityTrackerSourceName(s32 source)
{
	switch (source) {
	case ACCESSIBILITY_TRACKER_SOURCE_RTRACKER:
		return "rtracker";
	case ACCESSIBILITY_TRACKER_SOURCE_INFRARED:
		return "ir_scanner";
	}

	return "none";
}

static const char *accessibilityTrackerHeightName(s32 height)
{
	switch (height) {
	case ACCESSIBILITY_TRACKER_HEIGHT_ABOVE:
		return "above";
	case ACCESSIBILITY_TRACKER_HEIGHT_BELOW:
		return "below";
	}

	return "level";
}

static s32 accessibilityTrackerHeight(f32 delta, s32 previous)
{
	if (previous == ACCESSIBILITY_TRACKER_HEIGHT_ABOVE
			&& delta >= ACCESSIBILITY_TRACKER_HEIGHT_THRESHOLD
					- ACCESSIBILITY_TRACKER_HEIGHT_HYSTERESIS) {
		return ACCESSIBILITY_TRACKER_HEIGHT_ABOVE;
	}

	if (previous == ACCESSIBILITY_TRACKER_HEIGHT_BELOW
			&& delta <= -ACCESSIBILITY_TRACKER_HEIGHT_THRESHOLD
					+ ACCESSIBILITY_TRACKER_HEIGHT_HYSTERESIS) {
		return ACCESSIBILITY_TRACKER_HEIGHT_BELOW;
	}

	if (delta > ACCESSIBILITY_TRACKER_HEIGHT_THRESHOLD
			+ ACCESSIBILITY_TRACKER_HEIGHT_HYSTERESIS) {
		return ACCESSIBILITY_TRACKER_HEIGHT_ABOVE;
	}

	if (delta < -ACCESSIBILITY_TRACKER_HEIGHT_THRESHOLD
			- ACCESSIBILITY_TRACKER_HEIGHT_HYSTERESIS) {
		return ACCESSIBILITY_TRACKER_HEIGHT_BELOW;
	}

	return ACCESSIBILITY_TRACKER_HEIGHT_LEVEL;
}

static s32 accessibilityTrackerRear(f32 forwarddot, s32 previous)
{
	if (previous) {
		return forwarddot <= ACCESSIBILITY_TRACKER_REAR_HYSTERESIS;
	}

	return forwarddot < -ACCESSIBILITY_TRACKER_REAR_HYSTERESIS;
}

static void accessibilityTrackerSpeak(const char *event, const char *text,
		s32 interrupt)
{
	s32 playernum = g_Vars.currentplayernum;
	s32 accepted = accessibilityAnnouncementStatus(text, "rtracker",
			playernum, interrupt);

	accessibilityLogEvent("rtracker", "announcement",
			"event=%s accepted=%d interrupt=%d player=%d text=%s",
			event, accepted, interrupt, playernum, text);
}

static s32 accessibilityTrackerNativeActive(void)
{
	return g_Vars.currentplayer
			&& (g_Vars.currentplayer->devicesactive
					& ~g_Vars.currentplayer->devicesinhibit
					& DEVICE_RTRACKER);
}

static s32 accessibilityTrackerInfraredNativeActive(void)
{
	return g_Vars.currentplayer
			&& (g_Vars.currentplayer->devicesactive
					& ~g_Vars.currentplayer->devicesinhibit
					& DEVICE_IRSCANNER);
}

static s32 accessibilityTrackerXrayNativeActive(void)
{
	return g_Vars.currentplayer
			&& (g_Vars.currentplayer->devicesactive
					& ~g_Vars.currentplayer->devicesinhibit
					& DEVICE_XRAYSCANNER)
			&& g_Vars.currentplayer->visionmode == VISIONMODE_XRAY;
}

static const char *accessibilityTrackerScopeReason(s32 source)
{
	if (source == ACCESSIBILITY_TRACKER_SOURCE_RTRACKER
			&& !accessibilityIsRTrackerAudioEnabled()) {
		return "feature_disabled";
	}

	if (source == ACCESSIBILITY_TRACKER_SOURCE_INFRARED
			&& !accessibilityIsIrScannerAudioEnabled()) {
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

	if (g_Vars.currentplayer->activemenumode != AMMODE_CLOSED) {
		return "active_menu_open";
	}

	if (lvIsPaused()) {
		return "paused";
	}

	if (g_Vars.in_cutscene) {
		return "cutscene";
	}

	if (g_Vars.currentplayer->isdead) {
		return "player_dead";
	}

	/*
	 * lvupdate60 can be zero on an ordinary PC render/interpolation frame.
	 * The explicit gates above distinguish actual pauses and invalid scopes.
	 */
	return NULL;
}

static void accessibilityTrackerStopAudio(const char *reason)
{
	s32 i;
	s32 occupied = 0;

	for (i = 0; i < ACCESSIBILITY_TRACKER_SLOT_COUNT; i++) {
		if (g_AccessibilityTrackerSlots[i].identity) {
			occupied++;
		}

		accessibilityToneSetTrackerSlot(i, false, 1.0f, 0.0f, 0.0f,
				ACCESSIBILITY_TRACKER_FAR_PERIOD_MS,
				ACCESSIBILITY_TRACKER_HEIGHT_LEVEL, false, true);
		memset(&g_AccessibilityTrackerSlots[i], 0,
				sizeof(g_AccessibilityTrackerSlots[i]));
		g_AccessibilityTrackerSlots[i].propnum = -1;
	}

	if (occupied) {
		accessibilityLogEvent("rtracker", "audio_stop",
				"frame=%d reason=%s occupied=%d", g_Vars.lvframe60,
				reason ? reason : "", occupied);
	}
}

static s32 accessibilityTrackerFindCandidate(uintptr_t identity)
{
	s32 i;

	for (i = 0; i < g_AccessibilityTrackerCandidateCount; i++) {
		if (g_AccessibilityTrackerCandidates[i].identity == identity) {
			return i;
		}
	}

	return -1;
}

static s32 accessibilityTrackerFindSlot(uintptr_t identity)
{
	s32 i;

	for (i = 0; i < ACCESSIBILITY_TRACKER_SLOT_COUNT; i++) {
		if (g_AccessibilityTrackerSlots[i].identity == identity) {
			return i;
		}
	}

	return -1;
}

static void accessibilityTrackerScan(s32 source)
{
	struct prop *prop = g_Vars.activeprops;
	struct coord *playerpos = &g_Vars.currentplayer->prop->pos;
	f32 lookx = g_Vars.currentplayer->cam_look.x;
	f32 lookz = g_Vars.currentplayer->cam_look.z;
	f32 looklength = sqrtf(lookx * lookx + lookz * lookz);
	s32 overflow = 0;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	u64 scanstart = sysGetMicroseconds();
#endif

	g_AccessibilityTrackerCandidateCount = 0;
	g_AccessibilityTrackerScanCount++;

	if (looklength < 0.0001f) {
		lookx = 0.0f;
		lookz = 1.0f;
	} else {
		lookx /= looklength;
		lookz /= looklength;
	}

	while (prop) {
		s32 category = RADAR_TRACKED_NONE;
		f32 sourcedistance = -1.0f;

		if (source == ACCESSIBILITY_TRACKER_SOURCE_RTRACKER) {
			category = radarGetRTrackedType(prop);
			if (category != RADAR_TRACKED_NONE
					&& accessibilityLandmarkIsCompletedProp(prop)) {
				category = RADAR_TRACKED_NONE;
			}
		} else if (source == ACCESSIBILITY_TRACKER_SOURCE_INFRARED
				&& (prop->flags & PROPFLAG_ONANYSCREENPREVTICK)
				&& (prop->type == PROPTYPE_OBJ
					|| prop->type == PROPTYPE_DOOR
					|| prop->type == PROPTYPE_WEAPON)
				&& objIsHighlightedByInfrared(prop->obj)) {
			category = ACCESSIBILITY_TRACKER_CATEGORY_INFRARED;
		}

		if (category != RADAR_TRACKED_NONE) {
			struct accessibilitytrackercandidate *candidate = NULL;
			f32 dx = prop->pos.x - playerpos->x;
			f32 dz = prop->pos.z - playerpos->z;
			f32 distance = sqrtf(dx * dx + dz * dz);

			if (g_AccessibilityTrackerCandidateCount
					< ACCESSIBILITY_TRACKER_SLOT_COUNT) {
				candidate = &g_AccessibilityTrackerCandidates[
						g_AccessibilityTrackerCandidateCount++];
			} else {
				overflow++;

			}

			if (candidate) {
				memset(candidate, 0, sizeof(*candidate));
				candidate->identity = (uintptr_t)prop;
				candidate->prop = prop;
				candidate->propnum = accessibilityTrackerPropNum(prop);
				candidate->category = category;
				candidate->position = prop->pos;
				candidate->distance = distance;
				candidate->sourcedistance = sourcedistance;
				candidate->clampeddistance = distance
						< ACCESSIBILITY_TRACKER_RADAR_DISTANCE
							? distance : ACCESSIBILITY_TRACKER_RADAR_DISTANCE;
				candidate->heightdelta = prop->pos.y - playerpos->y;
				candidate->nativepan = AL_PAN_CENTER;
				if (distance > 0.0001f) {
					s32 nativepan = psCalculatePan2(&prop->pos, 0, -1.0f,
							NULL);

					candidate->nativepan = nativepan;
					candidate->pan = ((f32)nativepan
							- (f32)AL_PAN_CENTER)
							/ (f32)AL_PAN_CENTER;
					candidate->forwarddot = (dx * lookx + dz * lookz)
							/ distance;
				}
				candidate->frequencyhz
						= accessibilityTrackerCategoryFrequency(category);
				candidate->periodms = ACCESSIBILITY_TRACKER_NEAR_PERIOD_MS
						+ (s32)((ACCESSIBILITY_TRACKER_FAR_PERIOD_MS
								- ACCESSIBILITY_TRACKER_NEAR_PERIOD_MS)
								* candidate->clampeddistance
								/ ACCESSIBILITY_TRACKER_RADAR_DISTANCE);
			}
		}

		prop = prop->next;
	}

	if (overflow) {
		accessibilityLogEvent("rtracker", "overflow",
				"frame=%d source=%s capacity=%d retained=%d suppressed=%d",
				g_Vars.lvframe60, accessibilityTrackerSourceName(source),
				ACCESSIBILITY_TRACKER_SLOT_COUNT,
				g_AccessibilityTrackerCandidateCount, overflow);
	}

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	g_AccessibilityTrackerScanCurrentUs = sysGetMicroseconds() - scanstart;
	g_AccessibilityTrackerScanTotalUs += g_AccessibilityTrackerScanCurrentUs;

	if (g_AccessibilityTrackerScanCurrentUs
			> g_AccessibilityTrackerScanMaxUs) {
		g_AccessibilityTrackerScanMaxUs
				= g_AccessibilityTrackerScanCurrentUs;
	}
#endif
}

static void accessibilityTrackerUpdateSlots(s32 source)
{
	s32 i;

	for (i = 0; i < ACCESSIBILITY_TRACKER_SLOT_COUNT; i++) {
		struct accessibilitytrackerslot *slot = &g_AccessibilityTrackerSlots[i];

		if (slot->identity
				&& accessibilityTrackerFindCandidate(slot->identity) < 0) {
			accessibilityLogEvent("rtracker", "slot_release",
					"frame=%d source=%s slot=%d identity=%p propnum=%d category=%s reason=target_unavailable",
					g_Vars.lvframe60, accessibilityTrackerSourceName(source),
					i, (void *)slot->identity, slot->propnum,
					accessibilityTrackerCategoryName(slot->category));
			accessibilityToneSetTrackerSlot(i, false, 1.0f, 0.0f, 0.0f,
					ACCESSIBILITY_TRACKER_FAR_PERIOD_MS,
					ACCESSIBILITY_TRACKER_HEIGHT_LEVEL, false, true);
			memset(slot, 0, sizeof(*slot));
			slot->propnum = -1;
		}
	}

	for (i = 0; i < g_AccessibilityTrackerCandidateCount; i++) {
		struct accessibilitytrackercandidate *candidate
				= &g_AccessibilityTrackerCandidates[i];
		s32 slotnum = accessibilityTrackerFindSlot(candidate->identity);
		s32 restart = false;
		struct accessibilitytrackerslot *slot;
		s32 oldheight;
		s32 oldrear;

		if (slotnum < 0) {
			s32 j;

			for (j = 0; j < ACCESSIBILITY_TRACKER_SLOT_COUNT; j++) {
				if (!g_AccessibilityTrackerSlots[j].identity) {
					slotnum = j;
					break;
				}
			}
		}

		if (slotnum < 0) {
			continue;
		}

		slot = &g_AccessibilityTrackerSlots[slotnum];
		restart = !slot->identity;
		oldheight = slot->height;
		oldrear = slot->rear;

		if (restart) {
			memset(slot, 0, sizeof(*slot));
			slot->identity = candidate->identity;
			slot->propnum = candidate->propnum;
			slot->category = candidate->category;
			slot->height = candidate->heightdelta
					> ACCESSIBILITY_TRACKER_HEIGHT_THRESHOLD
						? ACCESSIBILITY_TRACKER_HEIGHT_ABOVE
						: candidate->heightdelta
								< -ACCESSIBILITY_TRACKER_HEIGHT_THRESHOLD
							? ACCESSIBILITY_TRACKER_HEIGHT_BELOW
							: ACCESSIBILITY_TRACKER_HEIGHT_LEVEL;
			slot->rear = candidate->forwarddot < 0.0f;
			accessibilityLogEvent("rtracker", "slot_assign",
					"frame=%d source=%s slot=%d identity=%p propnum=%d prop_type=%d category=%s position=%.3f,%.3f,%.3f",
					g_Vars.lvframe60, accessibilityTrackerSourceName(source),
					slotnum, (void *)slot->identity,
					slot->propnum, candidate->prop->type,
					accessibilityTrackerCategoryName(slot->category),
					candidate->position.x, candidate->position.y,
					candidate->position.z);
		} else {
			slot->category = candidate->category;
			slot->height = accessibilityTrackerHeight(
					candidate->heightdelta, slot->height);
			slot->rear = accessibilityTrackerRear(
					candidate->forwarddot, slot->rear);
		}

		accessibilityToneSetTrackerSlot(slotnum, true,
				candidate->frequencyhz, ACCESSIBILITY_TRACKER_VOLUME,
				candidate->pan, candidate->periodms, slot->height, slot->rear,
				restart);

		if (restart || oldheight != slot->height || oldrear != slot->rear
				|| g_AccessibilityTrackerScanCount % 60 == 0) {
			accessibilityLogEvent("rtracker", "candidate",
					"frame=%d scan=%" PRIu64 " source=%s slot=%d identity=%p propnum=%d prop_type=%d category=%s position=%.3f,%.3f,%.3f distance=%.3f source_distance=%.3f clamped_distance=%.3f height_delta=%.3f height=%s native_pan=%d pan=%.5f forward_dot=%.5f rear=%d frequency_hz=%.1f period_ms=%d restart=%d",
					g_Vars.lvframe60,
					(uint64_t)g_AccessibilityTrackerScanCount,
					accessibilityTrackerSourceName(source), slotnum,
					(void *)candidate->identity, candidate->propnum,
					candidate->prop->type,
					accessibilityTrackerCategoryName(candidate->category),
					candidate->position.x, candidate->position.y,
					candidate->position.z, candidate->distance,
					candidate->sourcedistance,
					candidate->clampeddistance, candidate->heightdelta,
					accessibilityTrackerHeightName(slot->height),
					candidate->nativepan, candidate->pan,
					candidate->forwarddot, slot->rear,
					candidate->frequencyhz, candidate->periodms, restart);
		}
	}
}

void accessibilityTrackerTick(void)
{
	const char *scopereason;
	s32 nativeactive;
	s32 infraredactive;
	s32 xrayactive;
	s32 source;
	s32 i;
	s32 occupied;

	if (g_AccessibilityTrackerLastStage != g_Vars.stagenum) {
		accessibilityTrackerReset("stage_changed");
		g_AccessibilityTrackerLastStage = g_Vars.stagenum;
	}

	nativeactive = accessibilityTrackerNativeActive();
	infraredactive = accessibilityTrackerInfraredNativeActive();
	xrayactive = accessibilityTrackerXrayNativeActive();

	if (nativeactive != g_AccessibilityTrackerDeviceActive) {
		g_AccessibilityTrackerDeviceActive = nativeactive;

		if (nativeactive && accessibilityIsRTrackerAudioEnabled()) {
			accessibilityTrackerSpeak("activated", "R-Tracker on", true);
			g_AccessibilityTrackerEmptyPending = true;
			g_AccessibilityTrackerEmptyDeadline = g_Vars.lvframe60
					+ ACCESSIBILITY_TRACKER_EMPTY_DELAY_TICKS;
		} else if (!nativeactive) {
			if (accessibilityIsRTrackerAudioEnabled()) {
				accessibilityTrackerSpeak("deactivated", "R-Tracker off",
						true);
			}
			g_AccessibilityTrackerEmptyPending = false;
		}
	}

	if (infraredactive != g_AccessibilityTrackerInfraredActive) {
		g_AccessibilityTrackerInfraredActive = infraredactive;
		accessibilityLogEvent("rtracker", "infrared_state",
				"frame=%d active=%d", g_Vars.lvframe60, infraredactive);
	}

	if (xrayactive != g_AccessibilityTrackerXrayActive) {
		g_AccessibilityTrackerXrayActive = xrayactive;
		accessibilityLogEvent("rtracker", "xray_state",
				"frame=%d active=%d eraser_position=%.3f,%.3f,%.3f eraser_prop_distance=%.3f",
				g_Vars.lvframe60, xrayactive,
				g_Vars.currentplayer ? g_Vars.currentplayer->eraserpos.x : 0.0f,
				g_Vars.currentplayer ? g_Vars.currentplayer->eraserpos.y : 0.0f,
				g_Vars.currentplayer ? g_Vars.currentplayer->eraserpos.z : 0.0f,
				g_Vars.currentplayer
						? g_Vars.currentplayer->eraserpropdist : 0.0f);
	}

	source = nativeactive ? ACCESSIBILITY_TRACKER_SOURCE_RTRACKER
			: infraredactive ? ACCESSIBILITY_TRACKER_SOURCE_INFRARED
			: ACCESSIBILITY_TRACKER_SOURCE_NONE;

	if (source != g_AccessibilityTrackerSource) {
		accessibilityLogEvent("rtracker", "source_changed",
				"frame=%d previous=%s current=%s", g_Vars.lvframe60,
				accessibilityTrackerSourceName(g_AccessibilityTrackerSource),
				accessibilityTrackerSourceName(source));
		accessibilityTrackerStopAudio("source_changed");
		g_AccessibilityTrackerSource = source;
		g_AccessibilityTrackerAudioSuppressed = false;
		g_AccessibilityTrackerLastScopeReason = NULL;
		g_AccessibilityTrackerScanCount = 0;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
		g_AccessibilityTrackerScanCurrentUs = 0;
		g_AccessibilityTrackerScanTotalUs = 0;
		g_AccessibilityTrackerScanMaxUs = 0;
#endif
	}

	if (source == ACCESSIBILITY_TRACKER_SOURCE_NONE) {
		return;
	}

	scopereason = accessibilityTrackerScopeReason(source);

	if (scopereason) {
		if (!g_AccessibilityTrackerAudioSuppressed
				|| g_AccessibilityTrackerLastScopeReason != scopereason) {
			accessibilityLogEvent("rtracker", "scope",
					"frame=%d source=%s active=0 reason=%s",
					g_Vars.lvframe60,
					accessibilityTrackerSourceName(source), scopereason);
			accessibilityTrackerStopAudio(scopereason);
		}
		g_AccessibilityTrackerAudioSuppressed = true;
		g_AccessibilityTrackerLastScopeReason = scopereason;
		return;
	}

	if (g_AccessibilityTrackerAudioSuppressed) {
		accessibilityLogEvent("rtracker", "scope",
				"frame=%d source=%s active=1 reason=eligible",
				g_Vars.lvframe60,
				accessibilityTrackerSourceName(source));
		g_AccessibilityTrackerAudioSuppressed = false;
		g_AccessibilityTrackerLastScopeReason = NULL;
	}

	/*
	 * Retain the current voices on an interpolation-only render frame, but do
	 * not rescan unchanged logical state or advance diagnostic scan counters.
	 */
	if (g_Vars.lvupdate60 <= 0) {
		return;
	}

	accessibilityTrackerScan(source);
	accessibilityTrackerUpdateSlots(source);

	if (g_AccessibilityTrackerScanCount % 60 == 0) {
		occupied = 0;

		for (i = 0; i < ACCESSIBILITY_TRACKER_SLOT_COUNT; i++) {
			if (g_AccessibilityTrackerSlots[i].identity) {
				occupied++;
			}
		}

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
		accessibilityLogEvent("rtracker", "scan_summary",
				"frame=%d source=%s scans=%" PRIu64 " candidates=%d occupied=%d scan_us=%" PRIu64 " scan_total_us=%" PRIu64 " scan_max_us=%" PRIu64,
				g_Vars.lvframe60, accessibilityTrackerSourceName(source),
				(uint64_t)g_AccessibilityTrackerScanCount,
				g_AccessibilityTrackerCandidateCount, occupied,
				(uint64_t)g_AccessibilityTrackerScanCurrentUs,
				(uint64_t)g_AccessibilityTrackerScanTotalUs,
				(uint64_t)g_AccessibilityTrackerScanMaxUs);
#else
		accessibilityLogEvent("rtracker", "scan_summary",
				"frame=%d source=%s scans=%" PRIu64 " candidates=%d occupied=%d",
				g_Vars.lvframe60, accessibilityTrackerSourceName(source),
				(uint64_t)g_AccessibilityTrackerScanCount,
				g_AccessibilityTrackerCandidateCount, occupied);
#endif
	}

	if (source == ACCESSIBILITY_TRACKER_SOURCE_RTRACKER
			&& g_AccessibilityTrackerCandidateCount > 0) {
		g_AccessibilityTrackerEmptyPending = false;
	} else if (source == ACCESSIBILITY_TRACKER_SOURCE_RTRACKER
			&& g_AccessibilityTrackerEmptyPending
			&& g_Vars.lvframe60 >= g_AccessibilityTrackerEmptyDeadline) {
		accessibilityTrackerSpeak("empty", "No tracked targets", false);
		g_AccessibilityTrackerEmptyPending = false;
	}
}

void accessibilityTrackerReset(const char *reason)
{
	accessibilityTrackerStopAudio(reason);
	memset(g_AccessibilityTrackerCandidates, 0,
			sizeof(g_AccessibilityTrackerCandidates));
	g_AccessibilityTrackerCandidateCount = 0;
	g_AccessibilityTrackerDeviceActive = false;
	g_AccessibilityTrackerInfraredActive = false;
	g_AccessibilityTrackerXrayActive = false;
	g_AccessibilityTrackerSource = ACCESSIBILITY_TRACKER_SOURCE_NONE;
	g_AccessibilityTrackerAudioSuppressed = false;
	g_AccessibilityTrackerEmptyPending = false;
	g_AccessibilityTrackerEmptyDeadline = 0;
	g_AccessibilityTrackerLastScopeReason = NULL;
	g_AccessibilityTrackerScanCount = 0;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	g_AccessibilityTrackerScanCurrentUs = 0;
	g_AccessibilityTrackerScanTotalUs = 0;
	g_AccessibilityTrackerScanMaxUs = 0;
#endif
	accessibilityLogEvent("rtracker", "reset",
			"frame=%d reason=%s", g_Vars.lvframe60, reason ? reason : "");
}
