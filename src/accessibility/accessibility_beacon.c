#include <ultra64.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "system.h"
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/atan2f.h"
#include "game/bg.h"
#include "game/chr.h"
#include "game/chraction.h"
#include "game/lv.h"
#include "game/propobj.h"
#include "game/propsnd.h"
#include "game/sight.h"
#include "lib/collision.h"
#include "lib/vars.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_beacon.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_tone.h"
#ifndef PLATFORM_N64
#include "input.h"
#endif

#define ACCESSIBILITY_BEACON_CAPACITY 64
#define ACCESSIBILITY_BEACON_SCAN_DISTANCE 1200.0f
#define ACCESSIBILITY_BEACON_SCAN_DISTANCE_SQ \
	(ACCESSIBILITY_BEACON_SCAN_DISTANCE * ACCESSIBILITY_BEACON_SCAN_DISTANCE)
#define ACCESSIBILITY_BEACON_FULL_DISTANCE 200.0f
#define ACCESSIBILITY_BEACON_FADE_DISTANCE 1200.0f
#define ACCESSIBILITY_BEACON_SILENT_DISTANCE 1400.0f
#define ACCESSIBILITY_BEACON_DOOR_FREQUENCY_HZ 440.0f
#define ACCESSIBILITY_BEACON_OBJECT_FREQUENCY_HZ 880.0f
#define ACCESSIBILITY_BEACON_PULSE_TICKS TICKS(45)
#define ACCESSIBILITY_BEACON_REFRESH_TICKS TICKS(30)
#define ACCESSIBILITY_BEACON_SWITCH_MARGIN 150.0f
#define ACCESSIBILITY_BEACON_MIN_SLOT_TICKS TICKS(18)
#define ACCESSIBILITY_BEACON_MAX_TARGETS_PER_CATEGORY 3
#define ACCESSIBILITY_BEACON_MAX_SCHEDULE_TARGETS \
	(ACCESSIBILITY_BEACON_MAX_TARGETS_PER_CATEGORY \
			* (ACCESSIBILITY_BEACON_CATEGORY_COUNT - 1))
#define ACCESSIBILITY_BEACON_TELEMETRY_TICKS TICKS(60 * 30)
#define ACCESSIBILITY_BEACON_MAX_DOOR_SIBLINGS 32

enum accessibilitybeaconcategory {
	ACCESSIBILITY_BEACON_CATEGORY_NONE = 0,
	ACCESSIBILITY_BEACON_CATEGORY_OBJECT = 1,
	ACCESSIBILITY_BEACON_CATEGORY_DOOR = 2,
	ACCESSIBILITY_BEACON_CATEGORY_PICKUP = 3,
	ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE = 4,
	ACCESSIBILITY_BEACON_CATEGORY_COUNT = 5,
};

enum accessibilitybeaconkind {
	ACCESSIBILITY_BEACON_KIND_NONE = 0,
	ACCESSIBILITY_BEACON_KIND_OBJECT = 1,
	ACCESSIBILITY_BEACON_KIND_DOOR = 2,
	ACCESSIBILITY_BEACON_KIND_PICKUP = 3,
	ACCESSIBILITY_BEACON_KIND_NON_HOSTILE = 4,
};

struct accessibilitybeaconresult {
	s32 category;
	s32 kind;
	s32 propnum;
	s32 canonicalpropnum;
	void *entity;
	u32 citag;
	f32 distance;
	f32 bearing;
	f32 vertical;
};

static struct accessibilitybeaconresult g_AccessibilityBeaconResults[ACCESSIBILITY_BEACON_CAPACITY];
static s32 g_AccessibilityBeaconResultCount;
static s32 g_AccessibilityBeaconSelectedIndex[ACCESSIBILITY_BEACON_CATEGORY_COUNT]
		= { -1, -1, -1, -1, -1 };
static s32 g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_COUNT];
static s32 g_AccessibilityBeaconNextRefresh60;
static s32 g_AccessibilityBeaconSchedule[ACCESSIBILITY_BEACON_MAX_SCHEDULE_TARGETS];
static s32 g_AccessibilityBeaconScheduleCount;
static s32 g_AccessibilityBeaconScheduleCursor;
static s32 g_AccessibilityBeaconNextScheduledPulse60;
static u64 g_AccessibilityBeaconScanCount;
static u64 g_AccessibilityBeaconPulseCount;
static s32 g_AccessibilityBeaconNextTelemetry60;
static s32 g_AccessibilityBeaconMemoryBaselineValid;
static u64 g_AccessibilityBeaconWorkingSetBaseline;
static u64 g_AccessibilityBeaconPrivateBaseline;

static void accessibilityBeaconResetTelemetry(void)
{
	g_AccessibilityBeaconNextTelemetry60 = 0;
	g_AccessibilityBeaconMemoryBaselineValid = false;
	g_AccessibilityBeaconWorkingSetBaseline = 0;
	g_AccessibilityBeaconPrivateBaseline = 0;
}

static void accessibilityBeaconLogTelemetry(const char *reason)
{
	u64 workingset = 0;
	u64 privatebytes = 0;
	s32 memoryavailable;
	s32 channels = IS4MB() ? 30 : 40;
	s32 inuse = 0;
	s32 stopped = 0;
	s32 i;

	for (i = 0; g_PsChannels && i < channels; i++) {
		if ((g_PsChannels[i].flags & PSFLAG_FREE) == 0) {
			inuse++;

			if (g_PsChannels[i].flags2 & PSFLAG2_STOPPED) {
				stopped++;
			}
		}
	}

	memoryavailable = sysGetProcessMemoryUsage(&workingset, &privatebytes);

	if (memoryavailable && !g_AccessibilityBeaconMemoryBaselineValid) {
		g_AccessibilityBeaconMemoryBaselineValid = true;
		g_AccessibilityBeaconWorkingSetBaseline = workingset;
		g_AccessibilityBeaconPrivateBaseline = privatebytes;
	}

	accessibilityLogEvent("beacon", "telemetry",
			"reason=%s tick=%d scans=%llu pulses=%llu memory_available=%d working_set_bytes=%llu working_set_delta=%lld private_bytes=%llu private_delta=%lld snd_states=%d prop_channels_in_use=%d prop_channels_total=%d prop_channels_stopped=%d procedural_chirp_lane=1 schedule_targets=%d schedule_cursor=%d next_schedule_tick=%d object_active=%d door_active=%d pickup_active=%d non_hostile_active=%d",
			reason, g_Vars.lvframe60,
			(unsigned long long)g_AccessibilityBeaconScanCount,
			(unsigned long long)g_AccessibilityBeaconPulseCount,
			memoryavailable, (unsigned long long)workingset,
			(long long)workingset - (long long)g_AccessibilityBeaconWorkingSetBaseline,
			(unsigned long long)privatebytes,
			(long long)privatebytes - (long long)g_AccessibilityBeaconPrivateBaseline,
			g_SndNumPlaying, inuse, channels, stopped,
			g_AccessibilityBeaconScheduleCount,
			g_AccessibilityBeaconScheduleCursor,
			g_AccessibilityBeaconNextScheduledPulse60,
			g_AccessibilityBeaconCategoryActive[
					ACCESSIBILITY_BEACON_CATEGORY_OBJECT],
			g_AccessibilityBeaconCategoryActive[
					ACCESSIBILITY_BEACON_CATEGORY_DOOR],
			g_AccessibilityBeaconCategoryActive[
					ACCESSIBILITY_BEACON_CATEGORY_PICKUP],
			g_AccessibilityBeaconCategoryActive[
					ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE]);

	g_AccessibilityBeaconNextTelemetry60
			= g_Vars.lvframe60 + ACCESSIBILITY_BEACON_TELEMETRY_TICKS;
}

static const char *accessibilityBeaconCategoryName(s32 category)
{
	switch (category) {
	case ACCESSIBILITY_BEACON_CATEGORY_OBJECT:
		return "interactable_object";
	case ACCESSIBILITY_BEACON_CATEGORY_DOOR:
		return "door";
	case ACCESSIBILITY_BEACON_CATEGORY_PICKUP:
		return "pickup_item";
	case ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE:
		return "non_hostile_character";
	default:
		return "none";
	}
}

static const char *accessibilityBeaconKindName(s32 kind)
{
	switch (kind) {
	case ACCESSIBILITY_BEACON_KIND_OBJECT:
		return "interactable_object";
	case ACCESSIBILITY_BEACON_KIND_DOOR:
		return "door";
	case ACCESSIBILITY_BEACON_KIND_PICKUP:
		return "pickup_item";
	case ACCESSIBILITY_BEACON_KIND_NON_HOSTILE:
		return "non_hostile_character";
	default:
		return "none";
	}
}

static f32 accessibilityBeaconCategoryFrequency(s32 category)
{
	return category == ACCESSIBILITY_BEACON_CATEGORY_DOOR
			|| category == ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE
		? ACCESSIBILITY_BEACON_DOOR_FREQUENCY_HZ
		: ACCESSIBILITY_BEACON_OBJECT_FREQUENCY_HZ;
}

static s32 accessibilityBeaconCharacterCombatCapable(struct chrdata *chr)
{
	return chr && !chrIsDead(chr)
			&& chr->actiontype != ACT_DRUGGEDDROP
			&& chr->actiontype != ACT_DRUGGEDKO;
}

static s32 accessibilityBeaconNonHostileEligible(struct prop *prop,
		const char **reason)
{
	struct chrdata *chr;

	if (!prop || prop->type != PROPTYPE_CHR || !prop->chr) {
		*reason = "not_character_category";
		return false;
	}

	if (!g_Vars.currentplayer || !g_Vars.currentplayer->prop
			|| !g_Vars.currentplayer->prop->chr
			|| prop == g_Vars.currentplayer->prop) {
		*reason = "player_or_relationship_unavailable";
		return false;
	}

	chr = prop->chr;

	if (!prop->active || (prop->flags & PROPFLAG_ENABLED) == 0) {
		*reason = "character_inactive_or_disabled";
		return false;
	}

	if (!accessibilityBeaconCharacterCombatCapable(chr)) {
		*reason = "dead_dying_or_knocked_out";
		return false;
	}

	if (chr->chrflags & CHRCFLAG_HIDDEN) {
		*reason = "character_hidden";
		return false;
	}

	if (chr->hidden & CHRHFLAG_UNTARGETABLE) {
		*reason = "character_untargetable";
		return false;
	}

	if ((chr->hidden & CHRHFLAG_CLOAKED) && !USINGDEVICE(DEVICE_IRSCANNER)) {
		*reason = "character_cloaked";
		return false;
	}

	if (chrCompareTeams(g_Vars.currentplayer->prop->chr, chr, COMPARE_ENEMIES)) {
		*reason = "character_hostile";
		return false;
	}

	*reason = sightIsPropFriendly(prop)
			? "friendly_character" : "neutral_character";
	return true;
}

static s32 accessibilityBeaconPropNum(const struct prop *prop)
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

	if (address < first || address >= end || (address - first) % sizeof(struct prop) != 0) {
		return -1;
	}

	return (s32)((address - first) / sizeof(struct prop));
}

static s32 accessibilityBeaconRoomValid(RoomNum room)
{
	return room > 0 && room < g_Vars.roomcount;
}

static s32 accessibilityBeaconRoomsRelated(RoomNum *playerrooms, RoomNum *targetrooms)
{
	s32 i;
	s32 j;

	if (!playerrooms || !targetrooms) {
		return false;
	}

	for (i = 0; i < 8 && playerrooms[i] != -1; i++) {
		if (!accessibilityBeaconRoomValid(playerrooms[i])) {
			continue;
		}

		for (j = 0; j < 8 && targetrooms[j] != -1; j++) {
			if (!accessibilityBeaconRoomValid(targetrooms[j])) {
				continue;
			}

			if (playerrooms[i] == targetrooms[j]
					|| bgRoomsAreNeighbours(playerrooms[i], targetrooms[j])) {
				return true;
			}
		}
	}

	return false;
}

static s32 accessibilityBeaconObjectEligible(struct prop *prop, u32 *citag, const char **reason)
{
	struct defaultobj *obj;
	u32 tag;

	if (!prop || (prop->type != PROPTYPE_OBJ && prop->type != PROPTYPE_WEAPON)) {
		*reason = "not_object_category";
		return false;
	}

	obj = prop->obj;

	if (!obj || obj->prop != prop) {
		*reason = "invalid_object_backlink";
		return false;
	}

	if (!prop->active || (obj->hidden & (OBJHFLAG_DELETING | OBJHFLAG_GONE))) {
		*reason = "object_inactive_or_hidden";
		return false;
	}

	if (!objIsHealthy(obj)) {
		*reason = "object_destroyed";
		return false;
	}

	if (obj->flags & (OBJFLAG_CANNOT_ACTIVATE | OBJFLAG_DEACTIVATED)) {
		*reason = "object_activation_disabled";
		return false;
	}

	tag = propobjGetCiTagId(prop);

	if (!tag && !(obj->flags3 & (OBJFLAG3_HTMTERMINAL | OBJFLAG3_INTERACTABLE))) {
		*reason = "object_not_deliberately_interactable";
		return false;
	}

	*citag = tag;
	*reason = tag ? "ci_tag" : "interaction_flag";
	return true;
}

static s32 accessibilityBeaconPickupEligible(struct prop *prop, const char **reason)
{
	struct defaultobj *obj;

	if (!prop || (prop->type != PROPTYPE_OBJ && prop->type != PROPTYPE_WEAPON)) {
		*reason = "not_pickup_prop_type";
		return false;
	}

	obj = prop->obj;

	if (!obj || obj->prop != prop) {
		*reason = "invalid_pickup_backlink";
		return false;
	}

	if (!prop->active || (obj->hidden & (OBJHFLAG_DELETING | OBJHFLAG_GONE))) {
		*reason = "pickup_inactive_or_hidden";
		return false;
	}

	if (obj->flags2 & OBJFLAG2_INVISIBLE) {
		*reason = "pickup_invisible";
		return false;
	}

	if (obj->flags & OBJFLAG_THROWNLAPTOP) {
		*reason = "manual_pickup";
		return true;
	}

	if (func0f085194(obj) && obj->type != OBJTYPE_HAT) {
		if (obj->flags & OBJFLAG_UNCOLLECTABLE) {
			*reason = "pickup_uncollectable";
			return false;
		}
	} else if ((obj->flags & OBJFLAG_COLLECTABLE) == 0) {
		*reason = "pickup_not_collectable";
		return false;
	}

	if ((obj->hidden & OBJHFLAG_PROJECTILE) && obj->projectile->pickuptimer240 > 0) {
		if (obj->projectile->pickupby == NULL) {
			if (obj->projectile->bouncecount == 0) {
				*reason = "pickup_projectile_in_flight";
				return false;
			}
		} else if (obj->projectile->pickupby != g_Vars.currentplayer->prop) {
			*reason = "pickup_reserved_for_other_character";
			return false;
		}
	}

	*reason = "collectable_item";
	return true;
}

static s32 accessibilityBeaconDoorEligible(struct prop *prop, const char **reason)
{
	struct doorobj *door;

	if (!prop || prop->type != PROPTYPE_DOOR) {
		*reason = "not_door_category";
		return false;
	}

	door = prop->door;

	if (!door || door->base.prop != prop) {
		*reason = "invalid_door_backlink";
		return false;
	}

	if (!prop->active || (door->base.hidden & (OBJHFLAG_DELETING | OBJHFLAG_GONE))) {
		*reason = "door_inactive_or_hidden";
		return false;
	}

	if (!objIsHealthy(&door->base)) {
		*reason = "door_destroyed";
		return false;
	}

	/*
	 * Door setup data commonly carries OBJFLAG_DEACTIVATED even though the
	 * normal door interaction path still permits it. Match doorTestForInteract's
	 * persistent state checks here rather than applying the object-only flag.
	 */
	if ((door->base.flags & OBJFLAG_CANNOT_ACTIVATE) || door->maxfrac <= 0.0f) {
		*reason = "door_activation_disabled";
		return false;
	}

	*reason = "usable_door";
	return true;
}

static s32 accessibilityBeaconRequiresLineOfSight(s32 category)
{
	return category == ACCESSIBILITY_BEACON_CATEGORY_DOOR
			|| category == ACCESSIBILITY_BEACON_CATEGORY_OBJECT
			|| category == ACCESSIBILITY_BEACON_CATEGORY_PICKUP
			|| category == ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE;
}

static s32 accessibilityBeaconHasLineOfSight(
		const struct accessibilitybeaconresult *result,
		struct prop *playerprop, struct prop *targetprop)
{
	if (result->kind == ACCESSIBILITY_BEACON_KIND_NON_HOSTILE) {
		struct coord targetpos;
		RoomNum camrooms[2];

		if (!targetprop->chr) {
			return false;
		}

		targetpos = targetprop->pos;
		targetpos.y = targetprop->chr->manground
				+ targetprop->chr->height * 0.5f;
		camrooms[0] = g_Vars.currentplayer->cam_room;
		camrooms[1] = -1;

		return cdTestLos03(&g_Vars.currentplayer->cam_pos, camrooms,
				&targetpos,
				CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_BG,
				GEOFLAG_BLOCK_SHOOT);
	}

	if (result->kind == ACCESSIBILITY_BEACON_KIND_PICKUP) {
		return cdTestLos05(&playerprop->pos, playerprop->rooms,
				&targetprop->pos, targetprop->rooms,
				CDTYPE_DOORS | CDTYPE_BG,
				GEOFLAG_WALL | GEOFLAG_BLOCK_SIGHT | GEOFLAG_BLOCK_SHOOT);
	}

	return cdTestLos06(&playerprop->pos, playerprop->rooms,
			&targetprop->pos, targetprop->rooms, CDTYPE_BG);
}

static struct prop *accessibilityBeaconCanonicalDoor(struct prop *prop, s32 *siblingcount)
{
	struct doorobj *door = prop->door;
	struct doorobj *sibling;
	struct prop *bestprop = prop;
	s32 bestnum = accessibilityBeaconPropNum(prop);
	s32 count = 1;
	s32 guard = 0;

	if (!door) {
		*siblingcount = 0;
		return prop;
	}

	sibling = door->sibling;

	while (sibling && sibling != door && guard++ < ACCESSIBILITY_BEACON_MAX_DOOR_SIBLINGS) {
		struct prop *siblingprop = sibling->base.prop;
		s32 siblingnum = accessibilityBeaconPropNum(siblingprop);

		count++;

		if (siblingnum >= 0 && (bestnum < 0 || siblingnum < bestnum)) {
			bestnum = siblingnum;
			bestprop = siblingprop;
		}

		sibling = sibling->sibling;
	}

	*siblingcount = count;
	return bestprop;
}

static f32 accessibilityBeaconCalculateSpatial(struct prop *prop, struct prop *playerprop,
		f32 *bearing, f32 *vertical)
{
	f32 x = prop->pos.x - playerprop->pos.x;
	f32 y = prop->pos.y - playerprop->pos.y;
	f32 z = prop->pos.z - playerprop->pos.z;
	f32 absolute = atan2f(x, z) * (180.0f / M_PI);
	f32 forward = 360.0f - g_Vars.currentplayer->vv_theta;
	f32 relative = absolute - forward;

	while (relative > 180.0f) {
		relative -= 360.0f;
	}

	while (relative < -180.0f) {
		relative += 360.0f;
	}

	*bearing = relative;
	*vertical = y;
	return sqrtf(x * x + y * y + z * z);
}

static s32 accessibilityBeaconResultCompare(const void *avalue, const void *bvalue)
{
	const struct accessibilitybeaconresult *a = avalue;
	const struct accessibilitybeaconresult *b = bvalue;

	if (a->distance < b->distance) {
		return -1;
	}

	if (a->distance > b->distance) {
		return 1;
	}

	if (a->category != b->category) {
		return a->category < b->category ? -1 : 1;
	}

	if (a->kind != b->kind) {
		return a->kind < b->kind ? -1 : 1;
	}

	if (a->canonicalpropnum != b->canonicalpropnum) {
		return a->canonicalpropnum < b->canonicalpropnum ? -1 : 1;
	}

	return 0;
}

static s32 accessibilityBeaconFindCanonical(s32 category, s32 canonicalpropnum)
{
	s32 i;

	for (i = 0; i < g_AccessibilityBeaconResultCount; i++) {
		if (g_AccessibilityBeaconResults[i].category == category
				&& g_AccessibilityBeaconResults[i].canonicalpropnum == canonicalpropnum) {
			return i;
		}
	}

	return -1;
}

static void accessibilityBeaconStoreResult(struct accessibilitybeaconresult *result)
{
	s32 i;
	s32 farthest = -1;

	if (g_AccessibilityBeaconResultCount < ACCESSIBILITY_BEACON_CAPACITY) {
		g_AccessibilityBeaconResults[g_AccessibilityBeaconResultCount++] = *result;
		return;
	}

	for (i = 0; i < g_AccessibilityBeaconResultCount; i++) {
		if (farthest < 0
				|| g_AccessibilityBeaconResults[i].distance
					> g_AccessibilityBeaconResults[farthest].distance) {
			farthest = i;
		}
	}

	if (farthest >= 0 && result->distance < g_AccessibilityBeaconResults[farthest].distance) {
		g_AccessibilityBeaconResults[farthest] = *result;
	}
}

static void accessibilityBeaconLogProp(struct prop *prop, s32 propnum, const char *decision,
		const char *reason, s32 canonicalpropnum, s32 siblingcount,
		f32 distance, f32 bearing, f32 vertical, u32 citag)
{
	struct defaultobj *obj = NULL;
	struct doorobj *door = NULL;
	struct chrdata *chr = NULL;
	s32 unlocked = -1;

	if (prop && (prop->type == PROPTYPE_OBJ || prop->type == PROPTYPE_WEAPON
			|| prop->type == PROPTYPE_DOOR)) {
		obj = prop->obj;
	}

	if (prop && prop->type == PROPTYPE_DOOR && prop->door) {
		door = prop->door;

		if (g_Vars.currentplayer && g_Vars.currentplayer->prop) {
			unlocked = doorIsUnlocked(g_Vars.currentplayer->prop, prop);
		}
	}

	if (prop && prop->type == PROPTYPE_CHR) {
		chr = prop->chr;
	}

	accessibilityLogEvent("beacon", "candidate",
			"scan=%llu prop=%p propnum=%d type=%d active=%d prop_flags=0x%02x entity=%p model=%d obj_type=%d obj_flags=0x%08x obj_flags2=0x%08x obj_flags3=0x%08x hidden=0x%08x damage=%d maxdamage=%d chr=%p chr_team=%d chr_action=%d chr_flags=0x%08x chr_hidden=0x%08x door_mode=%d door_frac=%.3f door_maxfrac=%.3f door_flags=0x%04x door_keyflags=0x%08x door_unlocked=%d rooms=%d,%d,%d,%d,%d,%d,%d,%d decision=%s reason=%s canonical_propnum=%d sibling_count=%d distance=%.3f bearing=%.3f vertical=%.3f ci_tag=0x%02x",
			(unsigned long long)g_AccessibilityBeaconScanCount,
			(void *)prop, propnum, prop ? prop->type : -1, prop ? prop->active : 0,
			prop ? prop->flags : 0, (void *)obj,
			obj ? obj->modelnum : -1, obj ? obj->type : -1,
			obj ? obj->flags : 0, obj ? obj->flags2 : 0,
			obj ? obj->flags3 : 0, obj ? obj->hidden : 0,
			obj ? obj->damage : -1, obj ? obj->maxdamage : -1,
			(void *)chr, chr ? chr->team : -1, chr ? chr->actiontype : -1,
			chr ? chr->chrflags : 0, chr ? chr->hidden : 0,
			door ? door->mode : -1, door ? door->frac : -1.0f,
			door ? door->maxfrac : -1.0f, door ? door->doorflags : 0,
			door ? door->keyflags : 0, unlocked,
			prop ? prop->rooms[0] : -1, prop ? prop->rooms[1] : -1,
			prop ? prop->rooms[2] : -1, prop ? prop->rooms[3] : -1,
			prop ? prop->rooms[4] : -1, prop ? prop->rooms[5] : -1,
			prop ? prop->rooms[6] : -1, prop ? prop->rooms[7] : -1,
			decision, reason, canonicalpropnum, siblingcount,
			distance, bearing, vertical, citag);
}

static s32 accessibilityBeaconScan(s32 detailed)
{
	struct prop *playerprop = g_Vars.currentplayer->prop;
	struct prop *prop = g_Vars.activeprops;
	s32 traversed = 0;
	s32 considered;
	s32 eligiblecount = 0;

	g_AccessibilityBeaconScanCount++;
	g_AccessibilityBeaconResultCount = 0;
	g_AccessibilityBeaconSelectedIndex[ACCESSIBILITY_BEACON_CATEGORY_OBJECT] = -1;
	g_AccessibilityBeaconSelectedIndex[ACCESSIBILITY_BEACON_CATEGORY_DOOR] = -1;
	g_AccessibilityBeaconSelectedIndex[ACCESSIBILITY_BEACON_CATEGORY_PICKUP] = -1;
	g_AccessibilityBeaconSelectedIndex[ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE] = -1;
	memset(g_AccessibilityBeaconResults, 0, sizeof(g_AccessibilityBeaconResults));

	accessibilityLogEvent("beacon", "scan_start",
			"scan=%llu mode=%s stage=%d player=%d player_prop=%p player_propnum=%d position=%.3f,%.3f,%.3f theta=%.3f rooms=%d,%d,%d,%d,%d,%d,%d,%d radius=%.1f capacity=%d",
			(unsigned long long)g_AccessibilityBeaconScanCount,
			detailed ? "toggle" : "automatic_refresh",
			g_Vars.stagenum, g_Vars.currentplayernum, (void *)playerprop,
			accessibilityBeaconPropNum(playerprop), playerprop->pos.x, playerprop->pos.y,
			playerprop->pos.z, g_Vars.currentplayer->vv_theta,
			playerprop->rooms[0], playerprop->rooms[1], playerprop->rooms[2], playerprop->rooms[3],
			playerprop->rooms[4], playerprop->rooms[5], playerprop->rooms[6], playerprop->rooms[7],
			ACCESSIBILITY_BEACON_SCAN_DISTANCE, ACCESSIBILITY_BEACON_CAPACITY);

	while (prop && prop != g_Vars.pausedprops && traversed <= g_Vars.maxprops) {
		struct prop *candidate = prop;
		struct accessibilitybeaconresult result;
		const char *reason = "unsupported_prop_type";
		s32 propnum = accessibilityBeaconPropNum(prop);
		s32 siblingcount = 0;
		s32 canonicalpropnum = propnum;
		s32 eligible = false;
		s32 duplicate;
		u32 citag = 0;
		f32 distance = -1.0f;
		f32 bearing = 0.0f;
		f32 vertical = 0.0f;

		traversed++;
		memset(&result, 0, sizeof(result));

		if (prop->type == PROPTYPE_DOOR
				&& g_Vars.stagenum == STAGE_CITRAINING
				&& g_AccessibilityBeaconCategoryActive[
						ACCESSIBILITY_BEACON_CATEGORY_DOOR]) {
			eligible = accessibilityBeaconDoorEligible(prop, &reason);

			if (eligible) {
				candidate = accessibilityBeaconCanonicalDoor(prop, &siblingcount);
				canonicalpropnum = accessibilityBeaconPropNum(candidate);

				if (!accessibilityBeaconDoorEligible(candidate, &reason)) {
					candidate = prop;
					canonicalpropnum = propnum;
				}
			}

			result.category = ACCESSIBILITY_BEACON_CATEGORY_DOOR;
			result.kind = ACCESSIBILITY_BEACON_KIND_DOOR;
		} else if ((prop->type == PROPTYPE_OBJ || prop->type == PROPTYPE_WEAPON)
				&& g_Vars.stagenum == STAGE_CITRAINING
				&& (g_AccessibilityBeaconCategoryActive[
						ACCESSIBILITY_BEACON_CATEGORY_OBJECT]
					|| g_AccessibilityBeaconCategoryActive[
						ACCESSIBILITY_BEACON_CATEGORY_PICKUP])) {
			eligible = accessibilityBeaconPickupEligible(prop, &reason);

			if (eligible) {
				if (g_AccessibilityBeaconCategoryActive[
						ACCESSIBILITY_BEACON_CATEGORY_PICKUP]) {
					result.kind = ACCESSIBILITY_BEACON_KIND_PICKUP;
					result.category = ACCESSIBILITY_BEACON_CATEGORY_PICKUP;
				} else {
					eligible = false;
					reason = "pickup_category_inactive";
				}
			} else {
				if (g_AccessibilityBeaconCategoryActive[
						ACCESSIBILITY_BEACON_CATEGORY_OBJECT]) {
					eligible = accessibilityBeaconObjectEligible(
							prop, &citag, &reason);
					result.kind = ACCESSIBILITY_BEACON_KIND_OBJECT;
					result.category = ACCESSIBILITY_BEACON_CATEGORY_OBJECT;
				}
			}
		} else if (prop->type == PROPTYPE_CHR
				&& g_AccessibilityBeaconCategoryActive[
						ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE]) {
			eligible = accessibilityBeaconNonHostileEligible(prop, &reason);
			result.kind = ACCESSIBILITY_BEACON_KIND_NON_HOSTILE;
			result.category = ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE;
		}

		if (eligible) {
			distance = accessibilityBeaconCalculateSpatial(candidate, playerprop, &bearing, &vertical);

			if (distance * distance > ACCESSIBILITY_BEACON_SCAN_DISTANCE_SQ) {
				eligible = false;
				reason = "outside_scan_radius";
			} else if (!accessibilityBeaconRoomsRelated(playerprop->rooms, candidate->rooms)) {
				eligible = false;
				reason = "outside_room_boundary";
			} else if (accessibilityBeaconRequiresLineOfSight(result.category)
					&& !accessibilityBeaconHasLineOfSight(
						&result, playerprop, candidate)) {
				eligible = false;
				reason = "line_of_sight_blocked";
			}
		}

		if (eligible) {
			duplicate = accessibilityBeaconFindCanonical(result.category, canonicalpropnum);

			if (duplicate >= 0) {
				if (detailed) {
					accessibilityBeaconLogProp(prop, propnum, "excluded", "duplicate_canonical_door",
							canonicalpropnum, siblingcount, distance, bearing, vertical, citag);
				}
				prop = prop->next;
				continue;
			}

			result.propnum = accessibilityBeaconPropNum(candidate);
			result.canonicalpropnum = canonicalpropnum;
			result.entity = result.kind == ACCESSIBILITY_BEACON_KIND_NON_HOSTILE
					? (void *)candidate->chr : (void *)candidate->obj;
			result.citag = citag;
			result.distance = distance;
			result.bearing = bearing;
			result.vertical = vertical;
			eligiblecount++;
			accessibilityBeaconStoreResult(&result);
			if (detailed) {
				accessibilityBeaconLogProp(prop, propnum, "included", reason,
						canonicalpropnum, siblingcount, distance, bearing, vertical, citag);
			}
		} else {
			if (detailed) {
				accessibilityBeaconLogProp(prop, propnum, "excluded", reason,
						canonicalpropnum, siblingcount, distance, bearing, vertical, citag);
			}
		}

		prop = prop->next;
	}

	if (traversed > g_Vars.maxprops) {
		accessibilityLogEvent("beacon", "scan_guard",
				"scan=%llu traversed=%d maxprops=%d reason=active_list_did_not_terminate",
				(unsigned long long)g_AccessibilityBeaconScanCount, traversed, g_Vars.maxprops);
	}

	considered = traversed;
	qsort(g_AccessibilityBeaconResults, g_AccessibilityBeaconResultCount,
			sizeof(g_AccessibilityBeaconResults[0]), accessibilityBeaconResultCompare);

	for (traversed = 0; detailed && traversed < g_AccessibilityBeaconResultCount; traversed++) {
		struct accessibilitybeaconresult *item = &g_AccessibilityBeaconResults[traversed];

		accessibilityLogEvent("beacon", "scan_result",
				"scan=%llu index=%d category=%s kind=%s propnum=%d canonical_propnum=%d entity=%p distance=%.3f bearing=%.3f vertical=%.3f ci_tag=0x%02x",
				(unsigned long long)g_AccessibilityBeaconScanCount, traversed,
				accessibilityBeaconCategoryName(item->category),
				accessibilityBeaconKindName(item->kind), item->propnum,
				item->canonicalpropnum, item->entity, item->distance,
				item->bearing, item->vertical, item->citag);
	}

	accessibilityLogEvent("beacon", "scan_complete",
			"scan=%llu mode=%s traversed=%d eligible=%d stored=%d truncated=%d",
			(unsigned long long)g_AccessibilityBeaconScanCount,
			detailed ? "toggle" : "automatic_refresh", considered,
			eligiblecount, g_AccessibilityBeaconResultCount,
			eligiblecount > g_AccessibilityBeaconResultCount);

	return g_AccessibilityBeaconResultCount;
}

static struct prop *accessibilityBeaconValidateResult(struct accessibilitybeaconresult *result,
		const char **reason)
{
	struct prop *prop;
	struct prop *playerprop = g_Vars.currentplayer->prop;
	u32 citag = 0;
	f32 bearing;
	f32 vertical;
	f32 distance;

	if (result->propnum < 0 || result->propnum >= g_Vars.maxprops || !g_Vars.props) {
		*reason = "prop_index_invalid";
		return NULL;
	}

	prop = &g_Vars.props[result->propnum];

	if ((result->kind == ACCESSIBILITY_BEACON_KIND_NON_HOSTILE
				&& prop->chr != result->entity)
			|| (result->kind != ACCESSIBILITY_BEACON_KIND_NON_HOSTILE
				&& prop->obj != result->entity)) {
		*reason = "entity_identity_changed";
		return NULL;
	}

	if (result->kind == ACCESSIBILITY_BEACON_KIND_NON_HOSTILE) {
		if (!accessibilityBeaconNonHostileEligible(prop, reason)) {
			return NULL;
		}
	} else if (result->kind == ACCESSIBILITY_BEACON_KIND_PICKUP) {
		if (!accessibilityBeaconPickupEligible(prop, reason)) {
			return NULL;
		}
	} else if (result->category == ACCESSIBILITY_BEACON_CATEGORY_OBJECT) {
		if (!accessibilityBeaconObjectEligible(prop, &citag, reason)) {
			return NULL;
		}
	} else if (result->category == ACCESSIBILITY_BEACON_CATEGORY_DOOR) {
		if (!accessibilityBeaconDoorEligible(prop, reason)) {
			return NULL;
		}
	} else {
		*reason = "category_invalid";
		return NULL;
	}

	distance = accessibilityBeaconCalculateSpatial(prop, playerprop, &bearing, &vertical);

	if (distance * distance > ACCESSIBILITY_BEACON_SCAN_DISTANCE_SQ) {
		*reason = "moved_outside_scan_radius";
		return NULL;
	}

	if (!accessibilityBeaconRoomsRelated(playerprop->rooms, prop->rooms)) {
		*reason = "moved_outside_room_boundary";
		return NULL;
	}

	if (accessibilityBeaconRequiresLineOfSight(result->category)
			&& !accessibilityBeaconHasLineOfSight(result, playerprop, prop)) {
		*reason = "line_of_sight_became_blocked";
		return NULL;
	}

	result->distance = distance;
	result->bearing = bearing;
	result->vertical = vertical;
	*reason = "valid";
	return prop;
}

static s32 accessibilityBeaconAnyActive(void)
{
	return g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_OBJECT]
			|| g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_DOOR]
			|| g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_PICKUP]
			|| g_AccessibilityBeaconCategoryActive[
					ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE];
}

static void accessibilityBeaconStopSound(s32 category,
		struct accessibilitybeaconresult *result, const char *reason)
{
	if (result) {
		accessibilityToneStopChirp();
		accessibilityLogEvent("beacon", "sound_stop",
				"lane=procedural_chirp propnum=%d category=%s kind=%s frequency_hz=%.1f reason=%s",
				result->propnum,
				accessibilityBeaconCategoryName(category),
				accessibilityBeaconKindName(result->kind),
				accessibilityBeaconCategoryFrequency(category), reason);
	}
}

static void accessibilityBeaconClearSelection(s32 category, const char *reason)
{
	struct accessibilitybeaconresult *selected = NULL;
	s32 index = g_AccessibilityBeaconSelectedIndex[category];

	if (index >= 0 && index < g_AccessibilityBeaconResultCount) {
		selected = &g_AccessibilityBeaconResults[index];
	}

	accessibilityBeaconStopSound(category, selected, reason);
	g_AccessibilityBeaconSelectedIndex[category] = -1;
}

static void accessibilityBeaconDeactivateCategory(s32 category, const char *reason)
{
	s32 wasactive = g_AccessibilityBeaconCategoryActive[category];
	s32 selected = g_AccessibilityBeaconSelectedIndex[category];

	accessibilityBeaconClearSelection(category, reason);

	if (wasactive || selected >= 0) {
		accessibilityLogEvent("beacon", "deactivate",
				"category=%s reason=%s selected=%d results=%d",
				accessibilityBeaconCategoryName(category), reason, selected,
				g_AccessibilityBeaconResultCount);
	}

	g_AccessibilityBeaconCategoryActive[category] = false;
}

static void accessibilityBeaconDeactivateAll(const char *reason, s32 clearresults)
{
	accessibilityBeaconDeactivateCategory(ACCESSIBILITY_BEACON_CATEGORY_OBJECT, reason);
	accessibilityBeaconDeactivateCategory(ACCESSIBILITY_BEACON_CATEGORY_DOOR, reason);
	accessibilityBeaconDeactivateCategory(ACCESSIBILITY_BEACON_CATEGORY_PICKUP, reason);
	accessibilityBeaconDeactivateCategory(
			ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE, reason);

	if (clearresults) {
		g_AccessibilityBeaconResultCount = 0;
		memset(g_AccessibilityBeaconResults, 0, sizeof(g_AccessibilityBeaconResults));
		memset(g_AccessibilityBeaconSchedule, -1,
				sizeof(g_AccessibilityBeaconSchedule));
		g_AccessibilityBeaconScheduleCount = 0;
		g_AccessibilityBeaconScheduleCursor = 0;
		g_AccessibilityBeaconNextScheduledPulse60 = 0;
		g_AccessibilityBeaconNextRefresh60 = 0;
		accessibilityBeaconResetTelemetry();
	}
}

static s32 accessibilityBeaconSelect(s32 category, s32 index, const char *reason)
{
	struct accessibilitybeaconresult *previous = NULL;
	struct accessibilitybeaconresult *selected;
	const char *validreason;

	if (index < 0 || index >= g_AccessibilityBeaconResultCount) {
		return false;
	}

	selected = &g_AccessibilityBeaconResults[index];

	if (selected->category != category) {
		return false;
	}

	if (g_AccessibilityBeaconSelectedIndex[category] >= 0
			&& g_AccessibilityBeaconSelectedIndex[category] < g_AccessibilityBeaconResultCount) {
		previous = &g_AccessibilityBeaconResults[g_AccessibilityBeaconSelectedIndex[category]];
	}

	if (previous) {
		accessibilityBeaconStopSound(category, previous, "selection_changed");
	}

	if (!accessibilityBeaconValidateResult(selected, &validreason)) {
		accessibilityLogEvent("beacon", "selection_rejected",
				"index=%d category=%s propnum=%d reason=%s",
				index, accessibilityBeaconCategoryName(selected->category),
				selected->propnum, validreason);
		return false;
	}

	g_AccessibilityBeaconSelectedIndex[category] = index;
	accessibilityLogEvent("beacon", "selection",
			"index=%d count=%d category=%s kind=%s propnum=%d canonical_propnum=%d entity=%p distance=%.3f bearing=%.3f vertical=%.3f frequency_hz=%.1f reason=%s",
			index, g_AccessibilityBeaconResultCount,
			accessibilityBeaconCategoryName(selected->category),
			accessibilityBeaconKindName(selected->kind), selected->propnum,
			selected->canonicalpropnum, selected->entity, selected->distance,
			selected->bearing, selected->vertical,
			accessibilityBeaconCategoryFrequency(selected->category), reason);
	return true;
}

static s32 accessibilityBeaconFindResultIdentity(
		const struct accessibilitybeaconresult *identity)
{
	s32 i;

	if (!identity) {
		return -1;
	}

	for (i = 0; i < g_AccessibilityBeaconResultCount; i++) {
		struct accessibilitybeaconresult *result = &g_AccessibilityBeaconResults[i];

		if (result->category == identity->category
				&& result->kind == identity->kind
				&& result->canonicalpropnum == identity->canonicalpropnum
				&& result->entity == identity->entity) {
			return i;
		}
	}

	return -1;
}

static s32 accessibilityBeaconScheduleContains(s32 *indices, s32 count, s32 index)
{
	s32 i;

	for (i = 0; i < count; i++) {
		if (indices[i] == index) {
			return true;
		}
	}

	return false;
}

static s32 accessibilityBeaconScheduleSlotTicks(void)
{
	s32 ticks;

	if (g_AccessibilityBeaconScheduleCount <= 0) {
		return ACCESSIBILITY_BEACON_PULSE_TICKS;
	}

	ticks = ACCESSIBILITY_BEACON_PULSE_TICKS / g_AccessibilityBeaconScheduleCount;

	if (ticks < ACCESSIBILITY_BEACON_MIN_SLOT_TICKS) {
		ticks = ACCESSIBILITY_BEACON_MIN_SLOT_TICKS;
	}

	return ticks;
}

static void accessibilityBeaconBuildSchedule(
		struct accessibilitybeaconresult *previous, s32 previouscount,
		struct accessibilitybeaconresult *nexttarget, s32 preservenext,
		const char *reason)
{
	s32 chosen[ACCESSIBILITY_BEACON_CATEGORY_COUNT][ACCESSIBILITY_BEACON_MAX_TARGETS_PER_CATEGORY];
	s32 chosencount[ACCESSIBILITY_BEACON_CATEGORY_COUNT] = { 0 };
	s32 category;
	s32 rank;
	s32 i;
	s32 j;
	s32 changed = false;

	memset(chosen, -1, sizeof(chosen));
	memset(g_AccessibilityBeaconSchedule, -1, sizeof(g_AccessibilityBeaconSchedule));
	g_AccessibilityBeaconScheduleCount = 0;

	for (category = ACCESSIBILITY_BEACON_CATEGORY_OBJECT;
			category < ACCESSIBILITY_BEACON_CATEGORY_COUNT; category++) {
		if (!g_AccessibilityBeaconCategoryActive[category]) {
			continue;
		}

		for (i = 0; i < previouscount
				&& chosencount[category] < ACCESSIBILITY_BEACON_MAX_TARGETS_PER_CATEGORY; i++) {
			s32 index;

			if (previous[i].category != category) {
				continue;
			}

			index = accessibilityBeaconFindResultIdentity(&previous[i]);

			if (index >= 0 && !accessibilityBeaconScheduleContains(
					chosen[category], chosencount[category], index)) {
				chosen[category][chosencount[category]++] = index;
			}
		}

		for (i = 0; i < g_AccessibilityBeaconResultCount
				&& chosencount[category] < ACCESSIBILITY_BEACON_MAX_TARGETS_PER_CATEGORY; i++) {
			if (g_AccessibilityBeaconResults[i].category == category
					&& !accessibilityBeaconScheduleContains(
						chosen[category], chosencount[category], i)) {
				chosen[category][chosencount[category]++] = i;
			}
		}

		for (i = 0; i < g_AccessibilityBeaconResultCount; i++) {
			s32 farthest = -1;

			if (g_AccessibilityBeaconResults[i].category != category
					|| accessibilityBeaconScheduleContains(
						chosen[category], chosencount[category], i)) {
				continue;
			}

			for (j = 0; j < chosencount[category]; j++) {
				if (farthest < 0
						|| g_AccessibilityBeaconResults[chosen[category][j]].distance
								> g_AccessibilityBeaconResults[chosen[category][farthest]].distance) {
					farthest = j;
				}
			}

			if (farthest >= 0
					&& g_AccessibilityBeaconResults[i].distance
							+ ACCESSIBILITY_BEACON_SWITCH_MARGIN
						< g_AccessibilityBeaconResults[chosen[category][farthest]].distance) {
				chosen[category][farthest] = i;
			}
		}

		for (i = 0; i < chosencount[category]; i++) {
			for (j = i + 1; j < chosencount[category]; j++) {
				if (g_AccessibilityBeaconResults[chosen[category][j]].distance
						< g_AccessibilityBeaconResults[chosen[category][i]].distance) {
					s32 temp = chosen[category][i];
					chosen[category][i] = chosen[category][j];
					chosen[category][j] = temp;
				}
			}
		}
	}

	for (rank = 0; rank < ACCESSIBILITY_BEACON_MAX_TARGETS_PER_CATEGORY; rank++) {
		for (category = ACCESSIBILITY_BEACON_CATEGORY_OBJECT;
				category < ACCESSIBILITY_BEACON_CATEGORY_COUNT; category++) {
			if (rank < chosencount[category]) {
				g_AccessibilityBeaconSchedule[g_AccessibilityBeaconScheduleCount++]
						= chosen[category][rank];
			}
		}
	}

	if (preservenext && nexttarget) {
		g_AccessibilityBeaconScheduleCursor = 0;

		for (i = 0; i < g_AccessibilityBeaconScheduleCount; i++) {
			struct accessibilitybeaconresult *result
					= &g_AccessibilityBeaconResults[g_AccessibilityBeaconSchedule[i]];

			if (result->category == nexttarget->category
					&& result->kind == nexttarget->kind
					&& result->canonicalpropnum == nexttarget->canonicalpropnum
					&& result->entity == nexttarget->entity) {
				g_AccessibilityBeaconScheduleCursor = i;
				break;
			}
		}
	} else {
		g_AccessibilityBeaconScheduleCursor = 0;
	}

	if (previouscount != g_AccessibilityBeaconScheduleCount) {
		changed = true;
	} else {
		for (i = 0; i < previouscount; i++) {
			struct accessibilitybeaconresult *result
					= &g_AccessibilityBeaconResults[g_AccessibilityBeaconSchedule[i]];

			if (previous[i].category != result->category
					|| previous[i].kind != result->kind
					|| previous[i].canonicalpropnum != result->canonicalpropnum
					|| previous[i].entity != result->entity) {
				changed = true;
				break;
			}
		}
	}

	accessibilityLogEvent("beacon", "schedule",
			"reason=%s changed=%d targets=%d cursor=%d slot_ticks=%d base_ticks=%d min_slot_ticks=%d per_category_cap=%d",
			reason, changed, g_AccessibilityBeaconScheduleCount,
			g_AccessibilityBeaconScheduleCursor, accessibilityBeaconScheduleSlotTicks(),
			ACCESSIBILITY_BEACON_PULSE_TICKS, ACCESSIBILITY_BEACON_MIN_SLOT_TICKS,
			ACCESSIBILITY_BEACON_MAX_TARGETS_PER_CATEGORY);

	if (changed) {
		for (i = 0; i < g_AccessibilityBeaconScheduleCount; i++) {
			struct accessibilitybeaconresult *result
					= &g_AccessibilityBeaconResults[g_AccessibilityBeaconSchedule[i]];

			accessibilityLogEvent("beacon", "schedule_target",
					"slot=%d category=%s kind=%s propnum=%d canonical_propnum=%d distance=%.3f entity=%p",
					i, accessibilityBeaconCategoryName(result->category),
					accessibilityBeaconKindName(result->kind), result->propnum,
					result->canonicalpropnum,
					result->distance, result->entity);
		}
	}
}

static void accessibilityBeaconRefreshActive(void)
{
	struct accessibilitybeaconresult previous[ACCESSIBILITY_BEACON_MAX_SCHEDULE_TARGETS];
	struct accessibilitybeaconresult playing[ACCESSIBILITY_BEACON_CATEGORY_COUNT];
	struct accessibilitybeaconresult nexttarget;
	s32 playingvalid[ACCESSIBILITY_BEACON_CATEGORY_COUNT] = { false };
	s32 previouscount = 0;
	s32 nextvalid = false;
	s32 category;
	s32 i;

	for (category = ACCESSIBILITY_BEACON_CATEGORY_OBJECT;
			category < ACCESSIBILITY_BEACON_CATEGORY_COUNT; category++) {
		s32 index = g_AccessibilityBeaconSelectedIndex[category];

		if (index >= 0 && index < g_AccessibilityBeaconResultCount) {
			playing[category] = g_AccessibilityBeaconResults[index];
			playingvalid[category] = true;
		}
	}

	for (i = 0; i < g_AccessibilityBeaconScheduleCount; i++) {
		s32 index = g_AccessibilityBeaconSchedule[i];

		if (index >= 0 && index < g_AccessibilityBeaconResultCount) {
			previous[previouscount++] = g_AccessibilityBeaconResults[index];
		}
	}

	if (g_AccessibilityBeaconScheduleCount > 0
			&& g_AccessibilityBeaconScheduleCursor >= 0
			&& g_AccessibilityBeaconScheduleCursor < g_AccessibilityBeaconScheduleCount) {
		nexttarget = g_AccessibilityBeaconResults[
				g_AccessibilityBeaconSchedule[g_AccessibilityBeaconScheduleCursor]];
		nextvalid = true;
	}

	accessibilityBeaconScan(false);

	for (category = ACCESSIBILITY_BEACON_CATEGORY_OBJECT;
			category < ACCESSIBILITY_BEACON_CATEGORY_COUNT; category++) {
		if (playingvalid[category]) {
			s32 index = accessibilityBeaconFindResultIdentity(&playing[category]);

			if (index >= 0 && g_AccessibilityBeaconCategoryActive[category]) {
				g_AccessibilityBeaconSelectedIndex[category] = index;
			} else {
				accessibilityBeaconStopSound(category, &playing[category],
						"automatic_target_removed");
			}
		}
	}

	accessibilityBeaconBuildSchedule(previous, previouscount,
			nextvalid ? &nexttarget : NULL, nextvalid, "automatic_refresh");

	if (g_AccessibilityBeaconScheduleCount > 0
			&& g_AccessibilityBeaconNextScheduledPulse60 == 0) {
		g_AccessibilityBeaconNextScheduledPulse60 = g_Vars.lvframe60;
	}

	g_AccessibilityBeaconNextRefresh60
			= g_Vars.lvframe60 + ACCESSIBILITY_BEACON_REFRESH_TICKS;
}

static void accessibilityBeaconRescanActive(const char *reason)
{
	accessibilityBeaconClearSelection(ACCESSIBILITY_BEACON_CATEGORY_OBJECT, "rescan");
	accessibilityBeaconClearSelection(ACCESSIBILITY_BEACON_CATEGORY_DOOR, "rescan");
	accessibilityBeaconClearSelection(ACCESSIBILITY_BEACON_CATEGORY_PICKUP, "rescan");
	accessibilityBeaconClearSelection(
			ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE, "rescan");
	accessibilityBeaconScan(true);
	accessibilityBeaconBuildSchedule(NULL, 0, NULL, false, reason);
	g_AccessibilityBeaconNextScheduledPulse60 = g_Vars.lvframe60;
	g_AccessibilityBeaconNextRefresh60
			= g_Vars.lvframe60 + ACCESSIBILITY_BEACON_REFRESH_TICKS;
}

static void accessibilityBeaconPulse(s32 category,
		struct accessibilitybeaconresult *selected, struct prop *prop)
{
	f32 frequencyhz = accessibilityBeaconCategoryFrequency(selected->category);
	s32 volume = psCalculateVolumeFromDistance(selected->distance,
			ACCESSIBILITY_BEACON_FULL_DISTANCE,
			ACCESSIBILITY_BEACON_FADE_DISTANCE,
			ACCESSIBILITY_BEACON_SILENT_DISTANCE, AL_VOL_FULL);
	s32 pan = psCalculatePan(&prop->pos,
			ACCESSIBILITY_BEACON_FULL_DISTANCE,
			ACCESSIBILITY_BEACON_FADE_DISTANCE,
			ACCESSIBILITY_BEACON_SILENT_DISTANCE,
			selected->distance, false, NULL);
	f32 normalizedvolume = (f32)volume / (f32)AL_VOL_FULL;
	f32 normalizedpan = ((f32)pan - (f32)AL_PAN_CENTER)
			/ (f32)AL_PAN_CENTER;
	s32 pulses = selected->kind == ACCESSIBILITY_BEACON_KIND_PICKUP ? 3
			: selected->kind == ACCESSIBILITY_BEACON_KIND_NON_HOSTILE ? 2 : 1;

	accessibilityTonePlayChirpPattern(frequencyhz, normalizedvolume,
			normalizedpan, pulses);
	g_AccessibilityBeaconPulseCount++;

	accessibilityLogEvent("beacon", "pulse",
			"pulse=%llu tick=%d next_tick=%d index=%d category=%s kind=%s pulse_count=%d frequency_hz=%.1f lane=procedural_chirp prop=%p propnum=%d position=%.3f,%.3f,%.3f distance=%.3f bearing=%.3f vertical=%.3f volume=%d normalized_volume=%.4f pan=%d normalized_pan=%.4f ranges=%.1f,%.1f,%.1f result=started",
			(unsigned long long)g_AccessibilityBeaconPulseCount,
			g_Vars.lvframe60, g_AccessibilityBeaconNextScheduledPulse60,
			g_AccessibilityBeaconSelectedIndex[category],
			accessibilityBeaconCategoryName(selected->category),
			accessibilityBeaconKindName(selected->kind), pulses, frequencyhz,
			(void *)prop, selected->propnum, prop->pos.x, prop->pos.y, prop->pos.z,
			selected->distance, selected->bearing, selected->vertical,
			volume, normalizedvolume, pan, normalizedpan,
			ACCESSIBILITY_BEACON_FULL_DISTANCE, ACCESSIBILITY_BEACON_FADE_DISTANCE,
			ACCESSIBILITY_BEACON_SILENT_DISTANCE);
}

static void accessibilityBeaconPlayScheduledPulse(void)
{
	s32 attempts = g_AccessibilityBeaconScheduleCount;

	while (attempts-- > 0 && g_AccessibilityBeaconScheduleCount > 0) {
		s32 slot = g_AccessibilityBeaconScheduleCursor;
		s32 index = g_AccessibilityBeaconSchedule[slot];
		struct accessibilitybeaconresult *selected;
		struct prop *prop;
		const char *validreason;
		s32 category;
		s32 slot_ticks = accessibilityBeaconScheduleSlotTicks();

		g_AccessibilityBeaconScheduleCursor
				= (g_AccessibilityBeaconScheduleCursor + 1)
						% g_AccessibilityBeaconScheduleCount;

		if (index < 0 || index >= g_AccessibilityBeaconResultCount) {
			accessibilityLogEvent("beacon", "schedule_skip",
					"slot=%d index=%d reason=result_index_invalid", slot, index);
			continue;
		}

		selected = &g_AccessibilityBeaconResults[index];
		category = selected->category;
		prop = accessibilityBeaconValidateResult(selected, &validreason);

		if (!prop || !g_AccessibilityBeaconCategoryActive[category]) {
			accessibilityLogEvent("beacon", "schedule_skip",
					"slot=%d index=%d category=%s propnum=%d reason=%s",
					slot, index, accessibilityBeaconCategoryName(category),
					selected->propnum,
					prop ? "category_inactive" : validreason);
			g_AccessibilityBeaconNextRefresh60 = g_Vars.lvframe60;
			continue;
		}

		accessibilityBeaconClearSelection(
				ACCESSIBILITY_BEACON_CATEGORY_OBJECT, "round_robin_advance");
		accessibilityBeaconClearSelection(
				ACCESSIBILITY_BEACON_CATEGORY_DOOR, "round_robin_advance");
		accessibilityBeaconClearSelection(
				ACCESSIBILITY_BEACON_CATEGORY_PICKUP, "round_robin_advance");
		accessibilityBeaconClearSelection(
				ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE,
				"round_robin_advance");

		if (!accessibilityBeaconSelect(category, index, "round_robin")) {
			continue;
		}

		g_AccessibilityBeaconNextScheduledPulse60
				= g_Vars.lvframe60 + slot_ticks;
		accessibilityBeaconPulse(category, selected, prop);
		accessibilityLogEvent("beacon", "schedule_pulse",
				"slot=%d next_cursor=%d targets=%d slot_ticks=%d category=%s kind=%s propnum=%d frequency_hz=%.1f lane=procedural_chirp next_tick=%d",
				slot, g_AccessibilityBeaconScheduleCursor,
				g_AccessibilityBeaconScheduleCount, slot_ticks,
				accessibilityBeaconCategoryName(category),
				accessibilityBeaconKindName(selected->kind), selected->propnum,
				accessibilityBeaconCategoryFrequency(category),
				g_AccessibilityBeaconNextScheduledPulse60);
		return;
	}

	g_AccessibilityBeaconNextScheduledPulse60
			= g_Vars.lvframe60 + ACCESSIBILITY_BEACON_MIN_SLOT_TICKS;
}

static const char *accessibilityBeaconScopeReason(void)
{
	if (!accessibilityIsInteractableBeaconsEnabled()
			&& !accessibilityIsNonHostileBeaconsEnabled()) {
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

void accessibilityBeaconTick(void)
{
	const char *scopereason = accessibilityBeaconScopeReason();
	s32 objectrequested = false;
	s32 doorrequested = false;
	s32 nonhostilerequested = false;
	s32 pickuprequested = false;
	s32 toggled = false;

	if (scopereason) {
		if (accessibilityBeaconAnyActive() || g_AccessibilityBeaconResultCount) {
			accessibilityBeaconDeactivateAll(scopereason, true);
		}
		return;
	}

	if ((!accessibilityIsInteractableBeaconsEnabled()
			|| g_Vars.stagenum != STAGE_CITRAINING)
			&& (g_AccessibilityBeaconCategoryActive[
					ACCESSIBILITY_BEACON_CATEGORY_OBJECT]
				|| g_AccessibilityBeaconCategoryActive[
					ACCESSIBILITY_BEACON_CATEGORY_DOOR]
				|| g_AccessibilityBeaconCategoryActive[
					ACCESSIBILITY_BEACON_CATEGORY_PICKUP])) {
		accessibilityBeaconDeactivateCategory(
				ACCESSIBILITY_BEACON_CATEGORY_OBJECT, "category_scope_lost");
		accessibilityBeaconDeactivateCategory(
				ACCESSIBILITY_BEACON_CATEGORY_DOOR, "category_scope_lost");
		accessibilityBeaconDeactivateCategory(
				ACCESSIBILITY_BEACON_CATEGORY_PICKUP, "category_scope_lost");
		toggled = true;
	}

	if (!accessibilityIsNonHostileBeaconsEnabled()
			&& g_AccessibilityBeaconCategoryActive[
					ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE]) {
		accessibilityBeaconDeactivateCategory(
				ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE,
				"category_scope_lost");
		toggled = true;
	}

#ifndef PLATFORM_N64
	if (accessibilityIsInteractableBeaconsEnabled()
			&& g_Vars.stagenum == STAGE_CITRAINING) {
		objectrequested = inputKeyJustPressed(VK_F5);
		doorrequested = inputKeyJustPressed(VK_F6);
		pickuprequested = inputKeyJustPressed(VK_F8);
	}
	if (accessibilityIsNonHostileBeaconsEnabled()) {
		nonhostilerequested = inputKeyJustPressed(VK_F7);
	}
#endif

	if (objectrequested) {
		accessibilityLogEvent("beacon", "command",
				"action=toggle key=F5 category=interactable_object active=%d results=%d selected=%d tick=%d",
				g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_OBJECT],
				g_AccessibilityBeaconResultCount,
				g_AccessibilityBeaconSelectedIndex[ACCESSIBILITY_BEACON_CATEGORY_OBJECT],
				g_Vars.lvframe60);

		if (g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_OBJECT]) {
			accessibilityBeaconDeactivateCategory(
					ACCESSIBILITY_BEACON_CATEGORY_OBJECT, "user_toggle");
		} else {
			g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_OBJECT] = true;
		}

		toggled = true;
	}

	if (doorrequested) {
		accessibilityLogEvent("beacon", "command",
				"action=toggle key=F6 category=door active=%d results=%d selected=%d tick=%d",
				g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_DOOR],
				g_AccessibilityBeaconResultCount,
				g_AccessibilityBeaconSelectedIndex[ACCESSIBILITY_BEACON_CATEGORY_DOOR],
				g_Vars.lvframe60);

		if (g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_DOOR]) {
			accessibilityBeaconDeactivateCategory(
					ACCESSIBILITY_BEACON_CATEGORY_DOOR, "user_toggle");
		} else {
			g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_DOOR] = true;
		}

		toggled = true;
	}

	if (pickuprequested) {
		accessibilityLogEvent("beacon", "command",
				"action=toggle key=F8 category=pickup_item active=%d results=%d selected=%d tick=%d",
				g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_PICKUP],
				g_AccessibilityBeaconResultCount,
				g_AccessibilityBeaconSelectedIndex[ACCESSIBILITY_BEACON_CATEGORY_PICKUP],
				g_Vars.lvframe60);

		if (g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_PICKUP]) {
			accessibilityBeaconDeactivateCategory(
					ACCESSIBILITY_BEACON_CATEGORY_PICKUP, "user_toggle");
		} else {
			g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_PICKUP] = true;
		}

		toggled = true;
	}

	if (nonhostilerequested) {
		accessibilityLogEvent("beacon", "command",
				"action=toggle key=F7 category=non_hostile_character active=%d results=%d selected=%d tick=%d",
				g_AccessibilityBeaconCategoryActive[
						ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE],
				g_AccessibilityBeaconResultCount,
				g_AccessibilityBeaconSelectedIndex[
						ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE],
				g_Vars.lvframe60);

		if (g_AccessibilityBeaconCategoryActive[
				ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE]) {
			accessibilityBeaconDeactivateCategory(
					ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE,
					"user_toggle");
		} else {
			g_AccessibilityBeaconCategoryActive[
					ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE] = true;
		}

		toggled = true;
	}

	if (toggled) {
		accessibilityLogEvent("beacon", "category_state",
				"object_active=%d door_active=%d pickup_active=%d non_hostile_active=%d tick=%d",
				g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_OBJECT],
				g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_DOOR],
				g_AccessibilityBeaconCategoryActive[ACCESSIBILITY_BEACON_CATEGORY_PICKUP],
				g_AccessibilityBeaconCategoryActive[
						ACCESSIBILITY_BEACON_CATEGORY_NON_HOSTILE],
				g_Vars.lvframe60);

		if (accessibilityBeaconAnyActive()) {
			accessibilityBeaconRescanActive("category_toggle");
		} else {
			g_AccessibilityBeaconResultCount = 0;
			memset(g_AccessibilityBeaconResults, 0, sizeof(g_AccessibilityBeaconResults));
			memset(g_AccessibilityBeaconSchedule, -1,
					sizeof(g_AccessibilityBeaconSchedule));
			g_AccessibilityBeaconScheduleCount = 0;
			g_AccessibilityBeaconScheduleCursor = 0;
			g_AccessibilityBeaconNextScheduledPulse60 = 0;
			g_AccessibilityBeaconNextRefresh60 = 0;
			accessibilityBeaconResetTelemetry();
		}
	}

	if (accessibilityBeaconAnyActive() && g_Vars.lvupdate60 > 0
			&& g_Vars.lvframe60 >= g_AccessibilityBeaconNextRefresh60) {
		accessibilityBeaconRefreshActive();
	}

	if (accessibilityBeaconAnyActive() && g_Vars.lvupdate60 > 0
			&& g_AccessibilityBeaconScheduleCount > 0
			&& g_Vars.lvframe60 >= g_AccessibilityBeaconNextScheduledPulse60) {
		accessibilityBeaconPlayScheduledPulse();
	}

	if (accessibilityBeaconAnyActive() && g_Vars.lvupdate60 > 0
			&& (g_AccessibilityBeaconNextTelemetry60 == 0
				|| g_Vars.lvframe60 >= g_AccessibilityBeaconNextTelemetry60)) {
		accessibilityBeaconLogTelemetry(toggled ? "category_toggle" : "periodic");
	}
}

void accessibilityBeaconReset(const char *reason)
{
	accessibilityBeaconDeactivateAll(reason ? reason : "reset", true);

	accessibilityLogEvent("beacon", "reset",
			"reason=%s scans=%llu pulses=%llu interactable_enabled=%d non_hostile_enabled=%d radius=%.1f base_cadence_ticks=%d refresh_ticks=%d min_slot_ticks=%d per_category_cap=%d object_frequency_hz=%.1f pickup_pulses=3 door_frequency_hz=%.1f non_hostile_pulses=2 non_hostile_frequency_hz=%.1f lane=procedural_chirp",
			reason ? reason : "reset",
			(unsigned long long)g_AccessibilityBeaconScanCount,
			(unsigned long long)g_AccessibilityBeaconPulseCount,
			accessibilityIsInteractableBeaconsEnabled(),
			accessibilityIsNonHostileBeaconsEnabled(),
			ACCESSIBILITY_BEACON_SCAN_DISTANCE, ACCESSIBILITY_BEACON_PULSE_TICKS,
			ACCESSIBILITY_BEACON_REFRESH_TICKS, ACCESSIBILITY_BEACON_MIN_SLOT_TICKS,
			ACCESSIBILITY_BEACON_MAX_TARGETS_PER_CATEGORY,
			ACCESSIBILITY_BEACON_OBJECT_FREQUENCY_HZ,
			ACCESSIBILITY_BEACON_DOOR_FREQUENCY_HZ,
			ACCESSIBILITY_BEACON_DOOR_FREQUENCY_HZ);

	g_AccessibilityBeaconScanCount = 0;
	g_AccessibilityBeaconPulseCount = 0;
	g_AccessibilityBeaconScheduleCount = 0;
	g_AccessibilityBeaconScheduleCursor = 0;
	g_AccessibilityBeaconNextScheduledPulse60 = 0;
	g_AccessibilityBeaconNextRefresh60 = 0;
	accessibilityBeaconResetTelemetry();
}
