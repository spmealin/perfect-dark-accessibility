#include <ultra64.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/chr.h"
#include "game/chraction.h"
#include "game/game_0b0fd0.h"
#include "game/lv.h"
#include "game/propobj.h"
#include "game/sight.h"
#include "game/training.h"
#include "lib/collision.h"
#include "lib/vars.h"
#include "lib/vi.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_targeting.h"

#define ACCESSIBILITY_TARGETING_AUDIT_TICKS TICKS(60)
#define ACCESSIBILITY_TARGETING_RANGE_OUTER_RADIUS 75.0f
#define ACCESSIBILITY_TARGETING_COMBAT_PROJECTION_CAPACITY \
	ACCESSIBILITY_TARGETING_MAX_CANDIDATES

struct accessibilitytargetinggameaudit {
	uintptr_t prop;
	uintptr_t obj;
	s32 propnum;
	s32 proptype;
	s32 modelnum;
	u32 propflags;
	u32 objflags2;
	u8 inuse;
	u8 active;
	u8 destroyed;
	u8 accepted;
	s32 shootability;
	const char *reason;
};

struct accessibilitytargetinggameprojection {
	uintptr_t prop;
	uintptr_t obj;
	s32 propnum;
	s32 projected;
	s32 finite;
	f32 x2;
	f32 x1;
	f32 y2;
	f32 y1;
};

struct accessibilitytargetingcombatprojection {
	uintptr_t prop;
	uintptr_t chr;
	s32 propnum;
	s32 projected;
	s32 finite;
	s32 lineofsight;
	f32 x2;
	f32 x1;
	f32 y2;
	f32 y1;
};

static struct accessibilitytargetinggameaudit
		g_AccessibilityTargetingGameAudit[18];
static s32 g_AccessibilityTargetingGameAuditValid;
static s32 g_AccessibilityTargetingGameNextAudit60;
static s32 g_AccessibilityTargetingGameLastFrame60;
static const char *g_AccessibilityTargetingGameLastScopeReason;
static uintptr_t g_AccessibilityTargetingGameLastRejectedAim;
static struct accessibilitytargetinggameprojection
		g_AccessibilityTargetingGameProjections[18];
static s32 g_AccessibilityTargetingGameProjectionFrame60 = -1;
static s32 g_AccessibilityTargetingGameProjectionPlayer = -1;
static s32 g_AccessibilityTargetingGameProjectionsValid;
static uintptr_t g_AccessibilityTargetingGameAimProp;
static struct coord g_AccessibilityTargetingGameAimHitPos;
static s32 g_AccessibilityTargetingGameAimHitValid;
static struct accessibilitytargetingcombatprojection
		g_AccessibilityTargetingCombatProjections[
			ACCESSIBILITY_TARGETING_COMBAT_PROJECTION_CAPACITY];
static s32 g_AccessibilityTargetingCombatProjectionCount;
static s32 g_AccessibilityTargetingGameLastSource;

static s32 accessibilityTargetingGameRelationship(struct prop *prop);

static f32 accessibilityTargetingGamePunchRange(void)
{
	struct gset gset = { WEAPON_UNARMED, 0, 0, FUNC_PRIMARY };
	struct weaponfunc *func = gsetGetWeaponFunction(&gset);

	if (func && (func->type & 0xff) == INVENTORYFUNCTYPE_MELEE) {
		struct weaponfunc_melee *melee = (struct weaponfunc_melee *)func;

		if (melee->range > 0.0f) {
			return melee->range;
		}
	}

	return 60.0f;
}

static s32 accessibilityTargetingGameCharacterCombatCapable(
		struct chrdata *chr)
{
	return chr && !chrIsDead(chr)
			&& chr->actiontype != ACT_DRUGGEDDROP
			&& chr->actiontype != ACT_DRUGGEDKO;
}

static s32 accessibilityTargetingGameAuditEqual(
		const struct accessibilitytargetinggameaudit *a,
		const struct accessibilitytargetinggameaudit *b)
{
	return a->prop == b->prop && a->obj == b->obj
			&& a->propnum == b->propnum && a->proptype == b->proptype
			&& a->modelnum == b->modelnum && a->propflags == b->propflags
			&& a->objflags2 == b->objflags2 && a->inuse == b->inuse
			&& a->active == b->active && a->destroyed == b->destroyed
			&& a->accepted == b->accepted && a->shootability == b->shootability
			&& a->reason == b->reason;
}

static s32 accessibilityTargetingGamePropNum(const struct prop *prop)
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

static const char *accessibilityTargetingGameScopeReason(void)
{
	if (!accessibilityIsTargetingFeedbackEnabled()) {
		return "feature_disabled";
	}

	if (PLAYERCOUNT() != 1) {
		return "unsupported_player_count";
	}

	if (!g_Vars.currentplayer || !g_Vars.currentplayer->prop) {
		return "player_unavailable";
	}

	if (g_MenuData.count > 0 || g_Vars.currentplayer->menuisactive
			|| g_Vars.currentplayer->mpmenuon) {
		return "menu_or_overlay_open";
	}

	if (lvIsPaused()) {
		return "paused";
	}

	if (g_MainIsEndscreen) {
		return "endscreen";
	}

	if (g_Vars.in_cutscene || g_Vars.tickmode != TICKMODE_NORMAL) {
		return "non_gameplay_camera";
	}

	if (g_Vars.currentplayer->isdead) {
		return "player_dead";
	}

	if (g_Vars.lvupdate60 <= 0) {
		return "logical_time_stopped";
	}

	return NULL;
}

static s32 accessibilityTargetingGameIsFiringRange(void)
{
	return g_Vars.stagenum == STAGE_CITRAINING && g_FrIsValidWeapon;
}

static s32 accessibilityTargetingGameNativeAlignmentExpected(struct prop *aimedprop)
{
	s32 i;

	if (!aimedprop || !g_Vars.currentplayer->lastsighton) {
		return false;
	}

	if (g_Vars.currentplayer->sighttracktype != SIGHTTRACKTYPE_DEFAULT
			&& g_Vars.currentplayer->sighttracktype != SIGHTTRACKTYPE_BETASCANNER
			&& g_Vars.currentplayer->sighttracktype != SIGHTTRACKTYPE_ROCKETLAUNCHER
			&& g_Vars.currentplayer->sighttracktype != SIGHTTRACKTYPE_FOLLOWLOCKON) {
		return false;
	}

	for (i = 0; i < ARRAYCOUNT(g_Vars.currentplayer->trackedprops); i++) {
		if (g_Vars.currentplayer->trackedprops[i].prop == aimedprop) {
			return true;
		}
	}

	return false;
}

static void accessibilityTargetingGameClearProjections(void)
{
	memset(g_AccessibilityTargetingGameProjections, 0,
			sizeof(g_AccessibilityTargetingGameProjections));
	g_AccessibilityTargetingGameProjectionFrame60 = -1;
	g_AccessibilityTargetingGameProjectionPlayer = -1;
	g_AccessibilityTargetingGameProjectionsValid = false;
	g_AccessibilityTargetingGameAimProp = 0;
	memset(&g_AccessibilityTargetingGameAimHitPos, 0,
			sizeof(g_AccessibilityTargetingGameAimHitPos));
	g_AccessibilityTargetingGameAimHitValid = false;
	memset(g_AccessibilityTargetingCombatProjections, 0,
			sizeof(g_AccessibilityTargetingCombatProjections));
	g_AccessibilityTargetingCombatProjectionCount = 0;
}

static void accessibilityTargetingCaptureCombat(void)
{
	struct prop **propptr;
	RoomNum camrooms[2];

	if (!g_Vars.onscreenprops || !g_Vars.endonscreenprops) {
		return;
	}

	camrooms[0] = g_Vars.currentplayer->cam_room;
	camrooms[1] = -1;

	for (propptr = g_Vars.onscreenprops;
			propptr < g_Vars.endonscreenprops
			&& g_AccessibilityTargetingCombatProjectionCount
					< ARRAYCOUNT(g_AccessibilityTargetingCombatProjections);
			propptr++) {
		struct prop *prop = *propptr;
		struct chrdata *chr;
		struct accessibilitytargetingcombatprojection *projection;
		struct coord targetpos;
		s32 propnum;

		if (!prop || (prop->type != PROPTYPE_CHR
				&& prop->type != PROPTYPE_PLAYER)
				|| prop == g_Vars.currentplayer->prop || !prop->chr) {
			continue;
		}

		chr = prop->chr;
		propnum = accessibilityTargetingGamePropNum(prop);
		projection = &g_AccessibilityTargetingCombatProjections[
				g_AccessibilityTargetingCombatProjectionCount++];
		projection->prop = (uintptr_t)prop;
		projection->chr = (uintptr_t)chr;
		projection->propnum = propnum;

		if (propnum < 0 || !chr->model || !chr->model->matrices
				|| !chr->model->definition
				|| (prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) == 0) {
			continue;
		}

		projection->projected = modelGetScreenCoords(chr->model,
				&projection->x2, &projection->x1,
				&projection->y2, &projection->y1);
		projection->finite = projection->projected
				&& isfinite(projection->x2) && isfinite(projection->x1)
				&& isfinite(projection->y2) && isfinite(projection->y1);

		targetpos = prop->pos;
		targetpos.y = chr->manground + chr->height * 0.5f;
		if (prop->active && (prop->flags & PROPFLAG_ENABLED)
				&& accessibilityTargetingGameCharacterCombatCapable(chr)
				&& (chr->chrflags & CHRCFLAG_HIDDEN) == 0
				&& (chr->hidden & CHRHFLAG_UNTARGETABLE) == 0
				&& ((chr->hidden & CHRHFLAG_CLOAKED) == 0
					|| USINGDEVICE(DEVICE_IRSCANNER))
				&& accessibilityTargetingGameRelationship(prop)
					== ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE) {
			projection->lineofsight = cdTestLos03(
					&g_Vars.currentplayer->cam_pos, camrooms,
					&targetpos,
					CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_BG,
					GEOFLAG_BLOCK_SHOOT);
		}
	}
}

void accessibilityTargetingCaptureGame(struct prop *queryaimedprop,
		const struct coord *queryhitpos)
{
	struct frdata *frdata;
	s32 i;

	accessibilityTargetingGameClearProjections();
	g_AccessibilityTargetingGameProjectionFrame60 = g_Vars.lvframe60;
	g_AccessibilityTargetingGameProjectionPlayer = g_Vars.currentplayernum;

	if (accessibilityTargetingGameScopeReason()) {
		return;
	}

	if (queryaimedprop && queryhitpos
			&& queryaimedprop == g_Vars.currentplayer->lookingatprop.prop
			&& accessibilityTargetingGamePropNum(queryaimedprop) >= 0
			&& isfinite(queryhitpos->x) && isfinite(queryhitpos->y)
			&& isfinite(queryhitpos->z)) {
		g_AccessibilityTargetingGameAimProp = (uintptr_t)queryaimedprop;
		g_AccessibilityTargetingGameAimHitPos = *queryhitpos;
		g_AccessibilityTargetingGameAimHitValid = true;
	}

	if (!accessibilityTargetingGameIsFiringRange()) {
		accessibilityTargetingCaptureCombat();
		g_AccessibilityTargetingGameProjectionsValid = true;
		return;
	}

	frdata = frGetData();

	for (i = 0; frdata && i < ARRAYCOUNT(frdata->targets)
			&& i < ARRAYCOUNT(g_AccessibilityTargetingGameProjections); i++) {
		struct frtarget *target = &frdata->targets[i];
		struct prop *prop = target->prop;
		struct defaultobj *obj = NULL;
		struct accessibilitytargetinggameprojection *projection
				= &g_AccessibilityTargetingGameProjections[i];
		s32 propnum = accessibilityTargetingGamePropNum(prop);

		if (propnum >= 0) {
			obj = prop->obj;
		}

		projection->prop = (uintptr_t)prop;
		projection->obj = (uintptr_t)obj;
		projection->propnum = propnum;

		if (propnum < 0 || !obj || !obj->model || !obj->model->matrices
				|| !obj->model->definition
				|| (prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) == 0) {
			continue;
		}

		projection->projected = modelGetScreenCoords(obj->model,
				&projection->x2, &projection->x1,
				&projection->y2, &projection->y1);
		projection->finite = projection->projected
				&& isfinite(projection->x2) && isfinite(projection->x1)
				&& isfinite(projection->y2) && isfinite(projection->y1);
	}

	g_AccessibilityTargetingGameProjectionsValid = true;
}

static s32 accessibilityTargetingGameRelationship(struct prop *prop)
{
	if (sightIsPropFriendly(prop)) {
		return ACCESSIBILITY_TARGETING_RELATIONSHIP_FRIENDLY;
	}

	if (prop && prop->chr && g_Vars.currentplayer
			&& g_Vars.currentplayer->prop
			&& chrCompareTeams(g_Vars.currentplayer->prop->chr,
				prop->chr, COMPARE_ENEMIES)) {
		return ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE;
	}

	return ACCESSIBILITY_TARGETING_RELATIONSHIP_NEUTRAL;
}

static void accessibilityTargetingObserveCombat(
		struct accessibilitytargetingobservation *observation,
		s32 detailed, s32 scopechanged)
{
	struct prop *aimedprop = g_Vars.currentplayer->lookingatprop.prop;
	f32 viewleft = (f32)viGetViewLeft() / g_ScaleX;
	f32 viewtop = viGetViewTop();
	f32 viewright = viewleft + (f32)viGetViewWidth() / g_ScaleX;
	f32 viewbottom = viewtop + viGetViewHeight();
	f32 viewcenterx = (viewleft + viewright) * 0.5f;
	s32 aimedshootability = ACCESSIBILITY_TARGETING_SHOOTABILITY_UNKNOWN;
	s32 i;

	observation->inscope = true;
	observation->distancecuereference = accessibilityTargetingGamePunchRange();
	observation->sighton = g_Vars.currentplayer->lastsighton;
	observation->targetindicatorvisible = !g_Vars.currentplayer->gunsightoff;

	for (i = 0; i < g_AccessibilityTargetingCombatProjectionCount; i++) {
		struct accessibilitytargetingcombatprojection *projection
				= &g_AccessibilityTargetingCombatProjections[i];
		struct prop *prop = (struct prop *)projection->prop;
		struct chrdata *chr = (struct chrdata *)projection->chr;
		struct accessibilitytargetingcandidate *candidate;
		const char *reason = "eligible";
		s32 relationship = ACCESSIBILITY_TARGETING_RELATIONSHIP_UNKNOWN;
		s32 eligible = true;
		s32 aimed = prop && prop == aimedprop;
		f32 dx;
		f32 dy;
		f32 dz;

		if (projection->propnum < 0 || !prop || !chr
				|| accessibilityTargetingGamePropNum(prop) != projection->propnum
				|| prop->chr != chr) {
			eligible = false;
			reason = "stale_or_invalid_identity";
		} else if (prop->type != PROPTYPE_CHR
				&& prop->type != PROPTYPE_PLAYER) {
			eligible = false;
			reason = "wrong_prop_type";
		} else if (!prop->active || (prop->flags & PROPFLAG_ENABLED) == 0) {
			eligible = false;
			reason = "inactive_or_disabled";
		} else if (!accessibilityTargetingGameCharacterCombatCapable(chr)) {
			eligible = false;
			reason = "dead_dying_or_knocked_out";
		} else if (chr->chrflags & CHRCFLAG_HIDDEN) {
			eligible = false;
			reason = "character_hidden";
		} else if (chr->hidden & CHRHFLAG_UNTARGETABLE) {
			eligible = false;
			reason = "character_untargetable";
		} else if ((chr->hidden & CHRHFLAG_CLOAKED)
				&& !USINGDEVICE(DEVICE_IRSCANNER)) {
			eligible = false;
			reason = "character_cloaked";
		} else if ((prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) == 0) {
			eligible = false;
			reason = "not_rendered_this_tick";
		} else if (!chr->model || !chr->model->matrices
				|| !chr->model->definition) {
			eligible = false;
			reason = "model_unavailable";
		} else if (!g_AccessibilityTargetingGameProjectionsValid
				|| g_AccessibilityTargetingGameProjectionFrame60
						!= g_Vars.lvframe60
				|| g_AccessibilityTargetingGameProjectionPlayer
						!= g_Vars.currentplayernum) {
			eligible = false;
			reason = "projection_capture_unavailable";
		} else if (!projection->projected) {
			eligible = false;
			reason = "projection_failed";
		} else if (!projection->finite) {
			eligible = false;
			reason = "projection_non_finite";
		} else if (projection->x2 < viewleft || projection->x1 > viewright
				|| projection->y2 < viewtop || projection->y1 > viewbottom) {
			eligible = false;
			reason = "outside_viewport";
		} else if (!projection->lineofsight && !aimed) {
			eligible = false;
			reason = "line_of_sight_blocked";
		} else {
			relationship = accessibilityTargetingGameRelationship(prop);

			if (relationship == ACCESSIBILITY_TARGETING_RELATIONSHIP_FRIENDLY) {
				eligible = false;
				reason = "friendly";
			} else if (relationship != ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE) {
				eligible = false;
				reason = "not_hostile";
			}
		}

		if (detailed) {
			accessibilityLogEvent("targeting", "combat_candidate",
					"frame=%d slot=%d accepted=%d reason=%s aimed=%d relationship=%d prop=%p propnum=%d chr=%p prop_type=%d prop_flags=0x%02x chr_flags=0x%08x chr_hidden=0x%08x action=%d capture_valid=%d projected=%d finite=%d line_of_sight=%d screen=%.3f,%.3f,%.3f,%.3f",
					g_Vars.lvframe60, i, eligible, reason, aimed,
					relationship, (void *)prop, projection->propnum,
					(void *)chr, prop ? prop->type : -1,
					prop ? prop->flags : 0, chr ? chr->chrflags : 0,
					chr ? chr->hidden : 0, chr ? chr->actiontype : -1,
					g_AccessibilityTargetingGameProjectionsValid
						&& g_AccessibilityTargetingGameProjectionFrame60
								== g_Vars.lvframe60
						&& g_AccessibilityTargetingGameProjectionPlayer
								== g_Vars.currentplayernum,
					projection->projected, projection->finite,
					projection->lineofsight, projection->x1, projection->y1,
					projection->x2, projection->y2);
		}

		if (!eligible || observation->candidatecount
				>= ACCESSIBILITY_TARGETING_MAX_CANDIDATES) {
			continue;
		}

		candidate = &observation->candidates[observation->candidatecount++];
		memset(candidate, 0, sizeof(*candidate));
		candidate->identity.playernum = g_Vars.currentplayernum;
		candidate->identity.source = ACCESSIBILITY_TARGETING_SOURCE_COMBAT;
		candidate->identity.sourceslot = projection->propnum;
		candidate->identity.propnum = projection->propnum;
		candidate->identity.proptype = prop->type;
		candidate->identity.objectidentity = (uintptr_t)chr;
		candidate->prop = prop;
		candidate->category = prop->type == PROPTYPE_PLAYER
				? ACCESSIBILITY_TARGETING_CATEGORY_PLAYER
				: ACCESSIBILITY_TARGETING_CATEGORY_CHARACTER;
		candidate->relationship = relationship;
		candidate->shootability = ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE;
		candidate->position = prop->pos;
		candidate->position.y = chr->manground + chr->height * 0.5f;
		candidate->screenx1 = projection->x1;
		candidate->screeny1 = projection->y1;
		candidate->screenx2 = projection->x2;
		candidate->screeny2 = projection->y2;
		candidate->horizontalscreenoffset = fabsf(
				((projection->x1 + projection->x2) * 0.5f) - viewcenterx);
		dx = candidate->position.x - g_Vars.currentplayer->prop->pos.x;
		dy = candidate->position.y - g_Vars.currentplayer->prop->pos.y;
		dz = candidate->position.z - g_Vars.currentplayer->prop->pos.z;
		candidate->distance = sqrtf(dx * dx + dy * dy + dz * dz);
		dx = candidate->position.x - g_Vars.currentplayer->cam_pos.x;
		dy = candidate->position.y - g_Vars.currentplayer->cam_pos.y;
		dz = candidate->position.z - g_Vars.currentplayer->cam_pos.z;
		candidate->hasdistancecue = true;
		candidate->distancecue = sqrtf(dx * dx + dy * dy + dz * dz)
				- chr->radius;
		if (candidate->distancecue < 0.0f) {
			candidate->distancecue = 0.0f;
		}

		if (aimed) {
			observation->hasaimedtarget = true;
			observation->aimedidentity = candidate->identity;
			aimedshootability = candidate->shootability;
		}
	}

	if (aimedprop && !observation->hasaimedtarget
			&& (detailed || (uintptr_t)aimedprop
					!= g_AccessibilityTargetingGameLastRejectedAim)) {
		s32 aimedpropnum = accessibilityTargetingGamePropNum(aimedprop);

		accessibilityLogEvent("targeting", "aimed_candidate_rejected",
				"frame=%d aimed_prop=%p propnum=%d prop_type=%d reason=not_in_visible_hostile_character_set",
				g_Vars.lvframe60, (void *)aimedprop,
				aimedpropnum, aimedpropnum >= 0 ? aimedprop->type : -1);
	}
	g_AccessibilityTargetingGameLastRejectedAim = observation->hasaimedtarget
			? 0 : (uintptr_t)aimedprop;
	observation->nativealignmentexpected = observation->hasaimedtarget
			&& aimedshootability == ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE
			&& accessibilityTargetingGameNativeAlignmentExpected(aimedprop);

	if (detailed || scopechanged) {
		accessibilityLogEvent("targeting", "scope_gate",
				"frame=%d stage=%d player=%d accepted=1 reason=in_scope mode=combat candidates=%d captured=%d aimed=%d aimed_prop=%p aimed_shootability=%d native_alignment_expected=%d viewport=%.3f,%.3f,%.3f,%.3f",
				g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum,
				observation->candidatecount,
				g_AccessibilityTargetingCombatProjectionCount,
				observation->hasaimedtarget, (void *)aimedprop,
				aimedshootability, observation->nativealignmentexpected,
				viewleft, viewtop, viewright, viewbottom);
	}

	accessibilityTargetingObserve(observation);
}

void accessibilityTargetingObserveGame(void)
{
	struct accessibilitytargetingobservation observation;
	struct frdata *frdata;
	struct prop *aimedprop;
	const char *scopereason = accessibilityTargetingGameScopeReason();
	f32 viewleft;
	f32 viewtop;
	f32 viewright;
	f32 viewbottom;
	f32 viewcenterx;
	s32 i;
	s32 detailed;
	s32 scopechanged;
	s32 aimedshootability = ACCESSIBILITY_TARGETING_SHOOTABILITY_UNKNOWN;

	memset(&observation, 0, sizeof(observation));
	observation.playernum = g_Vars.currentplayernum;
	observation.source = accessibilityTargetingGameIsFiringRange()
			? ACCESSIBILITY_TARGETING_SOURCE_FIRING_RANGE
			: ACCESSIBILITY_TARGETING_SOURCE_COMBAT;
	observation.profile = accessibilityTargetingGameIsFiringRange()
			? ACCESSIBILITY_TARGETING_PROFILE_FIRING_RANGE
			: ACCESSIBILITY_TARGETING_PROFILE_COMBAT;
	observation.stagenum = g_Vars.stagenum;
	observation.frame60 = g_Vars.lvframe60;

	if (g_Vars.lvframe60 < g_AccessibilityTargetingGameLastFrame60
			|| observation.source != g_AccessibilityTargetingGameLastSource) {
		g_AccessibilityTargetingGameNextAudit60 = 0;
		g_AccessibilityTargetingGameAuditValid = false;
	}
	g_AccessibilityTargetingGameLastFrame60 = g_Vars.lvframe60;
	g_AccessibilityTargetingGameLastSource = observation.source;
	detailed = g_AccessibilityTargetingGameNextAudit60 == 0
			|| g_Vars.lvframe60 >= g_AccessibilityTargetingGameNextAudit60;

	if (scopereason) {
		if (detailed || scopereason != g_AccessibilityTargetingGameLastScopeReason) {
			accessibilityLogEvent("targeting", "scope_gate",
				"frame=%d stage=%d player=%d accepted=0 reason=%s mode=%s valid_weapon=%d player_count=%d menu_count=%d tickmode=%d lvupdate60=%d",
				g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum,
				scopereason, accessibilityTargetingGameIsFiringRange()
					? "firing_range" : "combat",
				g_FrIsValidWeapon, PLAYERCOUNT(), g_MenuData.count,
				g_Vars.tickmode, g_Vars.lvupdate60);
		}
		g_AccessibilityTargetingGameLastScopeReason = scopereason;
		if (detailed) {
			g_AccessibilityTargetingGameNextAudit60
					= g_Vars.lvframe60 + ACCESSIBILITY_TARGETING_AUDIT_TICKS;
		}
		accessibilityTargetingObserve(&observation);
		accessibilityTargetingGameClearProjections();
		return;
	}
	scopechanged = g_AccessibilityTargetingGameLastScopeReason != NULL;
	g_AccessibilityTargetingGameLastScopeReason = NULL;

	if (!accessibilityTargetingGameIsFiringRange()) {
		accessibilityTargetingObserveCombat(&observation, detailed, scopechanged);
		if (detailed) {
			g_AccessibilityTargetingGameNextAudit60
					= g_Vars.lvframe60 + ACCESSIBILITY_TARGETING_AUDIT_TICKS;
		}
		accessibilityTargetingGameClearProjections();
		return;
	}

	observation.inscope = true;
	observation.sighton = g_Vars.currentplayer->lastsighton;
	observation.targetindicatorvisible = !g_Vars.currentplayer->gunsightoff;
	frdata = frGetData();
	aimedprop = g_Vars.currentplayer->lookingatprop.prop;

	viewleft = (f32)viGetViewLeft() / g_ScaleX;
	viewtop = viGetViewTop();
	viewright = viewleft + (f32)viGetViewWidth() / g_ScaleX;
	viewbottom = viewtop + viGetViewHeight();
	viewcenterx = (viewleft + viewright) * 0.5f;

	for (i = 0; frdata && i < ARRAYCOUNT(frdata->targets); i++) {
		struct frtarget *target = &frdata->targets[i];
		struct prop *prop = target->prop;
		struct defaultobj *obj = NULL;
		struct accessibilitytargetinggameprojection *projection
				= &g_AccessibilityTargetingGameProjections[i];
		struct accessibilitytargetingcandidate *candidate;
		const char *reason = "eligible";
		f32 x2;
		f32 x1;
		f32 y2;
		f32 y1;
		f32 dx;
		f32 dy;
		f32 dz;
		s32 propnum = accessibilityTargetingGamePropNum(prop);
		s32 eligible = true;
		s32 shootability = ACCESSIBILITY_TARGETING_SHOOTABILITY_UNKNOWN;
		struct accessibilitytargetinggameaudit audit;

		if (propnum >= 0) {
			obj = prop->obj;
		}

		if (!target->inuse) {
			eligible = false;
			reason = "not_in_use";
		} else if (!target->active) {
			eligible = false;
			reason = "not_active";
		} else if (target->destroyed) {
			eligible = false;
			reason = "destroyed";
		} else if (propnum < 0 || !obj) {
			eligible = false;
			reason = "invalid_prop_or_object";
		} else if (prop->type != PROPTYPE_OBJ) {
			eligible = false;
			reason = "wrong_prop_type";
		} else if (obj->modelnum != MODEL_TARGET) {
			eligible = false;
			reason = "wrong_model";
		} else if ((prop->flags & PROPFLAG_ENABLED) == 0) {
			eligible = false;
			reason = "prop_disabled";
		} else if (obj->flags2 & OBJFLAG2_INVISIBLE) {
			eligible = false;
			reason = "object_invisible";
		} else if ((prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) == 0) {
			eligible = false;
			reason = "not_rendered_this_tick";
		} else if (!obj->model || !obj->model->matrices || !obj->model->definition) {
			eligible = false;
			reason = "model_unavailable";
		} else if (!g_AccessibilityTargetingGameProjectionsValid
				|| g_AccessibilityTargetingGameProjectionFrame60 != g_Vars.lvframe60
				|| g_AccessibilityTargetingGameProjectionPlayer
						!= g_Vars.currentplayernum
				|| projection->prop != (uintptr_t)prop
				|| projection->obj != (uintptr_t)obj
				|| projection->propnum != propnum) {
			eligible = false;
			reason = "projection_capture_unavailable";
		} else if (!projection->projected) {
			eligible = false;
			reason = "projection_failed";
		} else if (!projection->finite) {
			eligible = false;
			reason = "projection_non_finite";
		} else {
			x2 = projection->x2;
			x1 = projection->x1;
			y2 = projection->y2;
			y1 = projection->y1;

			if (x2 < viewleft || x1 > viewright
					|| y2 < viewtop || y1 > viewbottom) {
				eligible = false;
				reason = "outside_viewport";
			}
		}

		if (eligible) {
			shootability = frIsTargetFacingPos(prop,
					&g_Vars.currentplayer->prop->pos)
				? ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE
				: ACCESSIBILITY_TARGETING_SHOOTABILITY_FACING_AWAY;
		}

		memset(&audit, 0, sizeof(audit));
		audit.prop = (uintptr_t)prop;
		audit.obj = (uintptr_t)obj;
		audit.propnum = propnum;
		audit.proptype = propnum >= 0 ? prop->type : -1;
		audit.modelnum = obj ? obj->modelnum : -1;
		audit.propflags = propnum >= 0 ? prop->flags : 0;
		audit.objflags2 = obj ? obj->flags2 : 0;
		audit.inuse = target->inuse;
		audit.active = target->active;
		audit.destroyed = target->destroyed;
		audit.accepted = eligible;
		audit.shootability = shootability;
		audit.reason = reason;

		if (detailed || !g_AccessibilityTargetingGameAuditValid
				|| !accessibilityTargetingGameAuditEqual(
					&audit, &g_AccessibilityTargetingGameAudit[i])) {
			accessibilityLogEvent("targeting", "range_candidate",
				"frame=%d slot=%d accepted=%d reason=%s shootability=%d shootability_reason=%s inuse=%d active=%d destroyed=%d prop=%p propnum=%d obj=%p prop_type=%d model=%d prop_flags=0x%02x obj_flags2=0x%08x capture_valid=%d projected=%d finite=%d screen=%.3f,%.3f,%.3f,%.3f",
				g_Vars.lvframe60, i, eligible, reason, shootability,
				shootability == ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE
						? "shootable" : shootability
								== ACCESSIBILITY_TARGETING_SHOOTABILITY_FACING_AWAY
							? "facing_away" : "unknown",
				target->inuse,
				target->active, target->destroyed, (void *)prop, propnum,
				(void *)obj, propnum >= 0 ? prop->type : -1,
				obj ? obj->modelnum : -1, propnum >= 0 ? prop->flags : 0,
				obj ? obj->flags2 : 0,
				g_AccessibilityTargetingGameProjectionsValid
						&& g_AccessibilityTargetingGameProjectionFrame60
								== g_Vars.lvframe60
						&& g_AccessibilityTargetingGameProjectionPlayer
								== g_Vars.currentplayernum,
				projection->projected, projection->finite,
				projection->x1, projection->y1,
				projection->x2, projection->y2);
		}
		g_AccessibilityTargetingGameAudit[i] = audit;

		if (!eligible || observation.candidatecount
				>= ACCESSIBILITY_TARGETING_MAX_CANDIDATES) {
			continue;
		}

		candidate = &observation.candidates[observation.candidatecount++];
		memset(candidate, 0, sizeof(*candidate));
		candidate->identity.playernum = g_Vars.currentplayernum;
		candidate->identity.source = ACCESSIBILITY_TARGETING_SOURCE_FIRING_RANGE;
		candidate->identity.sourceslot = i;
		candidate->identity.propnum = propnum;
		candidate->identity.proptype = prop->type;
		candidate->identity.objectidentity = (uintptr_t)obj;
		candidate->prop = prop;
		candidate->category = ACCESSIBILITY_TARGETING_CATEGORY_RANGE_TARGET;
		candidate->relationship = ACCESSIBILITY_TARGETING_RELATIONSHIP_NEUTRAL;
		candidate->shootability = shootability;
		candidate->position = prop->pos;
		candidate->screenx1 = x1;
		candidate->screeny1 = y1;
		candidate->screenx2 = x2;
		candidate->screeny2 = y2;
		candidate->horizontalscreenoffset
				= fabsf(((x1 + x2) * 0.5f) - viewcenterx);
		dx = prop->pos.x - g_Vars.currentplayer->prop->pos.x;
		dy = prop->pos.y - g_Vars.currentplayer->prop->pos.y;
		dz = prop->pos.z - g_Vars.currentplayer->prop->pos.z;
		candidate->distance = sqrtf(dx * dx + dy * dy + dz * dz);
		candidate->localizedname = "Firing range target";

		if (prop == aimedprop) {
			f32 aimdx;
			f32 aimdy;
			f32 aimdz;

			observation.hasaimedtarget = true;
			observation.aimedidentity = candidate->identity;
			aimedshootability = candidate->shootability;

			if (g_AccessibilityTargetingGameAimHitValid
					&& g_AccessibilityTargetingGameAimProp == (uintptr_t)prop) {
				aimdx = g_AccessibilityTargetingGameAimHitPos.x - prop->pos.x;
				aimdy = g_AccessibilityTargetingGameAimHitPos.y - prop->pos.y;
				aimdz = g_AccessibilityTargetingGameAimHitPos.z - prop->pos.z;
				candidate->aimdistance = sqrtf(aimdx * aimdx + aimdy * aimdy
						+ aimdz * aimdz);
				candidate->aimquality = 1.0f - candidate->aimdistance
						/ ACCESSIBILITY_TARGETING_RANGE_OUTER_RADIUS;

				if (candidate->aimquality < 0.0f) {
					candidate->aimquality = 0.0f;
				} else if (candidate->aimquality > 1.0f) {
					candidate->aimquality = 1.0f;
				}

				candidate->hasaimquality = true;
			}
		}
	}

	if (aimedprop && !observation.hasaimedtarget
			&& (detailed
				|| (uintptr_t)aimedprop != g_AccessibilityTargetingGameLastRejectedAim)) {
		s32 aimedpropnum = accessibilityTargetingGamePropNum(aimedprop);
		struct defaultobj *aimedobj
				= aimedpropnum >= 0 ? aimedprop->obj : NULL;

		accessibilityLogEvent("targeting", "aimed_candidate_rejected",
				"frame=%d aimed_prop=%p propnum=%d prop_type=%d obj=%p model=%d reason=not_in_visible_range_candidate_set",
				g_Vars.lvframe60, (void *)aimedprop, aimedpropnum,
				aimedpropnum >= 0 ? aimedprop->type : -1, (void *)aimedobj,
				aimedobj ? aimedobj->modelnum : -1);
	}
	g_AccessibilityTargetingGameLastRejectedAim
			= observation.hasaimedtarget ? 0 : (uintptr_t)aimedprop;

	observation.nativealignmentexpected
			= observation.hasaimedtarget
			&& aimedshootability == ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE
			&& accessibilityTargetingGameNativeAlignmentExpected(aimedprop);

	if (detailed || scopechanged) {
		accessibilityLogEvent("targeting", "scope_gate",
			"frame=%d stage=%d player=%d accepted=1 reason=in_scope candidates=%d aimed=%d aimed_prop=%p aimed_shootability=%d native_alignment_expected=%d viewport=%.3f,%.3f,%.3f,%.3f",
			g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum,
			observation.candidatecount, observation.hasaimedtarget,
			(void *)aimedprop, aimedshootability,
			observation.nativealignmentexpected,
			viewleft, viewtop, viewright, viewbottom);
	}
	g_AccessibilityTargetingGameAuditValid = true;
	if (detailed) {
		g_AccessibilityTargetingGameNextAudit60
				= g_Vars.lvframe60 + ACCESSIBILITY_TARGETING_AUDIT_TICKS;
	}

	accessibilityTargetingObserve(&observation);
	accessibilityTargetingGameClearProjections();
}
