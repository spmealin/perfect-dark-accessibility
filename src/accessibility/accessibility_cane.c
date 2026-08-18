#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "types.h"
#include "game/bondmove.h"
#include "game/lv.h"
#include "game/player.h"
#include "game/prop.h"
#include "game/propsnd.h"
#include "lib/collision.h"
#include "system.h"
#ifndef PLATFORM_N64
#include "input.h"
#endif
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_cane.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_observer.h"
#include "accessibility/accessibility_path_blocker.h"
#include "accessibility/accessibility_tone.h"

#define ACCESSIBILITY_CANE_PROBE_COUNT 9
#define ACCESSIBILITY_CANE_TERRAIN_BASE_SAMPLE_COUNT 4
#define ACCESSIBILITY_CANE_TERRAIN_PROBE_COUNT 12
#define ACCESSIBILITY_CANE_DROP_REFINEMENT_COUNT 5
#define ACCESSIBILITY_CANE_TERRAIN_VERTICAL_RANGE 200.0f
#define ACCESSIBILITY_CANE_TERRAIN_CONTOUR_RATIO 1.2f
#define ACCESSIBILITY_CANE_DROP_CONTOUR_RATIO 1.5f
#define ACCESSIBILITY_CANE_CROUCH_CONTOUR_RATIO 1.5f
#define ACCESSIBILITY_CANE_WALL_DURATION_MS 35
#define ACCESSIBILITY_CANE_TERRAIN_DURATION_MS 140
#define ACCESSIBILITY_CANE_DROP_DURATION_MS 260
#define ACCESSIBILITY_CANE_CROUCH_DURATION_MS 135
#define ACCESSIBILITY_CANE_LADDER_DURATION_MS 165
#define ACCESSIBILITY_CANE_LADDER_CONTOUR_RATIO 1.5f
#define ACCESSIBILITY_CANE_BREAKABLE_DURATION_MS 90
#define ACCESSIBILITY_CANE_BREAKABLE_START_RATIO 2.0f
#define ACCESSIBILITY_CANE_SLOW_CYCLE_TICKS TICKS(120)
#define ACCESSIBILITY_CANE_FAST_CYCLE_TICKS TICKS(60)
#define ACCESSIBILITY_CANE_LOG_BUFFER_SIZE 32768
#define ACCESSIBILITY_CANE_DEGREES_TO_RADIANS 0.01745329251994329577f

enum accessibilitycanesamplestate {
	ACCESSIBILITY_CANE_SAMPLE_PENDING,
	ACCESSIBILITY_CANE_SAMPLE_HIT,
	ACCESSIBILITY_CANE_SAMPLE_TERRAIN,
	ACCESSIBILITY_CANE_SAMPLE_DROP,
	ACCESSIBILITY_CANE_SAMPLE_CROUCH,
	ACCESSIBILITY_CANE_SAMPLE_LADDER,
	ACCESSIBILITY_CANE_SAMPLE_MISS,
	ACCESSIBILITY_CANE_SAMPLE_ERROR,
	ACCESSIBILITY_CANE_SAMPLE_SKIPPED,
};

enum accessibilitycaneterrainprobereason {
	ACCESSIBILITY_CANE_TERRAIN_PROBE_NONE,
	ACCESSIBILITY_CANE_TERRAIN_PROBE_NO_FLOOR,
	ACCESSIBILITY_CANE_TERRAIN_PROBE_VERTICAL_RANGE,
	ACCESSIBILITY_CANE_TERRAIN_PROBE_BELOW_THRESHOLD,
	ACCESSIBILITY_CANE_TERRAIN_PROBE_SELECTED,
	ACCESSIBILITY_CANE_TERRAIN_PROBE_DROP_CANDIDATE,
	ACCESSIBILITY_CANE_TERRAIN_PROBE_EDGE_SAFE,
	ACCESSIBILITY_CANE_TERRAIN_PROBE_EDGE_UNSAFE,
};

enum accessibilitycanefloorsupport {
	ACCESSIBILITY_CANE_FLOOR_SUPPORTED,
	ACCESSIBILITY_CANE_FLOOR_DROP,
	ACCESSIBILITY_CANE_FLOOR_TOO_HIGH,
};

struct accessibilitycaneterrainprobe {
	struct coord point;
	f32 distance;
	f32 ground;
	f32 delta;
	RoomNum room;
	RoomNum rooms[8];
	u16 flags;
	s32 reason;
};

struct accessibilitycanesample {
	s32 state;
	s32 angledegrees;
	s32 scheduledtick;
	s32 actualtick;
	s32 lateness;
	s32 result;
	s32 collisionpass;
	struct coord origin;
	struct coord forward;
	struct coord direction;
	struct coord requestedend;
	f32 radius;
	f32 ymin;
	f32 ymax;
	struct coord rawhit;
	struct coord audiosource;
	f32 distance;
	f32 frequency;
	f32 endfrequency;
	s32 durationms;
	s32 tonepattern;
	s32 terrain;
	s32 drop;
	s32 crouchpassage;
	s32 ladder;
	s32 crouchterrainmerge;
	s32 breakable;
	f32 terrainground;
	f32 terrainheight;
	f32 terraindistance;
	RoomNum terrainroom;
	u16 terrainflags;
	s32 terrainqueries;
	s32 droprefinements;
	f32 dropheightthreshold;
	s32 stancestate;
	s32 terraintraversaltested;
	s32 terraintraversable;
	s32 terrainplateau;
	s32 terrainsuppressed;
	s32 terrainclearanceresult;
	s32 terrainclearancequeries;
	f32 terrainclearancedistance;
	f32 terrainplateaudistance;
	s32 crouchresult;
	s32 crouchpass;
	f32 crouchymax;
	struct accessibilitycaneterrainprobe
			terrainprobes[ACCESSIBILITY_CANE_TERRAIN_PROBE_COUNT];
	struct prop *obstacle;
	struct prop *ignoredgrabbedprop;
	s32 obstacletype;
	u32 geoflags;
	struct coord normal;
	struct coord edge1;
	struct coord edge2;
	s32 volume;
	s32 pan;
	f32 normalizedvolume;
	f32 mastervolume;
	f32 effectivevolume;
	f32 normalizedpan;
	u64 queryus;
	struct prop *observerprop;
	s32 observerremote;
};

static const s32 g_AccessibilityCaneAngles[ACCESSIBILITY_CANE_PROBE_COUNT] = {
	-60, -45, -30, -15, 0, 15, 30, 45, 60,
};

static const s32 g_AccessibilityCaneSlowOffsets[ACCESSIBILITY_CANE_PROBE_COUNT] = {
	TICKS(0), TICKS(11), TICKS(23), TICKS(34), TICKS(45), TICKS(56),
	TICKS(68), TICKS(79), TICKS(90),
};

static const s32 g_AccessibilityCaneFastOffsets[ACCESSIBILITY_CANE_PROBE_COUNT] = {
	TICKS(0), TICKS(6), TICKS(11), TICKS(17), TICKS(23), TICKS(28),
	TICKS(34), TICKS(39), TICKS(45),
};

static struct accessibilitycanesample
		g_AccessibilityCaneSamples[ACCESSIBILITY_CANE_PROBE_COUNT];
static char g_AccessibilityCaneLogBuffer[ACCESSIBILITY_CANE_LOG_BUFFER_SIZE];
static s32 g_AccessibilityCaneScopeActive;
static s32 g_AccessibilityCaneSweepActive;
static s32 g_AccessibilityCaneCycleStartTick;
static s32 g_AccessibilityCaneCursor;
static s32 g_AccessibilityCaneLastQueryTick = -1;
static s32 g_AccessibilityCaneSweepMode;
static s32 g_AccessibilityCaneSweepId;
static s32 g_AccessibilityCaneSweepSamples;
static s32 g_AccessibilityCaneSweepSkipped;
static u64 g_AccessibilityCaneQueries;
static u64 g_AccessibilityCaneHits;
static u64 g_AccessibilityCaneMisses;
static u64 g_AccessibilityCaneTerrainHits;
static u64 g_AccessibilityCaneSkipped;
static u64 g_AccessibilityCaneSweeps;
static u64 g_AccessibilityCaneMissedCycles;
static u64 g_AccessibilityCaneQueryTotalUs;
static u64 g_AccessibilityCaneQueryMaxUs;
static uintptr_t g_AccessibilityCaneObserverProp;
static s32 g_AccessibilityCaneObserverRemote;

static const char *accessibilityCaneModeName(s32 mode)
{
	switch (mode) {
	case 1:
		return "slow";
	case 2:
		return "fast";
	default:
		return "off";
	}
}

static const char *accessibilityCaneSampleStateName(s32 state)
{
	switch (state) {
	case ACCESSIBILITY_CANE_SAMPLE_HIT:
		return "hit";
	case ACCESSIBILITY_CANE_SAMPLE_TERRAIN:
		return "terrain";
	case ACCESSIBILITY_CANE_SAMPLE_DROP:
		return "drop";
	case ACCESSIBILITY_CANE_SAMPLE_CROUCH:
		return "crouch";
	case ACCESSIBILITY_CANE_SAMPLE_LADDER:
		return "ladder";
	case ACCESSIBILITY_CANE_SAMPLE_MISS:
		return "miss";
	case ACCESSIBILITY_CANE_SAMPLE_ERROR:
		return "error";
	case ACCESSIBILITY_CANE_SAMPLE_SKIPPED:
		return "skipped";
	default:
		return "pending";
	}
}

static const char *accessibilityCaneStanceName(s32 stance)
{
	switch (stance) {
	case CROUCHPOS_STAND:
		return "stand";
	case CROUCHPOS_DUCK:
		return "duck";
	case CROUCHPOS_SQUAT:
		return "squat";
	default:
		return "remote";
	}
}

static const char *accessibilityCaneTerrainProbeReasonName(s32 reason)
{
	switch (reason) {
	case ACCESSIBILITY_CANE_TERRAIN_PROBE_NO_FLOOR:
		return "no_floor";
	case ACCESSIBILITY_CANE_TERRAIN_PROBE_VERTICAL_RANGE:
		return "vertical_range";
	case ACCESSIBILITY_CANE_TERRAIN_PROBE_BELOW_THRESHOLD:
		return "below_threshold";
	case ACCESSIBILITY_CANE_TERRAIN_PROBE_SELECTED:
		return "selected";
	case ACCESSIBILITY_CANE_TERRAIN_PROBE_DROP_CANDIDATE:
		return "drop_candidate";
	case ACCESSIBILITY_CANE_TERRAIN_PROBE_EDGE_SAFE:
		return "edge_safe";
	case ACCESSIBILITY_CANE_TERRAIN_PROBE_EDGE_UNSAFE:
		return "edge_unsafe";
	default:
		return "none";
	}
}

static s32 accessibilityCaneCycleTicks(s32 mode)
{
	return mode == 2 ? ACCESSIBILITY_CANE_FAST_CYCLE_TICKS
			: ACCESSIBILITY_CANE_SLOW_CYCLE_TICKS;
}

static const s32 *accessibilityCaneOffsets(s32 mode)
{
	return mode == 2 ? g_AccessibilityCaneFastOffsets
			: g_AccessibilityCaneSlowOffsets;
}

static const char *accessibilityCaneGameplayScopeReason(void)
{
	if (!accessibilityIsEnabled()) {
		return "accessibility_disabled";
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

	if (g_Vars.currentplayer->cameramode != CAMERAMODE_EYESPY
			&& g_Vars.currentplayer->bondmovemode != MOVEMODE_WALK
			&& g_Vars.currentplayer->bondmovemode != MOVEMODE_GRAB) {
		return "unsupported_movement_mode";
	}

	/*
	 * lvupdate60 can be zero on an ordinary PC render/interpolation frame.
	 * The explicit gates above distinguish actual pauses and invalid scopes.
	 */
	return NULL;
}

static void accessibilityCaneAppendLog(const char *format, ...)
{
	va_list args;
	size_t used = strlen(g_AccessibilityCaneLogBuffer);

	if (used >= sizeof(g_AccessibilityCaneLogBuffer) - 1) {
		return;
	}

	va_start(args, format);
	vsnprintf(g_AccessibilityCaneLogBuffer + used,
			sizeof(g_AccessibilityCaneLogBuffer) - used, format, args);
	va_end(args);
}

static void accessibilityCaneLogSweep(const char *reason)
{
	s32 i;

	if (!g_AccessibilityCaneSweepActive
			|| (g_AccessibilityCaneSweepSamples == 0
					&& g_AccessibilityCaneSweepSkipped == 0)) {
		return;
	}

	g_AccessibilityCaneLogBuffer[0] = '\0';
	accessibilityCaneAppendLog(
			"id=%d mode=%s reason=%s cycle_start=%d cycle_ticks=%d samples=%d skipped=%d total_queries=%" PRIu64 " total_hits=%" PRIu64 " total_terrain_hits=%" PRIu64 " total_misses=%" PRIu64 " total_skipped=%" PRIu64 " missed_cycles=%" PRIu64 " details=",
			g_AccessibilityCaneSweepId,
			accessibilityCaneModeName(g_AccessibilityCaneSweepMode),
			reason ? reason : "complete", g_AccessibilityCaneCycleStartTick,
			accessibilityCaneCycleTicks(g_AccessibilityCaneSweepMode),
			g_AccessibilityCaneSweepSamples, g_AccessibilityCaneSweepSkipped,
			(uint64_t)g_AccessibilityCaneQueries,
			(uint64_t)g_AccessibilityCaneHits,
			(uint64_t)g_AccessibilityCaneTerrainHits,
			(uint64_t)g_AccessibilityCaneMisses,
			(uint64_t)g_AccessibilityCaneSkipped,
			(uint64_t)g_AccessibilityCaneMissedCycles);

	for (i = 0; i < ACCESSIBILITY_CANE_PROBE_COUNT; i++) {
		struct accessibilitycanesample *sample = &g_AccessibilityCaneSamples[i];

		accessibilityCaneAppendLog(
				"%ss%d={angle:%d state:%s scheduled:%d actual:%d late:%d result:%d pass:%d observer:%p remote:%d ignored_grabbed_prop:%p origin:%.2f,%.2f,%.2f forward:%.5f,%.5f direction:%.5f,%.5f end:%.2f,%.2f,%.2f bbox:%.2f,%.2f,%.2f raw:%.2f,%.2f,%.2f audio:%.2f,%.2f,%.2f distance:%.2f frequency_hz:%.2f end_frequency_hz:%.2f duration_ms:%d tone_pattern:%d terrain:%d drop:%d crouch:%d ladder:%d crouch_terrain_merge:%d breakable:%d terrain_ground:%.2f terrain_height:%.2f terrain_distance:%.2f terrain_room:%d terrain_flags:0x%04x terrain_queries:%d drop_refinements:%d drop_threshold:%.2f stance:%s traversal_tested:%d traversable:%d plateau:%d terrain_suppressed:%d clearance_result:%d clearance_queries:%d clearance_distance:%.2f plateau_distance:%.2f crouch_result:%d crouch_pass:%d crouch_ymax:%.2f obstacle:%p type:%d geoflags:0x%08x normal:%.5f,%.5f,%.5f edge:%.2f,%.2f,%.2f,%.2f volume:%d pan:%d normalized:%.5f,%.5f master_volume:%.5f effective_volume:%.5f query_us:%" PRIu64 " probes=[",
				i ? " " : "", i, sample->angledegrees,
				accessibilityCaneSampleStateName(sample->state),
				sample->scheduledtick, sample->actualtick, sample->lateness,
				sample->result, sample->collisionpass,
				(void *)sample->observerprop, sample->observerremote,
				(void *)sample->ignoredgrabbedprop,
				sample->origin.x, sample->origin.y,
				sample->origin.z, sample->forward.x, sample->forward.z,
				sample->direction.x, sample->direction.z,
				sample->requestedend.x, sample->requestedend.y,
				sample->requestedend.z, sample->radius, sample->ymin,
				sample->ymax, sample->rawhit.x, sample->rawhit.y,
				sample->rawhit.z, sample->audiosource.x,
				sample->audiosource.y, sample->audiosource.z,
				sample->distance, sample->frequency, sample->endfrequency,
				sample->durationms, sample->tonepattern,
				sample->terrain, sample->drop, sample->crouchpassage,
				sample->ladder,
				sample->crouchterrainmerge,
				sample->breakable,
				sample->terrainground,
				sample->terrainheight, sample->terraindistance,
				sample->terrainroom, sample->terrainflags,
				sample->terrainqueries, sample->droprefinements,
				sample->dropheightthreshold,
				accessibilityCaneStanceName(sample->stancestate),
				sample->terraintraversaltested,
				sample->terraintraversable, sample->terrainplateau,
				sample->terrainsuppressed,
				sample->terrainclearanceresult,
				sample->terrainclearancequeries,
				sample->terrainclearancedistance,
				sample->terrainplateaudistance,
				sample->crouchresult,
				sample->crouchpass, sample->crouchymax,
				(void *)sample->obstacle,
				sample->obstacletype, sample->geoflags, sample->normal.x,
				sample->normal.y, sample->normal.z,
				sample->edge1.x, sample->edge1.z,
				sample->edge2.x, sample->edge2.z, sample->volume,
				sample->pan, sample->normalizedvolume,
				sample->normalizedpan, sample->mastervolume,
				sample->effectivevolume, (uint64_t)sample->queryus);

		{
			s32 j;

			for (j = 0; j < sample->terrainqueries
					&& j < ACCESSIBILITY_CANE_TERRAIN_PROBE_COUNT; j++) {
				struct accessibilitycaneterrainprobe *probe
						= &sample->terrainprobes[j];

				accessibilityCaneAppendLog(
						"%sp%d={distance:%.2f point:%.2f,%.2f,%.2f rooms:%d,%d,%d,%d,%d,%d,%d,%d floor_room:%d ground:%.2f delta:%.2f flags:0x%04x reason:%s}",
						j ? " " : "", j, probe->distance,
						probe->point.x, probe->point.y, probe->point.z,
						probe->rooms[0], probe->rooms[1],
						probe->rooms[2], probe->rooms[3],
						probe->rooms[4], probe->rooms[5],
						probe->rooms[6], probe->rooms[7],
						probe->room, probe->ground, probe->delta,
						probe->flags,
						accessibilityCaneTerrainProbeReasonName(
								probe->reason));
			}
		}

		accessibilityCaneAppendLog("]}");
	}

	accessibilityLogEventMessage("cane", "sweep",
			g_AccessibilityCaneLogBuffer);
}

static void accessibilityCaneBeginSweep(s32 mode, s32 starttick)
{
	s32 i;
	const s32 *offsets = accessibilityCaneOffsets(mode);

	memset(g_AccessibilityCaneSamples, 0,
			sizeof(g_AccessibilityCaneSamples));

	for (i = 0; i < ACCESSIBILITY_CANE_PROBE_COUNT; i++) {
		g_AccessibilityCaneSamples[i].angledegrees
				= g_AccessibilityCaneAngles[i];
		g_AccessibilityCaneSamples[i].scheduledtick = starttick + offsets[i];
		g_AccessibilityCaneSamples[i].terrainroom = -1;
		g_AccessibilityCaneSamples[i].crouchresult = -1;
		g_AccessibilityCaneSamples[i].terrainclearanceresult = -1;
		g_AccessibilityCaneSamples[i].stancestate = -1;
	}

	g_AccessibilityCaneSweepId++;
	g_AccessibilityCaneSweeps++;
	g_AccessibilityCaneSweepMode = mode;
	g_AccessibilityCaneCycleStartTick = starttick;
	g_AccessibilityCaneCursor = 0;
	g_AccessibilityCaneSweepSamples = 0;
	g_AccessibilityCaneSweepSkipped = 0;
	g_AccessibilityCaneSweepActive = true;
}

static void accessibilityCaneMarkSkipped(s32 slot, s32 actualtick)
{
	struct accessibilitycanesample *sample;

	if (slot < 0 || slot >= ACCESSIBILITY_CANE_PROBE_COUNT) {
		return;
	}

	sample = &g_AccessibilityCaneSamples[slot];
	sample->state = ACCESSIBILITY_CANE_SAMPLE_SKIPPED;
	sample->actualtick = actualtick;
	sample->lateness = actualtick - sample->scheduledtick;
	g_AccessibilityCaneSkipped++;
	g_AccessibilityCaneSweepSkipped++;
}

static void accessibilityCaneClosestPointOnEdge(const struct coord *point,
		const struct coord *edge1, const struct coord *edge2,
		struct coord *closest)
{
	f32 dx = edge2->x - edge1->x;
	f32 dz = edge2->z - edge1->z;
	f32 lengthsq = dx * dx + dz * dz;
	f32 fraction = 0.0f;

	if (lengthsq > 0.0001f) {
		fraction = ((point->x - edge1->x) * dx
				+ (point->z - edge1->z) * dz) / lengthsq;
		if (fraction < 0.0f) {
			fraction = 0.0f;
		} else if (fraction > 1.0f) {
			fraction = 1.0f;
		}
	}

	closest->x = edge1->x + dx * fraction;
	closest->y = point->y;
	closest->z = edge1->z + dz * fraction;
}

static void accessibilityCaneNormalFromEdge(const struct coord *origin,
		const struct coord *hit, const struct coord *edge1,
		const struct coord *edge2, struct coord *normal)
{
	f32 dx = edge2->x - edge1->x;
	f32 dz = edge2->z - edge1->z;
	f32 length = sqrtf(dx * dx + dz * dz);

	if (length < 0.0001f) {
		normal->x = 0.0f;
		normal->y = 0.0f;
		normal->z = 0.0f;
		return;
	}

	normal->x = -dz / length;
	normal->y = 0.0f;
	normal->z = dx / length;

	if ((origin->x - hit->x) * normal->x
			+ (origin->z - hit->z) * normal->z < 0.0f) {
		normal->x = -normal->x;
		normal->z = -normal->z;
	}
}

static f32 accessibilityCaneFrequencyForDistance(f32 distance, f32 reach,
		f32 nearfrequency, f32 farfrequency)
{
	f32 fraction;

	if (reach <= 0.0f || distance <= 0.0f) {
		return nearfrequency;
	}

	fraction = distance / reach;

	if (fraction > 1.0f) {
		fraction = 1.0f;
	}

	return nearfrequency * powf(farfrequency / nearfrequency, fraction);
}

static s32 accessibilityCaneProbeFloor(
		struct accessibilitycanesample *sample,
		const struct accessibilityobserver *observer, f32 distance,
		f32 heightthreshold, f32 dropheightthreshold,
		struct accessibilitycaneterrainprobe **resultprobe)
{
	RoomNum rooms[8];
	RoomNum morerooms[22];
	struct coord origin = observer->origin;
	struct coord roompoint;
	struct accessibilitycaneterrainprobe *probe;
	f32 ground = 0.0f;
	f32 delta;
	u16 flags = 0;
	RoomNum room;
	s32 i;

	if (sample->terrainqueries >= ACCESSIBILITY_CANE_TERRAIN_PROBE_COUNT) {
		return ACCESSIBILITY_CANE_FLOOR_TOO_HIGH;
	}

	probe = &sample->terrainprobes[sample->terrainqueries++];
	memset(probe, 0, sizeof(*probe));
	roompoint.x = observer->origin.x + sample->direction.x * distance;
	roompoint.y = observer->origin.y;
	roompoint.z = observer->origin.z + sample->direction.z * distance;

	for (i = 0; i < 8; i++) {
		rooms[i] = -1;
	}

	func0f065dfc(&origin, observer->prop->rooms,
			&roompoint, rooms, morerooms, 20);

	if (!observer->isremote) {
		bmoveFindEnteredRoomsByPos(g_Vars.currentplayer, &roompoint, rooms);
	}

	probe->point = roompoint;
	probe->point.y = observer->ground
			+ ACCESSIBILITY_CANE_TERRAIN_VERTICAL_RANGE;
	probe->distance = distance;
	probe->room = -1;

	for (i = 0; i < 8; i++) {
		probe->rooms[i] = rooms[i];
	}

#if VERSION >= VERSION_NTSC_1_0
	room = cdFindFloorRoomYColourFlagsAtPos(
			&probe->point, rooms, &ground, NULL, &flags);
#else
	room = cdFindFloorRoomYColourFlagsAtPos(
			&probe->point, rooms, &ground, NULL);
#endif
	probe->room = room;
	probe->flags = flags;

	if (resultprobe) {
		*resultprobe = probe;
	}

	if (room < 0) {
		probe->reason = ACCESSIBILITY_CANE_TERRAIN_PROBE_NO_FLOOR;
		return ACCESSIBILITY_CANE_FLOOR_DROP;
	}

	delta = ground - observer->ground;
	probe->ground = ground;
	probe->delta = delta;

	if (delta < -dropheightthreshold) {
		probe->reason = ACCESSIBILITY_CANE_TERRAIN_PROBE_DROP_CANDIDATE;
		return ACCESSIBILITY_CANE_FLOOR_DROP;
	}

	if (delta > ACCESSIBILITY_CANE_TERRAIN_VERTICAL_RANGE) {
		probe->reason = ACCESSIBILITY_CANE_TERRAIN_PROBE_VERTICAL_RANGE;
		return ACCESSIBILITY_CANE_FLOOR_TOO_HIGH;
	}

	probe->reason = fabsf(delta) >= heightthreshold
			? ACCESSIBILITY_CANE_TERRAIN_PROBE_SELECTED
			: ACCESSIBILITY_CANE_TERRAIN_PROBE_BELOW_THRESHOLD;
	return ACCESSIBILITY_CANE_FLOOR_SUPPORTED;
}

static s32 accessibilityCaneFindTerrain(
		struct accessibilitycanesample *sample,
		const struct accessibilityobserver *observer, f32 barrier)
{
	static const f32 fractions[ACCESSIBILITY_CANE_TERRAIN_BASE_SAMPLE_COUNT]
			= { 0.10f, 0.25f, 0.50f, 1.0f };
	struct accessibilitycaneterrainprobe *probe;
	f32 reach;
	f32 heightthreshold;
	f32 dropheightthreshold;
	f32 limit;
	f32 distance;
	f32 lastsafedistance = 0.0f;
	s32 i;

	accessibilityGetVirtualCaneTerrainTuning(&reach, &heightthreshold,
			&dropheightthreshold);
	(void)dropheightthreshold;
	sample->dropheightthreshold = dropheightthreshold;
	limit = reach;

	if (barrier > 0.0f && barrier < limit) {
		limit = barrier - 1.0f;
	}

	if (limit < 1.0f) {
		return false;
	}

	for (i = 0; i < ACCESSIBILITY_CANE_TERRAIN_BASE_SAMPLE_COUNT; i++) {
		s32 support;

		distance = limit * fractions[i];
		support = accessibilityCaneProbeFloor(sample, observer, distance,
				heightthreshold, dropheightthreshold, &probe);

		if (support == ACCESSIBILITY_CANE_FLOOR_DROP) {
			f32 low = lastsafedistance;
			f32 high = distance;
			s32 unsafecount = 1;
			s32 refinement;
			f32 dropground = probe->ground;
			f32 dropdelta = probe->room < 0
					? -dropheightthreshold : probe->delta;
			RoomNum droproom = probe->room;
			u16 dropflags = probe->flags;

			for (refinement = 0;
					refinement < ACCESSIBILITY_CANE_DROP_REFINEMENT_COUNT;
					refinement++) {
				f32 middle = (low + high) * 0.5f;
				s32 middlesupport = accessibilityCaneProbeFloor(sample,
						observer, middle, heightthreshold,
						dropheightthreshold, &probe);

				sample->droprefinements++;

				if (middlesupport == ACCESSIBILITY_CANE_FLOOR_DROP) {
					probe->reason = ACCESSIBILITY_CANE_TERRAIN_PROBE_EDGE_UNSAFE;
					high = middle;
					unsafecount++;
					dropground = probe->ground;
					dropdelta = probe->room < 0
							? -dropheightthreshold : probe->delta;
					droproom = probe->room;
					dropflags = probe->flags;
				} else {
					probe->reason = ACCESSIBILITY_CANE_TERRAIN_PROBE_EDGE_SAFE;
					low = middle;
				}
			}

			/* A second unsafe result rejects isolated room-resolution misses. */
			if (unsafecount >= 2) {
				distance = (low + high) * 0.5f;
				sample->terrain = -2;
				sample->drop = true;
				sample->terrainground = dropground;
				sample->terrainheight = dropdelta;
				sample->terraindistance = distance;
				sample->terrainroom = droproom;
				sample->terrainflags = dropflags;
				sample->audiosource.x = observer->origin.x
						+ sample->direction.x * distance;
				sample->audiosource.y = observer->camera.y;
				sample->audiosource.z = observer->origin.z
						+ sample->direction.z * distance;
				return true;
			}

			lastsafedistance = distance;
			continue;
		}

		if (support == ACCESSIBILITY_CANE_FLOOR_SUPPORTED) {
			lastsafedistance = distance;

			if (!sample->terrain
					&& fabsf(probe->delta) >= heightthreshold) {
				sample->terrain = probe->delta > 0.0f ? 1 : -1;
				sample->terrainground = probe->ground;
				sample->terrainheight = probe->delta;
				sample->terraindistance = distance;
				sample->terrainroom = probe->room;
				sample->terrainflags = probe->flags;
				sample->audiosource = probe->point;
				sample->audiosource.y = observer->camera.y;
			}
		}
	}

	return sample->terrain != 0;
}

static void accessibilityCaneClassifyTraversableRise(
		struct accessibilitycanesample *sample,
		const struct accessibilityobserver *observer,
		f32 barrierdistance, s32 types)
{
	RoomNum rooms[8];
	struct coord testpos;
	f32 reach;
	f32 heightthreshold;
	f32 dropheightthreshold;
	f32 plateautolerance;
	s32 oldenableslopes;
	s32 allclear = true;
	s32 i;

	if (sample->terrain <= 0 || sample->drop || observer->isremote
			|| !g_Vars.currentplayer
			|| g_Vars.currentplayer->bondmovemode != MOVEMODE_WALK) {
		return;
	}

	accessibilityGetVirtualCaneTerrainTuning(&reach, &heightthreshold,
			&dropheightthreshold);
	plateautolerance = heightthreshold > 5.0f ? heightthreshold : 5.0f;
	sample->terraintraversaltested = true;
	sample->stancestate = g_Vars.currentplayer->crouchpos;

	/*
	 * A nearby rise that settles onto a stable floor is a short ledge rather
	 * than a sustained stair or ramp. Requiring two sampled floors avoids
	 * treating an isolated room/floor result as a plateau.
	 */
	for (i = 1; i < sample->terrainqueries; i++) {
		struct accessibilitycaneterrainprobe *previous
				= &sample->terrainprobes[i - 1];
		struct accessibilitycaneterrainprobe *current
				= &sample->terrainprobes[i];

		if (previous->room >= 0 && current->room >= 0
				&& previous->delta >= heightthreshold
				&& current->delta >= heightthreshold
				&& fabsf(current->ground - previous->ground)
						<= plateautolerance
				&& previous->distance <= reach * 0.25f + 0.01f) {
			sample->terrainplateau = true;
			sample->terrainplateaudistance = previous->distance;
			break;
		}
	}

	oldenableslopes = g_Vars.enableslopes;
	g_Vars.enableslopes
			= (g_Vars.currentplayer->floorflags & GEOFLAG_SLOPE) == 0;
	propSetPerimEnabled(observer->prop, false);

	/*
	 * Test the live player envelope at each sampled raised floor. This uses
	 * the current animated standing/duck/squat height, not a synthetic fixed
	 * stance. Stop before a farther barrier so its own volume does not make
	 * the approach floor look untraversable.
	 */
	for (i = 0; i < sample->terrainqueries; i++) {
		struct accessibilitycaneterrainprobe *probe
				= &sample->terrainprobes[i];

		if (probe->room < 0 || probe->delta < heightthreshold) {
			continue;
		}

		if (barrierdistance > 0.0f
				&& probe->distance + sample->radius >= barrierdistance) {
			break;
		}

		testpos.x = observer->origin.x
				+ sample->direction.x * probe->distance;
		testpos.y = observer->origin.y + probe->delta;
		testpos.z = observer->origin.z
				+ sample->direction.z * probe->distance;
		{
			s32 j;

			for (j = 0; j < ARRAYCOUNT(rooms); j++) {
				rooms[j] = probe->rooms[j];
			}
		}
		bmoveFindEnteredRoomsByPos(g_Vars.currentplayer, &testpos, rooms);
		sample->terrainclearanceresult = cdTestVolume(&testpos,
				sample->radius, rooms, types, CHECKVERTICAL_YES,
				sample->ymax - observer->origin.y,
				sample->ymin - observer->origin.y - 0.1f);
		sample->terrainclearancequeries++;
		sample->terrainclearancedistance = probe->distance;

		if (sample->terrainclearanceresult != CDRESULT_NOCOLLISION) {
			allclear = false;
			break;
		}
	}

	propSetPerimEnabled(observer->prop, true);
	g_Vars.enableslopes = oldenableslopes;
	sample->terraintraversable = allclear
			&& sample->terrainclearancequeries > 0
			&& sample->terrainclearancedistance + 0.01f
					>= sample->terraindistance;
}

static s32 accessibilityCaneFindCrouchPassage(
		struct accessibilitycanesample *sample,
		const struct accessibilityobserver *observer, struct coord *start,
		f32 barrierdistance, f32 maxdistance, s32 types)
{
	RoomNum dstrooms[8];
	RoomNum morerooms[22];
	struct coord end;
	f32 distance;
	f32 squatymax;
	s32 result;

	if (observer->isremote || !g_Vars.currentplayer
			|| g_Vars.currentplayer->bondmovemode != MOVEMODE_WALK) {
		return false;
	}

	squatymax = g_Vars.currentplayer->vv_manground
			+ g_Vars.currentplayer->vv_headheight - 90.0f;

	if (squatymax < g_Vars.currentplayer->vv_manground + 80.0f) {
		squatymax = g_Vars.currentplayer->vv_manground + 80.0f;
	}

	sample->crouchymax = squatymax;

	/* Already-low collision envelopes should continue to describe clear space. */
	if (observer->ymax <= squatymax + 1.0f) {
		return false;
	}

	distance = barrierdistance + sample->radius * 2.0f;
	if (distance > maxdistance) {
		distance = maxdistance;
	}
	if (distance <= barrierdistance) {
		return false;
	}

	end.x = start->x + sample->direction.x * distance;
	end.y = start->y;
	end.z = start->z + sample->direction.z * distance;
	func0f065dfc(start, observer->prop->rooms,
			&end, dstrooms, morerooms, 20);
	bmoveFindEnteredRoomsByPos(g_Vars.currentplayer, &end, dstrooms);

	result = cdExamCylMove06(start, observer->prop->rooms,
			&end, dstrooms, sample->radius, types, 1,
			squatymax - start->y, sample->ymin - start->y);

	if (result == CDRESULT_COLLISION) {
		sample->crouchpass = 1;
	} else if (result == CDRESULT_NOCOLLISION) {
		result = cdExamCylMove02(start, &end, sample->radius,
				dstrooms, types, true, squatymax - start->y,
				sample->ymin - start->y);
		if (result == CDRESULT_COLLISION) {
			sample->crouchpass = 2;
		}
	}

	sample->crouchresult = result;
	sample->crouchpassage = result == CDRESULT_NOCOLLISION;
	return sample->crouchpassage;
}

static s32 accessibilityCaneQuery(struct accessibilitycanesample *sample)
{
	struct accessibilityobserver observer;
	RoomNum dstrooms[8];
	RoomNum morerooms[22];
	struct coord start;
	struct coord end;
	f32 horizontal;
	f32 maxdistance;
	f32 fulldistance;
	f32 fadedistance;
	f32 silentdistance;
	f32 nearfrequency;
	f32 farfrequency;
	f32 angle;
	f32 cosine;
	f32 sine;
	f32 barrierdistance = -1.0f;
	s32 types;
	s32 result;
	s32 collisionpass = 0;
	s32 terrainfound = false;
	s32 terraincedes = false;
	s32 crouchfound = false;
	struct prop *grabbedprop = NULL;
#if VERSION < VERSION_NTSC_1_0
	s32 i;
#endif
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	u64 querystart;
#endif

	if (!accessibilityObserverGet(&observer)) {
		sample->state = ACCESSIBILITY_CANE_SAMPLE_ERROR;
		sample->result = CDRESULT_ERROR;
		return CDRESULT_ERROR;
	}

	start = observer.origin;
	sample->observerprop = observer.prop;
	sample->observerremote = observer.isremote;
	sample->stancestate = observer.isremote
			? -1 : g_Vars.currentplayer->crouchpos;
	sample->origin = start;
	sample->forward.x = observer.look.x;
	sample->forward.y = 0.0f;
	sample->forward.z = observer.look.z;
	horizontal = sqrtf(sample->forward.x * sample->forward.x
			+ sample->forward.z * sample->forward.z);

	if (horizontal < 0.0001f) {
		sample->state = ACCESSIBILITY_CANE_SAMPLE_ERROR;
		sample->result = CDRESULT_ERROR;
		return CDRESULT_ERROR;
	}

	sample->forward.x /= horizontal;
	sample->forward.z /= horizontal;
	/* The world rotation convention is opposite the cane's screen-left/right labels. */
	angle = -sample->angledegrees * ACCESSIBILITY_CANE_DEGREES_TO_RADIANS;
	cosine = cosf(angle);
	sine = sinf(angle);
	sample->direction.x = sample->forward.x * cosine
			+ sample->forward.z * sine;
	sample->direction.y = 0.0f;
	sample->direction.z = -sample->forward.x * sine
			+ sample->forward.z * cosine;
	accessibilityGetVirtualCaneTuning(&maxdistance, &fulldistance,
			&fadedistance, &silentdistance);
	accessibilityGetVirtualCanePitch(&nearfrequency, &farfrequency);
	end.x = start.x + sample->direction.x * maxdistance;
	end.y = start.y;
	end.z = start.z + sample->direction.z * maxdistance;
	sample->requestedend = end;

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	querystart = sysGetMicroseconds();
#endif
	sample->radius = observer.radius;
	sample->ymax = observer.ymax;
	sample->ymin = observer.ymin;
	func0f065dfc(&start, observer.prop->rooms,
			&end, dstrooms, morerooms, 20);

#if VERSION < VERSION_NTSC_1_0
	if (!observer.isremote) {
		for (i = 0; dstrooms[i] != -1; i++) {
			if (dstrooms[i] == g_Vars.currentplayer->floorroom) {
				dstrooms[0] = g_Vars.currentplayer->floorroom;
				dstrooms[1] = -1;
				break;
			}
		}
	}
#endif

	if (!observer.isremote) {
		bmoveFindEnteredRoomsByPos(g_Vars.currentplayer, &end, dstrooms);
	}
	types = g_Vars.bondcollisions
			? CDTYPE_BG | CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER
			: CDTYPE_BG;

	/*
	 * The grabbed object travels immediately in front of Joanna and remains
	 * part of ordinary world collision. Exclude exactly that live perimeter
	 * while sampling, as the native grab-movement queries do, so the cane
	 * describes the route beyond the carried object rather than the object
	 * itself. Other props and all background geometry remain eligible.
	 */
	if (!observer.isremote && g_Vars.currentplayer
			&& g_Vars.currentplayer->bondmovemode == MOVEMODE_GRAB
			&& g_Vars.currentplayer->grabbedprop) {
		grabbedprop = g_Vars.currentplayer->grabbedprop;
		sample->ignoredgrabbedprop = grabbedprop;
		propSetPerimEnabled(grabbedprop, false);
	}

	result = cdExamCylMove06(&start, observer.prop->rooms,
			&end, dstrooms, sample->radius, types, 1,
			sample->ymax - start.y, sample->ymin - start.y);

	if (result == CDRESULT_COLLISION) {
		collisionpass = 1;
	} else if (result == CDRESULT_NOCOLLISION) {
		result = cdExamCylMove02(&start, &end, sample->radius,
				dstrooms, types, true, sample->ymax - start.y,
				sample->ymin - start.y);
		if (result == CDRESULT_COLLISION) {
			collisionpass = 2;
		}
	}

	sample->result = result;
	g_AccessibilityCaneQueries++;

	if (result == CDRESULT_COLLISION) {
		sample->obstacle = cdGetObstacleProp();
		sample->obstacletype = sample->obstacle
				? sample->obstacle->type : -1;
		sample->collisionpass = collisionpass;

		if (collisionpass == 1) {
			cdGetPos(&sample->rawhit, __LINE__, "accessibility_cane.c");
			sample->geoflags = cdGetGeoFlags();
			cdGetObstacleNormal(&sample->normal);
		} else {
			cdGetEdge(&sample->edge1, &sample->edge2,
					__LINE__, "accessibility_cane.c");
			accessibilityCaneClosestPointOnEdge(&end, &sample->edge1,
					&sample->edge2, &sample->rawhit);
			accessibilityCaneNormalFromEdge(&start, &sample->rawhit,
					&sample->edge1, &sample->edge2, &sample->normal);
			sample->geoflags = GEOFLAG_WALL;
		}

		barrierdistance = sqrtf((sample->rawhit.x - start.x)
					* (sample->rawhit.x - start.x)
				+ (sample->rawhit.z - start.z)
					* (sample->rawhit.z - start.z));
	}

	if (result == CDRESULT_COLLISION || result == CDRESULT_NOCOLLISION) {
		terrainfound = accessibilityCaneFindTerrain(
				sample, &observer, barrierdistance);
		if (terrainfound) {
			accessibilityCaneClassifyTraversableRise(sample, &observer,
					barrierdistance, types);
			terraincedes = sample->terraintraversable
					&& (barrierdistance > 0.0f || sample->terrainplateau);
			sample->terrainsuppressed = terraincedes;
		}
	}

	if (result == CDRESULT_COLLISION && !sample->drop
			&& (!terrainfound || terraincedes
				|| sample->terraindistance + sample->radius
						>= barrierdistance)) {
		crouchfound = accessibilityCaneFindCrouchPassage(sample,
				&observer, &start, barrierdistance, maxdistance, types);
		sample->crouchterrainmerge = crouchfound && terrainfound
				&& sample->terraindistance < barrierdistance;
	}

	if (grabbedprop) {
		propSetPerimEnabled(grabbedprop, true);
	}

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	sample->queryus = sysGetMicroseconds() - querystart;
	g_AccessibilityCaneQueryTotalUs += sample->queryus;
	if (sample->queryus > g_AccessibilityCaneQueryMaxUs) {
		g_AccessibilityCaneQueryMaxUs = sample->queryus;
	}
#endif

	if (terrainfound && !terraincedes
			&& (barrierdistance < 0.0f
				|| sample->terraindistance < barrierdistance)
			&& !sample->crouchterrainmerge) {
		horizontal = sample->terraindistance;
		sample->state = sample->drop
				? ACCESSIBILITY_CANE_SAMPLE_DROP
				: ACCESSIBILITY_CANE_SAMPLE_TERRAIN;
		g_AccessibilityCaneTerrainHits++;
	} else if (result == CDRESULT_COLLISION) {
		sample->audiosource = sample->rawhit;
		sample->audiosource.y = observer.camera.y;
		horizontal = barrierdistance;
		sample->ladder = (sample->geoflags
				& (GEOFLAG_LADDER | GEOFLAG_LADDER_PLAYERONLY)) != 0;
		sample->state = sample->ladder
				? ACCESSIBILITY_CANE_SAMPLE_LADDER
				: crouchfound
			? ACCESSIBILITY_CANE_SAMPLE_CROUCH
			: ACCESSIBILITY_CANE_SAMPLE_HIT;
		if (!crouchfound && !sample->ladder) {
			sample->breakable
					= accessibilityPathBlockerIsBreakable(sample->obstacle);
		}
		g_AccessibilityCaneHits++;
	} else {
		sample->state = result == CDRESULT_NOCOLLISION
				? ACCESSIBILITY_CANE_SAMPLE_MISS
				: ACCESSIBILITY_CANE_SAMPLE_ERROR;
		g_AccessibilityCaneMisses++;
		return result;
	}

	sample->distance = horizontal;
	sample->frequency = accessibilityCaneFrequencyForDistance(horizontal,
			maxdistance, nearfrequency, farfrequency);
	sample->endfrequency = sample->frequency;
	sample->tonepattern = ACCESSIBILITY_TONE_CANE_PATTERN_CONTOUR;

	if (sample->state == ACCESSIBILITY_CANE_SAMPLE_DROP) {
		sample->durationms = ACCESSIBILITY_CANE_DROP_DURATION_MS;
		sample->frequency *= ACCESSIBILITY_CANE_DROP_CONTOUR_RATIO;
		sample->endfrequency /= ACCESSIBILITY_CANE_DROP_CONTOUR_RATIO;
	} else if (sample->state == ACCESSIBILITY_CANE_SAMPLE_CROUCH) {
		sample->durationms = ACCESSIBILITY_CANE_CROUCH_DURATION_MS;
		sample->endfrequency /= ACCESSIBILITY_CANE_CROUCH_CONTOUR_RATIO;
		sample->tonepattern = ACCESSIBILITY_TONE_CANE_PATTERN_CROUCH_DOUBLE;
	} else if (sample->state == ACCESSIBILITY_CANE_SAMPLE_LADDER) {
		sample->durationms = ACCESSIBILITY_CANE_LADDER_DURATION_MS;
		sample->endfrequency *= ACCESSIBILITY_CANE_LADDER_CONTOUR_RATIO;
		sample->tonepattern = ACCESSIBILITY_TONE_CANE_PATTERN_LADDER_TRIPLE;
	} else if (sample->state == ACCESSIBILITY_CANE_SAMPLE_TERRAIN) {
		sample->durationms = ACCESSIBILITY_CANE_TERRAIN_DURATION_MS;
		if (sample->terrain > 0) {
			sample->frequency /= ACCESSIBILITY_CANE_TERRAIN_CONTOUR_RATIO;
			sample->endfrequency *= ACCESSIBILITY_CANE_TERRAIN_CONTOUR_RATIO;
		} else {
			sample->frequency *= ACCESSIBILITY_CANE_TERRAIN_CONTOUR_RATIO;
			sample->endfrequency /= ACCESSIBILITY_CANE_TERRAIN_CONTOUR_RATIO;
		}
	} else if (sample->breakable) {
		sample->durationms = ACCESSIBILITY_CANE_BREAKABLE_DURATION_MS;
		sample->endfrequency = sample->frequency;
		sample->frequency *= ACCESSIBILITY_CANE_BREAKABLE_START_RATIO;
	} else {
		sample->durationms = ACCESSIBILITY_CANE_WALL_DURATION_MS;
	}

	sample->volume = psCalculateVolumeFromDistance(horizontal,
			fulldistance, fadedistance, silentdistance, AL_VOL_FULL);
	sample->pan = psCalculatePan(&sample->audiosource,
			fulldistance, fadedistance, silentdistance,
			horizontal, false, NULL);
	sample->normalizedvolume = (f32)sample->volume / (f32)AL_VOL_FULL;
	sample->mastervolume = accessibilityGetVirtualCaneVolume();
	sample->effectivevolume = sample->normalizedvolume * sample->mastervolume;
	sample->normalizedpan = ((f32)sample->pan - (f32)AL_PAN_CENTER)
			/ (f32)AL_PAN_CENTER;

	accessibilityTonePlayCaneSlot(sample - g_AccessibilityCaneSamples,
			sample->frequency, sample->endfrequency,
			sample->effectivevolume,
			sample->normalizedpan, sample->durationms,
			sample->tonepattern);

	return result;
}

static void accessibilityCaneStop(const char *reason, s32 logscope)
{
	if (g_AccessibilityCaneSweepActive) {
		accessibilityCaneLogSweep(reason ? reason : "stopped");
	}

	if (g_AccessibilityCaneScopeActive || g_AccessibilityCaneSweepActive) {
		accessibilityToneStopCane();
	}

	if (logscope && g_AccessibilityCaneScopeActive) {
		accessibilityLogEvent("cane", "scope",
				"state=lost reason=%s tick=%d stage=%d player=%d",
				reason ? reason : "unknown", g_Vars.lvframe60,
				g_Vars.stagenum, g_Vars.currentplayernum);
	}

	g_AccessibilityCaneScopeActive = false;
	g_AccessibilityCaneSweepActive = false;
	g_AccessibilityCaneCursor = 0;
	g_AccessibilityCaneLastQueryTick = -1;
	g_AccessibilityCaneObserverProp = 0;
	g_AccessibilityCaneObserverRemote = false;
}

static void accessibilityCaneCycleMode(void)
{
	s32 oldmode = accessibilityGetVirtualCaneMode();
	s32 newmode = oldmode >= 2 ? 0 : oldmode + 1;

	accessibilityCaneStop("mode_change", false);
	accessibilitySetVirtualCaneMode(newmode);
	accessibilityTonePlayCaneModeConfirmation(newmode);
	accessibilityLogEvent("cane", "command",
			"action=cycle key=F4 old_mode=%s old_value=%d new_mode=%s new_value=%d tick=%d stage=%d player=%d alt=0 earcon=%s base_frequency_hz=880 changed_frequency_hz=%d pulses=%d beep_ms=35 gap_ms=25 lane=toggle_confirmation",
			accessibilityCaneModeName(oldmode), oldmode,
			accessibilityCaneModeName(newmode), newmode,
			g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum,
			newmode == 0 ? "falling" : "rising",
			newmode == 0 ? 440 : 1320, newmode == 2 ? 3 : 2);
}

void accessibilityCaneTick(void)
{
	struct accessibilityobserver observer;
	const char *scopereason = accessibilityCaneGameplayScopeReason();
	s32 mode;
	s32 now;
	s32 cycleticks;
	s32 elapsed;
	s32 cycleselapsed;
	const s32 *offsets;
#ifndef PLATFORM_N64
	u32 modifiers;
	s32 f4pressed;
#endif

	if (scopereason) {
		accessibilityCaneStop(scopereason, true);
		return;
	}

	if (!accessibilityObserverGet(&observer)) {
		accessibilityCaneStop("observer_unavailable", true);
		return;
	}

	if (g_AccessibilityCaneObserverProp
			&& (g_AccessibilityCaneObserverProp != (uintptr_t)observer.prop
				|| g_AccessibilityCaneObserverRemote != observer.isremote)) {
		accessibilityCaneStop("observer_changed", false);
		accessibilityLogEvent("cane", "observer_change",
				"tick=%d stage=%d player=%d observer=%p remote=%d",
				g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum,
				(void *)observer.prop, observer.isremote);
	}
	g_AccessibilityCaneObserverProp = (uintptr_t)observer.prop;
	g_AccessibilityCaneObserverRemote = observer.isremote;

	if (!g_AccessibilityCaneScopeActive) {
		g_AccessibilityCaneScopeActive = true;
		accessibilityLogEvent("cane", "scope",
				"state=entered tick=%d stage=%d player=%d",
				g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum);
	}

#ifndef PLATFORM_N64
	modifiers = inputGetKeyModState();
	f4pressed = inputKeyJustPressed(VK_F4);
	if (f4pressed && !(modifiers & KM_ALT)) {
		accessibilityCaneCycleMode();
		g_AccessibilityCaneScopeActive = true;
	}
#endif

	mode = accessibilityGetVirtualCaneMode();
	if (mode == 0) {
		if (g_AccessibilityCaneSweepActive) {
			accessibilityCaneStop("mode_off", false);
			g_AccessibilityCaneScopeActive = true;
		}
		return;
	}

	now = g_Vars.lvframe60;
	if (!g_AccessibilityCaneSweepActive
			|| g_AccessibilityCaneSweepMode != mode) {
		accessibilityCaneBeginSweep(mode, now);
	}

	cycleticks = accessibilityCaneCycleTicks(mode);
	offsets = accessibilityCaneOffsets(mode);
	elapsed = now - g_AccessibilityCaneCycleStartTick;

	if (elapsed >= cycleticks) {
		while (g_AccessibilityCaneCursor < ACCESSIBILITY_CANE_PROBE_COUNT) {
			accessibilityCaneMarkSkipped(g_AccessibilityCaneCursor, now);
			g_AccessibilityCaneCursor++;
		}

		accessibilityCaneLogSweep("cycle_boundary");
		cycleselapsed = elapsed / cycleticks;
		if (cycleselapsed > 1) {
			g_AccessibilityCaneMissedCycles += cycleselapsed - 1;
			g_AccessibilityCaneSkipped += (u64)(cycleselapsed - 1)
					* ACCESSIBILITY_CANE_PROBE_COUNT;
		}
		accessibilityCaneBeginSweep(mode,
				g_AccessibilityCaneCycleStartTick
						+ cycleselapsed * cycleticks);
		elapsed = now - g_AccessibilityCaneCycleStartTick;
	}

	while (g_AccessibilityCaneCursor
			< ACCESSIBILITY_CANE_PROBE_COUNT - 1
			&& elapsed >= offsets[g_AccessibilityCaneCursor + 1]) {
		accessibilityCaneMarkSkipped(g_AccessibilityCaneCursor, now);
		g_AccessibilityCaneCursor++;
	}

	if (g_AccessibilityCaneCursor < ACCESSIBILITY_CANE_PROBE_COUNT
			&& elapsed >= offsets[g_AccessibilityCaneCursor]
			&& g_AccessibilityCaneLastQueryTick != now) {
		struct accessibilitycanesample *sample
				= &g_AccessibilityCaneSamples[g_AccessibilityCaneCursor];

		sample->actualtick = now;
		sample->lateness = now - sample->scheduledtick;
		accessibilityCaneQuery(sample);
		g_AccessibilityCaneSweepSamples++;
		g_AccessibilityCaneLastQueryTick = now;
		g_AccessibilityCaneCursor++;
	}
}

void accessibilityCaneReset(const char *reason)
{
	accessibilityCaneStop(reason ? reason : "reset", false);
	accessibilityToneStopCane();
	accessibilityLogEvent("cane", "reset", "reason=%s tick=%d stage=%d",
			reason ? reason : "reset", g_Vars.lvframe60, g_Vars.stagenum);
}

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
void accessibilityCaneGetDiagnostics(
		struct accessibilitycanediagnostics *diagnostics)
{
	if (!diagnostics) {
		return;
	}

	diagnostics->queries = g_AccessibilityCaneQueries;
	diagnostics->hits = g_AccessibilityCaneHits;
	diagnostics->misses = g_AccessibilityCaneMisses;
	diagnostics->skipped = g_AccessibilityCaneSkipped;
	diagnostics->sweeps = g_AccessibilityCaneSweeps;
	diagnostics->missedcycles = g_AccessibilityCaneMissedCycles;
	diagnostics->querytotalus = g_AccessibilityCaneQueryTotalUs;
	diagnostics->querymaxus = g_AccessibilityCaneQueryMaxUs;
}
#endif
