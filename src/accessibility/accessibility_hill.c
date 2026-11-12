#include <math.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/camera.h"
#include "game/lv.h"
#include "game/pad.h"
#include "game/propsnd.h"
#include "game/radar.h"
#include "lib/collision.h"
#include "lib/mtx.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_combat_radar.h"
#include "accessibility/accessibility_hill.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_observer.h"
#include "accessibility/accessibility_tone.h"
#include "accessibility/accessibility_visibility.h"

#define ACCESSIBILITY_HILL_INNER_DISTANCE 100.0f
#define ACCESSIBILITY_HILL_RANGE_HYSTERESIS 75.0f
#define ACCESSIBILITY_OBJECTIVE_VISIBLE_RADAR_MIN_GAIN 0.50f
#define ACCESSIBILITY_OBJECTIVE_GUIDE_GAIN 0.65f
#define ACCESSIBILITY_HILL_LOG_TICKS TICKS(60)
#define ACCESSIBILITY_HILL_SCORING_GRACE_TICKS TICKS(2)

static s32 g_AccessibilityHillAudible;
static s32 g_AccessibilityHillLocal;
static s32 g_AccessibilityHillInRange;
static s32 g_AccessibilityHillLineOfSight;
static s32 g_AccessibilityHillRadarShown;
static s32 g_AccessibilityHillInside;
static s32 g_AccessibilityHillScoring;
static s32 g_AccessibilityHillIndex = -1;
static s32 g_AccessibilityHillElapsed240 = -1;
static s32 g_AccessibilityHillLastProgressTick = -1000;
static s32 g_AccessibilityHillNextLogTick;
static s32 g_AccessibilityHillSuppressed;
static s32 g_AccessibilityObjectiveGuide;
static s32 g_AccessibilityObjectiveScenario = -1;
static const struct prop *g_AccessibilityObjectiveToken;
static s32 g_AccessibilityObjectiveProfile;

static const char *accessibilityHillScopeReason(void)
{
	if (!g_Vars.normmplayerisrunning) {
		return "not_normal_combat_sim";
	}
	if (PLAYERCOUNT() != 1) {
		return "unsupported_player_count";
	}
	if (g_MpSetup.scenario == MPSCENARIO_KINGOFTHEHILL) {
		if (!accessibilityIsKingOfTheHillBeaconEnabled()) {
			return "feature_disabled";
		}
	} else if (g_MpSetup.scenario == MPSCENARIO_HOLDTHEBRIEFCASE
			|| g_MpSetup.scenario == MPSCENARIO_CAPTURETHECASE) {
		if (!accessibilityIsCombatRadarAudioEnabled()) {
			return "feature_disabled";
		}
	} else {
		return "no_supported_objective";
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
	if (g_Vars.currentplayer->isdead) {
		return "player_dead";
	}
	if (g_Vars.currentplayer->mpmenuon) {
		return "player_menu";
	}
	if (g_MpSetup.scenario == MPSCENARIO_KINGOFTHEHILL) {
		if (g_ScenarioData.koh.hillindex < 0 || g_ScenarioData.koh.movehill) {
			return "hill_unavailable_or_moving";
		}
	} else if (g_MpSetup.scenario == MPSCENARIO_HOLDTHEBRIEFCASE
			&& (!g_ScenarioData.htb.token
				|| g_ScenarioData.htb.token == g_Vars.currentplayer->prop)) {
		return "case_unavailable_or_owned";
	}
	return NULL;
}

static f32 accessibilityHillDistance(const struct coord *from,
		const struct coord *to)
{
	f32 x = to->x - from->x;
	f32 y = to->y - from->y;
	f32 z = to->z - from->z;

	return sqrtf(x * x + y * y + z * z);
}

static f32 accessibilityHillDistanceGain(f32 distance, f32 range)
{
	f32 progress;

	if (distance <= ACCESSIBILITY_HILL_INNER_DISTANCE) {
		return 1.0f;
	}
	if (distance >= range || range <= ACCESSIBILITY_HILL_INNER_DISTANCE) {
		return 0.0f;
	}

	progress = (range - distance)
			/ (range - ACCESSIBILITY_HILL_INNER_DISTANCE);
	return progress * progress;
}

static s32 accessibilityHillHasLineOfSight(
		const struct accessibilityobserver *observer)
{
	struct coord from = observer->camera;
	struct coord to = g_ScenarioData.koh.hillpos;
	RoomNum fromrooms[2];
	RoomNum torooms[2];

	if (observer->room <= 0 || g_ScenarioData.koh.hillrooms[0] <= 0) {
		return false;
	}

	fromrooms[0] = observer->room;
	fromrooms[1] = -1;
	torooms[0] = g_ScenarioData.koh.hillrooms[0];
	torooms[1] = -1;
	to.y += 30.0f;

	return cdTestLos05(&from, fromrooms, &to, torooms,
			CDTYPE_DOORS | CDTYPE_BG,
			GEOFLAG_WALL | GEOFLAG_BLOCK_SIGHT | GEOFLAG_BLOCK_SHOOT);
}

static s32 accessibilityObjectiveIsRear(const struct accessibilityobserver *observer,
		const struct coord *position)
{
	f32 x = position->x - observer->camera.x;
	f32 z = position->z - observer->camera.z;

	return x * observer->look.x + z * observer->look.z < 0.0f;
}

static s32 accessibilityObjectiveInViewport(struct coord *position)
{
	Mtxf *worldtoscreen = camGetWorldToScreenMtxf();
	struct coord view;
	f32 screen[2];
	f32 left = camGetScreenLeft();
	f32 top = camGetScreenTop();

	if (!worldtoscreen) {
		return false;
	}
	mtx4TransformVec(worldtoscreen, position, &view);
	if (!(view.z < 0.0f)) {
		return false;
	}
	cam0f0b4d68(&view, screen);
	return screen[0] >= left && screen[0] < left + camGetScreenWidth()
			&& screen[1] >= top && screen[1] < top + camGetScreenHeight();
}

static s32 accessibilityCaseHasLineOfSight(const struct accessibilityobserver *observer,
		struct prop *token, struct coord *position)
{
	struct coord from = observer->camera;
	RoomNum rooms[] = { observer->room, -1 };

	return observer->room > 0 && token->rooms[0] > 0
			&& (token->flags & PROPFLAG_ONTHISSCREENTHISTICK)
			&& accessibilityVisibilityHasVisualLineOfSight(
					&from, rooms, position, token->rooms, token);
}

static void accessibilityObjectivePublishGuide(const struct accessibilityobserver *observer,
		struct coord *position, s32 profile, s32 restart)
{
	f32 x = position->x - observer->camera.x;
	f32 z = position->z - observer->camera.z;
	s32 duration = accessibilityCombatRadarObjectiveDurationMs(sqrtf(x * x + z * z));
	f32 volume;
	f32 pan = (psCalculatePan2(position, 0, -1.0f, NULL) - (f32)AL_PAN_CENTER)
			/ AL_PAN_CENTER;

	accessibilityGetCombatRadarTuning(NULL, NULL, &volume);
#if ACCESSIBILITY_BINAURAL_EXPERIMENT
	accessibilityToneSetSpatialPosition(ACCESSIBILITY_SPATIAL_HILL, position);
#endif
	accessibilityToneSetObjectiveGuide(volume * ACCESSIBILITY_OBJECTIVE_GUIDE_GAIN
			* ACCESSIBILITY_COMBAT_RADAR_OBJECTIVE_GAIN_SCALE,
			pan, accessibilityObjectiveIsRear(observer, position), duration, profile, restart);
	if (restart) {
		accessibilityLogEvent("objective_guide", "pulse",
				"scenario=%d profile=%d duration_ms=%d position=%.3f,%.3f,%.3f",
				g_MpSetup.scenario, profile, duration, position->x, position->y, position->z);
	}
}

static void accessibilityCaseTick(const struct accessibilityobserver *observer, s32 restart)
{
	struct prop *token = g_ScenarioData.htb.token;
	struct coord point = token->pos;
	struct coord radarpos;
	f32 range, volume, gain, pan, distance;
	s32 carrier = token->type == PROPTYPE_PLAYER || token->type == PROPTYPE_CHR;
	s32 visible, radarshown, guide;

	/* A carrier's prop origin is at their feet; test toward the torso. */
	if (carrier) {
		point.y += 70.0f;
	}
	radarshown = accessibilityCombatRadarGetScenarioObjective(
			MPSCENARIO_HOLDTHEBRIEFCASE, token, &radarpos);
	visible = (carrier
			? token->chr && !(token->chr->hidden & CHRHFLAG_CLOAKED)
				&& (radarshown || (g_MpSetup.options & MPOPTION_HTB_HIGHLIGHTBRIEFCASE))
			: token->type == PROPTYPE_WEAPON && token->obj
				&& !(token->obj->flags2 & OBJFLAG2_INVISIBLE)
				&& !(token->obj->hidden & (OBJHFLAG_GONE | OBJHFLAG_DELETING)))
			&& accessibilityObjectiveInViewport(&point)
			&& accessibilityCaseHasLineOfSight(observer, token, &point);
	guide = radarshown && !visible && !accessibilityCombatRadarIsManualObjectivePlaying();
	accessibilityGetMarkerTuning(&range, &volume);
	distance = accessibilityHillDistance(&observer->camera, &token->pos);
	gain = visible ? accessibilityHillDistanceGain(distance, range) : 0.0f;
	if (visible && radarshown && gain < ACCESSIBILITY_OBJECTIVE_VISIBLE_RADAR_MIN_GAIN) {
		gain = ACCESSIBILITY_OBJECTIVE_VISIBLE_RADAR_MIN_GAIN;
	}
	gain *= volume;
	restart |= token != g_AccessibilityObjectiveToken
			|| g_AccessibilityObjectiveGuide != guide || g_AccessibilityHillLocal != visible;
	if (guide) {
		accessibilityObjectivePublishGuide(observer, &radarpos, ACCESSIBILITY_OBJECTIVE_STANDARD, restart);
	} else {
		pan = (psCalculatePan2(&token->pos, 0, -1.0f, NULL) - (f32)AL_PAN_CENTER)
				/ AL_PAN_CENTER;
#if ACCESSIBILITY_BINAURAL_EXPERIMENT
		accessibilityToneSetSpatialPosition(ACCESSIBILITY_SPATIAL_HILL, &token->pos);
#endif
		accessibilityToneSetHillBeacon(gain > 0.0f, gain, pan, false, false, false, restart);
	}
	if (restart || g_AccessibilityHillRadarShown != radarshown) {
		accessibilityLogEvent("objective_guide", "state",
				"scenario=briefcase token=%p carrier=%d radar_shown=%d visible=%d guide=%d distance=%.3f marker_gain=%.4f",
				(void *)token, carrier, radarshown, visible, guide, distance, gain);
	}
	g_AccessibilityObjectiveToken = token;
	g_AccessibilityObjectiveGuide = guide;
	g_AccessibilityHillLocal = visible;
	g_AccessibilityHillRadarShown = radarshown;
	g_AccessibilityHillAudible = guide || gain > 0.0f;
}

struct accessibilitycapturecasecue {
	struct prop *token;
	struct coord position;
	f32 distance;
	s32 radarshown;
	s32 visible;
	s32 profile;
};

static s32 accessibilityCaseIsCarried(const struct prop *token)
{
	return token && (token->type == PROPTYPE_CHR || token->type == PROPTYPE_PLAYER);
}

static struct accessibilitycapturecasecue accessibilityCaptureCaseCue(
		const struct accessibilityobserver *observer, struct prop *token, s32 profile)
{
	struct accessibilitycapturecasecue cue = {0};
	struct coord point;
	s32 carrier;

	cue.profile = profile;
	if (!token || token == observer->prop) {
		return cue;
	}
	cue.token = token;
	cue.radarshown = accessibilityCombatRadarGetScenarioObjective(
			MPSCENARIO_CAPTURETHECASE, token, &cue.position);
	carrier = accessibilityCaseIsCarried(token);
	point = token->pos;
	if (carrier) {
		point.y += 70.0f;
	}
	cue.visible = (carrier
			? cue.radarshown && token->chr && !(token->chr->hidden & CHRHFLAG_CLOAKED)
			: token->type == PROPTYPE_WEAPON && token->obj
				&& !(token->obj->flags2 & OBJFLAG2_INVISIBLE)
				&& !(token->obj->hidden & (OBJHFLAG_GONE | OBJHFLAG_DELETING)))
			&& accessibilityObjectiveInViewport(&point)
			&& accessibilityCaseHasLineOfSight(observer, token, &point);
	if (cue.visible || !cue.radarshown) {
		cue.position = token->pos;
	}
	cue.distance = accessibilityHillDistance(&observer->camera, &cue.position);
	return cue;
}

static struct accessibilitycapturecasecue accessibilityCaptureHomeCue(
		const struct accessibilityobserver *observer, s32 team, f32 range)
{
	struct accessibilitycapturecasecue cue = {0};
	s32 index = g_ScenarioData.ctc.teamindexes[team];
	struct pad pad;
	struct coord point, from;
	RoomNum rooms[2], fromrooms[2];

	cue.profile = ACCESSIBILITY_OBJECTIVE_HOME;
	if (index < 0 || index >= ARRAYCOUNT(g_ScenarioData.ctc.spawnpadsperteam)
			|| g_ScenarioData.ctc.spawnpadsperteam[index].homepad < 0
			|| g_ScenarioData.ctc.baserooms[team] <= 0 || observer->room <= 0) {
		return cue;
	}
	padUnpack(g_ScenarioData.ctc.spawnpadsperteam[index].homepad, PADFIELD_POS | PADFIELD_ROOM, &pad);
	cue.position = pad.pos;
	cue.distance = accessibilityHillDistance(&observer->camera, &cue.position);
	point = pad.pos;
	point.y += 30.0f;
	from = observer->camera;
	fromrooms[0] = observer->room;
	fromrooms[1] = -1;
	rooms[0] = g_ScenarioData.ctc.baserooms[team];
	rooms[1] = -1;
	cue.visible = cue.distance < range && accessibilityObjectiveInViewport(&point)
			&& cdTestLos05(&from, fromrooms, &point, rooms, CDTYPE_DOORS | CDTYPE_BG,
					GEOFLAG_WALL | GEOFLAG_BLOCK_SIGHT | GEOFLAG_BLOCK_SHOOT);
	return cue;
}

static void accessibilityCaptureCaseTick(const struct accessibilityobserver *observer, s32 restart)
{
	s32 team = radarGetTeamIndex(observer->prop->chr->team);
	s32 i, carrying = false, stolen, guide;
	struct prop *owncase;
	struct accessibilitycapturecasecue cue = {0};
	f32 range, volume, gain = 0.0f;

	if (team >= ARRAYCOUNT(g_ScenarioData.ctc.tokens)) {
		accessibilityToneStopHillBeacon();
		g_AccessibilityHillLocal = g_AccessibilityHillAudible = false;
		g_AccessibilityObjectiveToken = NULL;
		return;
	}
	owncase = g_ScenarioData.ctc.tokens[team];
	stolen = accessibilityCaseIsCarried(owncase);
	accessibilityGetMarkerTuning(&range, &volume);
	for (i = 0; i < ARRAYCOUNT(g_ScenarioData.ctc.tokens); i++) {
		if (i != team && g_ScenarioData.ctc.tokens[i] == observer->prop) {
			carrying = true;
		}
	}
	if (stolen) {
		cue = accessibilityCaptureCaseCue(observer, owncase, ACCESSIBILITY_OBJECTIVE_RECOVER);
	} else if (carrying) {
		cue = accessibilityCaptureCaseCue(observer, owncase, ACCESSIBILITY_OBJECTIVE_HOME);
	}

	if (!cue.visible && !cue.radarshown && !carrying) {
		for (i = 0; i < ARRAYCOUNT(g_ScenarioData.ctc.tokens); i++) {
			struct accessibilitycapturecasecue candidate;

			if (i == team) {
				continue;
			}
			candidate = accessibilityCaptureCaseCue(observer,
					g_ScenarioData.ctc.tokens[i], ACCESSIBILITY_OBJECTIVE_STANDARD);
			if ((candidate.radarshown || (candidate.visible && candidate.distance < range))
					&& ((!cue.radarshown && !cue.visible) || candidate.distance < cue.distance)) {
				cue = candidate;
			}
		}
	}

	/* The empty base is a local landmark, never a hidden radar waypoint. */
	if ((!cue.visible && (!cue.radarshown || (!carrying && !stolen)))
			|| (carrying && !stolen)) {
		struct accessibilitycapturecasecue home = accessibilityCaptureHomeCue(observer, team, range);
		if (home.visible) {
			home.token = !stolen ? owncase : NULL;
			cue = home;
		}
	}

	if (cue.visible) {
		gain = accessibilityHillDistanceGain(cue.distance, range);
		if (cue.radarshown && gain < ACCESSIBILITY_OBJECTIVE_VISIBLE_RADAR_MIN_GAIN) {
			gain = ACCESSIBILITY_OBJECTIVE_VISIBLE_RADAR_MIN_GAIN;
		}
		gain *= volume;
	}
	guide = cue.radarshown && !cue.visible && !accessibilityCombatRadarIsManualObjectivePlaying();
	restart |= g_AccessibilityObjectiveToken != cue.token
			|| g_AccessibilityObjectiveProfile != cue.profile || g_AccessibilityObjectiveGuide != guide
			|| g_AccessibilityHillLocal != (gain > 0.0f);
	if (guide) {
		accessibilityObjectivePublishGuide(observer, &cue.position, cue.profile, restart);
	} else if (gain > 0.0f) {
		f32 pan = (psCalculatePan2(&cue.position, 0, -1.0f, NULL) - (f32)AL_PAN_CENTER)
				/ AL_PAN_CENTER;
#if ACCESSIBILITY_BINAURAL_EXPERIMENT
		accessibilityToneSetSpatialPosition(ACCESSIBILITY_SPATIAL_HILL, &cue.position);
#endif
		if (cue.profile == ACCESSIBILITY_OBJECTIVE_HOME) {
			accessibilityToneSetHomeMarker(gain, pan, restart);
		} else {
			accessibilityToneSetHillBeacon(true, gain, pan, false,
					cue.profile == ACCESSIBILITY_OBJECTIVE_RECOVER, false, restart);
		}
	} else if (g_AccessibilityHillAudible) {
		accessibilityToneStopHillBeacon();
	}
	if (restart || g_AccessibilityHillRadarShown != cue.radarshown) {
		accessibilityLogEvent("objective_guide", "capture_case",
				"team=%d carrying=%d stolen=%d profile=%d token=%p visible=%d radar=%d guide=%d gain=%.4f",
				team, carrying, stolen, cue.profile, (void *)cue.token, cue.visible,
				cue.radarshown, guide, gain);
	}
	g_AccessibilityObjectiveToken = cue.token;
	g_AccessibilityObjectiveProfile = cue.profile;
	g_AccessibilityObjectiveGuide = guide;
	g_AccessibilityHillLocal = gain > 0.0f;
	g_AccessibilityHillAudible = guide || g_AccessibilityHillLocal;
	g_AccessibilityHillRadarShown = cue.radarshown;
}

void accessibilityHillTick(void)
{
	struct accessibilityobserver observer;
	const char *reason = accessibilityHillScopeReason();
	f32 range;
	f32 mastervolume;
	f32 distance;
	f32 localgain;
	f32 radargain;
	f32 gain;
	f32 limit;
	struct coord point;
	struct coord radarpos;
	f32 normalizedpan;
	s32 pan;
	s32 inrange;
	s32 lineofsight;
	s32 radarshown;
	s32 local;
	s32 guide;
	s32 inside;
	s32 playerteam;
	s32 teamcontrols;
	s32 scoring;
	s32 rear;
	s32 restart;
	s32 wasSuppressed;

	if (reason || !accessibilityObserverGet(&observer)) {
		if (!reason) {
			reason = "observer_unavailable";
		}
		if (!g_AccessibilityHillSuppressed) {
			accessibilityLogEvent("hill_beacon", "scope",
					"state=suppressed reason=%s tick=%d hill_index=%d",
					reason, g_Vars.lvframe60, g_AccessibilityHillIndex);
			accessibilityToneStopHillBeacon();
		}
		g_AccessibilityHillSuppressed = true;
		g_AccessibilityHillAudible = false;
		g_AccessibilityHillLocal = false;
		g_AccessibilityHillInRange = false;
		g_AccessibilityHillLineOfSight = false;
		g_AccessibilityHillRadarShown = false;
		g_AccessibilityHillInside = false;
		g_AccessibilityHillScoring = false;
		g_AccessibilityHillElapsed240 = -1;
		g_AccessibilityHillLastProgressTick = -1000;
		g_AccessibilityObjectiveGuide = false;
		g_AccessibilityObjectiveScenario = -1;
		g_AccessibilityObjectiveToken = NULL;
		return;
	}

	wasSuppressed = g_AccessibilityHillSuppressed;
	if (wasSuppressed) {
		accessibilityLogEvent("hill_beacon", "scope",
				"state=resumed reason=eligible tick=%d hill_index=%d",
				g_Vars.lvframe60, g_ScenarioData.koh.hillindex);
		g_AccessibilityHillSuppressed = false;
	}
	wasSuppressed |= g_AccessibilityObjectiveScenario != g_MpSetup.scenario;
	g_AccessibilityObjectiveScenario = g_MpSetup.scenario;
	if (g_MpSetup.scenario == MPSCENARIO_HOLDTHEBRIEFCASE) {
		accessibilityCaseTick(&observer, wasSuppressed);
		return;
	}
	if (g_MpSetup.scenario == MPSCENARIO_CAPTURETHECASE) {
		if (observer.prop->chr) {
			accessibilityCaptureCaseTick(&observer, wasSuppressed);
		}
		return;
	}

	accessibilityGetMarkerTuning(&range, &mastervolume);
	distance = accessibilityHillDistance(&observer.camera,
			&g_ScenarioData.koh.hillpos);
	limit = g_AccessibilityHillInRange
			? range + ACCESSIBILITY_HILL_RANGE_HYSTERESIS : range;
	inrange = distance <= limit;
	radarshown = accessibilityCombatRadarGetScenarioObjective(MPSCENARIO_KINGOFTHEHILL,
			NULL, &radarpos);
	point = g_ScenarioData.koh.hillpos;
	point.y += 30.0f;
	lineofsight = (inrange || radarshown) && accessibilityObjectiveInViewport(&point)
			&& accessibilityHillHasLineOfSight(&observer);
	localgain = lineofsight
			? accessibilityHillDistanceGain(distance, range) : 0.0f;
	radargain = radarshown && lineofsight ? ACCESSIBILITY_OBJECTIVE_VISIBLE_RADAR_MIN_GAIN : 0.0f;
	gain = localgain > radargain ? localgain : radargain;
	gain *= mastervolume;
	local = gain > 0.0f;
	guide = radarshown && !lineofsight && !accessibilityCombatRadarIsManualObjectivePlaying();
	inside = g_Vars.currentplayer->prop->rooms[0]
			== g_ScenarioData.koh.hillrooms[0];
	playerteam = radarGetTeamIndex(g_Vars.currentplayer->prop->chr->team);
	teamcontrols = playerteam == g_ScenarioData.koh.occupiedteam;

	if (g_AccessibilityHillIndex == g_ScenarioData.koh.hillindex
			&& g_AccessibilityHillElapsed240 >= 0
			&& g_ScenarioData.koh.elapsed240 > g_AccessibilityHillElapsed240) {
		g_AccessibilityHillLastProgressTick = g_Vars.lvframe60;
	}

	scoring = inside && teamcontrols
			&& g_Vars.lvframe60 - g_AccessibilityHillLastProgressTick
				<= ACCESSIBILITY_HILL_SCORING_GRACE_TICKS;

	pan = psCalculatePan2(&g_ScenarioData.koh.hillpos, 0, -1.0f, NULL);
	normalizedpan = ((f32)pan - (f32)AL_PAN_CENTER)
			/ (f32)AL_PAN_CENTER;
	rear = accessibilityObjectiveIsRear(&observer, &g_ScenarioData.koh.hillpos);
	restart = wasSuppressed
			|| (!g_AccessibilityHillAudible && gain > 0.0f)
			|| g_AccessibilityHillIndex != g_ScenarioData.koh.hillindex
			|| g_AccessibilityHillLocal != local || g_AccessibilityObjectiveGuide != guide
			|| g_AccessibilityHillScoring != scoring;

	if (guide) {
		accessibilityObjectivePublishGuide(&observer, &radarpos, ACCESSIBILITY_OBJECTIVE_STANDARD, restart);
	} else {
#if ACCESSIBILITY_BINAURAL_EXPERIMENT
		accessibilityToneSetSpatialPosition(ACCESSIBILITY_SPATIAL_HILL, &g_ScenarioData.koh.hillpos);
#endif
		accessibilityToneSetHillBeacon(gain > 0.0f, gain, normalizedpan,
				rear, gain > 0.0f, scoring, restart);
	}

	if (restart || g_AccessibilityHillInRange != inrange
			|| g_AccessibilityHillLineOfSight != lineofsight
			|| g_AccessibilityHillRadarShown != radarshown
			|| g_AccessibilityHillInside != inside
			|| g_AccessibilityHillScoring != scoring) {
		accessibilityLogEvent("hill_beacon", "state",
				"tick=%d hill_index=%d position=%.3f,%.3f,%.3f room=%d observer=%p remote=%d observer_position=%.3f,%.3f,%.3f observer_room=%d distance=%.3f range=%.3f in_range=%d line_of_sight=%d radar_shown=%d mode=%s gain=%.5f pan=%.5f rear=%d inside=%d player_team=%d occupied_team=%d elapsed240=%d scoring=%d restart=%d",
				g_Vars.lvframe60, g_ScenarioData.koh.hillindex,
				g_ScenarioData.koh.hillpos.x,
				g_ScenarioData.koh.hillpos.y,
				g_ScenarioData.koh.hillpos.z,
				g_ScenarioData.koh.hillrooms[0],
				(void *)observer.prop, observer.isremote,
				observer.camera.x, observer.camera.y, observer.camera.z,
				observer.room, distance, range, inrange, lineofsight,
				radarshown, local ? "local" : guide ? "radar_pulse" : "silent",
				gain, normalizedpan, rear, inside, playerteam,
				g_ScenarioData.koh.occupiedteam,
				g_ScenarioData.koh.elapsed240, scoring, restart);
	}

	if (gain > 0.0f && g_Vars.lvframe60 >= g_AccessibilityHillNextLogTick) {
		accessibilityLogEvent("hill_beacon", "summary",
				"tick=%d hill_index=%d mode=%s distance=%.3f gain=%.5f pan=%.5f rear=%d in_range=%d line_of_sight=%d radar_shown=%d inside=%d player_team=%d occupied_team=%d elapsed240=%d scoring=%d",
				g_Vars.lvframe60, g_ScenarioData.koh.hillindex,
				local ? "local" : "radar", distance, gain, normalizedpan,
				rear, inrange, lineofsight, radarshown, inside, playerteam,
				g_ScenarioData.koh.occupiedteam,
				g_ScenarioData.koh.elapsed240, scoring);
		g_AccessibilityHillNextLogTick
				= g_Vars.lvframe60 + ACCESSIBILITY_HILL_LOG_TICKS;
	}

	g_AccessibilityHillAudible = gain > 0.0f;
	g_AccessibilityHillLocal = local;
	g_AccessibilityHillInRange = inrange;
	g_AccessibilityHillLineOfSight = lineofsight;
	g_AccessibilityHillRadarShown = radarshown;
	g_AccessibilityHillInside = inside;
	g_AccessibilityHillScoring = scoring;
	g_AccessibilityHillIndex = g_ScenarioData.koh.hillindex;
	g_AccessibilityHillElapsed240 = g_ScenarioData.koh.elapsed240;
	g_AccessibilityObjectiveGuide = guide;
}

s32 accessibilityObjectiveHasVisibleMarker(s32 scenario, const struct prop *token)
{
	if (scenario != g_AccessibilityObjectiveScenario || !g_AccessibilityHillLocal
			|| !g_AccessibilityHillAudible || g_AccessibilityObjectiveGuide) {
		return false;
	}
	if (scenario == MPSCENARIO_CAPTURETHECASE) {
		return token && token == g_AccessibilityObjectiveToken;
	}
	if (scenario == MPSCENARIO_HOLDTHEBRIEFCASE) {
		return token && token == g_AccessibilityObjectiveToken
				&& token == g_ScenarioData.htb.token;
	}
	return scenario == MPSCENARIO_KINGOFTHEHILL && !token
			&& !g_ScenarioData.koh.movehill
			&& g_AccessibilityHillIndex == g_ScenarioData.koh.hillindex;
}

void accessibilityHillReset(const char *reason)
{
	accessibilityLogEvent("hill_beacon", "reset",
			"reason=%s audible=%d local=%d hill_index=%d",
			reason ? reason : "reset", g_AccessibilityHillAudible,
			g_AccessibilityHillLocal, g_AccessibilityHillIndex);
	accessibilityToneStopHillBeacon();
	g_AccessibilityHillAudible = false;
	g_AccessibilityHillLocal = false;
	g_AccessibilityHillInRange = false;
	g_AccessibilityHillLineOfSight = false;
	g_AccessibilityHillRadarShown = false;
	g_AccessibilityHillInside = false;
	g_AccessibilityHillScoring = false;
	g_AccessibilityHillIndex = -1;
	g_AccessibilityHillElapsed240 = -1;
	g_AccessibilityHillLastProgressTick = -1000;
	g_AccessibilityHillNextLogTick = 0;
	g_AccessibilityHillSuppressed = false;
	g_AccessibilityObjectiveGuide = false;
	g_AccessibilityObjectiveScenario = -1;
	g_AccessibilityObjectiveToken = NULL;
}
