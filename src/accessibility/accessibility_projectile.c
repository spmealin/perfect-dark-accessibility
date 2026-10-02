#include <math.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "types.h"
#include "game/lv.h"
#include "game/objectives.h"
#include "game/propobj.h"
#include "game/propsnd.h"
#include "lib/vars.h"
#include "system.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_observer.h"
#include "accessibility/accessibility_projectile.h"
#include "accessibility/accessibility_relationship.h"
#include "accessibility/accessibility_tone.h"
#include "accessibility/accessibility_visibility.h"

#define ACCESSIBILITY_PROJECTILE_CANDIDATE_COUNT 16
#define ACCESSIBILITY_PROJECTILE_FULL_DISTANCE 250.0f
#define ACCESSIBILITY_PROJECTILE_LOG_TICKS TICKS(60)
#define ACCESSIBILITY_PROJECTILE_APPROACH_DISTANCE 900.0f
#define ACCESSIBILITY_PROJECTILE_APPROACH_TICKS 180.0f
#define ACCESSIBILITY_PROJECTILE_FAR_FREQUENCY_HZ 260.0f
#define ACCESSIBILITY_PROJECTILE_NEAR_FREQUENCY_HZ 520.0f
#define ACCESSIBILITY_PROJECTILE_FAR_PERIOD_MS 500
#define ACCESSIBILITY_PROJECTILE_NEAR_PERIOD_MS 110
#define ACCESSIBILITY_PROJECTILE_FAR_DURATION_MS 160
#define ACCESSIBILITY_PROJECTILE_NEAR_DURATION_MS 90

struct accessibilityprojectilecandidate {
	s32 propnum;
	struct weaponobj *weapon;
	struct coord position;
	f32 distance;
	f32 closing;
	f32 closestdistance;
	f32 closestticks;
	f32 priority;
	s32 targetplayer;
	s32 relationship;
};

struct accessibilityprojectileslot {
	s32 assigned;
	s32 propnum;
	struct weaponobj *weapon;
};

static struct accessibilityprojectileslot
		g_AccessibilityProjectileSlots[ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT];
static s32 g_AccessibilityProjectileNextLogTick;
static u64 g_AccessibilityProjectileScanCount;
static u64 g_AccessibilityProjectileAssignmentCount;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
static u64 g_AccessibilityProjectileScanTimeTotalUs;
static u64 g_AccessibilityProjectileScanTimeMaxUs;
static u64 g_AccessibilityProjectileScanTimingCount;
#endif

static s32 accessibilityProjectilePropNum(const struct prop *prop)
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

static s32 accessibilityProjectileIsRocket(s32 weaponnum)
{
	return weaponnum == WEAPON_ROCKET
			|| weaponnum == WEAPON_HOMINGROCKET
			|| weaponnum == WEAPON_SKROCKET;
}

static s32 accessibilityProjectileOwnerIsHostileVehicle(struct prop *owner)
{
	struct chopperobj *chopper;

	if (!owner || owner->type != PROPTYPE_OBJ || !owner->obj
			|| owner->obj->type != OBJTYPE_CHOPPER) {
		return false;
	}

	chopper = (struct chopperobj *)owner->obj;
	return owner->active && (owner->flags & PROPFLAG_ENABLED)
			&& (chopper->base.flags & (OBJFLAG_DEACTIVATED
				| OBJFLAG_CHOPPER_INACTIVE)) == 0
			&& (chopper->base.flags2 & OBJFLAG2_INVISIBLE) == 0
			&& !chopper->dead
			&& chopper->attackmode != CHOPPERMODE_FALL
			&& chopper->attackmode != CHOPPERMODE_DEAD
			&& chopper->weaponsarmed
			&& chopperGetTargetProp(chopper) == g_Vars.currentplayer->prop;
}

static const char *accessibilityProjectileScopeReason(
		struct accessibilityobserver *observer)
{
	if (!accessibilityIsProjectileHazardsEnabled()) return "feature_disabled";
	if (PLAYERCOUNT() != 1) return "unsupported_player_count";
	if (!g_Vars.currentplayer || !g_Vars.currentplayer->prop) return "player_unavailable";
	if (g_MenuData.count > 0) return "menu_open";
	if (lvIsPaused()) return "paused";
	if (g_Vars.in_cutscene) return "cutscene";
	if (g_Vars.currentplayer->isdead) return "player_dead";
	if (!accessibilityObserverGet(observer)) return "observer_unavailable";
	if (observer->isremote) return "remote_observer";
	return NULL;
}

static s32 accessibilityProjectileEvaluate(struct prop *prop,
		const struct accessibilityobserver *observer, f32 range,
		struct accessibilityprojectilecandidate *candidate)
{
	struct weaponobj *weapon;
	struct projectile *projectile;
	struct coord relative;
	RoomNum viewrooms[2];
	f32 distancesq;
	f32 speedsq;
	f32 dot;
	f32 predictedticks = 0.0f;
	f32 closestdistance;
	s32 relationship = ACCESSIBILITY_RELATIONSHIP_UNKNOWN;
	s32 targetplayer;

	if (!prop || !prop->active
			|| (prop->type != PROPTYPE_OBJ && prop->type != PROPTYPE_WEAPON)
			|| !prop->obj
			|| prop->obj->type != OBJTYPE_WEAPON) {
		return false;
	}

	weapon = (struct weaponobj *)prop->obj;
	if (weapon->base.prop != prop || !accessibilityProjectileIsRocket(
			weapon->weaponnum)
			|| !(weapon->base.hidden & OBJHFLAG_PROJECTILE)
			|| !weapon->base.projectile
			|| (weapon->base.hidden & (OBJHFLAG_DELETING | OBJHFLAG_GONE))) {
		return false;
	}

	/* The cue substitutes for seeing a rocket in flight. Do not expose
	 * projectiles behind the camera or outside the current viewport. */
	if ((prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) == 0) {
		return false;
	}

	projectile = weapon->base.projectile;
	if (projectile->ownerprop == g_Vars.currentplayer->prop) {
		return false;
	}

	targetplayer = projectile->targetprop == g_Vars.currentplayer->prop;
	if (projectile->ownerprop) {
		relationship = accessibilityRelationshipClassifyCharacter(
				projectile->ownerprop);
	}
	if (!targetplayer && relationship != ACCESSIBILITY_RELATIONSHIP_HOSTILE
			&& !accessibilityProjectileOwnerIsHostileVehicle(
				projectile->ownerprop)) {
		return false;
	}

	relative.x = prop->pos.x - observer->camera.x;
	relative.y = prop->pos.y - observer->camera.y;
	relative.z = prop->pos.z - observer->camera.z;
	distancesq = relative.x * relative.x + relative.y * relative.y
			+ relative.z * relative.z;
	if (distancesq > range * range) return false;

	viewrooms[0] = observer->room;
	viewrooms[1] = -1;
	if (!accessibilityVisibilityHasVisualLineOfSight(
			(struct coord *)&observer->camera, viewrooms,
			&prop->pos, prop->rooms, prop)) {
		return false;
	}

	candidate->distance = sqrtf(distancesq);
	speedsq = projectile->speed.x * projectile->speed.x
			+ projectile->speed.y * projectile->speed.y
			+ projectile->speed.z * projectile->speed.z;
	dot = relative.x * projectile->speed.x
			+ relative.y * projectile->speed.y
			+ relative.z * projectile->speed.z;
	candidate->closing = candidate->distance > 0.001f
			? -dot / candidate->distance : 0.0f;
	closestdistance = candidate->distance;
	if (speedsq > 0.0001f && dot < 0.0f) {
		struct coord closest;
		predictedticks = -dot / speedsq;
		closest.x = relative.x + projectile->speed.x * predictedticks;
		closest.y = relative.y + projectile->speed.y * predictedticks;
		closest.z = relative.z + projectile->speed.z * predictedticks;
		closestdistance = sqrtf(closest.x * closest.x + closest.y * closest.y
				+ closest.z * closest.z);
	}

	candidate->propnum = accessibilityProjectilePropNum(prop);
	candidate->weapon = weapon;
	candidate->position = prop->pos;
	candidate->closestdistance = closestdistance;
	candidate->closestticks = predictedticks;
	candidate->targetplayer = targetplayer;
	candidate->relationship = relationship;
	/* Approaching rockets whose path comes near the player sort ahead of
	 * equally distant receding or lateral rockets. */
	candidate->priority = candidate->distance;
	if (predictedticks > 0.0f
			&& predictedticks <= ACCESSIBILITY_PROJECTILE_APPROACH_TICKS
			&& closestdistance < ACCESSIBILITY_PROJECTILE_APPROACH_DISTANCE) {
		candidate->priority = closestdistance
				+ predictedticks * 2.0f;
	}
	if (targetplayer) candidate->priority *= 0.75f;
	return candidate->propnum >= 0;
}

static void accessibilityProjectileInsert(
		struct accessibilityprojectilecandidate *candidates, s32 *count,
		const struct accessibilityprojectilecandidate *candidate)
{
	s32 index = *count;
	s32 i;

	if (index < ACCESSIBILITY_PROJECTILE_CANDIDATE_COUNT) {
		(*count)++;
	} else if (candidate->priority < candidates[index - 1].priority) {
		index--;
	} else {
		return;
	}

	for (i = index; i > 0
			&& candidate->priority < candidates[i - 1].priority; i--) {
		if (i < ACCESSIBILITY_PROJECTILE_CANDIDATE_COUNT) {
			candidates[i] = candidates[i - 1];
		}
	}
	candidates[i] = *candidate;
}

static void accessibilityProjectileClearSlot(s32 slot, const char *reason)
{
	if (g_AccessibilityProjectileSlots[slot].assigned) {
		accessibilityLogEvent("projectile_hazard", "slot_release",
				"slot=%d propnum=%d weapon=%p reason=%s",
				slot, g_AccessibilityProjectileSlots[slot].propnum,
				g_AccessibilityProjectileSlots[slot].weapon, reason);
	}
	memset(&g_AccessibilityProjectileSlots[slot], 0,
			sizeof(g_AccessibilityProjectileSlots[slot]));
	g_AccessibilityProjectileSlots[slot].propnum = -1;
	accessibilityToneSetProjectileSlot(slot, false, 1.0f, 0.0f, 0.0f,
			1, 1, true);
}

static void accessibilityProjectileApply(s32 slot,
		const struct accessibilityprojectilecandidate *candidate,
		f32 range, f32 mastervolume, s32 restart)
{
	f32 proximity = 1.0f - candidate->distance / range;
	f32 approach = 0.0f;
	f32 urgency;
	f32 frequency;
	f32 volume;
	f32 pan;
	s32 gamevolume;
	s32 gamepan;
	s32 period;
	s32 duration;

	if (proximity < 0.0f) proximity = 0.0f;
	if (proximity > 1.0f) proximity = 1.0f;
	if (candidate->closestticks > 0.0f
			&& candidate->closestticks <= ACCESSIBILITY_PROJECTILE_APPROACH_TICKS
			&& candidate->closestdistance < ACCESSIBILITY_PROJECTILE_APPROACH_DISTANCE) {
		approach = 1.0f - candidate->closestdistance
				/ ACCESSIBILITY_PROJECTILE_APPROACH_DISTANCE;
		approach *= 1.0f - candidate->closestticks
				/ ACCESSIBILITY_PROJECTILE_APPROACH_TICKS;
	}
	urgency = proximity > approach ? proximity : approach;
	frequency = ACCESSIBILITY_PROJECTILE_FAR_FREQUENCY_HZ
			+ (ACCESSIBILITY_PROJECTILE_NEAR_FREQUENCY_HZ
				- ACCESSIBILITY_PROJECTILE_FAR_FREQUENCY_HZ) * sqrtf(urgency);
	period = ACCESSIBILITY_PROJECTILE_FAR_PERIOD_MS
			- (s32)((ACCESSIBILITY_PROJECTILE_FAR_PERIOD_MS
				- ACCESSIBILITY_PROJECTILE_NEAR_PERIOD_MS) * urgency);
	duration = ACCESSIBILITY_PROJECTILE_FAR_DURATION_MS
			- (s32)((ACCESSIBILITY_PROJECTILE_FAR_DURATION_MS
				- ACCESSIBILITY_PROJECTILE_NEAR_DURATION_MS) * urgency);
	gamevolume = psCalculateVolumeFromDistance(candidate->distance,
			ACCESSIBILITY_PROJECTILE_FULL_DISTANCE, range * 0.65f,
			range, AL_VOL_FULL);
	gamepan = psCalculatePan((struct coord *)&candidate->position,
			ACCESSIBILITY_PROJECTILE_FULL_DISTANCE, range * 0.65f,
			range, candidate->distance, false, NULL);
	volume = (f32)gamevolume / (f32)AL_VOL_FULL * mastervolume;
	pan = ((f32)gamepan - (f32)AL_PAN_CENTER) / (f32)AL_PAN_CENTER;

	accessibilityToneSetProjectileSlot(slot, true, frequency, volume, pan,
			period, duration, restart);

	if (restart || g_AccessibilityProjectileNextLogTick == 0
			|| g_Vars.lvframe60 >= g_AccessibilityProjectileNextLogTick) {
		accessibilityLogEvent("projectile_hazard", restart ? "slot_assign" : "slot_update",
				"slot=%d propnum=%d weapon=%p weaponnum=%d target_player=%d relationship=%d distance=%.3f closing=%.3f closest_distance=%.3f closest_ticks=%.3f priority=%.3f urgency=%.5f frequency_hz=%.3f period_ms=%d duration_ms=%d volume=%.5f pan=%.5f",
				slot, candidate->propnum, candidate->weapon,
				candidate->weapon->weaponnum, candidate->targetplayer,
				candidate->relationship, candidate->distance,
				candidate->closing, candidate->closestdistance,
				candidate->closestticks, candidate->priority, urgency,
				frequency, period, duration, volume, pan);
	}
}

void accessibilityProjectileTick(void)
{
	struct accessibilityobserver observer;
	struct accessibilityprojectilecandidate
			candidates[ACCESSIBILITY_PROJECTILE_CANDIDATE_COUNT];
	s32 candidateused[ACCESSIBILITY_PROJECTILE_CANDIDATE_COUNT];
	s32 slotused[ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT];
	const char *scopereason = accessibilityProjectileScopeReason(&observer);
	struct prop *prop;
	f32 range;
	f32 volume;
	s32 count = 0;
	s32 traversed = 0;
	s32 i;
	s32 slot;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	u64 scanstart;
	u64 scanelapsed;
#endif

	if (scopereason) {
		for (slot = 0; slot < ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT; slot++) {
			if (g_AccessibilityProjectileSlots[slot].assigned) {
				accessibilityProjectileClearSlot(slot, scopereason);
			}
		}
		return;
	}

	accessibilityGetProjectileHazardTuning(&range, &volume);
	memset(candidateused, 0, sizeof(candidateused));
	memset(slotused, 0, sizeof(slotused));
	g_AccessibilityProjectileScanCount++;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	scanstart = sysGetMicroseconds();
#endif
	prop = g_Vars.activeprops;

	while (prop && prop != g_Vars.pausedprops && traversed < g_Vars.maxprops) {
		struct accessibilityprojectilecandidate candidate;

		traversed++;
		memset(&candidate, 0, sizeof(candidate));
		if (accessibilityProjectileEvaluate(prop, &observer, range, &candidate)) {
			accessibilityProjectileInsert(candidates, &count, &candidate);
		}
		prop = prop->next;
	}
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	scanelapsed = sysGetMicroseconds() - scanstart;
	g_AccessibilityProjectileScanTimeTotalUs += scanelapsed;
	g_AccessibilityProjectileScanTimingCount++;
	if (scanelapsed > g_AccessibilityProjectileScanTimeMaxUs) {
		g_AccessibilityProjectileScanTimeMaxUs = scanelapsed;
	}
#endif

	for (slot = 0; slot < ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT; slot++) {
		if (!g_AccessibilityProjectileSlots[slot].assigned) continue;
		for (i = 0; i < count && i < ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT;
				i++) {
			if (!candidateused[i]
					&& candidates[i].propnum
							== g_AccessibilityProjectileSlots[slot].propnum
					&& candidates[i].weapon
							== g_AccessibilityProjectileSlots[slot].weapon) {
				candidateused[i] = true;
				slotused[slot] = true;
				accessibilityProjectileApply(slot, &candidates[i], range,
						volume, false);
				break;
			}
		}
	}

	for (i = 0; i < count && i < ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT; i++) {
		if (candidateused[i]) continue;
		for (slot = 0; slot < ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT; slot++) {
			if (!slotused[slot]) break;
		}
		if (slot >= ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT) break;
		if (g_AccessibilityProjectileSlots[slot].assigned) {
			accessibilityProjectileClearSlot(slot, "replaced_by_higher_priority");
		}
		g_AccessibilityProjectileSlots[slot].assigned = true;
		g_AccessibilityProjectileSlots[slot].propnum = candidates[i].propnum;
		g_AccessibilityProjectileSlots[slot].weapon = candidates[i].weapon;
		g_AccessibilityProjectileAssignmentCount++;
		candidateused[i] = true;
		slotused[slot] = true;
		accessibilityProjectileApply(slot, &candidates[i], range, volume, true);
	}

	for (slot = 0; slot < ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT; slot++) {
		if (g_AccessibilityProjectileSlots[slot].assigned && !slotused[slot]) {
			accessibilityProjectileClearSlot(slot, "no_longer_eligible");
		}
	}

	if (g_AccessibilityProjectileNextLogTick == 0
			|| g_Vars.lvframe60 >= g_AccessibilityProjectileNextLogTick) {
		accessibilityLogEvent("projectile_hazard", "scan",
				"scan=%llu tick=%d traversed=%d eligible=%d voices=%d range=%.3f master_volume=%.4f"
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
				" scan_elapsed_us=%llu scan_average_us=%.3f scan_max_us=%llu scan_timing_count=%llu"
#endif
				,
				(unsigned long long)g_AccessibilityProjectileScanCount,
				g_Vars.lvframe60, traversed, count,
				ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT, range, volume
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
				, (unsigned long long)scanelapsed,
				g_AccessibilityProjectileScanTimingCount > 0
						? (f64)g_AccessibilityProjectileScanTimeTotalUs
							/ (f64)g_AccessibilityProjectileScanTimingCount : 0.0,
				(unsigned long long)g_AccessibilityProjectileScanTimeMaxUs,
				(unsigned long long)g_AccessibilityProjectileScanTimingCount
#endif
				);
		g_AccessibilityProjectileNextLogTick = g_Vars.lvframe60
				+ ACCESSIBILITY_PROJECTILE_LOG_TICKS;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
		g_AccessibilityProjectileScanTimeTotalUs = 0;
		g_AccessibilityProjectileScanTimeMaxUs = 0;
		g_AccessibilityProjectileScanTimingCount = 0;
#endif
	}
}

void accessibilityProjectileReset(const char *reason)
{
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT; slot++) {
		accessibilityProjectileClearSlot(slot, reason ? reason : "reset");
	}
	accessibilityToneStopProjectiles();
	accessibilityLogEvent("projectile_hazard", "reset",
			"reason=%s scans=%llu assignments=%llu voices=%d",
			reason ? reason : "reset",
			(unsigned long long)g_AccessibilityProjectileScanCount,
			(unsigned long long)g_AccessibilityProjectileAssignmentCount,
			ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT);
	g_AccessibilityProjectileScanCount = 0;
	g_AccessibilityProjectileAssignmentCount = 0;
	g_AccessibilityProjectileNextLogTick = 0;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	g_AccessibilityProjectileScanTimeTotalUs = 0;
	g_AccessibilityProjectileScanTimeMaxUs = 0;
	g_AccessibilityProjectileScanTimingCount = 0;
#endif
}
