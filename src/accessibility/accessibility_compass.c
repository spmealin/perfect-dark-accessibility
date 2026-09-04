#include <math.h>
#include <stdint.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/lv.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_compass.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_observer.h"
#include "accessibility/accessibility_tone.h"
#ifndef PLATFORM_N64
#include "input.h"
#endif

#define ACCESSIBILITY_COMPASS_CARDINAL_COUNT 4
#define ACCESSIBILITY_COMPASS_DEGREES_PER_CARDINAL 90.0f
#define ACCESSIBILITY_COMPASS_HYSTERESIS_DEGREES 6.0f
#define ACCESSIBILITY_COMPASS_MAX_TICK_ROTATION_DEGREES 170.0f
#define ACCESSIBILITY_COMPASS_RADIANS_TO_DEGREES 57.295779513082320876f

enum accessibilitycompasscardinal {
	ACCESSIBILITY_COMPASS_NORTH,
	ACCESSIBILITY_COMPASS_EAST,
	ACCESSIBILITY_COMPASS_SOUTH,
	ACCESSIBILITY_COMPASS_WEST,
};

/* Prototype accessibility vocabulary, isolated here for future localization. */
static const char *g_AccessibilityCompassCardinalNames[] = {
	"North",
	"East",
	"South",
	"West",
};

static s32 g_AccessibilityCompassScopeActive;
static s32 g_AccessibilityCompassHasHeading;
static f32 g_AccessibilityCompassPreviousHeading;
static s32 g_AccessibilityCompassLastCardinal = -1;
static s32 g_AccessibilityCompassLastCardinalArmed = true;
static uintptr_t g_AccessibilityCompassObserverProp;
static s32 g_AccessibilityCompassObserverRemote;
#ifndef PLATFORM_N64
static s32 g_AccessibilityCompassF4WasDown;
#endif

static const char *accessibilityCompassModeName(s32 mode)
{
	switch (mode) {
	case 1:
		return "speech_and_sonification";
	case 2:
		return "sonification_only";
	default:
		return "off";
	}
}

static const char *accessibilityCompassScopeReason(void)
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
	if (g_MenuData.count > 0 || g_Vars.currentplayer->mpmenuon) {
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
	return NULL;
}

static f32 accessibilityCompassNormalizeHeading(f32 heading)
{
	while (heading < 0.0f) {
		heading += 360.0f;
	}
	while (heading >= 360.0f) {
		heading -= 360.0f;
	}
	return heading;
}

static f32 accessibilityCompassHeading(const struct accessibilityobserver *observer)
{
	return accessibilityCompassNormalizeHeading(atan2f(-observer->look.x,
			observer->look.z) * ACCESSIBILITY_COMPASS_RADIANS_TO_DEGREES);
}

static f32 accessibilityCompassAngularDistance(f32 first, f32 second)
{
	f32 difference = fabsf(first - second);

	return difference > 180.0f ? 360.0f - difference : difference;
}

static s32 accessibilityCompassCardinalFromBoundary(f32 boundary)
{
	s32 cardinal = (s32)lroundf(boundary
			/ ACCESSIBILITY_COMPASS_DEGREES_PER_CARDINAL);

	cardinal %= ACCESSIBILITY_COMPASS_CARDINAL_COUNT;
	if (cardinal < 0) {
		cardinal += ACCESSIBILITY_COMPASS_CARDINAL_COUNT;
	}
	return cardinal;
}

static s32 accessibilityCompassFindCrossing(f32 previous, f32 current,
		f32 *rotation)
{
	f32 delta = current - previous;
	f32 boundary;
	f32 end;
	s32 cardinal = -1;

	if (delta > 180.0f) {
		delta -= 360.0f;
	} else if (delta < -180.0f) {
		delta += 360.0f;
	}

	if (rotation) {
		*rotation = delta;
	}

	if (fabsf(delta) < 0.001f
			|| fabsf(delta) > ACCESSIBILITY_COMPASS_MAX_TICK_ROTATION_DEGREES) {
		return -1;
	}

	end = previous + delta;

	if (delta > 0.0f) {
		boundary = (floorf(previous / ACCESSIBILITY_COMPASS_DEGREES_PER_CARDINAL)
				+ 1.0f) * ACCESSIBILITY_COMPASS_DEGREES_PER_CARDINAL;
		while (boundary <= end + 0.001f) {
			cardinal = accessibilityCompassCardinalFromBoundary(boundary);
			boundary += ACCESSIBILITY_COMPASS_DEGREES_PER_CARDINAL;
		}
	} else {
		boundary = (ceilf(previous / ACCESSIBILITY_COMPASS_DEGREES_PER_CARDINAL)
				- 1.0f) * ACCESSIBILITY_COMPASS_DEGREES_PER_CARDINAL;
		while (boundary >= end - 0.001f) {
			cardinal = accessibilityCompassCardinalFromBoundary(boundary);
			boundary -= ACCESSIBILITY_COMPASS_DEGREES_PER_CARDINAL;
		}
	}

	return cardinal;
}

static void accessibilityCompassAnnounce(s32 cardinal, f32 heading,
		f32 rotation)
{
	s32 mode = accessibilityGetCompassMode();
	s32 speechaccepted = 0;

	accessibilityTonePlayCompass(cardinal + 1);

	if (mode == 1) {
		speechaccepted = accessibilityAnnouncementStatus(
				g_AccessibilityCompassCardinalNames[cardinal], "compass",
				g_Vars.currentplayernum, true);
	}

		accessibilityLogEvent("compass", "cardinal",
			"mode=%s mode_value=%d cardinal=%s cardinal_value=%d clicks=%d heading=%.3f rotation=%.3f speech_requested=%d speech_accepted=%d click_ms=30 gap_ms=65 frequency_hz=600 tick=%d stage=%d player=%d observer=%p remote=%d",
			accessibilityCompassModeName(mode), mode,
			g_AccessibilityCompassCardinalNames[cardinal], cardinal,
			cardinal + 1, heading, rotation, mode == 1, speechaccepted,
			g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum,
			(void *)g_AccessibilityCompassObserverProp,
			g_AccessibilityCompassObserverRemote);
}

static void accessibilityCompassCycleMode(void)
{
	s32 oldmode = accessibilityGetCompassMode();
	s32 newmode = oldmode >= 2 ? 0 : oldmode + 1;
	const char *announcement;

	accessibilitySetCompassMode(newmode);
	accessibilityToneStopCompass();

	announcement = newmode == 1 ? "Compass speech and sound"
			: newmode == 2 ? "Compass sound only" : "Compass off";
	accessibilityAnnouncementStatus(announcement, "compass_mode",
			g_Vars.currentplayernum, true);
	accessibilityLogEvent("compass", "command",
			"action=cycle key=Shift+F4 old_mode=%s old_value=%d new_mode=%s new_value=%d tick=%d stage=%d player=%d",
			accessibilityCompassModeName(oldmode), oldmode,
			accessibilityCompassModeName(newmode), newmode,
			g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum);
}

void accessibilityCompassTick(void)
{
	struct accessibilityobserver observer;
	const char *reason = accessibilityCompassScopeReason();
	f32 heading;
	f32 rotation = 0.0f;
	s32 cardinal;
#ifndef PLATFORM_N64
	u32 modifiers;
	s32 f4down;
	s32 f4pressed;

	modifiers = inputGetKeyModState();
	f4down = inputKeyPressed(VK_F4);
	f4pressed = f4down && !g_AccessibilityCompassF4WasDown;
	g_AccessibilityCompassF4WasDown = f4down;
#endif

	if (reason || !accessibilityObserverGet(&observer)) {
		if (g_AccessibilityCompassScopeActive) {
			accessibilityLogEvent("compass", "scope",
					"state=lost reason=%s mode=%s tick=%d stage=%d player=%d",
					reason ? reason : "observer_unavailable",
					accessibilityCompassModeName(accessibilityGetCompassMode()),
					g_Vars.lvframe60, g_Vars.stagenum,
					g_Vars.currentplayernum);
		}
		if (g_AccessibilityCompassScopeActive
				|| g_AccessibilityCompassHasHeading) {
			accessibilityToneStopCompass();
		}
		g_AccessibilityCompassScopeActive = false;
		g_AccessibilityCompassHasHeading = false;
		return;
	}

	if (!g_AccessibilityCompassScopeActive) {
		g_AccessibilityCompassScopeActive = true;
		accessibilityLogEvent("compass", "scope",
				"state=entered mode=%s tick=%d stage=%d player=%d",
				accessibilityCompassModeName(accessibilityGetCompassMode()),
				g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum);
	}

#ifndef PLATFORM_N64
	if (f4pressed
			&& (modifiers & KM_SHIFT)
			&& !(modifiers & (KM_ALT | KM_CTRL))) {
		accessibilityCompassCycleMode();
	}
#endif

	heading = accessibilityCompassHeading(&observer);

	if (g_AccessibilityCompassObserverProp != (uintptr_t)observer.prop
			|| g_AccessibilityCompassObserverRemote != observer.isremote) {
		if (g_AccessibilityCompassObserverProp != 0) {
			accessibilityToneStopCompass();
			accessibilityLogEvent("compass", "observer_change",
					"old_observer=%p old_remote=%d new_observer=%p new_remote=%d tick=%d stage=%d player=%d",
					(void *)g_AccessibilityCompassObserverProp,
					g_AccessibilityCompassObserverRemote,
					(void *)observer.prop, observer.isremote,
					g_Vars.lvframe60, g_Vars.stagenum,
					g_Vars.currentplayernum);
		}
		g_AccessibilityCompassObserverProp = (uintptr_t)observer.prop;
		g_AccessibilityCompassObserverRemote = observer.isremote;
		g_AccessibilityCompassHasHeading = false;
		g_AccessibilityCompassLastCardinal = -1;
		g_AccessibilityCompassLastCardinalArmed = true;
	}

	if (accessibilityGetCompassMode() == 0) {
		g_AccessibilityCompassHasHeading = false;
		return;
	}

	if (!g_AccessibilityCompassHasHeading) {
		g_AccessibilityCompassPreviousHeading = heading;
		g_AccessibilityCompassHasHeading = true;
		return;
	}

	if (g_AccessibilityCompassLastCardinal >= 0
			&& accessibilityCompassAngularDistance(heading,
					g_AccessibilityCompassLastCardinal
						* ACCESSIBILITY_COMPASS_DEGREES_PER_CARDINAL)
				>= ACCESSIBILITY_COMPASS_HYSTERESIS_DEGREES) {
		g_AccessibilityCompassLastCardinalArmed = true;
	}

	cardinal = accessibilityCompassFindCrossing(
			g_AccessibilityCompassPreviousHeading, heading, &rotation);
	g_AccessibilityCompassPreviousHeading = heading;

	if (cardinal >= 0 && (cardinal != g_AccessibilityCompassLastCardinal
			|| g_AccessibilityCompassLastCardinalArmed)) {
		accessibilityCompassAnnounce(cardinal, heading, rotation);
		g_AccessibilityCompassLastCardinal = cardinal;
		g_AccessibilityCompassLastCardinalArmed = false;
	}
}

void accessibilityCompassReset(const char *reason)
{
	accessibilityToneStopCompass();
	g_AccessibilityCompassScopeActive = false;
	g_AccessibilityCompassHasHeading = false;
	g_AccessibilityCompassPreviousHeading = 0.0f;
	g_AccessibilityCompassLastCardinal = -1;
	g_AccessibilityCompassLastCardinalArmed = true;
	g_AccessibilityCompassObserverProp = 0;
	g_AccessibilityCompassObserverRemote = false;
	accessibilityLogEvent("compass", "reset", "reason=%s mode=%s",
			reason ? reason : "unknown",
			accessibilityCompassModeName(accessibilityGetCompassMode()));
}
