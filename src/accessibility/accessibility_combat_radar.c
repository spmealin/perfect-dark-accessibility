#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/chraction.h"
#include "game/lv.h"
#include "game/propsnd.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_combat_radar.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_tone.h"
#ifndef PLATFORM_N64
#include "input.h"
#endif

#define ACCESSIBILITY_COMBAT_RADAR_MARKER_CAPACITY 16
#define ACCESSIBILITY_COMBAT_RADAR_CONTACT_CAPACITY 16
#define ACCESSIBILITY_COMBAT_RADAR_EVENT_CAPACITY 48
#define ACCESSIBILITY_COMBAT_RADAR_DISTANCE 4000.0f
#define ACCESSIBILITY_COMBAT_RADAR_MIN_FREQUENCY_HZ 650.0f
#define ACCESSIBILITY_COMBAT_RADAR_MAX_FREQUENCY_HZ 1400.0f
#define ACCESSIBILITY_COMBAT_RADAR_HEIGHT_THRESHOLD 250.0f
#define ACCESSIBILITY_COMBAT_RADAR_MANUAL_SWEEP_TICKS TICKS(48)
#define ACCESSIBILITY_COMBAT_RADAR_MANUAL_START_TICKS TICKS(2)
#define ACCESSIBILITY_COMBAT_RADAR_MIN_START_TICKS TICKS(3)
#define ACCESSIBILITY_COMBAT_RADAR_MANUAL_AGE_TICKS TICKS(12)
#define ACCESSIBILITY_COMBAT_RADAR_MISSING_TICKS TICKS(30)
#define ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS TICKS(180)
#define ACCESSIBILITY_COMBAT_RADAR_TELEMETRY_TICKS TICKS(3600)
#define ACCESSIBILITY_COMBAT_RADAR_CLOSE_REARM_SCALE 1.2f
#define ACCESSIBILITY_COMBAT_RADAR_MEDIUM_REARM_SCALE 1.15f
#define ACCESSIBILITY_COMBAT_RADAR_CLOSE_GAIN 1.15f
#define ACCESSIBILITY_COMBAT_RADAR_ALLY_GAIN 0.72f
#define ACCESSIBILITY_COMBAT_RADAR_OTHER_GAIN 0.85f
#define ACCESSIBILITY_COMBAT_RADAR_TWO_PI 6.28318530717958647692f

enum accessibilitycombatradarcategory {
	ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ENEMY,
	ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OBJECTIVE,
	ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ALLY,
	ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OTHER,
	ACCESSIBILITY_COMBAT_RADAR_CATEGORY_COUNT,
};

enum accessibilitycombatradarheight {
	ACCESSIBILITY_COMBAT_RADAR_HEIGHT_LEVEL,
	ACCESSIBILITY_COMBAT_RADAR_HEIGHT_ABOVE,
	ACCESSIBILITY_COMBAT_RADAR_HEIGHT_BELOW,
};

enum accessibilitycombatradarband {
	ACCESSIBILITY_COMBAT_RADAR_BAND_FAR,
	ACCESSIBILITY_COMBAT_RADAR_BAND_MEDIUM,
	ACCESSIBILITY_COMBAT_RADAR_BAND_CLOSE,
};

enum accessibilitycombatradareventtype {
	ACCESSIBILITY_COMBAT_RADAR_EVENT_MANUAL,
	ACCESSIBILITY_COMBAT_RADAR_EVENT_NEW,
	ACCESSIBILITY_COMBAT_RADAR_EVENT_MEDIUM,
	ACCESSIBILITY_COMBAT_RADAR_EVENT_CLOSE,
	ACCESSIBILITY_COMBAT_RADAR_EVENT_LAUNCH,
	ACCESSIBILITY_COMBAT_RADAR_EVENT_EMPTY,
	ACCESSIBILITY_COMBAT_RADAR_EVENT_UNAVAILABLE,
};

struct accessibilitycombatradarmarker {
	uintptr_t identity;
	s32 identityslot;
	s32 drawindex;
	s32 propnum;
	s32 proptype;
	s32 category;
	s32 relationship;
	struct coord position;
	struct coord relative;
	f32 horizontaldistance;
	f32 verticaldistance;
	f32 bearing;
	f32 pan;
	s32 rear;
	s32 height;
	u32 colour1;
	u32 colour2;
	s32 swapcolours;
};

struct accessibilitycombatradarframe {
	u64 generation;
	s32 available;
	s32 playernum;
	s32 scenario;
	u32 options;
	u32 displayoptions;
	s32 totaldots;
	s32 ownmarkerexcluded;
	s32 retainedcount;
	s32 overflow[ACCESSIBILITY_COMBAT_RADAR_CATEGORY_COUNT];
	struct accessibilitycombatradarmarker
			markers[ACCESSIBILITY_COMBAT_RADAR_MARKER_CAPACITY];
};

struct accessibilitycombatradarcontact {
	uintptr_t identity;
	s32 propnum;
	s32 present;
	s32 seen;
	s32 missingticks;
	s32 band;
	s32 mediumarmed;
	s32 closearmed;
	s32 lastnewtick;
	s32 lastmediumtick;
	s32 lastclosetick;
};

struct accessibilitycombatradarevent {
	s32 type;
	s32 priority;
	s32 dueframe;
	s32 manual;
	uintptr_t identity;
	s32 propnum;
	s32 category;
	s32 height;
	s32 rear;
	f32 distance;
	f32 frequencyhz;
	f32 gain;
	f32 pan;
};

static struct accessibilitycombatradarframe g_AccessibilityCombatRadarBuilding;
static struct accessibilitycombatradarframe g_AccessibilityCombatRadarFrame;
static struct accessibilitycombatradarcontact
		g_AccessibilityCombatRadarContacts[ACCESSIBILITY_COMBAT_RADAR_CONTACT_CAPACITY];
static struct accessibilitycombatradarevent
		g_AccessibilityCombatRadarEvents[ACCESSIBILITY_COMBAT_RADAR_EVENT_CAPACITY];
static u64 g_AccessibilityCombatRadarGeneration;
static u64 g_AccessibilityCombatRadarObservedGeneration;
static u64 g_AccessibilityCombatRadarCapturedFrames;
static u64 g_AccessibilityCombatRadarCommands;
static u64 g_AccessibilityCombatRadarTransitions;
static u64 g_AccessibilityCombatRadarEventsEmitted;
static u64 g_AccessibilityCombatRadarEventsDropped;
static s32 g_AccessibilityCombatRadarCaptureActive;
static s32 g_AccessibilityCombatRadarCapturingOwnMarker;
static s32 g_AccessibilityCombatRadarObjectiveSlot;
static s32 g_AccessibilityCombatRadarEventCount;
static s32 g_AccessibilityCombatRadarMaximumQueue;
static s32 g_AccessibilityCombatRadarNextVoiceFrame;
static s32 g_AccessibilityCombatRadarNextTelemetryFrame;
static s32 g_AccessibilityCombatRadarLastAvailability = -1;
static s32 g_AccessibilityCombatRadarProcessedAvailability = -1;
static s32 g_AccessibilityCombatRadarSuppressed;
static s32 g_AccessibilityCombatRadarNeedsBaseline = true;

static const char *accessibilityCombatRadarCategoryName(s32 category)
{
	switch (category) {
	case ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ENEMY:
		return "enemy";
	case ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OBJECTIVE:
		return "objective";
	case ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ALLY:
		return "ally";
	default:
		return "other";
	}
}

static const char *accessibilityCombatRadarHeightName(s32 height)
{
	switch (height) {
	case ACCESSIBILITY_COMBAT_RADAR_HEIGHT_ABOVE:
		return "above";
	case ACCESSIBILITY_COMBAT_RADAR_HEIGHT_BELOW:
		return "below";
	default:
		return "level";
	}
}

static const char *accessibilityCombatRadarBandName(s32 band)
{
	switch (band) {
	case ACCESSIBILITY_COMBAT_RADAR_BAND_MEDIUM:
		return "medium";
	case ACCESSIBILITY_COMBAT_RADAR_BAND_CLOSE:
		return "close";
	default:
		return "far";
	}
}

static const char *accessibilityCombatRadarEventName(s32 type)
{
	switch (type) {
	case ACCESSIBILITY_COMBAT_RADAR_EVENT_MANUAL:
		return "manual";
	case ACCESSIBILITY_COMBAT_RADAR_EVENT_NEW:
		return "new_contact";
	case ACCESSIBILITY_COMBAT_RADAR_EVENT_MEDIUM:
		return "medium";
	case ACCESSIBILITY_COMBAT_RADAR_EVENT_CLOSE:
		return "close";
	case ACCESSIBILITY_COMBAT_RADAR_EVENT_LAUNCH:
		return "launch";
	case ACCESSIBILITY_COMBAT_RADAR_EVENT_EMPTY:
		return "empty";
	default:
		return "unavailable";
	}
}

static s32 accessibilityCombatRadarPropNum(const struct prop *prop)
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

static s32 accessibilityCombatRadarCategoryPriority(s32 category)
{
	switch (category) {
	case ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ENEMY:
		return 4;
	case ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OBJECTIVE:
		return 3;
	case ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ALLY:
		return 2;
	default:
		return 1;
	}
}

static s32 accessibilityCombatRadarClassify(struct prop *prop,
		s32 *relationship)
{
	*relationship = 0;

	if (prop && (prop->type == PROPTYPE_CHR || prop->type == PROPTYPE_PLAYER)
			&& prop->chr && g_Vars.currentplayer
			&& g_Vars.currentplayer->prop
			&& g_Vars.currentplayer->prop->chr) {
		if (chrCompareTeams(g_Vars.currentplayer->prop->chr,
				prop->chr, COMPARE_ENEMIES)) {
			*relationship = COMPARE_ENEMIES;
			return ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ENEMY;
		}

		*relationship = COMPARE_FRIENDS;
		return ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ALLY;
	}

	if (!prop || prop->type == PROPTYPE_OBJ || prop->type == PROPTYPE_DOOR
			|| prop->type == PROPTYPE_WEAPON) {
		return ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OBJECTIVE;
	}

	return ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OTHER;
}

static f32 accessibilityCombatRadarFrequency(f32 distance)
{
	f32 clamped = distance;

	if (clamped < 0.0f) {
		clamped = 0.0f;
	} else if (clamped > ACCESSIBILITY_COMBAT_RADAR_DISTANCE) {
		clamped = ACCESSIBILITY_COMBAT_RADAR_DISTANCE;
	}

	return ACCESSIBILITY_COMBAT_RADAR_MIN_FREQUENCY_HZ
			+ (ACCESSIBILITY_COMBAT_RADAR_MAX_FREQUENCY_HZ
				- ACCESSIBILITY_COMBAT_RADAR_MIN_FREQUENCY_HZ)
				* (1.0f - clamped / ACCESSIBILITY_COMBAT_RADAR_DISTANCE);
}

static void accessibilityCombatRadarPublish(void)
{
	g_AccessibilityCombatRadarBuilding.generation
			= ++g_AccessibilityCombatRadarGeneration;
	g_AccessibilityCombatRadarFrame = g_AccessibilityCombatRadarBuilding;
	g_AccessibilityCombatRadarCapturedFrames++;

	if (g_AccessibilityCombatRadarLastAvailability
			!= g_AccessibilityCombatRadarFrame.available
			|| g_AccessibilityCombatRadarFrame.overflow[
					ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ENEMY]
			|| g_AccessibilityCombatRadarFrame.overflow[
					ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OBJECTIVE]
			|| g_AccessibilityCombatRadarFrame.overflow[
					ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ALLY]
			|| g_AccessibilityCombatRadarFrame.overflow[
					ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OTHER]) {
		accessibilityLogEvent("combat_radar", "frame",
				"generation=%" PRIu64 " available=%d player=%d scenario=%d options=0x%08x display_options=0x%08x total_dots=%d own_excluded=%d retained=%d overflow_enemy=%d overflow_objective=%d overflow_ally=%d overflow_other=%d",
				(uint64_t)g_AccessibilityCombatRadarFrame.generation,
				g_AccessibilityCombatRadarFrame.available,
				g_AccessibilityCombatRadarFrame.playernum,
				g_AccessibilityCombatRadarFrame.scenario,
				g_AccessibilityCombatRadarFrame.options,
				g_AccessibilityCombatRadarFrame.displayoptions,
				g_AccessibilityCombatRadarFrame.totaldots,
				g_AccessibilityCombatRadarFrame.ownmarkerexcluded,
				g_AccessibilityCombatRadarFrame.retainedcount,
				g_AccessibilityCombatRadarFrame.overflow[
						ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ENEMY],
				g_AccessibilityCombatRadarFrame.overflow[
						ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OBJECTIVE],
				g_AccessibilityCombatRadarFrame.overflow[
						ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ALLY],
				g_AccessibilityCombatRadarFrame.overflow[
						ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OTHER]);
		g_AccessibilityCombatRadarLastAvailability
				= g_AccessibilityCombatRadarFrame.available;
	}
}

void accessibilityCombatRadarCaptureBegin(s32 available)
{
	memset(&g_AccessibilityCombatRadarBuilding, 0,
			sizeof(g_AccessibilityCombatRadarBuilding));
	g_AccessibilityCombatRadarBuilding.available = available
			&& accessibilityIsCombatRadarAudioEnabled()
			&& g_Vars.normmplayerisrunning && PLAYERCOUNT() == 1;
	g_AccessibilityCombatRadarBuilding.playernum = g_Vars.currentplayernum;
	g_AccessibilityCombatRadarBuilding.scenario = g_MpSetup.scenario;
	g_AccessibilityCombatRadarBuilding.options = g_MpSetup.options;
	if (g_Vars.currentplayerstats) {
		g_AccessibilityCombatRadarBuilding.displayoptions
				= g_PlayerConfigsArray[
						g_Vars.currentplayerstats->mpindex].base.displayoptions;
	}
	g_AccessibilityCombatRadarCaptureActive
			= g_AccessibilityCombatRadarBuilding.available;
	g_AccessibilityCombatRadarCapturingOwnMarker = false;
	g_AccessibilityCombatRadarObjectiveSlot = 0;

	if (!g_AccessibilityCombatRadarCaptureActive) {
		accessibilityCombatRadarPublish();
	}
}

void accessibilityCombatRadarCaptureSetOwnMarker(s32 ownmarker)
{
	g_AccessibilityCombatRadarCapturingOwnMarker = ownmarker != 0;
}

void accessibilityCombatRadarCaptureDot(struct prop *prop,
		const struct coord *relative, u32 colour1, u32 colour2,
		s32 swapcolours, s32 yindicators)
{
	struct accessibilitycombatradarmarker marker;
	f32 lookx;
	f32 lookz;
	f32 looklength;
	f32 rightdot = 0.0f;
	f32 forwarddot = 1.0f;
	s32 replace = -1;
	s32 category;
	s32 i;

	if (!g_AccessibilityCombatRadarCaptureActive || !relative) {
		return;
	}

	g_AccessibilityCombatRadarBuilding.totaldots++;

	if (g_AccessibilityCombatRadarCapturingOwnMarker
			|| (g_Vars.currentplayer
				&& prop == g_Vars.currentplayer->prop)) {
		g_AccessibilityCombatRadarBuilding.ownmarkerexcluded++;
		return;
	}

	memset(&marker, 0, sizeof(marker));
	category = accessibilityCombatRadarClassify(prop, &marker.relationship);
	marker.identity = (uintptr_t)prop;
	marker.identityslot = prop ? -1 : g_AccessibilityCombatRadarObjectiveSlot++;
	marker.drawindex = g_AccessibilityCombatRadarBuilding.totaldots - 1;
	marker.propnum = accessibilityCombatRadarPropNum(prop);
	marker.proptype = prop ? prop->type : -1;
	marker.category = category;
	marker.relative = *relative;
	marker.horizontaldistance = sqrtf(
			relative->x * relative->x + relative->z * relative->z);
	marker.verticaldistance = relative->y;
	marker.height = yindicators && relative->y
			> ACCESSIBILITY_COMBAT_RADAR_HEIGHT_THRESHOLD
		? ACCESSIBILITY_COMBAT_RADAR_HEIGHT_ABOVE
		: yindicators && relative->y
				< -ACCESSIBILITY_COMBAT_RADAR_HEIGHT_THRESHOLD
			? ACCESSIBILITY_COMBAT_RADAR_HEIGHT_BELOW
			: ACCESSIBILITY_COMBAT_RADAR_HEIGHT_LEVEL;
	marker.colour1 = colour1;
	marker.colour2 = colour2;
	marker.swapcolours = swapcolours;

	if (g_Vars.currentplayer && g_Vars.currentplayer->prop) {
		s32 nativepan;

		marker.position.x = g_Vars.currentplayer->prop->pos.x + relative->x;
		marker.position.y = g_Vars.currentplayer->prop->pos.y + relative->y;
		marker.position.z = g_Vars.currentplayer->prop->pos.z + relative->z;
		nativepan = psCalculatePan2(&marker.position, 0, -1.0f, NULL);
		marker.pan = ((f32)nativepan - (f32)AL_PAN_CENTER)
				/ (f32)AL_PAN_CENTER;
	}

	lookx = g_Vars.currentplayer ? g_Vars.currentplayer->cam_look.x : 0.0f;
	lookz = g_Vars.currentplayer ? g_Vars.currentplayer->cam_look.z : 1.0f;
	looklength = sqrtf(lookx * lookx + lookz * lookz);

	if (looklength > 0.0001f && marker.horizontaldistance > 0.0001f) {
		lookx /= looklength;
		lookz /= looklength;
		forwarddot = (relative->x * lookx + relative->z * lookz)
				/ marker.horizontaldistance;
		rightdot = (relative->x * lookz - relative->z * lookx)
				/ marker.horizontaldistance;
	}

	marker.bearing = atan2f(rightdot, forwarddot);
	if (marker.bearing < 0.0f) {
		marker.bearing += ACCESSIBILITY_COMBAT_RADAR_TWO_PI;
	}
	marker.rear = forwarddot < 0.0f;

	if (g_AccessibilityCombatRadarBuilding.retainedcount
			< ACCESSIBILITY_COMBAT_RADAR_MARKER_CAPACITY) {
		g_AccessibilityCombatRadarBuilding.markers[
				g_AccessibilityCombatRadarBuilding.retainedcount++] = marker;
		return;
	}

	for (i = 0; i < ACCESSIBILITY_COMBAT_RADAR_MARKER_CAPACITY; i++) {
		struct accessibilitycombatradarmarker *retained
				= &g_AccessibilityCombatRadarBuilding.markers[i];

		if (accessibilityCombatRadarCategoryPriority(retained->category)
				< accessibilityCombatRadarCategoryPriority(category)
				&& (replace < 0
					|| accessibilityCombatRadarCategoryPriority(
							retained->category)
						< accessibilityCombatRadarCategoryPriority(
							g_AccessibilityCombatRadarBuilding
									.markers[replace].category))) {
			replace = i;
		}
	}

	if (replace >= 0) {
		g_AccessibilityCombatRadarBuilding.overflow[
				g_AccessibilityCombatRadarBuilding.markers[replace].category]++;
		g_AccessibilityCombatRadarBuilding.markers[replace] = marker;
	} else {
		g_AccessibilityCombatRadarBuilding.overflow[category]++;
	}
}

void accessibilityCombatRadarCaptureEnd(void)
{
	if (!g_AccessibilityCombatRadarCaptureActive) {
		return;
	}

	g_AccessibilityCombatRadarCaptureActive = false;
	g_AccessibilityCombatRadarCapturingOwnMarker = false;
	accessibilityCombatRadarPublish();
}

static void accessibilityCombatRadarClearEvents(s32 manualonly,
		const char *reason)
{
	s32 write = 0;
	s32 i;

	for (i = 0; i < g_AccessibilityCombatRadarEventCount; i++) {
		if (manualonly && !g_AccessibilityCombatRadarEvents[i].manual) {
			if (write != i) {
				g_AccessibilityCombatRadarEvents[write]
						= g_AccessibilityCombatRadarEvents[i];
			}
			write++;
		}
	}

	if (!manualonly) {
		write = 0;
	}

	if (write != g_AccessibilityCombatRadarEventCount) {
		accessibilityLogEvent("combat_radar", "event",
				"action=clear reason=%s manual_only=%d cleared=%d retained=%d",
				reason, manualonly,
				g_AccessibilityCombatRadarEventCount - write, write);
	}

	g_AccessibilityCombatRadarEventCount = write;
}

static void accessibilityCombatRadarClearAutomaticEvents(const char *reason)
{
	s32 write = 0;
	s32 i;

	for (i = 0; i < g_AccessibilityCombatRadarEventCount; i++) {
		if (g_AccessibilityCombatRadarEvents[i].manual) {
			if (write != i) {
				g_AccessibilityCombatRadarEvents[write]
						= g_AccessibilityCombatRadarEvents[i];
			}
			write++;
		}
	}

	if (write != g_AccessibilityCombatRadarEventCount) {
		accessibilityLogEvent("combat_radar", "event",
				"action=clear reason=%s manual_only=0 automatic_only=1 cleared=%d retained=%d",
				reason, g_AccessibilityCombatRadarEventCount - write, write);
	}

	g_AccessibilityCombatRadarEventCount = write;
}

static s32 accessibilityCombatRadarQueueEvent(
		const struct accessibilitycombatradarevent *event)
{
	s32 replace = -1;
	s32 i;

	if (g_AccessibilityCombatRadarEventCount
			< ACCESSIBILITY_COMBAT_RADAR_EVENT_CAPACITY) {
		g_AccessibilityCombatRadarEvents[
				g_AccessibilityCombatRadarEventCount++] = *event;
	} else {
		for (i = 0; i < g_AccessibilityCombatRadarEventCount; i++) {
			if (g_AccessibilityCombatRadarEvents[i].priority < event->priority
					&& (replace < 0
						|| g_AccessibilityCombatRadarEvents[i].priority
							< g_AccessibilityCombatRadarEvents[
									replace].priority)) {
				replace = i;
			}
		}

		if (replace < 0) {
			g_AccessibilityCombatRadarEventsDropped++;
			accessibilityLogEvent("combat_radar", "event",
					"type=%s action=dropped reason=queue_full priority=%d queue=%d",
					accessibilityCombatRadarEventName(event->type),
					event->priority, g_AccessibilityCombatRadarEventCount);
			return false;
		}

		g_AccessibilityCombatRadarEventsDropped++;
		g_AccessibilityCombatRadarEvents[replace] = *event;
	}

	if (g_AccessibilityCombatRadarEventCount
			> g_AccessibilityCombatRadarMaximumQueue) {
		g_AccessibilityCombatRadarMaximumQueue
				= g_AccessibilityCombatRadarEventCount;
	}

	accessibilityLogEvent("combat_radar", "event",
			"type=%s action=queued priority=%d due_frame=%d manual=%d identity=%p propnum=%d category=%s distance=%.3f frequency_hz=%.1f gain=%.3f pan=%.4f rear=%d height=%s queue=%d",
			accessibilityCombatRadarEventName(event->type), event->priority,
			event->dueframe, event->manual, (void *)event->identity,
			event->propnum,
			accessibilityCombatRadarCategoryName(event->category),
			event->distance, event->frequencyhz, event->gain, event->pan,
			event->rear, accessibilityCombatRadarHeightName(event->height),
			g_AccessibilityCombatRadarEventCount);
	return true;
}

static void accessibilityCombatRadarFillMarkerEvent(
		struct accessibilitycombatradarevent *event,
		const struct accessibilitycombatradarmarker *marker,
		s32 type, s32 dueframe)
{
	f32 volume;

	memset(event, 0, sizeof(*event));
	accessibilityGetCombatRadarTuning(NULL, NULL, &volume);
	event->type = type;
	event->priority = type == ACCESSIBILITY_COMBAT_RADAR_EVENT_CLOSE ? 4
			: type == ACCESSIBILITY_COMBAT_RADAR_EVENT_MEDIUM ? 3
			: type == ACCESSIBILITY_COMBAT_RADAR_EVENT_NEW ? 2 : 1;
	event->dueframe = dueframe;
	event->manual = type == ACCESSIBILITY_COMBAT_RADAR_EVENT_MANUAL
			|| type == ACCESSIBILITY_COMBAT_RADAR_EVENT_LAUNCH
			|| type == ACCESSIBILITY_COMBAT_RADAR_EVENT_EMPTY
			|| type == ACCESSIBILITY_COMBAT_RADAR_EVENT_UNAVAILABLE;
	event->identity = marker ? marker->identity : 0;
	event->propnum = marker ? marker->propnum : -1;
	event->category = marker ? marker->category
			: ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OTHER;
	event->height = marker ? marker->height
			: ACCESSIBILITY_COMBAT_RADAR_HEIGHT_LEVEL;
	event->rear = marker ? marker->rear : false;
	event->distance = marker ? marker->horizontaldistance : 0.0f;
	event->frequencyhz = marker
			? accessibilityCombatRadarFrequency(marker->horizontaldistance)
			: type == ACCESSIBILITY_COMBAT_RADAR_EVENT_LAUNCH ? 1500.0f
			: type == ACCESSIBILITY_COMBAT_RADAR_EVENT_EMPTY ? 760.0f
			: 320.0f;
	event->pan = marker ? marker->pan : 0.0f;
	event->gain = volume;

	if (marker && marker->category == ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ALLY) {
		event->gain *= ACCESSIBILITY_COMBAT_RADAR_ALLY_GAIN;
	} else if (marker
			&& marker->category == ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OTHER) {
		event->gain *= ACCESSIBILITY_COMBAT_RADAR_OTHER_GAIN;
	}
	if (type == ACCESSIBILITY_COMBAT_RADAR_EVENT_CLOSE) {
		event->gain *= ACCESSIBILITY_COMBAT_RADAR_CLOSE_GAIN;
	}
}

static s32 accessibilityCombatRadarBandForDistance(f32 distance,
		s32 previous, f32 mediumdistance, f32 closedistance)
{
	if (previous == ACCESSIBILITY_COMBAT_RADAR_BAND_CLOSE
			&& distance <= closedistance
					* ACCESSIBILITY_COMBAT_RADAR_CLOSE_REARM_SCALE) {
		return ACCESSIBILITY_COMBAT_RADAR_BAND_CLOSE;
	}
	if (distance <= closedistance) {
		return ACCESSIBILITY_COMBAT_RADAR_BAND_CLOSE;
	}
	if (previous == ACCESSIBILITY_COMBAT_RADAR_BAND_MEDIUM
			&& distance <= mediumdistance
					* ACCESSIBILITY_COMBAT_RADAR_MEDIUM_REARM_SCALE) {
		return ACCESSIBILITY_COMBAT_RADAR_BAND_MEDIUM;
	}
	if (distance <= mediumdistance) {
		return ACCESSIBILITY_COMBAT_RADAR_BAND_MEDIUM;
	}
	return ACCESSIBILITY_COMBAT_RADAR_BAND_FAR;
}

static s32 accessibilityCombatRadarFindContact(uintptr_t identity)
{
	s32 i;

	for (i = 0; i < ACCESSIBILITY_COMBAT_RADAR_CONTACT_CAPACITY; i++) {
		if (g_AccessibilityCombatRadarContacts[i].identity == identity) {
			return i;
		}
	}

	return -1;
}

static s32 accessibilityCombatRadarFindFreeContact(void)
{
	s32 i;

	for (i = 0; i < ACCESSIBILITY_COMBAT_RADAR_CONTACT_CAPACITY; i++) {
		if (!g_AccessibilityCombatRadarContacts[i].identity) {
			return i;
		}
	}

	return -1;
}

static void accessibilityCombatRadarQueueContactEvent(
		const struct accessibilitycombatradarmarker *marker, s32 type)
{
	struct accessibilitycombatradarevent event;

	if (!accessibilityGetCombatRadarContactAlerts()) {
		return;
	}

	accessibilityCombatRadarFillMarkerEvent(&event, marker, type,
			g_Vars.lvframe60);
	accessibilityCombatRadarQueueEvent(&event);
	g_AccessibilityCombatRadarTransitions++;
}

static void accessibilityCombatRadarBaselineContacts(void)
{
	f32 mediumdistance;
	f32 closedistance;
	s32 i;

	memset(g_AccessibilityCombatRadarContacts, 0,
			sizeof(g_AccessibilityCombatRadarContacts));
	accessibilityGetCombatRadarTuning(&mediumdistance, &closedistance, NULL);

	for (i = 0; i < g_AccessibilityCombatRadarFrame.retainedcount; i++) {
		struct accessibilitycombatradarmarker *marker
				= &g_AccessibilityCombatRadarFrame.markers[i];
		s32 slot;

		if (marker->category != ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ENEMY
				|| !marker->identity) {
			continue;
		}

		slot = accessibilityCombatRadarFindFreeContact();
		if (slot < 0) {
			break;
		}

		g_AccessibilityCombatRadarContacts[slot].identity = marker->identity;
		g_AccessibilityCombatRadarContacts[slot].propnum = marker->propnum;
		g_AccessibilityCombatRadarContacts[slot].present = true;
		g_AccessibilityCombatRadarContacts[slot].seen = true;
		g_AccessibilityCombatRadarContacts[slot].band
				= accessibilityCombatRadarBandForDistance(
						marker->horizontaldistance,
						ACCESSIBILITY_COMBAT_RADAR_BAND_FAR,
						mediumdistance, closedistance);
		g_AccessibilityCombatRadarContacts[slot].mediumarmed
				= marker->horizontaldistance
					> mediumdistance
						* ACCESSIBILITY_COMBAT_RADAR_MEDIUM_REARM_SCALE;
		g_AccessibilityCombatRadarContacts[slot].closearmed
				= marker->horizontaldistance
					> closedistance
						* ACCESSIBILITY_COMBAT_RADAR_CLOSE_REARM_SCALE;
		g_AccessibilityCombatRadarContacts[slot].lastnewtick
				= -ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS;
		g_AccessibilityCombatRadarContacts[slot].lastmediumtick
				= -ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS;
		g_AccessibilityCombatRadarContacts[slot].lastclosetick
				= -ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS;
		accessibilityLogEvent("combat_radar", "contact",
				"action=baseline identity=%p propnum=%d band=%s distance=%.3f medium_enter=%.3f close_enter=%.3f medium_armed=%d close_armed=%d",
				(void *)marker->identity, marker->propnum,
				accessibilityCombatRadarBandName(
						g_AccessibilityCombatRadarContacts[slot].band),
				marker->horizontaldistance, mediumdistance, closedistance,
				g_AccessibilityCombatRadarContacts[slot].mediumarmed,
				g_AccessibilityCombatRadarContacts[slot].closearmed);
	}

	g_AccessibilityCombatRadarNeedsBaseline = false;
}

static void accessibilityCombatRadarUpdateContacts(void)
{
	f32 mediumdistance;
	f32 closedistance;
	s32 i;

	if (!accessibilityGetCombatRadarContactAlerts()) {
		return;
	}

	if (g_AccessibilityCombatRadarNeedsBaseline) {
		accessibilityCombatRadarBaselineContacts();
		return;
	}

	accessibilityGetCombatRadarTuning(&mediumdistance, &closedistance, NULL);

	for (i = 0; i < ACCESSIBILITY_COMBAT_RADAR_CONTACT_CAPACITY; i++) {
		g_AccessibilityCombatRadarContacts[i].seen = false;
	}

	for (i = 0; i < g_AccessibilityCombatRadarFrame.retainedcount; i++) {
		struct accessibilitycombatradarmarker *marker
				= &g_AccessibilityCombatRadarFrame.markers[i];
		struct accessibilitycombatradarcontact *contact;
		s32 slot;
		s32 oldband;
		s32 newband;
		s32 eventtype = -1;

		if (marker->category != ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ENEMY
				|| !marker->identity) {
			continue;
		}

		slot = accessibilityCombatRadarFindContact(marker->identity);
		if (slot < 0
				|| !g_AccessibilityCombatRadarContacts[slot].present) {
			s32 cooldownready;

			if (slot < 0) {
				slot = accessibilityCombatRadarFindFreeContact();
			}
			if (slot < 0) {
				continue;
			}

			contact = &g_AccessibilityCombatRadarContacts[slot];
			if (!contact->identity) {
				memset(contact, 0, sizeof(*contact));
				contact->identity = marker->identity;
				contact->lastnewtick
						= -ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS;
				contact->lastmediumtick
						= -ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS;
				contact->lastclosetick
						= -ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS;
			}
			contact->propnum = marker->propnum;
			contact->present = true;
			contact->band = accessibilityCombatRadarBandForDistance(
					marker->horizontaldistance,
					ACCESSIBILITY_COMBAT_RADAR_BAND_FAR,
					mediumdistance, closedistance);
			eventtype = contact->band == ACCESSIBILITY_COMBAT_RADAR_BAND_CLOSE
					? ACCESSIBILITY_COMBAT_RADAR_EVENT_CLOSE
					: contact->band == ACCESSIBILITY_COMBAT_RADAR_BAND_MEDIUM
						? ACCESSIBILITY_COMBAT_RADAR_EVENT_MEDIUM
						: ACCESSIBILITY_COMBAT_RADAR_EVENT_NEW;
			if (eventtype == ACCESSIBILITY_COMBAT_RADAR_EVENT_CLOSE) {
				cooldownready = g_Vars.lvframe60 - contact->lastclosetick
						>= ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS;
				contact->lastclosetick = g_Vars.lvframe60;
			} else if (eventtype
					== ACCESSIBILITY_COMBAT_RADAR_EVENT_MEDIUM) {
				cooldownready = g_Vars.lvframe60 - contact->lastmediumtick
						>= ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS;
				contact->lastmediumtick = g_Vars.lvframe60;
			} else {
				cooldownready = g_Vars.lvframe60 - contact->lastnewtick
						>= ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS;
				contact->lastnewtick = g_Vars.lvframe60;
			}
			contact->mediumarmed
					= marker->horizontaldistance > mediumdistance
						* ACCESSIBILITY_COMBAT_RADAR_MEDIUM_REARM_SCALE;
			contact->closearmed
					= marker->horizontaldistance > closedistance
						* ACCESSIBILITY_COMBAT_RADAR_CLOSE_REARM_SCALE;
			accessibilityLogEvent("combat_radar", "contact",
					"action=admit identity=%p propnum=%d band=%s distance=%.3f event=%s cooldown_ready=%d",
					(void *)contact->identity, contact->propnum,
					accessibilityCombatRadarBandName(contact->band),
					marker->horizontaldistance,
					accessibilityCombatRadarEventName(eventtype),
					cooldownready);
			if (cooldownready) {
				accessibilityCombatRadarQueueContactEvent(marker, eventtype);
			}
		} else {
			contact = &g_AccessibilityCombatRadarContacts[slot];
			oldband = contact->band;
			if (marker->horizontaldistance
					> mediumdistance
						* ACCESSIBILITY_COMBAT_RADAR_MEDIUM_REARM_SCALE) {
				contact->mediumarmed = true;
			}
			if (marker->horizontaldistance
					> closedistance
						* ACCESSIBILITY_COMBAT_RADAR_CLOSE_REARM_SCALE) {
				contact->closearmed = true;
			}

			newband = accessibilityCombatRadarBandForDistance(
					marker->horizontaldistance, oldband,
					mediumdistance, closedistance);

			if (newband == ACCESSIBILITY_COMBAT_RADAR_BAND_CLOSE
					&& oldband != ACCESSIBILITY_COMBAT_RADAR_BAND_CLOSE
					&& contact->closearmed
					&& g_Vars.lvframe60 - contact->lastclosetick
						>= ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS) {
				eventtype = ACCESSIBILITY_COMBAT_RADAR_EVENT_CLOSE;
				contact->lastclosetick = g_Vars.lvframe60;
				contact->closearmed = false;
				contact->mediumarmed = false;
			} else if (newband == ACCESSIBILITY_COMBAT_RADAR_BAND_MEDIUM
					&& oldband == ACCESSIBILITY_COMBAT_RADAR_BAND_FAR
					&& contact->mediumarmed
					&& g_Vars.lvframe60 - contact->lastmediumtick
						>= ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS) {
				eventtype = ACCESSIBILITY_COMBAT_RADAR_EVENT_MEDIUM;
				contact->lastmediumtick = g_Vars.lvframe60;
				contact->mediumarmed = false;
			}

			if (newband != oldband) {
				accessibilityLogEvent("combat_radar", "contact",
						"action=band identity=%p propnum=%d previous=%s current=%s distance=%.3f event=%s medium_armed=%d close_armed=%d cooldown_ticks=%d",
						(void *)contact->identity, contact->propnum,
						accessibilityCombatRadarBandName(oldband),
						accessibilityCombatRadarBandName(newband),
						marker->horizontaldistance,
						eventtype >= 0
							? accessibilityCombatRadarEventName(eventtype)
							: "none",
						contact->mediumarmed, contact->closearmed,
						ACCESSIBILITY_COMBAT_RADAR_COOLDOWN_TICKS);
			}

			contact->band = newband;
			if (eventtype >= 0) {
				accessibilityCombatRadarQueueContactEvent(marker, eventtype);
			}
		}

		contact->seen = true;
		contact->missingticks = 0;
	}

	for (i = 0; i < ACCESSIBILITY_COMBAT_RADAR_CONTACT_CAPACITY; i++) {
		struct accessibilitycombatradarcontact *contact
				= &g_AccessibilityCombatRadarContacts[i];

		if (!contact->identity || !contact->present || contact->seen) {
			continue;
		}

		contact->missingticks += g_Vars.lvupdate60 > 0
				? g_Vars.lvupdate60 : 0;
		if (contact->missingticks >= ACCESSIBILITY_COMBAT_RADAR_MISSING_TICKS) {
			accessibilityLogEvent("combat_radar", "contact",
					"action=remove identity=%p propnum=%d previous_band=%s missing_ticks=%d reason=native_marker_absent",
					(void *)contact->identity, contact->propnum,
					accessibilityCombatRadarBandName(contact->band),
					contact->missingticks);
			contact->present = false;
			contact->seen = false;
			contact->missingticks = 0;
		}
	}
}

static void accessibilityCombatRadarLogMarker(
		const struct accessibilitycombatradarmarker *marker, s32 retained)
{
	accessibilityLogEvent("combat_radar", "marker",
			"generation=%" PRIu64 " draw_index=%d retained=%d identity=%p identity_slot=%d propnum=%d prop_type=%d category=%s relationship=%d position=%.3f,%.3f,%.3f relative=%.3f,%.3f,%.3f horizontal_distance=%.3f vertical_distance=%.3f bearing_radians=%.5f pan=%.5f rear=%d height=%s colour1=0x%08x colour2=0x%08x swap=%d",
			(uint64_t)g_AccessibilityCombatRadarFrame.generation,
			marker->drawindex, retained, (void *)marker->identity,
			marker->identityslot, marker->propnum, marker->proptype,
			accessibilityCombatRadarCategoryName(marker->category),
			marker->relationship,
			marker->position.x, marker->position.y, marker->position.z,
			marker->relative.x, marker->relative.y, marker->relative.z,
			marker->horizontaldistance, marker->verticaldistance,
			marker->bearing, marker->pan, marker->rear,
			accessibilityCombatRadarHeightName(marker->height),
			marker->colour1, marker->colour2, marker->swapcolours);
}

static void accessibilityCombatRadarScheduleManual(void)
{
	s32 order[ACCESSIBILITY_COMBAT_RADAR_MARKER_CAPACITY];
	s32 start = g_Vars.lvframe60 + ACCESSIBILITY_COMBAT_RADAR_MANUAL_START_TICKS;
	s32 previousdue = -1;
	s32 count = g_AccessibilityCombatRadarFrame.retainedcount;
	s32 i;
	s32 j;
	struct accessibilitycombatradarevent event;

	accessibilityCombatRadarClearEvents(true, "manual_replaced");
	accessibilityCombatRadarFillMarkerEvent(&event, NULL,
			ACCESSIBILITY_COMBAT_RADAR_EVENT_LAUNCH, g_Vars.lvframe60);
	accessibilityCombatRadarQueueEvent(&event);

	for (i = 0; i < count; i++) {
		order[i] = i;
	}
	for (i = 1; i < count; i++) {
		s32 value = order[i];

		for (j = i; j > 0
				&& g_AccessibilityCombatRadarFrame.markers[order[j - 1]].bearing
					> g_AccessibilityCombatRadarFrame.markers[value].bearing;
				j--) {
			order[j] = order[j - 1];
		}
		order[j] = value;
	}

	for (i = 0; i < count; i++) {
		struct accessibilitycombatradarmarker *marker
				= &g_AccessibilityCombatRadarFrame.markers[order[i]];
		s32 due = start + (s32)(marker->bearing
				/ ACCESSIBILITY_COMBAT_RADAR_TWO_PI
				* (f32)ACCESSIBILITY_COMBAT_RADAR_MANUAL_SWEEP_TICKS);

		if (previousdue >= 0
				&& due < previousdue
						+ ACCESSIBILITY_COMBAT_RADAR_MIN_START_TICKS) {
			due = previousdue + ACCESSIBILITY_COMBAT_RADAR_MIN_START_TICKS;
		}

		accessibilityCombatRadarFillMarkerEvent(&event, marker,
				ACCESSIBILITY_COMBAT_RADAR_EVENT_MANUAL, due);
		accessibilityCombatRadarQueueEvent(&event);
		accessibilityCombatRadarLogMarker(marker, true);
		previousdue = due;
	}
}

static s32 accessibilityCombatRadarToneKind(
		const struct accessibilitycombatradarevent *event)
{
	if (event->type == ACCESSIBILITY_COMBAT_RADAR_EVENT_LAUNCH) {
		return ACCESSIBILITY_TONE_RADAR_LAUNCH;
	}
	if (event->type == ACCESSIBILITY_COMBAT_RADAR_EVENT_EMPTY) {
		return ACCESSIBILITY_TONE_RADAR_EMPTY;
	}
	if (event->type == ACCESSIBILITY_COMBAT_RADAR_EVENT_UNAVAILABLE) {
		return ACCESSIBILITY_TONE_RADAR_UNAVAILABLE;
	}
	switch (event->category) {
	case ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ENEMY:
		return ACCESSIBILITY_TONE_RADAR_ENEMY;
	case ACCESSIBILITY_COMBAT_RADAR_CATEGORY_OBJECTIVE:
		return ACCESSIBILITY_TONE_RADAR_OBJECTIVE;
	case ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ALLY:
		return ACCESSIBILITY_TONE_RADAR_ALLY;
	default:
		return ACCESSIBILITY_TONE_RADAR_OTHER;
	}
}

static s32 accessibilityCombatRadarToneTicks(
		const struct accessibilitycombatradarevent *event)
{
	if (event->type == ACCESSIBILITY_COMBAT_RADAR_EVENT_LAUNCH) {
		return TICKS(2);
	}
	if (event->type == ACCESSIBILITY_COMBAT_RADAR_EVENT_EMPTY) {
		return TICKS(5);
	}
	if (event->type == ACCESSIBILITY_COMBAT_RADAR_EVENT_UNAVAILABLE) {
		return TICKS(6);
	}
	return event->height == ACCESSIBILITY_COMBAT_RADAR_HEIGHT_LEVEL
			? TICKS(3) : TICKS(6);
}

static void accessibilityCombatRadarEmitNext(void)
{
	s32 selected = -1;
	s32 agedmanual = -1;
	s32 i;
	struct accessibilitycombatradarevent event;

	if (g_Vars.lvframe60 < g_AccessibilityCombatRadarNextVoiceFrame) {
		return;
	}

	for (i = 0; i < g_AccessibilityCombatRadarEventCount; i++) {
		struct accessibilitycombatradarevent *candidate
				= &g_AccessibilityCombatRadarEvents[i];

		if (candidate->dueframe > g_Vars.lvframe60) {
			continue;
		}
		if (candidate->manual
				&& candidate->dueframe
					<= g_Vars.lvframe60
						- ACCESSIBILITY_COMBAT_RADAR_MANUAL_AGE_TICKS
				&& (agedmanual < 0
					|| candidate->dueframe
						< g_AccessibilityCombatRadarEvents[
								agedmanual].dueframe)) {
			agedmanual = i;
		}
		if (selected < 0
				|| candidate->priority
					> g_AccessibilityCombatRadarEvents[selected].priority
				|| (candidate->priority
						== g_AccessibilityCombatRadarEvents[selected].priority
					&& candidate->dueframe
						< g_AccessibilityCombatRadarEvents[selected].dueframe)) {
			selected = i;
		}
	}

	if (agedmanual >= 0 && (selected < 0
			|| g_AccessibilityCombatRadarEvents[selected].type
				!= ACCESSIBILITY_COMBAT_RADAR_EVENT_CLOSE)) {
		selected = agedmanual;
	}
	if (selected < 0) {
		return;
	}

	event = g_AccessibilityCombatRadarEvents[selected];
	for (i = selected + 1; i < g_AccessibilityCombatRadarEventCount; i++) {
		g_AccessibilityCombatRadarEvents[i - 1]
				= g_AccessibilityCombatRadarEvents[i];
	}
	g_AccessibilityCombatRadarEventCount--;

	accessibilityTonePlayRadarPing(event.frequencyhz, event.gain, event.pan,
			event.height, event.rear,
			accessibilityCombatRadarToneKind(&event));
	g_AccessibilityCombatRadarNextVoiceFrame
			= g_Vars.lvframe60 + accessibilityCombatRadarToneTicks(&event);
	g_AccessibilityCombatRadarEventsEmitted++;
	accessibilityLogEvent("combat_radar", "event",
			"type=%s action=emitted frame=%d scheduled_frame=%d latency_ticks=%d priority=%d identity=%p propnum=%d category=%s distance=%.3f frequency_hz=%.1f gain=%.3f pan=%.4f rear=%d height=%s queue_remaining=%d next_voice_frame=%d",
			accessibilityCombatRadarEventName(event.type),
			g_Vars.lvframe60, event.dueframe,
			g_Vars.lvframe60 - event.dueframe, event.priority,
			(void *)event.identity, event.propnum,
			accessibilityCombatRadarCategoryName(event.category),
			event.distance, event.frequencyhz, event.gain, event.pan,
			event.rear, accessibilityCombatRadarHeightName(event.height),
			g_AccessibilityCombatRadarEventCount,
			g_AccessibilityCombatRadarNextVoiceFrame);
}

static const char *accessibilityCombatRadarScopeReason(void)
{
	if (!accessibilityIsCombatRadarAudioEnabled()) {
		return "feature_disabled";
	}
	if (!g_Vars.normmplayerisrunning) {
		return "not_normal_combat_sim";
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
	if (g_Vars.in_cutscene) {
		return "cutscene";
	}
	if (g_Vars.currentplayer->isdead) {
		return "player_dead";
	}
	if (g_Vars.currentplayer->mpmenuon) {
		return "player_menu";
	}
	return NULL;
}

static void accessibilityCombatRadarSuppress(const char *reason)
{
	if (!g_AccessibilityCombatRadarSuppressed
			|| g_AccessibilityCombatRadarEventCount > 0) {
		accessibilityLogEvent("combat_radar", "scope",
				"state=suppressed reason=%s frame=%d alerts=%d native_available=%d queue=%d",
				reason, g_Vars.lvframe60,
				accessibilityGetCombatRadarContactAlerts(),
				g_AccessibilityCombatRadarFrame.available,
				g_AccessibilityCombatRadarEventCount);
		accessibilityCombatRadarClearEvents(false, reason);
		accessibilityToneStopRadar();
		g_AccessibilityCombatRadarNextVoiceFrame = 0;
	}
	g_AccessibilityCombatRadarSuppressed = true;
	g_AccessibilityCombatRadarNeedsBaseline = true;
}

static void accessibilityCombatRadarHandleNativeAvailability(void)
{
	s32 available = g_AccessibilityCombatRadarFrame.available;

	if (available == g_AccessibilityCombatRadarProcessedAvailability) {
		return;
	}

	g_AccessibilityCombatRadarProcessedAvailability = available;
	g_AccessibilityCombatRadarNeedsBaseline = true;

	if (!available) {
		accessibilityCombatRadarClearEvents(false,
				"native_radar_unavailable");
		accessibilityToneStopRadar();
		g_AccessibilityCombatRadarNextVoiceFrame = 0;
	}

	accessibilityLogEvent("combat_radar", "scope",
			"state=%s reason=native_radar_%s frame=%d alerts=%d",
			available ? "resumed" : "suppressed",
			available ? "available" : "unavailable",
			g_Vars.lvframe60,
			accessibilityGetCombatRadarContactAlerts());
}

void accessibilityCombatRadarTick(void)
{
	const char *scopereason = accessibilityCombatRadarScopeReason();
	s32 f3pressed = false;
	s32 shiftpressed = false;

#ifndef PLATFORM_N64
	f3pressed = inputKeyJustPressed(VK_F3);
	shiftpressed = inputKeyPressed(VK_LSHIFT) || inputKeyPressed(VK_RSHIFT);
#endif

	if (scopereason) {
		if (f3pressed) {
			accessibilityLogEvent("combat_radar", "command",
					"key=F3 shift=%d accepted=0 reason=%s",
					shiftpressed, scopereason);
		}
		accessibilityCombatRadarSuppress(scopereason);
		return;
	}

	if (g_AccessibilityCombatRadarSuppressed) {
		g_AccessibilityCombatRadarSuppressed = false;
		accessibilityLogEvent("combat_radar", "scope",
				"state=resumed reason=gameplay_eligible frame=%d alerts=%d",
				g_Vars.lvframe60,
				accessibilityGetCombatRadarContactAlerts());
	}

	if (g_AccessibilityCombatRadarObservedGeneration
			!= g_AccessibilityCombatRadarFrame.generation) {
		g_AccessibilityCombatRadarObservedGeneration
				= g_AccessibilityCombatRadarFrame.generation;
		accessibilityCombatRadarHandleNativeAvailability();
		if (g_AccessibilityCombatRadarFrame.available) {
			accessibilityCombatRadarUpdateContacts();
		}
	}

	if (f3pressed) {
		g_AccessibilityCombatRadarCommands++;
		if (shiftpressed) {
			s32 previous = accessibilityGetCombatRadarContactAlerts();
			s32 enabled = !previous;

			accessibilitySetCombatRadarContactAlerts(enabled);
			accessibilityTonePlayToggleConfirmation(enabled);
			accessibilityCombatRadarClearAutomaticEvents("alert_toggle");
			memset(g_AccessibilityCombatRadarContacts, 0,
					sizeof(g_AccessibilityCombatRadarContacts));
			g_AccessibilityCombatRadarNeedsBaseline = true;
			accessibilityLogEvent("combat_radar", "command",
					"key=F3 shift=1 action=toggle_alerts accepted=1 previous=%d current=%d native_available=%d",
					previous, enabled,
					g_AccessibilityCombatRadarFrame.available);
		} else {
			struct accessibilitycombatradarevent event;

			accessibilityLogEvent("combat_radar", "command",
					"key=F3 shift=0 action=manual_pulse accepted=1 native_available=%d snapshot_count=%d generation=%" PRIu64 " alerts=%d",
					g_AccessibilityCombatRadarFrame.available,
					g_AccessibilityCombatRadarFrame.retainedcount,
					(uint64_t)g_AccessibilityCombatRadarFrame.generation,
					accessibilityGetCombatRadarContactAlerts());
			if (!g_AccessibilityCombatRadarFrame.available) {
				accessibilityCombatRadarClearEvents(true,
						"manual_unavailable");
				accessibilityCombatRadarFillMarkerEvent(&event, NULL,
						ACCESSIBILITY_COMBAT_RADAR_EVENT_UNAVAILABLE,
						g_Vars.lvframe60);
				accessibilityCombatRadarQueueEvent(&event);
			} else if (g_AccessibilityCombatRadarFrame.retainedcount == 0) {
				accessibilityCombatRadarClearEvents(true, "manual_empty");
				accessibilityCombatRadarFillMarkerEvent(&event, NULL,
						ACCESSIBILITY_COMBAT_RADAR_EVENT_EMPTY,
						g_Vars.lvframe60);
				accessibilityCombatRadarQueueEvent(&event);
			} else {
				accessibilityCombatRadarScheduleManual();
			}
		}
	}

	accessibilityCombatRadarEmitNext();

	if (g_AccessibilityCombatRadarNextTelemetryFrame == 0
			|| g_Vars.lvframe60
				>= g_AccessibilityCombatRadarNextTelemetryFrame) {
		accessibilityLogEvent("combat_radar", "telemetry",
				"frame=%d captured_frames=%" PRIu64 " commands=%" PRIu64 " transitions=%" PRIu64 " events_emitted=%" PRIu64 " events_dropped=%" PRIu64 " queue=%d maximum_queue=%d alerts=%d native_available=%d",
				g_Vars.lvframe60,
				(uint64_t)g_AccessibilityCombatRadarCapturedFrames,
				(uint64_t)g_AccessibilityCombatRadarCommands,
				(uint64_t)g_AccessibilityCombatRadarTransitions,
				(uint64_t)g_AccessibilityCombatRadarEventsEmitted,
				(uint64_t)g_AccessibilityCombatRadarEventsDropped,
				g_AccessibilityCombatRadarEventCount,
				g_AccessibilityCombatRadarMaximumQueue,
				accessibilityGetCombatRadarContactAlerts(),
				g_AccessibilityCombatRadarFrame.available);
		g_AccessibilityCombatRadarNextTelemetryFrame
				= g_Vars.lvframe60
					+ ACCESSIBILITY_COMBAT_RADAR_TELEMETRY_TICKS;
	}
}

void accessibilityCombatRadarReset(const char *reason)
{
	s32 contactcount = 0;
	s32 i;

	for (i = 0; i < ACCESSIBILITY_COMBAT_RADAR_CONTACT_CAPACITY; i++) {
		contactcount += g_AccessibilityCombatRadarContacts[i].identity != 0
				&& g_AccessibilityCombatRadarContacts[i].present;
	}
	accessibilityLogEvent("combat_radar", "reset",
			"reason=%s captured_frames=%" PRIu64 " contacts=%d queue=%d emitted=%" PRIu64 " dropped=%" PRIu64 " alerts=%d",
			reason ? reason : "reset",
			(uint64_t)g_AccessibilityCombatRadarCapturedFrames,
			contactcount, g_AccessibilityCombatRadarEventCount,
			(uint64_t)g_AccessibilityCombatRadarEventsEmitted,
			(uint64_t)g_AccessibilityCombatRadarEventsDropped,
			accessibilityGetCombatRadarContactAlerts());

	accessibilityToneStopRadar();
	memset(&g_AccessibilityCombatRadarBuilding, 0,
			sizeof(g_AccessibilityCombatRadarBuilding));
	memset(&g_AccessibilityCombatRadarFrame, 0,
			sizeof(g_AccessibilityCombatRadarFrame));
	memset(g_AccessibilityCombatRadarContacts, 0,
			sizeof(g_AccessibilityCombatRadarContacts));
	memset(g_AccessibilityCombatRadarEvents, 0,
			sizeof(g_AccessibilityCombatRadarEvents));
	g_AccessibilityCombatRadarGeneration = 0;
	g_AccessibilityCombatRadarObservedGeneration = 0;
	g_AccessibilityCombatRadarCapturedFrames = 0;
	g_AccessibilityCombatRadarCommands = 0;
	g_AccessibilityCombatRadarTransitions = 0;
	g_AccessibilityCombatRadarEventsEmitted = 0;
	g_AccessibilityCombatRadarEventsDropped = 0;
	g_AccessibilityCombatRadarCaptureActive = false;
	g_AccessibilityCombatRadarCapturingOwnMarker = false;
	g_AccessibilityCombatRadarObjectiveSlot = 0;
	g_AccessibilityCombatRadarEventCount = 0;
	g_AccessibilityCombatRadarMaximumQueue = 0;
	g_AccessibilityCombatRadarNextVoiceFrame = 0;
	g_AccessibilityCombatRadarNextTelemetryFrame = 0;
	g_AccessibilityCombatRadarLastAvailability = -1;
	g_AccessibilityCombatRadarProcessedAvailability = -1;
	g_AccessibilityCombatRadarSuppressed = false;
	g_AccessibilityCombatRadarNeedsBaseline = true;
}
