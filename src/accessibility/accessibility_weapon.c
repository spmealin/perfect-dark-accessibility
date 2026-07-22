#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_tone.h"
#include "accessibility/accessibility_weapon.h"

struct accessibilityweaponfunctionstate {
	s32 initialized;
	s32 stagenum;
	s32 weaponnum;
	s32 secondary;
};

static struct accessibilityweaponfunctionstate
		g_AccessibilityWeaponFunctionStates[MAX_PLAYERS];

void accessibilityWeaponFunctionObserve(s32 playernum, s32 stagenum,
		s32 weaponnum, s32 secondary)
{
	struct accessibilityweaponfunctionstate *state;

	if (playernum < 0 || playernum >= MAX_PLAYERS) {
		return;
	}

	state = &g_AccessibilityWeaponFunctionStates[playernum];
	secondary = secondary != 0;

	if (!accessibilityIsWeaponFunctionCuesEnabled()) {
		memset(state, 0, sizeof(*state));
		return;
	}

	if (!state->initialized || state->stagenum != stagenum
			|| state->weaponnum != weaponnum) {
		state->initialized = true;
		state->stagenum = stagenum;
		state->weaponnum = weaponnum;
		state->secondary = secondary;
		return;
	}

	if (state->secondary != secondary) {
		accessibilityTonePlayWeaponFunction(secondary);
		accessibilityLogEvent("weapon_function", "state_change",
				"player=%d stage=%d weapon=%d previous=%s current=%s beep_count=%d frequency_hz=1000 beep_ms=35 gap_ms=30",
				playernum, stagenum, weaponnum,
				state->secondary ? "secondary" : "primary",
				secondary ? "secondary" : "primary",
				secondary ? 2 : 1);
		state->secondary = secondary;
	}
}

void accessibilityWeaponFunctionReset(const char *reason)
{
	memset(g_AccessibilityWeaponFunctionStates, 0,
			sizeof(g_AccessibilityWeaponFunctionStates));
	accessibilityToneStopWeaponFunction();
	accessibilityLogEvent("weapon_function", "reset", "reason=%s",
			reason ? reason : "unspecified");
}
