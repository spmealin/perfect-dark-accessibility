#include <math.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/lv.h"
#include "game/propsnd.h"
#include "lib/collision.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_combat_radar.h"
#include "accessibility/accessibility_hill.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_observer.h"
#include "accessibility/accessibility_tone.h"

#define ACCESSIBILITY_HILL_INNER_DISTANCE 100.0f
#define ACCESSIBILITY_HILL_RANGE_HYSTERESIS 75.0f
#define ACCESSIBILITY_HILL_RADAR_GAIN 0.25f
#define ACCESSIBILITY_HILL_LOG_TICKS TICKS(60)

static s32 g_AccessibilityHillAudible;
static s32 g_AccessibilityHillLocal;
static s32 g_AccessibilityHillInRange;
static s32 g_AccessibilityHillLineOfSight;
static s32 g_AccessibilityHillRadarShown;
static s32 g_AccessibilityHillIndex = -1;
static s32 g_AccessibilityHillNextLogTick;
static s32 g_AccessibilityHillSuppressed;

static const char *accessibilityHillScopeReason(void)
{
	if (!accessibilityIsKingOfTheHillBeaconEnabled()) {
		return "feature_disabled";
	}
	if (!g_Vars.normmplayerisrunning) {
		return "not_normal_combat_sim";
	}
	if (PLAYERCOUNT() != 1) {
		return "unsupported_player_count";
	}
	if (g_MpSetup.scenario != MPSCENARIO_KINGOFTHEHILL) {
		return "not_king_of_the_hill";
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
	if (g_ScenarioData.koh.hillindex < 0) {
		return "hill_unavailable";
	}
	if (g_ScenarioData.koh.movehill) {
		return "hill_moving";
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

static s32 accessibilityHillIsRear(
		const struct accessibilityobserver *observer)
{
	f32 x = g_ScenarioData.koh.hillpos.x - observer->camera.x;
	f32 z = g_ScenarioData.koh.hillpos.z - observer->camera.z;

	return x * observer->look.x + z * observer->look.z < 0.0f;
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
	f32 normalizedpan;
	s32 pan;
	s32 inrange;
	s32 lineofsight;
	s32 radarshown;
	s32 local;
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
		return;
	}

	wasSuppressed = g_AccessibilityHillSuppressed;
	if (wasSuppressed) {
		accessibilityLogEvent("hill_beacon", "scope",
				"state=resumed reason=eligible tick=%d hill_index=%d",
				g_Vars.lvframe60, g_ScenarioData.koh.hillindex);
		g_AccessibilityHillSuppressed = false;
	}

	accessibilityGetMarkerTuning(&range, &mastervolume);
	distance = accessibilityHillDistance(&observer.camera,
			&g_ScenarioData.koh.hillpos);
	limit = g_AccessibilityHillInRange
			? range + ACCESSIBILITY_HILL_RANGE_HYSTERESIS : range;
	inrange = distance <= limit;
	lineofsight = inrange && accessibilityHillHasLineOfSight(&observer);
	localgain = lineofsight
			? accessibilityHillDistanceGain(distance, range) : 0.0f;
	radarshown = accessibilityCombatRadarIsHillShown();
	radargain = radarshown ? ACCESSIBILITY_HILL_RADAR_GAIN : 0.0f;
	gain = localgain > radargain ? localgain : radargain;
	gain *= mastervolume;
	local = localgain > 0.0f;

	pan = psCalculatePan2(&g_ScenarioData.koh.hillpos, 0, -1.0f, NULL);
	normalizedpan = ((f32)pan - (f32)AL_PAN_CENTER)
			/ (f32)AL_PAN_CENTER;
	rear = accessibilityHillIsRear(&observer);
	restart = wasSuppressed
			|| (!g_AccessibilityHillAudible && gain > 0.0f)
			|| g_AccessibilityHillIndex != g_ScenarioData.koh.hillindex
			|| g_AccessibilityHillLocal != local;

	accessibilityToneSetHillBeacon(gain > 0.0f, gain, normalizedpan,
			rear, local, restart);

	if (restart || g_AccessibilityHillInRange != inrange
			|| g_AccessibilityHillLineOfSight != lineofsight
			|| g_AccessibilityHillRadarShown != radarshown) {
		accessibilityLogEvent("hill_beacon", "state",
				"tick=%d hill_index=%d position=%.3f,%.3f,%.3f room=%d observer=%p remote=%d observer_position=%.3f,%.3f,%.3f observer_room=%d distance=%.3f range=%.3f in_range=%d line_of_sight=%d radar_shown=%d mode=%s gain=%.5f pan=%.5f rear=%d restart=%d",
				g_Vars.lvframe60, g_ScenarioData.koh.hillindex,
				g_ScenarioData.koh.hillpos.x,
				g_ScenarioData.koh.hillpos.y,
				g_ScenarioData.koh.hillpos.z,
				g_ScenarioData.koh.hillrooms[0],
				(void *)observer.prop, observer.isremote,
				observer.camera.x, observer.camera.y, observer.camera.z,
				observer.room, distance, range, inrange, lineofsight,
				radarshown, local ? "local" : radarshown ? "radar" : "silent",
				gain, normalizedpan, rear, restart);
	}

	if (gain > 0.0f && g_Vars.lvframe60 >= g_AccessibilityHillNextLogTick) {
		accessibilityLogEvent("hill_beacon", "summary",
				"tick=%d hill_index=%d mode=%s distance=%.3f gain=%.5f pan=%.5f rear=%d in_range=%d line_of_sight=%d radar_shown=%d",
				g_Vars.lvframe60, g_ScenarioData.koh.hillindex,
				local ? "local" : "radar", distance, gain, normalizedpan,
				rear, inrange, lineofsight, radarshown);
		g_AccessibilityHillNextLogTick
				= g_Vars.lvframe60 + ACCESSIBILITY_HILL_LOG_TICKS;
	}

	g_AccessibilityHillAudible = gain > 0.0f;
	g_AccessibilityHillLocal = local;
	g_AccessibilityHillInRange = inrange;
	g_AccessibilityHillLineOfSight = lineofsight;
	g_AccessibilityHillRadarShown = radarshown;
	g_AccessibilityHillIndex = g_ScenarioData.koh.hillindex;
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
	g_AccessibilityHillIndex = -1;
	g_AccessibilityHillNextLogTick = 0;
	g_AccessibilityHillSuppressed = false;
}
