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
#include "accessibility/accessibility_tone.h"

#define ACCESSIBILITY_CANE_PROBE_COUNT 7
#define ACCESSIBILITY_CANE_MAX_DISTANCE 600.0f
#define ACCESSIBILITY_CANE_FULL_DISTANCE 75.0f
#define ACCESSIBILITY_CANE_FADE_DISTANCE 500.0f
#define ACCESSIBILITY_CANE_SILENT_DISTANCE 650.0f
#define ACCESSIBILITY_CANE_FREQUENCY_HZ 330.0f
#define ACCESSIBILITY_CANE_SLOW_CYCLE_TICKS TICKS(120)
#define ACCESSIBILITY_CANE_FAST_CYCLE_TICKS TICKS(60)
#define ACCESSIBILITY_CANE_LOG_BUFFER_SIZE 8192
#define ACCESSIBILITY_CANE_DEGREES_TO_RADIANS 0.01745329251994329577f

enum accessibilitycanesamplestate {
	ACCESSIBILITY_CANE_SAMPLE_PENDING,
	ACCESSIBILITY_CANE_SAMPLE_HIT,
	ACCESSIBILITY_CANE_SAMPLE_MISS,
	ACCESSIBILITY_CANE_SAMPLE_ERROR,
	ACCESSIBILITY_CANE_SAMPLE_SKIPPED,
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
	struct prop *obstacle;
	s32 obstacletype;
	u32 geoflags;
	struct coord normal;
	struct coord edge1;
	struct coord edge2;
	s32 volume;
	s32 pan;
	f32 normalizedvolume;
	f32 normalizedpan;
	u64 queryus;
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
static u64 g_AccessibilityCaneSkipped;
static u64 g_AccessibilityCaneSweeps;
static u64 g_AccessibilityCaneMissedCycles;
static u64 g_AccessibilityCaneQueryTotalUs;
static u64 g_AccessibilityCaneQueryMaxUs;

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

	if (g_Vars.currentplayer->bondmovemode != MOVEMODE_WALK) {
		return "unsupported_movement_mode";
	}

	if (g_Vars.lvupdate60 <= 0) {
		return "simulation_stopped";
	}

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
			"id=%d mode=%s reason=%s cycle_start=%d cycle_ticks=%d samples=%d skipped=%d total_queries=%" PRIu64 " total_hits=%" PRIu64 " total_misses=%" PRIu64 " total_skipped=%" PRIu64 " missed_cycles=%" PRIu64 " details=",
			g_AccessibilityCaneSweepId,
			accessibilityCaneModeName(g_AccessibilityCaneSweepMode),
			reason ? reason : "complete", g_AccessibilityCaneCycleStartTick,
			accessibilityCaneCycleTicks(g_AccessibilityCaneSweepMode),
			g_AccessibilityCaneSweepSamples, g_AccessibilityCaneSweepSkipped,
			(uint64_t)g_AccessibilityCaneQueries,
			(uint64_t)g_AccessibilityCaneHits,
			(uint64_t)g_AccessibilityCaneMisses,
			(uint64_t)g_AccessibilityCaneSkipped,
			(uint64_t)g_AccessibilityCaneMissedCycles);

	for (i = 0; i < ACCESSIBILITY_CANE_PROBE_COUNT; i++) {
		struct accessibilitycanesample *sample = &g_AccessibilityCaneSamples[i];

		accessibilityCaneAppendLog(
				"%ss%d={angle:%d state:%s scheduled:%d actual:%d late:%d result:%d pass:%d origin:%.2f,%.2f,%.2f forward:%.5f,%.5f direction:%.5f,%.5f end:%.2f,%.2f,%.2f bbox:%.2f,%.2f,%.2f raw:%.2f,%.2f,%.2f audio:%.2f,%.2f,%.2f distance:%.2f obstacle:%p type:%d geoflags:0x%08x normal:%.5f,%.5f,%.5f edge:%.2f,%.2f,%.2f,%.2f volume:%d pan:%d normalized:%.5f,%.5f query_us:%" PRIu64 "}",
				i ? " " : "", i, sample->angledegrees,
				accessibilityCaneSampleStateName(sample->state),
				sample->scheduledtick, sample->actualtick, sample->lateness,
				sample->result, sample->collisionpass,
				sample->origin.x, sample->origin.y,
				sample->origin.z, sample->forward.x, sample->forward.z,
				sample->direction.x, sample->direction.z,
				sample->requestedend.x, sample->requestedend.y,
				sample->requestedend.z, sample->radius, sample->ymin,
				sample->ymax, sample->rawhit.x, sample->rawhit.y,
				sample->rawhit.z, sample->audiosource.x,
				sample->audiosource.y, sample->audiosource.z,
				sample->distance, (void *)sample->obstacle,
				sample->obstacletype, sample->geoflags, sample->normal.x,
				sample->normal.y, sample->normal.z,
				sample->edge1.x, sample->edge1.z,
				sample->edge2.x, sample->edge2.z, sample->volume,
				sample->pan, sample->normalizedvolume,
				sample->normalizedpan, (uint64_t)sample->queryus);
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

static s32 accessibilityCaneQuery(struct accessibilitycanesample *sample)
{
	RoomNum dstrooms[8];
	RoomNum morerooms[22];
	struct coord start;
	struct coord end;
	f32 horizontal;
	f32 angle;
	f32 cosine;
	f32 sine;
	s32 types;
	s32 result;
	s32 collisionpass = 0;
#if VERSION < VERSION_NTSC_1_0
	s32 i;
#endif
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	u64 querystart;
#endif

	start = g_Vars.currentplayer->prop->pos;
	sample->origin = start;
	sample->forward.x = g_Vars.currentplayer->cam_look.x;
	sample->forward.y = 0.0f;
	sample->forward.z = g_Vars.currentplayer->cam_look.z;
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
	end.x = start.x + sample->direction.x * ACCESSIBILITY_CANE_MAX_DISTANCE;
	end.y = start.y;
	end.z = start.z + sample->direction.z * ACCESSIBILITY_CANE_MAX_DISTANCE;
	sample->requestedend = end;

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	querystart = sysGetMicroseconds();
#endif
	playerGetBbox(g_Vars.currentplayer->prop, &sample->radius,
			&sample->ymax, &sample->ymin);
	func0f065dfc(&start, g_Vars.currentplayer->prop->rooms,
			&end, dstrooms, morerooms, 20);

#if VERSION < VERSION_NTSC_1_0
	for (i = 0; dstrooms[i] != -1; i++) {
		if (dstrooms[i] == g_Vars.currentplayer->floorroom) {
			dstrooms[0] = g_Vars.currentplayer->floorroom;
			dstrooms[1] = -1;
			break;
		}
	}
#endif

	bmoveFindEnteredRoomsByPos(g_Vars.currentplayer, &end, dstrooms);
	types = g_Vars.bondcollisions
			? CDTYPE_BG | CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER
			: CDTYPE_BG;

	result = cdExamCylMove06(&start, g_Vars.currentplayer->prop->rooms,
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
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	sample->queryus = sysGetMicroseconds() - querystart;
	g_AccessibilityCaneQueryTotalUs += sample->queryus;
	if (sample->queryus > g_AccessibilityCaneQueryMaxUs) {
		g_AccessibilityCaneQueryMaxUs = sample->queryus;
	}
#endif

	sample->result = result;
	g_AccessibilityCaneQueries++;

	if (result != CDRESULT_COLLISION) {
		sample->state = result == CDRESULT_NOCOLLISION
				? ACCESSIBILITY_CANE_SAMPLE_MISS
				: ACCESSIBILITY_CANE_SAMPLE_ERROR;
		g_AccessibilityCaneMisses++;
		return result;
	}

	sample->obstacle = cdGetObstacleProp();
	sample->obstacletype = sample->obstacle ? sample->obstacle->type : -1;
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
	sample->audiosource = sample->rawhit;
	sample->audiosource.y = g_Vars.currentplayer->cam_pos.y;
	horizontal = sqrtf((sample->audiosource.x - start.x)
				* (sample->audiosource.x - start.x)
			+ (sample->audiosource.z - start.z)
				* (sample->audiosource.z - start.z));
	sample->distance = horizontal;
	sample->volume = psCalculateVolumeFromDistance(horizontal,
			ACCESSIBILITY_CANE_FULL_DISTANCE,
			ACCESSIBILITY_CANE_FADE_DISTANCE,
			ACCESSIBILITY_CANE_SILENT_DISTANCE, AL_VOL_FULL);
	sample->pan = psCalculatePan(&sample->audiosource,
			ACCESSIBILITY_CANE_FULL_DISTANCE,
			ACCESSIBILITY_CANE_FADE_DISTANCE,
			ACCESSIBILITY_CANE_SILENT_DISTANCE, horizontal, false, NULL);
	sample->normalizedvolume = (f32)sample->volume / (f32)AL_VOL_FULL;
	sample->normalizedpan = ((f32)sample->pan - (f32)AL_PAN_CENTER)
			/ (f32)AL_PAN_CENTER;
	sample->state = ACCESSIBILITY_CANE_SAMPLE_HIT;
	g_AccessibilityCaneHits++;

	accessibilityTonePlayCaneSlot(sample - g_AccessibilityCaneSamples,
			ACCESSIBILITY_CANE_FREQUENCY_HZ, sample->normalizedvolume,
			sample->normalizedpan);

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
}

static void accessibilityCaneCycleMode(void)
{
	s32 oldmode = accessibilityGetVirtualCaneMode();
	s32 newmode = oldmode >= 2 ? 0 : oldmode + 1;

	accessibilityCaneStop("mode_change", false);
	accessibilitySetVirtualCaneMode(newmode);
	accessibilityLogEvent("cane", "command",
			"action=cycle key=F4 old_mode=%s old_value=%d new_mode=%s new_value=%d tick=%d stage=%d player=%d alt=0",
			accessibilityCaneModeName(oldmode), oldmode,
			accessibilityCaneModeName(newmode), newmode,
			g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum);
}

void accessibilityCaneTick(void)
{
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
