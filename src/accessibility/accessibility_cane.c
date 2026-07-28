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

#define ACCESSIBILITY_CANE_PROBE_COUNT 7
#define ACCESSIBILITY_CANE_TERRAIN_SAMPLE_COUNT 4
#define ACCESSIBILITY_CANE_TERRAIN_VERTICAL_RANGE 200.0f
#define ACCESSIBILITY_CANE_TERRAIN_CONTOUR_RATIO 1.2f
#define ACCESSIBILITY_CANE_WALL_DURATION_MS 35
#define ACCESSIBILITY_CANE_TERRAIN_DURATION_MS 140
#define ACCESSIBILITY_CANE_BREAKABLE_DURATION_MS 90
#define ACCESSIBILITY_CANE_BREAKABLE_START_RATIO 2.0f
#define ACCESSIBILITY_CANE_SLOW_CYCLE_TICKS TICKS(120)
#define ACCESSIBILITY_CANE_FAST_CYCLE_TICKS TICKS(60)
#define ACCESSIBILITY_CANE_LOG_BUFFER_SIZE 16384
#define ACCESSIBILITY_CANE_DEGREES_TO_RADIANS 0.01745329251994329577f

enum accessibilitycanesamplestate {
	ACCESSIBILITY_CANE_SAMPLE_PENDING,
	ACCESSIBILITY_CANE_SAMPLE_HIT,
	ACCESSIBILITY_CANE_SAMPLE_TERRAIN,
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
	s32 terrain;
	s32 breakable;
	f32 terrainground;
	f32 terrainheight;
	f32 terraindistance;
	RoomNum terrainroom;
	u16 terrainflags;
	s32 terrainqueries;
	struct accessibilitycaneterrainprobe
			terrainprobes[ACCESSIBILITY_CANE_TERRAIN_SAMPLE_COUNT];
	struct prop *obstacle;
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
	-45, -30, -15, 0, 15, 30, 45,
};

static const s32 g_AccessibilityCaneSlowOffsets[ACCESSIBILITY_CANE_PROBE_COUNT] = {
	TICKS(0), TICKS(15), TICKS(30), TICKS(45), TICKS(60), TICKS(75), TICKS(90),
};

static const s32 g_AccessibilityCaneFastOffsets[ACCESSIBILITY_CANE_PROBE_COUNT] = {
	TICKS(0), TICKS(8), TICKS(15), TICKS(23), TICKS(30), TICKS(38), TICKS(45),
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
			&& g_Vars.currentplayer->bondmovemode != MOVEMODE_WALK) {
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
				"%ss%d={angle:%d state:%s scheduled:%d actual:%d late:%d result:%d pass:%d observer:%p remote:%d origin:%.2f,%.2f,%.2f forward:%.5f,%.5f direction:%.5f,%.5f end:%.2f,%.2f,%.2f bbox:%.2f,%.2f,%.2f raw:%.2f,%.2f,%.2f audio:%.2f,%.2f,%.2f distance:%.2f frequency_hz:%.2f end_frequency_hz:%.2f duration_ms:%d terrain:%d breakable:%d terrain_ground:%.2f terrain_height:%.2f terrain_distance:%.2f terrain_room:%d terrain_flags:0x%04x terrain_queries:%d obstacle:%p type:%d geoflags:0x%08x normal:%.5f,%.5f,%.5f edge:%.2f,%.2f,%.2f,%.2f volume:%d pan:%d normalized:%.5f,%.5f master_volume:%.5f effective_volume:%.5f query_us:%" PRIu64 " probes=[",
				i ? " " : "", i, sample->angledegrees,
				accessibilityCaneSampleStateName(sample->state),
				sample->scheduledtick, sample->actualtick, sample->lateness,
				sample->result, sample->collisionpass,
				(void *)sample->observerprop, sample->observerremote,
				sample->origin.x, sample->origin.y,
				sample->origin.z, sample->forward.x, sample->forward.z,
				sample->direction.x, sample->direction.z,
				sample->requestedend.x, sample->requestedend.y,
				sample->requestedend.z, sample->radius, sample->ymin,
				sample->ymax, sample->rawhit.x, sample->rawhit.y,
				sample->rawhit.z, sample->audiosource.x,
				sample->audiosource.y, sample->audiosource.z,
				sample->distance, sample->frequency, sample->endfrequency,
				sample->durationms,
				sample->terrain, sample->breakable,
				sample->terrainground,
				sample->terrainheight, sample->terraindistance,
				sample->terrainroom, sample->terrainflags,
				sample->terrainqueries,
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
					&& j < ACCESSIBILITY_CANE_TERRAIN_SAMPLE_COUNT; j++) {
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

static s32 accessibilityCaneFindTerrain(
		struct accessibilitycanesample *sample,
		const struct accessibilityobserver *observer, f32 barrier)
{
	RoomNum rooms[8];
	RoomNum morerooms[22];
	struct coord origin;
	struct coord roompoint;
	struct coord point;
	f32 reach;
	f32 threshold;
	f32 limit;
	f32 distance;
	f32 ground;
	f32 delta;
	u16 flags;
	RoomNum room;
	s32 i;

	accessibilityGetVirtualCaneTerrainTuning(&reach, &threshold);
	origin = observer->origin;
	limit = reach;

	if (barrier > 0.0f && barrier < limit) {
		limit = barrier - 1.0f;
	}

	if (limit < 1.0f) {
		return false;
	}

	for (i = 1; i <= ACCESSIBILITY_CANE_TERRAIN_SAMPLE_COUNT; i++) {
		struct accessibilitycaneterrainprobe *probe
				= &sample->terrainprobes[i - 1];
		s32 j;

		distance = limit * (f32)i
				/ (f32)ACCESSIBILITY_CANE_TERRAIN_SAMPLE_COUNT;
		roompoint.x = observer->origin.x + sample->direction.x * distance;
		roompoint.y = observer->origin.y;
		roompoint.z = observer->origin.z + sample->direction.z * distance;

		for (j = 0; j < 8; j++) {
			rooms[j] = -1;
		}

		func0f065dfc(&origin, observer->prop->rooms,
				&roompoint, rooms, morerooms, 20);

		if (!observer->isremote) {
			bmoveFindEnteredRoomsByPos(
					g_Vars.currentplayer, &roompoint, rooms);
		}

		point = roompoint;
		point.y = observer->ground
				+ ACCESSIBILITY_CANE_TERRAIN_VERTICAL_RANGE;
		probe->point = point;
		probe->distance = distance;
		probe->room = -1;

		for (j = 0; j < 8; j++) {
			probe->rooms[j] = rooms[j];
		}

		flags = 0;
#if VERSION >= VERSION_NTSC_1_0
		room = cdFindFloorRoomYColourFlagsAtPos(
				&point, rooms, &ground, NULL, &flags);
#else
		room = cdFindFloorRoomYColourFlagsAtPos(
				&point, rooms, &ground, NULL);
#endif
		sample->terrainqueries++;
		probe->room = room;
		probe->flags = flags;

		if (room < 0) {
			probe->reason = ACCESSIBILITY_CANE_TERRAIN_PROBE_NO_FLOOR;
			continue;
		}

		delta = ground - observer->ground;
		probe->ground = ground;
		probe->delta = delta;

		if (delta > ACCESSIBILITY_CANE_TERRAIN_VERTICAL_RANGE
				|| delta < -ACCESSIBILITY_CANE_TERRAIN_VERTICAL_RANGE) {
			probe->reason = ACCESSIBILITY_CANE_TERRAIN_PROBE_VERTICAL_RANGE;
			continue;
		}

		if (fabsf(delta) >= threshold) {
			probe->reason = ACCESSIBILITY_CANE_TERRAIN_PROBE_SELECTED;
			sample->terrain = delta > 0.0f ? 1 : -1;
			sample->terrainground = ground;
			sample->terrainheight = delta;
			sample->terraindistance = distance;
			sample->terrainroom = room;
			sample->terrainflags = flags;
			sample->audiosource = point;
			sample->audiosource.y = observer->camera.y;
			return true;
		}

		probe->reason = ACCESSIBILITY_CANE_TERRAIN_PROBE_BELOW_THRESHOLD;
	}

	return false;
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
	}

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	sample->queryus = sysGetMicroseconds() - querystart;
	g_AccessibilityCaneQueryTotalUs += sample->queryus;
	if (sample->queryus > g_AccessibilityCaneQueryMaxUs) {
		g_AccessibilityCaneQueryMaxUs = sample->queryus;
	}
#endif

	if (terrainfound
			&& (barrierdistance < 0.0f
				|| sample->terraindistance < barrierdistance)) {
		horizontal = sample->terraindistance;
		sample->state = ACCESSIBILITY_CANE_SAMPLE_TERRAIN;
		g_AccessibilityCaneTerrainHits++;
	} else if (result == CDRESULT_COLLISION) {
		sample->audiosource = sample->rawhit;
		sample->audiosource.y = observer.camera.y;
		horizontal = barrierdistance;
		sample->state = ACCESSIBILITY_CANE_SAMPLE_HIT;
		sample->breakable
				= accessibilityPathBlockerIsBreakable(sample->obstacle);
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

	if (sample->state == ACCESSIBILITY_CANE_SAMPLE_TERRAIN) {
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
			sample->normalizedpan, sample->durationms);

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
