#include <math.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "types.h"
#include "game/lv.h"
#include "game/propobj.h"
#include "game/propsnd.h"
#include "lib/collision.h"
#include "system.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_hazard.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_tone.h"

#define ACCESSIBILITY_HAZARD_FREQUENCY_HZ 220.0f
#define ACCESSIBILITY_HAZARD_MAX_DISTANCE 500.0f
#define ACCESSIBILITY_HAZARD_MAX_DISTANCE_SQ \
	(ACCESSIBILITY_HAZARD_MAX_DISTANCE * ACCESSIBILITY_HAZARD_MAX_DISTANCE)
#define ACCESSIBILITY_HAZARD_FULL_DISTANCE 100.0f
#define ACCESSIBILITY_HAZARD_FADE_DISTANCE 400.0f
#define ACCESSIBILITY_HAZARD_SILENT_DISTANCE ACCESSIBILITY_HAZARD_MAX_DISTANCE
#define ACCESSIBILITY_HAZARD_FACING_DOT 0.9063078f
#define ACCESSIBILITY_HAZARD_SWITCH_MARGIN 75.0f
#define ACCESSIBILITY_HAZARD_SWEEP_CYCLE_TICKS TICKS(90)
#define ACCESSIBILITY_HAZARD_SCAN_TICKS TICKS(3)
#define ACCESSIBILITY_HAZARD_LOG_TICKS TICKS(60)

struct accessibilityhazardcandidate {
	s32 propnum;
	void *identity;
	struct coord endpoint1;
	struct coord endpoint2;
	struct coord closest;
	f32 distance;
	f32 facingdot;
};

static struct accessibilityhazardcandidate g_AccessibilityHazardSelected;
static s32 g_AccessibilityHazardSelectedValid;
static f32 g_AccessibilityHazardSweepPhase;
static s32 g_AccessibilityHazardNextScanTick;
static s32 g_AccessibilityHazardNextLogTick;
static s32 g_AccessibilityHazardNextAuditTick;
static u64 g_AccessibilityHazardScanCount;
static u64 g_AccessibilityHazardSelectionCount;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
static u64 g_AccessibilityHazardScanTimeTotalUs;
static u64 g_AccessibilityHazardScanTimeMaxUs;
static u64 g_AccessibilityHazardScanTimingCount;
#endif

static s32 accessibilityHazardPropNum(const struct prop *prop)
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

static void accessibilityHazardClosestPoint(const struct coord *point,
		const struct coord *endpoint1, const struct coord *endpoint2,
		struct coord *closest, f32 *distance)
{
	f32 segmentx = endpoint2->x - endpoint1->x;
	f32 segmenty = endpoint2->y - endpoint1->y;
	f32 segmentz = endpoint2->z - endpoint1->z;
	f32 pointx = point->x - endpoint1->x;
	f32 pointy = point->y - endpoint1->y;
	f32 pointz = point->z - endpoint1->z;
	f32 lengthsq = segmentx * segmentx + segmenty * segmenty + segmentz * segmentz;
	f32 fraction = 0.5f;
	f32 dx;
	f32 dy;
	f32 dz;

	if (lengthsq > 0.0001f) {
		fraction = (pointx * segmentx + pointy * segmenty + pointz * segmentz)
				/ lengthsq;

		if (fraction < 0.0f) {
			fraction = 0.0f;
		} else if (fraction > 1.0f) {
			fraction = 1.0f;
		}
	}

	closest->x = endpoint1->x + segmentx * fraction;
	closest->y = endpoint1->y + segmenty * fraction;
	closest->z = endpoint1->z + segmentz * fraction;
	dx = closest->x - point->x;
	dy = closest->y - point->y;
	dz = closest->z - point->z;
	*distance = sqrtf(dx * dx + dy * dy + dz * dz);
}

static s32 accessibilityHazardGetEndpoints(struct doorobj *door,
		struct coord *endpoint1, struct coord *endpoint2)
{
	struct modelrodata_bbox bbox;
	struct coord vertices[4];
	f32 bestlengthsq = -1.0f;
	s32 bestindex = -1;
	s32 i;

	if (!door || !door->base.model || !door->base.prop) {
		return false;
	}

	doorGetBbox(door, &bbox);
	bbox.ymin = bbox.ymax = (bbox.ymin + bbox.ymax) * 0.5f;
	func0f070a1c(&bbox, door->base.realrot, &door->base.prop->pos, vertices);

	for (i = 0; i < 4; i++) {
		s32 next = (i + 1) % 4;
		f32 x = vertices[next].x - vertices[i].x;
		f32 y = vertices[next].y - vertices[i].y;
		f32 z = vertices[next].z - vertices[i].z;
		f32 lengthsq = x * x + y * y + z * z;

		if (lengthsq > bestlengthsq) {
			bestlengthsq = lengthsq;
			bestindex = i;
		}
	}

	if (bestindex < 0 || bestlengthsq < 1.0f) {
		return false;
	}

	*endpoint1 = vertices[bestindex];
	*endpoint2 = vertices[(bestindex + 1) % 4];
	return true;
}

static s32 accessibilityHazardEvaluate(struct prop *prop,
		struct accessibilityhazardcandidate *candidate, const char **reason)
{
	struct doorobj *door;
	struct coord direction;
	f32 directionlength;

	if (!prop || prop->type != PROPTYPE_DOOR || !prop->door) {
		*reason = "not_laser_door";
		return false;
	}

	door = prop->door;

	if (door->doortype != DOORTYPE_LASER
			|| !(door->doorflags & DOORFLAG_DAMAGEONCONTACT)) {
		*reason = "not_damaging_laser";
		return false;
	}

	if (!prop->active || door->base.prop != prop
			|| (door->base.hidden & (OBJHFLAG_DELETING | OBJHFLAG_GONE
					| OBJHFLAG_DOORPERIMDISABLED))) {
		*reason = "inactive_or_noncolliding";
		return false;
	}

	if (door->laserfade == 0 || door->frac >= door->perimfrac) {
		*reason = "invisible_or_open";
		return false;
	}

	if (!accessibilityHazardGetEndpoints(door,
			&candidate->endpoint1, &candidate->endpoint2)) {
		*reason = "geometry_unavailable";
		return false;
	}

	accessibilityHazardClosestPoint(&g_Vars.currentplayer->cam_pos,
			&candidate->endpoint1, &candidate->endpoint2,
			&candidate->closest, &candidate->distance);

	if (candidate->distance * candidate->distance
			> ACCESSIBILITY_HAZARD_MAX_DISTANCE_SQ) {
		*reason = "outside_short_range";
		return false;
	}

	direction.x = candidate->closest.x - g_Vars.currentplayer->cam_pos.x;
	direction.y = candidate->closest.y - g_Vars.currentplayer->cam_pos.y;
	direction.z = candidate->closest.z - g_Vars.currentplayer->cam_pos.z;
	directionlength = candidate->distance;

	if (directionlength < 1.0f) {
		direction.x = (candidate->endpoint1.x + candidate->endpoint2.x) * 0.5f
				- g_Vars.currentplayer->cam_pos.x;
		direction.y = (candidate->endpoint1.y + candidate->endpoint2.y) * 0.5f
				- g_Vars.currentplayer->cam_pos.y;
		direction.z = (candidate->endpoint1.z + candidate->endpoint2.z) * 0.5f
				- g_Vars.currentplayer->cam_pos.z;
		directionlength = sqrtf(direction.x * direction.x
				+ direction.y * direction.y + direction.z * direction.z);
	}

	if (directionlength < 0.0001f) {
		*reason = "direction_unavailable";
		return false;
	}

	candidate->facingdot = (direction.x * g_Vars.currentplayer->cam_look.x
			+ direction.y * g_Vars.currentplayer->cam_look.y
			+ direction.z * g_Vars.currentplayer->cam_look.z) / directionlength;

	if (candidate->facingdot < ACCESSIBILITY_HAZARD_FACING_DOT) {
		*reason = "outside_facing_cone";
		return false;
	}

	if (!cdTestLos06(&g_Vars.currentplayer->cam_pos,
			g_Vars.currentplayer->prop->rooms, &candidate->closest,
			prop->rooms, CDTYPE_BG)) {
		*reason = "line_of_sight_blocked";
		return false;
	}

	candidate->propnum = accessibilityHazardPropNum(prop);
	candidate->identity = door;
	*reason = "eligible";
	return candidate->propnum >= 0;
}

static const char *accessibilityHazardScopeReason(void)
{
	if (!accessibilityIsEnvironmentalHazardsEnabled()) {
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

	if (g_Vars.in_cutscene) {
		return "cutscene";
	}

	if (g_Vars.currentplayer->isdead) {
		return "player_dead";
	}

	return NULL;
}

static void accessibilityHazardStop(const char *reason)
{
	if (g_AccessibilityHazardSelectedValid) {
		accessibilityLogEvent("hazard", "selection_lost",
				"reason=%s propnum=%d identity=%p distance=%.3f facing_dot=%.5f",
				reason, g_AccessibilityHazardSelected.propnum,
				g_AccessibilityHazardSelected.identity,
				g_AccessibilityHazardSelected.distance,
				g_AccessibilityHazardSelected.facingdot);
	}

	accessibilityToneSetHazard(0, 0.0f, 0.0f, 0.0f);
	memset(&g_AccessibilityHazardSelected, 0,
			sizeof(g_AccessibilityHazardSelected));
	g_AccessibilityHazardSelected.propnum = -1;
	g_AccessibilityHazardSelectedValid = false;
	g_AccessibilityHazardSweepPhase = 0.0f;
	g_AccessibilityHazardNextLogTick = 0;
}

static void accessibilityHazardSelect(
		const struct accessibilityhazardcandidate *candidate, const char *reason)
{
	s32 changed = !g_AccessibilityHazardSelectedValid
			|| candidate->propnum != g_AccessibilityHazardSelected.propnum
			|| candidate->identity != g_AccessibilityHazardSelected.identity;

	if (changed) {
		g_AccessibilityHazardSelectionCount++;
		g_AccessibilityHazardSweepPhase = 0.0f;
		g_AccessibilityHazardNextLogTick = g_Vars.lvframe60;
		accessibilityLogEvent("hazard", "selection",
				"selection=%llu reason=%s propnum=%d identity=%p distance=%.3f facing_dot=%.5f endpoint1=%.3f,%.3f,%.3f endpoint2=%.3f,%.3f,%.3f frequency_hz=%.1f range=%.1f facing_threshold=%.5f switch_margin=%.1f",
				(unsigned long long)g_AccessibilityHazardSelectionCount,
				reason, candidate->propnum, candidate->identity,
				candidate->distance, candidate->facingdot,
				candidate->endpoint1.x, candidate->endpoint1.y,
				candidate->endpoint1.z, candidate->endpoint2.x,
				candidate->endpoint2.y, candidate->endpoint2.z,
				ACCESSIBILITY_HAZARD_FREQUENCY_HZ,
				ACCESSIBILITY_HAZARD_MAX_DISTANCE,
				ACCESSIBILITY_HAZARD_FACING_DOT,
				ACCESSIBILITY_HAZARD_SWITCH_MARGIN);
	}

	g_AccessibilityHazardSelected = *candidate;
	g_AccessibilityHazardSelectedValid = true;
}

static void accessibilityHazardUpdateSound(void)
{
	struct coord source;
	f32 triangle;
	f32 sourcex;
	f32 sourcey;
	f32 sourcez;
	f32 sourcedistance;
	s32 volume;
	s32 pan;
	f32 normalizedvolume;
	f32 normalizedpan;

	if (g_Vars.lvupdate60 > 0) {
		g_AccessibilityHazardSweepPhase += g_Vars.lvupdate60freal
				/ (f32)ACCESSIBILITY_HAZARD_SWEEP_CYCLE_TICKS;

		while (g_AccessibilityHazardSweepPhase >= 1.0f) {
			g_AccessibilityHazardSweepPhase -= 1.0f;
		}
	}

	triangle = g_AccessibilityHazardSweepPhase < 0.5f
			? g_AccessibilityHazardSweepPhase * 2.0f
			: (1.0f - g_AccessibilityHazardSweepPhase) * 2.0f;
	source.x = g_AccessibilityHazardSelected.endpoint1.x
			+ (g_AccessibilityHazardSelected.endpoint2.x
				- g_AccessibilityHazardSelected.endpoint1.x) * triangle;
	source.y = g_AccessibilityHazardSelected.endpoint1.y
			+ (g_AccessibilityHazardSelected.endpoint2.y
				- g_AccessibilityHazardSelected.endpoint1.y) * triangle;
	source.z = g_AccessibilityHazardSelected.endpoint1.z
			+ (g_AccessibilityHazardSelected.endpoint2.z
				- g_AccessibilityHazardSelected.endpoint1.z) * triangle;
	sourcex = source.x - g_Vars.currentplayer->cam_pos.x;
	sourcey = source.y - g_Vars.currentplayer->cam_pos.y;
	sourcez = source.z - g_Vars.currentplayer->cam_pos.z;
	sourcedistance = sqrtf(sourcex * sourcex + sourcey * sourcey
			+ sourcez * sourcez);
	volume = psCalculateVolumeFromDistance(sourcedistance,
			ACCESSIBILITY_HAZARD_FULL_DISTANCE,
			ACCESSIBILITY_HAZARD_FADE_DISTANCE,
			ACCESSIBILITY_HAZARD_SILENT_DISTANCE, AL_VOL_FULL);
	pan = psCalculatePan(&source, ACCESSIBILITY_HAZARD_FULL_DISTANCE,
			ACCESSIBILITY_HAZARD_FADE_DISTANCE,
			ACCESSIBILITY_HAZARD_SILENT_DISTANCE, sourcedistance,
			false, NULL);
	normalizedvolume = (f32)volume / (f32)AL_VOL_FULL;
	normalizedpan = ((f32)pan - (f32)AL_PAN_CENTER)
			/ (f32)AL_PAN_CENTER;
	accessibilityToneSetHazard(1, ACCESSIBILITY_HAZARD_FREQUENCY_HZ,
			normalizedvolume, normalizedpan);

	if (g_AccessibilityHazardNextLogTick == 0
			|| g_Vars.lvframe60 >= g_AccessibilityHazardNextLogTick) {
		accessibilityLogEvent("hazard", "sweep",
				"scan=%llu tick=%d propnum=%d identity=%p phase=%.5f triangle=%.5f source=%.3f,%.3f,%.3f closest_distance=%.3f source_distance=%.3f facing_dot=%.5f volume=%d normalized_volume=%.5f pan=%d normalized_pan=%.5f frequency_hz=%.1f cycle_ticks=%d",
				(unsigned long long)g_AccessibilityHazardScanCount,
				g_Vars.lvframe60, g_AccessibilityHazardSelected.propnum,
				g_AccessibilityHazardSelected.identity,
				g_AccessibilityHazardSweepPhase, triangle,
				source.x, source.y, source.z,
				g_AccessibilityHazardSelected.distance, sourcedistance,
				g_AccessibilityHazardSelected.facingdot,
				volume, normalizedvolume, pan, normalizedpan,
				ACCESSIBILITY_HAZARD_FREQUENCY_HZ,
				ACCESSIBILITY_HAZARD_SWEEP_CYCLE_TICKS);
		g_AccessibilityHazardNextLogTick
				= g_Vars.lvframe60 + ACCESSIBILITY_HAZARD_LOG_TICKS;
	}
}

void accessibilityHazardTick(void)
{
	const char *scopereason = accessibilityHazardScopeReason();
	struct accessibilityhazardcandidate best;
	struct accessibilityhazardcandidate retained;
	s32 bestvalid = false;
	s32 retainedvalid = false;
	struct prop *prop;
	s32 traversed = 0;
	const char *selectedrejection = NULL;
	s32 lasers = 0;
	s32 eligible = 0;
	s32 inactive = 0;
	s32 open = 0;
	s32 geometry = 0;
	s32 range = 0;
	s32 facing = 0;
	s32 lineofsight = 0;
	s32 other = 0;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	u64 scanstart;
	u64 scanelapsed;
#endif

	if (scopereason) {
		if (g_AccessibilityHazardSelectedValid) {
			accessibilityHazardStop(scopereason);
		}
		return;
	}

	if (g_AccessibilityHazardNextScanTick > g_Vars.lvframe60) {
		if (g_AccessibilityHazardSelectedValid) {
			accessibilityHazardUpdateSound();
		}
		return;
	}

	g_AccessibilityHazardScanCount++;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	scanstart = sysGetMicroseconds();
#endif
	g_AccessibilityHazardNextScanTick
			= g_Vars.lvframe60 + ACCESSIBILITY_HAZARD_SCAN_TICKS;
	memset(&best, 0, sizeof(best));
	memset(&retained, 0, sizeof(retained));
	prop = g_Vars.activeprops;

	while (prop && prop != g_Vars.pausedprops && traversed < g_Vars.maxprops) {
		struct accessibilityhazardcandidate candidate;
		const char *reason;

		traversed++;
		memset(&candidate, 0, sizeof(candidate));

		if (prop->type == PROPTYPE_DOOR && prop->door
				&& prop->door->doortype == DOORTYPE_LASER
				&& (prop->door->doorflags & DOORFLAG_DAMAGEONCONTACT)) {
			lasers++;
		}

		if (accessibilityHazardEvaluate(prop, &candidate, &reason)) {
			eligible++;
			if (g_AccessibilityHazardSelectedValid
					&& candidate.propnum == g_AccessibilityHazardSelected.propnum
					&& candidate.identity == g_AccessibilityHazardSelected.identity) {
				retained = candidate;
				retainedvalid = true;
			}

			if (!bestvalid || candidate.distance < best.distance) {
				best = candidate;
				bestvalid = true;
			}
		} else if (g_AccessibilityHazardSelectedValid
				&& accessibilityHazardPropNum(prop)
						== g_AccessibilityHazardSelected.propnum
				&& prop->type == PROPTYPE_DOOR
				&& prop->door == g_AccessibilityHazardSelected.identity) {
			selectedrejection = reason;
		}

		if (prop->type == PROPTYPE_DOOR && prop->door
				&& prop->door->doortype == DOORTYPE_LASER
				&& (prop->door->doorflags & DOORFLAG_DAMAGEONCONTACT)
				&& strcmp(reason, "eligible") != 0) {
			if (strcmp(reason, "inactive_or_noncolliding") == 0) {
				inactive++;
			} else if (strcmp(reason, "invisible_or_open") == 0) {
				open++;
			} else if (strcmp(reason, "geometry_unavailable") == 0) {
				geometry++;
			} else if (strcmp(reason, "outside_short_range") == 0) {
				range++;
			} else if (strcmp(reason, "outside_facing_cone") == 0
					|| strcmp(reason, "direction_unavailable") == 0) {
				facing++;
			} else if (strcmp(reason, "line_of_sight_blocked") == 0) {
				lineofsight++;
			} else {
				other++;
			}
		}

		prop = prop->next;
	}

	if (prop && prop != g_Vars.pausedprops) {
		accessibilityLogEvent("hazard", "scan_guard",
				"scan=%llu traversed=%d maxprops=%d reason=active_list_did_not_terminate",
				(unsigned long long)g_AccessibilityHazardScanCount,
				traversed, g_Vars.maxprops);
	}

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	scanelapsed = sysGetMicroseconds() - scanstart;
	g_AccessibilityHazardScanTimeTotalUs += scanelapsed;
	g_AccessibilityHazardScanTimingCount++;

	if (scanelapsed > g_AccessibilityHazardScanTimeMaxUs) {
		g_AccessibilityHazardScanTimeMaxUs = scanelapsed;
	}
#endif

	if (g_AccessibilityHazardNextAuditTick == 0
			|| g_Vars.lvframe60 >= g_AccessibilityHazardNextAuditTick) {
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
		accessibilityLogEvent("hazard", "scan",
				"scan=%llu tick=%d traversed=%d lasers=%d eligible=%d inactive=%d invisible_or_open=%d geometry_unavailable=%d outside_range=%d outside_facing=%d line_of_sight_blocked=%d other=%d selected_valid=%d selected_propnum=%d scan_elapsed_us=%llu scan_average_us=%.3f scan_max_us=%llu scan_timing_count=%llu",
				(unsigned long long)g_AccessibilityHazardScanCount,
				g_Vars.lvframe60, traversed, lasers, eligible, inactive,
				open, geometry, range, facing, lineofsight, other,
				g_AccessibilityHazardSelectedValid,
				g_AccessibilityHazardSelectedValid
					? g_AccessibilityHazardSelected.propnum : -1,
				(unsigned long long)scanelapsed,
				g_AccessibilityHazardScanTimingCount > 0
					? (f64)g_AccessibilityHazardScanTimeTotalUs
						/ (f64)g_AccessibilityHazardScanTimingCount : 0.0,
				(unsigned long long)g_AccessibilityHazardScanTimeMaxUs,
				(unsigned long long)g_AccessibilityHazardScanTimingCount);
#else
		accessibilityLogEvent("hazard", "scan",
				"scan=%llu tick=%d traversed=%d lasers=%d eligible=%d inactive=%d invisible_or_open=%d geometry_unavailable=%d outside_range=%d outside_facing=%d line_of_sight_blocked=%d other=%d selected_valid=%d selected_propnum=%d",
				(unsigned long long)g_AccessibilityHazardScanCount,
				g_Vars.lvframe60, traversed, lasers, eligible, inactive,
				open, geometry, range, facing, lineofsight, other,
				g_AccessibilityHazardSelectedValid,
				g_AccessibilityHazardSelectedValid
					? g_AccessibilityHazardSelected.propnum : -1);
#endif
		g_AccessibilityHazardNextAuditTick
				= g_Vars.lvframe60 + ACCESSIBILITY_HAZARD_LOG_TICKS;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
		g_AccessibilityHazardScanTimeTotalUs = 0;
		g_AccessibilityHazardScanTimeMaxUs = 0;
		g_AccessibilityHazardScanTimingCount = 0;
#endif
	}

	if (retainedvalid && bestvalid
			&& retained.distance <= best.distance + ACCESSIBILITY_HAZARD_SWITCH_MARGIN) {
		accessibilityHazardSelect(&retained, "retained_with_hysteresis");
	} else if (bestvalid) {
		accessibilityHazardSelect(&best, retainedvalid
				? "nearer_candidate" : "nearest_eligible");
	} else {
		if (g_AccessibilityHazardSelectedValid) {
			accessibilityHazardStop(selectedrejection
					? selectedrejection : "no_eligible_laser");
		}
		return;
	}

	accessibilityHazardUpdateSound();
}

void accessibilityHazardReset(const char *reason)
{
	accessibilityHazardStop(reason ? reason : "reset");
	accessibilityLogEvent("hazard", "reset",
			"reason=%s scans=%llu selections=%llu enabled=%d frequency_hz=%.1f range=%.1f facing_dot=%.5f switch_margin=%.1f cycle_ticks=%d scan_ticks=%d lane=procedural_hazard",
			reason ? reason : "reset",
			(unsigned long long)g_AccessibilityHazardScanCount,
			(unsigned long long)g_AccessibilityHazardSelectionCount,
			accessibilityIsEnvironmentalHazardsEnabled(),
			ACCESSIBILITY_HAZARD_FREQUENCY_HZ,
			ACCESSIBILITY_HAZARD_MAX_DISTANCE,
			ACCESSIBILITY_HAZARD_FACING_DOT,
			ACCESSIBILITY_HAZARD_SWITCH_MARGIN,
			ACCESSIBILITY_HAZARD_SWEEP_CYCLE_TICKS,
			ACCESSIBILITY_HAZARD_SCAN_TICKS);
	g_AccessibilityHazardScanCount = 0;
	g_AccessibilityHazardSelectionCount = 0;
	g_AccessibilityHazardNextScanTick = 0;
	g_AccessibilityHazardNextAuditTick = 0;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	g_AccessibilityHazardScanTimeTotalUs = 0;
	g_AccessibilityHazardScanTimeMaxUs = 0;
	g_AccessibilityHazardScanTimingCount = 0;
#endif
}
