#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include "platform.h"
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "system.h"
#include "game/inv.h"
#include "game/game_0b0fd0.h"
#include "game/lang.h"
#include "game/lv.h"
#include "game/objectives.h"
#include "game/player.h"
#include "game/propobj.h"
#include "game/setuputils.h"
#include "lib/collision.h"
#include "lib/joy.h"
#include "lib/vars.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_beacon.h"
#include "accessibility/accessibility_incident.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_observer.h"
#ifndef PLATFORM_N64
#include "input.h"
#endif

#define ACCESSIBILITY_INCIDENT_SAMPLE_TICKS TICKS(15)
#define ACCESSIBILITY_INCIDENT_HISTORY_SAMPLES 60
#define ACCESSIBILITY_INCIDENT_PROP_RANGE 12000.0f
#define ACCESSIBILITY_INCIDENT_PROP_RANGE_SQ \
	(ACCESSIBILITY_INCIDENT_PROP_RANGE * ACCESSIBILITY_INCIDENT_PROP_RANGE)
#define ACCESSIBILITY_INCIDENT_OBJECTIVE_COMMAND_LIMIT 64
#define ACCESSIBILITY_INCIDENT_INVENTORY_LIMIT 64

#define ACCESSIBILITY_INCIDENT_KEY_W       0x00000001
#define ACCESSIBILITY_INCIDENT_KEY_A       0x00000002
#define ACCESSIBILITY_INCIDENT_KEY_S       0x00000004
#define ACCESSIBILITY_INCIDENT_KEY_D       0x00000008
#define ACCESSIBILITY_INCIDENT_KEY_Q       0x00000010
#define ACCESSIBILITY_INCIDENT_KEY_E       0x00000020
#define ACCESSIBILITY_INCIDENT_KEY_R       0x00000040
#define ACCESSIBILITY_INCIDENT_KEY_F       0x00000080
#define ACCESSIBILITY_INCIDENT_KEY_SPACE   0x00000100
#define ACCESSIBILITY_INCIDENT_MOUSE_LEFT  0x00000200
#define ACCESSIBILITY_INCIDENT_MOUSE_RIGHT 0x00000400

struct accessibilityincidentframe {
	u64 timestampus;
	s32 stage;
	s32 tick;
	s32 playernum;
	s32 tickmode;
	s32 observervalid;
	s32 observerremote;
	struct coord origin;
	struct coord camera;
	struct coord look;
	RoomNum room;
	u32 keymask;
	u32 modifiers;
	s8 stickx[4];
	s8 sticky[4];
	u32 buttons[4];
	s32 menucount;
	s32 paused;
	s32 cutscene;
	s32 cameramode;
	s32 dead;
};

static struct accessibilityincidentframe
		g_AccessibilityIncidentHistory[ACCESSIBILITY_INCIDENT_HISTORY_SAMPLES];
static s32 g_AccessibilityIncidentHistoryCount;
static s32 g_AccessibilityIncidentHistoryWrite;
static s32 g_AccessibilityIncidentNextSampleTick;
static s32 g_AccessibilityIncidentStage = -1;
static u64 g_AccessibilityIncidentCaptureId;

static struct defaultobj *accessibilityIncidentGetObj(struct prop *prop)
{
	if (prop && (prop->type == PROPTYPE_OBJ
			|| prop->type == PROPTYPE_WEAPON
			|| prop->type == PROPTYPE_DOOR)) {
		return prop->obj;
	}

	return NULL;
}

static s32 accessibilityIncidentPropNum(struct prop *prop)
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

static const char *accessibilityIncidentObjectiveStatusName(s32 status)
{
	switch (status) {
	case OBJECTIVE_COMPLETE:
		return "complete";
	case OBJECTIVE_FAILED:
		return "failed";
	default:
		return "incomplete";
	}
}

static const char *accessibilityIncidentRequirementName(s32 type)
{
	switch (type) {
	case OBJECTIVETYPE_DESTROYOBJ:
		return "destroy_object";
	case OBJECTIVETYPE_COMPFLAGS:
		return "complete_flags";
	case OBJECTIVETYPE_FAILFLAGS:
		return "fail_flags";
	case OBJECTIVETYPE_COLLECTOBJ:
		return "collect_object";
	case OBJECTIVETYPE_THROWOBJ:
		return "throw_object";
	case OBJECTIVETYPE_HOLOGRAPH:
		return "holograph_object";
	case OBJECTIVETYPE_ENTERROOM:
		return "enter_room";
	case OBJECTIVETYPE_THROWINROOM:
		return "throw_in_room";
	default:
		return "other";
	}
}

static u32 accessibilityIncidentReadKeyMask(void)
{
	u32 mask = 0;

#ifndef PLATFORM_N64
	if (inputKeyPressed(VK_A + ('W' - 'A'))) mask |= ACCESSIBILITY_INCIDENT_KEY_W;
	if (inputKeyPressed(VK_A)) mask |= ACCESSIBILITY_INCIDENT_KEY_A;
	if (inputKeyPressed(VK_A + ('S' - 'A'))) mask |= ACCESSIBILITY_INCIDENT_KEY_S;
	if (inputKeyPressed(VK_A + ('D' - 'A'))) mask |= ACCESSIBILITY_INCIDENT_KEY_D;
	if (inputKeyPressed(VK_A + ('Q' - 'A'))) mask |= ACCESSIBILITY_INCIDENT_KEY_Q;
	if (inputKeyPressed(VK_A + ('E' - 'A'))) mask |= ACCESSIBILITY_INCIDENT_KEY_E;
	if (inputKeyPressed(VK_A + ('R' - 'A'))) mask |= ACCESSIBILITY_INCIDENT_KEY_R;
	if (inputKeyPressed(VK_A + ('F' - 'A'))) mask |= ACCESSIBILITY_INCIDENT_KEY_F;
	if (inputKeyPressed(VK_SPACE)) mask |= ACCESSIBILITY_INCIDENT_KEY_SPACE;
	if (inputKeyPressed(VK_MOUSE_LEFT)) mask |= ACCESSIBILITY_INCIDENT_MOUSE_LEFT;
	if (inputKeyPressed(VK_MOUSE_RIGHT)) mask |= ACCESSIBILITY_INCIDENT_MOUSE_RIGHT;
#endif

	return mask;
}

static void accessibilityIncidentRecordFrame(void)
{
	struct accessibilityincidentframe *frame
			= &g_AccessibilityIncidentHistory[g_AccessibilityIncidentHistoryWrite];
	struct accessibilityobserver observer;
	s32 i;

	memset(frame, 0, sizeof(*frame));
	frame->timestampus = sysGetMicroseconds();
	frame->stage = g_Vars.stagenum;
	frame->tick = g_Vars.lvframe60;
	frame->playernum = g_Vars.currentplayernum;
	frame->tickmode = g_Vars.tickmode;
	frame->observervalid = accessibilityObserverGet(&observer);

	if (frame->observervalid) {
		frame->observerremote = observer.isremote;
		frame->origin = observer.origin;
		frame->camera = observer.camera;
		frame->look = observer.look;
		frame->room = observer.room;
	}

	frame->keymask = accessibilityIncidentReadKeyMask();
#ifndef PLATFORM_N64
	frame->modifiers = inputGetKeyModState();
#endif

	for (i = 0; i < 4; i++) {
		frame->stickx[i] = joyGetStickX(i);
		frame->sticky[i] = joyGetStickY(i);
		frame->buttons[i] = joyGetButtons(i, 0xffffffff);
	}

	frame->menucount = g_MenuData.count;
	frame->paused = lvIsPaused();
	frame->cutscene = g_Vars.in_cutscene;

	if (g_Vars.currentplayer) {
		frame->cameramode = g_Vars.currentplayer->cameramode;
		frame->dead = g_Vars.currentplayer->isdead;
	}

	g_AccessibilityIncidentHistoryWrite
			= (g_AccessibilityIncidentHistoryWrite + 1)
			% ACCESSIBILITY_INCIDENT_HISTORY_SAMPLES;

	if (g_AccessibilityIncidentHistoryCount
			< ACCESSIBILITY_INCIDENT_HISTORY_SAMPLES) {
		g_AccessibilityIncidentHistoryCount++;
	}
}

static void accessibilityIncidentDumpHistory(u64 captureid)
{
	s32 first = (g_AccessibilityIncidentHistoryWrite
			- g_AccessibilityIncidentHistoryCount
			+ ACCESSIBILITY_INCIDENT_HISTORY_SAMPLES)
			% ACCESSIBILITY_INCIDENT_HISTORY_SAMPLES;
	s32 i;

	for (i = 0; i < g_AccessibilityIncidentHistoryCount; i++) {
		struct accessibilityincidentframe *frame
				= &g_AccessibilityIncidentHistory[
						(first + i) % ACCESSIBILITY_INCIDENT_HISTORY_SAMPLES];

		accessibilityLogEvent("incident", "history",
				"capture=%llu index=%d age_samples=%d t_us=%llu stage=%d tick=%d player=%d tickmode=%d observer_valid=%d observer_remote=%d origin=%.3f,%.3f,%.3f camera=%.3f,%.3f,%.3f look=%.6f,%.6f,%.6f room=%d keymask=0x%08x modifiers=0x%04x controller0=%d,%d,0x%08x controller1=%d,%d,0x%08x controller2=%d,%d,0x%08x controller3=%d,%d,0x%08x menus=%d paused=%d cutscene=%d camera_mode=%d dead=%d",
				(unsigned long long)captureid, i,
				g_AccessibilityIncidentHistoryCount - 1 - i,
				(unsigned long long)frame->timestampus,
				frame->stage, frame->tick, frame->playernum,
				frame->tickmode, frame->observervalid,
				frame->observerremote,
				frame->origin.x, frame->origin.y, frame->origin.z,
				frame->camera.x, frame->camera.y, frame->camera.z,
				frame->look.x, frame->look.y, frame->look.z,
				frame->room, frame->keymask, frame->modifiers,
				frame->stickx[0], frame->sticky[0], frame->buttons[0],
				frame->stickx[1], frame->sticky[1], frame->buttons[1],
				frame->stickx[2], frame->sticky[2], frame->buttons[2],
				frame->stickx[3], frame->sticky[3], frame->buttons[3],
				frame->menucount, frame->paused, frame->cutscene,
				frame->cameramode, frame->dead);
	}
}

static void accessibilityIncidentDumpPlayer(u64 captureid)
{
	struct player *player = g_Vars.currentplayer;
	struct accessibilityobserver observer;
	s32 observervalid = accessibilityObserverGet(&observer);
	s32 i;

	accessibilityLogEvent("incident", "state",
			"capture=%llu stage=%d difficulty=%d tick=%d frame=%d update60=%d player=%d player_count=%d tickmode=%d menus=%d paused=%d cutscene=%d mplayer=%d normal_mplayer=%d observer_valid=%d observer_remote=%d observer_prop=%p observer_propnum=%d origin=%.3f,%.3f,%.3f camera=%.3f,%.3f,%.3f look=%.6f,%.6f,%.6f observer_room=%d player_prop=%p player_propnum=%d player_rooms=%d,%d,%d,%d,%d,%d,%d,%d camera_mode=%d dead=%d health=%.5f shield=%.5f devices_active=0x%08x devices_inhibit=0x%08x weapon=%d weapon_previous=%d weapon_pending=%d weapon_function_inverted=%d inventory_count=%d current_inventory_index=%u cane_mode=%d radar_contact_alerts=%d",
			(unsigned long long)captureid,
			g_Vars.stagenum, lvGetDifficulty(), g_Vars.lvframe60,
			g_Vars.lvframenum, g_Vars.lvupdate60,
			g_Vars.currentplayernum, PLAYERCOUNT(), g_Vars.tickmode,
			g_MenuData.count, lvIsPaused(), g_Vars.in_cutscene,
			g_Vars.mplayerisrunning, g_Vars.normmplayerisrunning,
			observervalid, observervalid ? observer.isremote : 0,
			observervalid ? (void *)observer.prop : NULL,
			observervalid ? accessibilityIncidentPropNum(observer.prop) : -1,
			observervalid ? observer.origin.x : 0.0f,
			observervalid ? observer.origin.y : 0.0f,
			observervalid ? observer.origin.z : 0.0f,
			observervalid ? observer.camera.x : 0.0f,
			observervalid ? observer.camera.y : 0.0f,
			observervalid ? observer.camera.z : 0.0f,
			observervalid ? observer.look.x : 0.0f,
			observervalid ? observer.look.y : 0.0f,
			observervalid ? observer.look.z : 0.0f,
			observervalid ? observer.room : -1,
			player ? (void *)player->prop : NULL,
			player ? accessibilityIncidentPropNum(player->prop) : -1,
			player && player->prop ? player->prop->rooms[0] : -1,
			player && player->prop ? player->prop->rooms[1] : -1,
			player && player->prop ? player->prop->rooms[2] : -1,
			player && player->prop ? player->prop->rooms[3] : -1,
			player && player->prop ? player->prop->rooms[4] : -1,
			player && player->prop ? player->prop->rooms[5] : -1,
			player && player->prop ? player->prop->rooms[6] : -1,
			player && player->prop ? player->prop->rooms[7] : -1,
			player ? player->cameramode : -1,
			player ? player->isdead : 0,
			player ? player->bondhealth : 0.0f,
			player ? playerGetShieldFrac() : 0.0f,
			player ? player->devicesactive : 0,
			player ? player->devicesinhibit : 0,
			player ? player->gunctrl.weaponnum : -1,
			player ? player->gunctrl.prevweaponnum : -1,
			player ? player->gunctrl.switchtoweaponnum : -1,
			player ? player->gunctrl.invertgunfunc : 0,
			player ? invGetCount() : 0,
			player ? invGetCurrentIndex() : 0,
			accessibilityGetVirtualCaneMode(),
			accessibilityGetCombatRadarContactAlerts());

	for (i = 0; player && i < ARRAYCOUNT(player->trackedprops); i++) {
		struct trackedprop *tracked = &player->trackedprops[i];
		struct prop *prop = tracked->prop;
		struct defaultobj *obj = prop
				&& (prop->type == PROPTYPE_OBJ
					|| prop->type == PROPTYPE_WEAPON
					|| prop->type == PROPTYPE_DOOR)
				? prop->obj : NULL;

		accessibilityLogEvent("incident", "threat_detector_state",
				"capture=%llu active=%d sight_track_type=%d slot=%d occupied=%d prop=%p propnum=%d prop_type=%d active_prop=%d onscreen=%d object=%p object_type=%d model=%d weapon=%d bounds=%d,%d,%d,%d",
				(unsigned long long)captureid,
				gsetHasFunctionFlags(
					&player->hands[HAND_RIGHT].gset,
					FUNCFLAG_THREATDETECTOR),
				player->sighttracktype, i, prop != NULL, (void *)prop,
				accessibilityIncidentPropNum(prop),
				prop ? prop->type : -1, prop ? prop->active : 0,
				prop ? (prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) != 0 : 0,
				(void *)obj, obj ? obj->type : -1,
				obj ? obj->modelnum : -1,
				obj && obj->type == OBJTYPE_WEAPON
					? ((struct weaponobj *)obj)->weaponnum : -1,
				tracked->x1, tracked->y1, tracked->x2, tracked->y2);
	}
}

static void accessibilityIncidentDumpObjectives(u64 captureid)
{
	s32 count = objectiveGetCount();
	s32 difficulty = lvGetDifficulty();
	s32 i;

	for (i = 0; i < count && i < MAX_OBJECTIVES; i++) {
		struct objective *objective = g_Objectives[i];
		u32 diffbits = objectiveGetDifficultyBits(i);
		s32 status = objectiveCheck(i);
		const char *text = objective ? langGet(objective->text) : "";

		accessibilityLogEvent("incident", "objective",
				"capture=%llu index=%d applicable=%d difficulty_bits=0x%02x difficulty=%d status=%s status_value=%d objective=%p text=%s",
				(unsigned long long)captureid, i,
				(diffbits & (1 << difficulty)) != 0, diffbits,
				difficulty, accessibilityIncidentObjectiveStatusName(status),
				status, (void *)objective, text ? text : "");

		if (objective) {
			u32 *cmd = (u32 *)objective;
			s32 commandcount = 0;

			while (commandcount++ < ACCESSIBILITY_INCIDENT_OBJECTIVE_COMMAND_LIMIT) {
				s32 type = (u8)PD_BE32(cmd[0]);
				s32 length = setupGetCmdLength(cmd);
				s32 tag = -1;
				struct defaultobj *obj = NULL;

				if (type == OBJTYPE_ENDOBJECTIVE) {
					break;
				}

				if (length <= 0) {
					accessibilityLogEvent("incident", "objective_error",
							"capture=%llu objective=%d command=%d type=%d reason=invalid_length length=%d",
							(unsigned long long)captureid, i,
							commandcount - 1, type, length);
					break;
				}

				if (type == OBJECTIVETYPE_DESTROYOBJ
						|| type == OBJECTIVETYPE_COLLECTOBJ
						|| type == OBJECTIVETYPE_THROWOBJ
						|| type == OBJECTIVETYPE_HOLOGRAPH) {
					tag = length > 1 ? cmd[1] : -1;
					obj = objFindByTagId(tag);
				}

				if (type != OBJTYPE_BEGINOBJECTIVE) {
					accessibilityLogEvent("incident",
							"objective_requirement",
							"capture=%llu objective=%d command=%d type=%d kind=%s raw1=0x%08x raw2=0x%08x raw3=0x%08x tag=%d object=%p prop=%p propnum=%d model=%d object_type=%d healthy=%d",
							(unsigned long long)captureid, i,
							commandcount - 1, type,
							accessibilityIncidentRequirementName(type),
							length > 1 ? cmd[1] : 0,
							length > 2 ? cmd[2] : 0,
							length > 3 ? cmd[3] : 0,
							tag, (void *)obj,
							obj ? (void *)obj->prop : NULL,
							obj ? accessibilityIncidentPropNum(obj->prop) : -1,
							obj ? obj->modelnum : -1,
							obj ? obj->type : -1,
							obj ? objIsHealthy(obj) : 0);
				}

				cmd += length;
			}
		}
	}
}

static void accessibilityIncidentDumpInventory(u64 captureid)
{
	s32 count = g_Vars.currentplayer ? invGetCount() : 0;
	s32 limit = count < ACCESSIBILITY_INCIDENT_INVENTORY_LIMIT
			? count : ACCESSIBILITY_INCIDENT_INVENTORY_LIMIT;
	s32 i;

	for (i = 0; i < limit; i++) {
		struct invitem *item = invGetItemByIndex(i);
		const char *name = invGetNameByIndex(i);
		const char *shortname = invGetShortNameByIndex(i);

		accessibilityLogEvent("incident", "inventory",
				"capture=%llu index=%d current=%d item=%p type=%d weapon=%d name=%s short_name=%s",
				(unsigned long long)captureid, i,
				(u32)i == invGetCurrentIndex(), (void *)item,
				item ? item->type : -1, invGetWeaponNumByIndex(i),
				name ? name : "", shortname ? shortname : "");
	}

	if (count > limit) {
		accessibilityLogEvent("incident", "inventory_truncated",
				"capture=%llu count=%d logged=%d",
				(unsigned long long)captureid, count, limit);
	}
}

static void accessibilityIncidentDumpProps(u64 captureid)
{
	struct accessibilityobserver observer;
	struct coord origin;
	struct prop *prop = g_Vars.activeprops;
	s32 observervalid = accessibilityObserverGet(&observer);
	s32 traversed = 0;
	s32 inrange = 0;

	if (observervalid) {
		origin = observer.camera;
	} else if (g_Vars.currentplayer && g_Vars.currentplayer->prop) {
		origin = g_Vars.currentplayer->prop->pos;
	} else {
		memset(&origin, 0, sizeof(origin));
	}

	while (prop && prop != g_Vars.pausedprops
			&& traversed++ <= g_Vars.maxprops) {
		struct defaultobj *obj = accessibilityIncidentGetObj(prop);
		struct chrdata *chr = prop->type == PROPTYPE_CHR
				|| prop->type == PROPTYPE_PLAYER ? prop->chr : NULL;
		f32 dx = prop->pos.x - origin.x;
		f32 dy = prop->pos.y - origin.y;
		f32 dz = prop->pos.z - origin.z;
		f32 distsq = dx * dx + dy * dy + dz * dz;
		s32 tag = -1;
		u32 citag = 0;
		s32 glass = obj && (obj->type == OBJTYPE_GLASS
				|| obj->type == OBJTYPE_TINTEDGLASS);

		if (distsq > ACCESSIBILITY_INCIDENT_PROP_RANGE_SQ) {
			prop = prop->next;
			continue;
		}

		inrange++;

		if (obj) {
			tag = objGetTagNum(obj);
			citag = propobjGetCiTagId(prop);
		}

		accessibilityLogEvent("incident", "prop",
				"capture=%llu prop=%p propnum=%d prop_type=%d active=%d prop_flags=0x%02x onscreen=%d position=%.3f,%.3f,%.3f distance=%.3f rooms=%d,%d,%d,%d,%d,%d,%d,%d object=%p object_type=%d model=%d tag=%d ci_tag=0x%02x object_flags=0x%08x object_flags2=0x%08x object_flags3=0x%08x hidden=0x%08x healthy=%d damage=%d maxdamage=%d glass=%d breakable_glass=%d chr=%p chr_action=%d chr_team=%d chr_flags=0x%08x chr_hidden=0x%08x",
				(unsigned long long)captureid, (void *)prop,
				accessibilityIncidentPropNum(prop), prop->type,
				prop->active, prop->flags,
				(prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) != 0,
				prop->pos.x, prop->pos.y, prop->pos.z, sqrtf(distsq),
				prop->rooms[0], prop->rooms[1], prop->rooms[2],
				prop->rooms[3], prop->rooms[4], prop->rooms[5],
				prop->rooms[6], prop->rooms[7],
				(void *)obj, obj ? obj->type : -1,
				obj ? obj->modelnum : -1, tag, citag,
				obj ? obj->flags : 0, obj ? obj->flags2 : 0,
				obj ? obj->flags3 : 0, obj ? obj->hidden : 0,
				obj ? objIsHealthy(obj) : 0,
				obj ? obj->damage : -1, obj ? obj->maxdamage : -1,
				glass, glass && objIsHealthy(obj) && objIsMortal(obj),
				(void *)chr, chr ? chr->actiontype : -1,
				chr ? chr->team : -1, chr ? chr->chrflags : 0,
				chr ? chr->hidden : 0);

		prop = prop->next;
	}

	accessibilityLogEvent("incident", "prop_summary",
			"capture=%llu traversed=%d in_range=%d range=%.1f guard_hit=%d",
			(unsigned long long)captureid, traversed, inrange,
			ACCESSIBILITY_INCIDENT_PROP_RANGE,
			traversed > g_Vars.maxprops);
}

static void accessibilityIncidentDumpViewBlocker(u64 captureid)
{
	struct accessibilityobserver observer;
	struct coord end;
	struct prop *blocker;
	struct defaultobj *blockerobj;
	RoomNum camrooms[2];
	s32 result;
	s32 glass;

	if (!accessibilityObserverGet(&observer)) {
		accessibilityLogEvent("incident", "view_blocker",
				"capture=%llu result=observer_unavailable",
				(unsigned long long)captureid);
		return;
	}

	end.x = observer.camera.x
			+ observer.look.x * ACCESSIBILITY_INCIDENT_PROP_RANGE;
	end.y = observer.camera.y
			+ observer.look.y * ACCESSIBILITY_INCIDENT_PROP_RANGE;
	end.z = observer.camera.z
			+ observer.look.z * ACCESSIBILITY_INCIDENT_PROP_RANGE;
	camrooms[0] = observer.room;
	camrooms[1] = -1;

	result = cdExamLos08(&observer.camera, camrooms, &end,
			CDTYPE_BG | CDTYPE_DOORS | CDTYPE_OBJS | CDTYPE_PATHBLOCKER,
			GEOFLAG_WALL | GEOFLAG_BLOCK_SIGHT | GEOFLAG_BLOCK_SHOOT);
	blocker = result == CDRESULT_COLLISION ? cdGetObstacleProp() : NULL;
	blockerobj = accessibilityIncidentGetObj(blocker);
	glass = blockerobj && (blockerobj->type == OBJTYPE_GLASS
			|| blockerobj->type == OBJTYPE_TINTEDGLASS);

	accessibilityLogEvent("incident", "view_blocker",
			"capture=%llu result=%d collision=%d from=%.3f,%.3f,%.3f to=%.3f,%.3f,%.3f blocker=%p blocker_propnum=%d blocker_prop_type=%d blocker_object_type=%d blocker_model=%d blocker_glass=%d blocker_breakable=%d",
			(unsigned long long)captureid, result,
			result == CDRESULT_COLLISION,
			observer.camera.x, observer.camera.y, observer.camera.z,
			end.x, end.y, end.z, (void *)blocker,
			accessibilityIncidentPropNum(blocker),
			blocker ? blocker->type : -1,
			blockerobj ? blockerobj->type : -1,
			blockerobj ? blockerobj->modelnum : -1,
			glass, glass && objIsHealthy(blockerobj)
					&& objIsMortal(blockerobj));
}

static void accessibilityIncidentCapture(void)
{
	char speech[64];
	u64 captureid = ++g_AccessibilityIncidentCaptureId;
	u64 startedus;

	if (!accessibilityLogIsOpen()) {
		accessibilityAnnouncementStatus("Diagnostic logging unavailable",
				"incident_capture", g_Vars.currentplayernum, false);
		return;
	}

	startedus = sysGetMicroseconds();
	accessibilityLogEvent("incident", "capture_begin",
			"capture=%llu key=Shift+F2 stage=%d tick=%d player=%d history_samples=%d sample_ticks=%d prop_range=%.1f",
			(unsigned long long)captureid, g_Vars.stagenum,
			g_Vars.lvframe60, g_Vars.currentplayernum,
			g_AccessibilityIncidentHistoryCount,
			ACCESSIBILITY_INCIDENT_SAMPLE_TICKS,
			ACCESSIBILITY_INCIDENT_PROP_RANGE);
	accessibilityIncidentDumpHistory(captureid);
	accessibilityIncidentDumpPlayer(captureid);
	accessibilityIncidentDumpObjectives(captureid);
	accessibilityIncidentDumpInventory(captureid);
	accessibilityBeaconDumpDiagnostics(captureid);
	accessibilityIncidentDumpProps(captureid);
	accessibilityIncidentDumpViewBlocker(captureid);
	accessibilityLogEvent("incident", "capture_end",
			"capture=%llu stage=%d tick=%d result=saved duration_us=%llu",
			(unsigned long long)captureid, g_Vars.stagenum,
			g_Vars.lvframe60,
			(unsigned long long)(sysGetMicroseconds() - startedus));

	snprintf(speech, sizeof(speech), "Diagnostic capture %llu saved",
			(unsigned long long)captureid);
	accessibilityAnnouncementStatus(speech, "incident_capture",
			g_Vars.currentplayernum, false);
}

void accessibilityIncidentTick(void)
{
	s32 requested = false;

	if (!accessibilityIsEnabled()) {
		return;
	}

	if (g_AccessibilityIncidentStage != g_Vars.stagenum) {
		accessibilityIncidentReset("stage_changed");
		g_AccessibilityIncidentStage = g_Vars.stagenum;
	}

	if (g_AccessibilityIncidentHistoryCount == 0
			|| g_Vars.lvframe60 >= g_AccessibilityIncidentNextSampleTick) {
		accessibilityIncidentRecordFrame();
		g_AccessibilityIncidentNextSampleTick
				= g_Vars.lvframe60 + ACCESSIBILITY_INCIDENT_SAMPLE_TICKS;
	}

#ifndef PLATFORM_N64
	if (inputKeyJustPressed(VK_F2)) {
		u32 modifiers = inputGetKeyModState();

		requested = (modifiers & KM_SHIFT) != 0
				&& (modifiers & (KM_CTRL | KM_ALT)) == 0;
	}
#endif

	if (requested) {
		accessibilityIncidentRecordFrame();
		accessibilityIncidentCapture();
	}
}

void accessibilityIncidentReset(const char *reason)
{
	if (g_AccessibilityIncidentHistoryCount > 0
			&& accessibilityLogIsOpen()) {
		accessibilityLogEvent("incident", "reset",
				"reason=%s stage=%d tick=%d retained_samples=%d",
				reason ? reason : "unknown", g_AccessibilityIncidentStage,
				g_Vars.lvframe60, g_AccessibilityIncidentHistoryCount);
	}

	memset(g_AccessibilityIncidentHistory, 0,
			sizeof(g_AccessibilityIncidentHistory));
	g_AccessibilityIncidentHistoryCount = 0;
	g_AccessibilityIncidentHistoryWrite = 0;
	g_AccessibilityIncidentNextSampleTick = 0;
}
