#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/bondgun.h"
#include "game/game_0b0fd0.h"
#include "game/inv.h"
#include "game/lang.h"
#include "types.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_tone.h"
#include "accessibility/accessibility_weapon.h"

#define ACCESSIBILITY_WEAPON_TEXT_MAX 1024
#define ACCESSIBILITY_WEAPON_PENDING_TICKS 300

enum accessibilityweaponchangesource {
	ACCESSIBILITY_WEAPON_CHANGE_NONE,
	ACCESSIBILITY_WEAPON_CHANGE_ACTIVE_MENU,
	ACCESSIBILITY_WEAPON_CHANGE_QUICK,
};

struct accessibilityweaponfunctionstate {
	s32 initialized;
	s32 stagenum;
	s32 weaponnum;
	s32 secondary;
	s32 dual;
	s32 activemenuopen;
	s32 activemenuselection;
	s32 pendingweaponnum;
	s32 pendingticks;
	enum accessibilityweaponchangesource pendingsource;
};

static struct accessibilityweaponfunctionstate
		g_AccessibilityWeaponFunctionStates[MAX_PLAYERS];

static const char *accessibilityWeaponChangeSourceName(
		enum accessibilityweaponchangesource source)
{
	switch (source) {
	case ACCESSIBILITY_WEAPON_CHANGE_ACTIVE_MENU:
		return "active_menu";
	case ACCESSIBILITY_WEAPON_CHANGE_QUICK:
		return "quick_change";
	default:
		return "none";
	}
}

static void accessibilityWeaponCopyNormalized(char *dst, size_t dstlen,
		const char *src)
{
	size_t out = 0;
	s32 pending_space = false;

	if (!dstlen) {
		return;
	}

	while (src && *src && out + 1 < dstlen) {
		unsigned char c = (unsigned char)*src++;

		if (c <= ' ') {
			pending_space = out > 0;
		} else {
			if (pending_space && out + 1 < dstlen) {
				dst[out++] = ' ';
			}

			pending_space = false;
			dst[out++] = c;
		}
	}

	dst[out] = '\0';
}

static struct inventory_ammo *accessibilityWeaponGetDisplayedAmmo(
		s32 weaponnum, s32 secondary)
{
	struct inventory_ammo *ammo = weaponGetAmmoByFunction(weaponnum,
			secondary ? FUNC_SECONDARY : FUNC_PRIMARY);

	if (!ammo) {
		ammo = weaponGetAmmoByFunction(weaponnum,
				secondary ? FUNC_PRIMARY : FUNC_SECONDARY);
	}

	return ammo;
}

static void accessibilityWeaponFormatName(char *dst, size_t dstlen,
		s32 weaponnum, s32 dual, const char *name)
{
	char normalized[ACCESSIBILITY_WEAPON_TEXT_MAX];

	accessibilityWeaponCopyNormalized(normalized, sizeof(normalized), name);

	if (dual && normalized[0]) {
		snprintf(dst, dstlen, "%s%s", langGet(L_PROPOBJ_001), normalized);
	} else {
		snprintf(dst, dstlen, "%s", normalized);
	}
}

static void accessibilityWeaponAnnounceWieldState(s32 playernum,
		s32 weaponnum, s32 dual)
{
	char utterance[ACCESSIBILITY_WEAPON_TEXT_MAX];

	accessibilityWeaponFormatName(utterance, sizeof(utterance), weaponnum,
			dual, bgunGetName(weaponnum));

	if (utterance[0]) {
		accessibilityAnnouncementWeaponChange(utterance, "wield_state",
				playernum, true);
		accessibilityLogEvent("weapon_change", "wield_state_announced",
				"player=%d stage=%d weapon=%d dual=%d text=%s",
				playernum, g_Vars.stagenum, weaponnum, dual, utterance);
	}
}

static void accessibilityWeaponAnnounceChange(s32 playernum, s32 weaponnum,
		s32 secondary, s32 dual,
		enum accessibilityweaponchangesource source)
{
	struct inventory_ammo *ammo;
	char weaponname[ACCESSIBILITY_WEAPON_TEXT_MAX];
	char utterance[ACCESSIBILITY_WEAPON_TEXT_MAX];
	const char *ammoname;
	const char *name;
	s32 ammocount;
	s32 currentindex;
	s32 hasname = false;
	s32 hasammo = false;

	weaponname[0] = '\0';
	utterance[0] = '\0';
	ammo = accessibilityWeaponGetDisplayedAmmo(weaponnum, secondary);

	if (ammo) {
		ammocount = bgunGetAmmoCount(ammo->type);
		ammoname = langGet(L_PROPOBJ_010);
		hasammo = ammoname && ammoname[0];
	}

	if (source == ACCESSIBILITY_WEAPON_CHANGE_QUICK || dual) {
		currentindex = invGetCurrentIndex();
		name = !dual && currentindex >= 0 && currentindex < invGetCount()
			? langGet(invGetNameIdByIndex(currentindex))
			: bgunGetName(weaponnum);
		accessibilityWeaponFormatName(weaponname, sizeof(weaponname),
				weaponnum, dual, name);
		hasname = weaponname[0] != '\0';
	}

	if (hasname && hasammo) {
		snprintf(utterance, sizeof(utterance), "%s, %d %s",
				weaponname, ammocount, ammoname);
	} else if (hasname) {
		snprintf(utterance, sizeof(utterance), "%s", weaponname);
	} else if (hasammo) {
		snprintf(utterance, sizeof(utterance), "%d %s",
				ammocount, ammoname);
	}

	if (utterance[0]) {
		accessibilityAnnouncementWeaponChange(utterance,
				accessibilityWeaponChangeSourceName(source), playernum,
				source == ACCESSIBILITY_WEAPON_CHANGE_QUICK);
		accessibilityLogEvent("weapon_change", "announced",
				"player=%d stage=%d source=%s weapon=%d secondary=%d dual=%d ammo_type=%d ammo_count=%d text=%s",
				playernum, g_Vars.stagenum,
				accessibilityWeaponChangeSourceName(source), weaponnum,
				secondary, dual, ammo ? (s32)ammo->type : -1,
				ammo ? ammocount : -1, utterance);
	} else {
		accessibilityLogEvent("weapon_change", "speech_suppressed",
				"player=%d stage=%d source=%s weapon=%d secondary=%d reason=no_ammo_or_name",
				playernum, g_Vars.stagenum,
				accessibilityWeaponChangeSourceName(source), weaponnum,
				secondary);
	}
}

void accessibilityWeaponActiveMenuObserve(s32 playernum, s32 open,
		s32 selectedweaponnum)
{
	struct accessibilityweaponfunctionstate *state;

	if (playernum < 0 || playernum >= MAX_PLAYERS) {
		return;
	}

	state = &g_AccessibilityWeaponFunctionStates[playernum];

	if (!accessibilityIsWeaponChangeAnnouncementsEnabled()) {
		state->activemenuopen = false;
		state->activemenuselection = -1;

		if (state->pendingsource == ACCESSIBILITY_WEAPON_CHANGE_ACTIVE_MENU) {
			state->pendingsource = ACCESSIBILITY_WEAPON_CHANGE_NONE;
		}

		return;
	}

	if (open) {
		state->activemenuopen = true;
		state->activemenuselection = selectedweaponnum;
		return;
	}

	if (state->activemenuopen) {
		if (state->activemenuselection >= WEAPON_UNARMED) {
			state->pendingweaponnum = state->activemenuselection;
			state->pendingsource = ACCESSIBILITY_WEAPON_CHANGE_ACTIVE_MENU;
			state->pendingticks = 0;
			accessibilityLogEvent("weapon_change", "pending",
					"player=%d source=active_menu weapon=%d",
					playernum, state->pendingweaponnum);
		} else if (state->pendingsource
				== ACCESSIBILITY_WEAPON_CHANGE_ACTIVE_MENU) {
			state->pendingsource = ACCESSIBILITY_WEAPON_CHANGE_NONE;
		}
	}

	state->activemenuopen = false;
	state->activemenuselection = -1;
}

void accessibilityWeaponQuickChangeRequested(s32 playernum,
		s32 selectedweaponnum)
{
	struct accessibilityweaponfunctionstate *state;

	if (playernum < 0 || playernum >= MAX_PLAYERS
			|| selectedweaponnum < WEAPON_UNARMED
			|| !accessibilityIsWeaponChangeAnnouncementsEnabled()) {
		return;
	}

	state = &g_AccessibilityWeaponFunctionStates[playernum];

	if (state->initialized && state->weaponnum == selectedweaponnum) {
		accessibilityLogEvent("weapon_change", "request_suppressed",
				"player=%d source=quick_change weapon=%d reason=already_equipped",
				playernum, selectedweaponnum);
		return;
	}

	state->pendingweaponnum = selectedweaponnum;
	state->pendingsource = ACCESSIBILITY_WEAPON_CHANGE_QUICK;
	state->pendingticks = 0;
	accessibilityLogEvent("weapon_change", "pending",
			"player=%d source=quick_change weapon=%d",
			playernum, selectedweaponnum);
}

void accessibilityWeaponFunctionObserve(s32 playernum, s32 stagenum,
		s32 weaponnum, s32 secondary, s32 dual,
		const char *visiblefunctionname)
{
	struct accessibilityweaponfunctionstate *state;
	s32 weaponchangeannounced = false;

	if (playernum < 0 || playernum >= MAX_PLAYERS) {
		return;
	}

	state = &g_AccessibilityWeaponFunctionStates[playernum];
	secondary = secondary != 0;
	dual = dual != 0;

	if (!accessibilityIsWeaponFunctionCuesEnabled()
			&& !accessibilityIsWeaponChangeAnnouncementsEnabled()
			&& !accessibilityIsHudMessagesEnabled()) {
		memset(state, 0, sizeof(*state));
		return;
	}

	if (!state->initialized || state->stagenum != stagenum) {
		s32 activemenuopen = state->activemenuopen;
		s32 activemenuselection = state->activemenuselection;

		memset(state, 0, sizeof(*state));
		state->initialized = true;
		state->stagenum = stagenum;
		state->weaponnum = weaponnum;
		state->secondary = secondary;
		state->dual = dual;
		state->activemenuopen = activemenuopen;
		state->activemenuselection = activemenuselection;
		return;
	}

	if (state->pendingsource != ACCESSIBILITY_WEAPON_CHANGE_NONE) {
		if (state->pendingweaponnum == weaponnum) {
			accessibilityWeaponAnnounceChange(playernum, weaponnum, secondary,
					dual, state->pendingsource);
			weaponchangeannounced = true;
			state->pendingsource = ACCESSIBILITY_WEAPON_CHANGE_NONE;
			state->pendingticks = 0;
		} else if (++state->pendingticks > ACCESSIBILITY_WEAPON_PENDING_TICKS) {
			accessibilityLogEvent("weapon_change", "pending_expired",
					"player=%d stage=%d source=%s requested_weapon=%d current_weapon=%d ticks=%d",
					playernum, stagenum,
					accessibilityWeaponChangeSourceName(state->pendingsource),
					state->pendingweaponnum, weaponnum, state->pendingticks);
			state->pendingsource = ACCESSIBILITY_WEAPON_CHANGE_NONE;
			state->pendingticks = 0;
		}
	}

	if (state->weaponnum != weaponnum) {
		if (accessibilityIsWeaponChangeAnnouncementsEnabled()
				&& dual != state->dual && !weaponchangeannounced) {
			accessibilityWeaponAnnounceWieldState(
					playernum, weaponnum, dual);
		}

		state->weaponnum = weaponnum;
		state->secondary = secondary;
		state->dual = dual;
		return;
	}

	if (accessibilityIsWeaponChangeAnnouncementsEnabled()
			&& dual != state->dual) {
		accessibilityWeaponAnnounceWieldState(playernum, weaponnum, dual);
	}

	if (accessibilityIsWeaponFunctionCuesEnabled()
			&& state->secondary != secondary) {
		accessibilityTonePlayWeaponFunction(secondary);
		accessibilityLogEvent("weapon_function", "state_change",
				"player=%d stage=%d weapon=%d previous=%s current=%s beep_count=%d frequency_hz=1000 beep_ms=35 gap_ms=30",
				playernum, stagenum, weaponnum,
				state->secondary ? "secondary" : "primary",
				secondary ? "secondary" : "primary",
				secondary ? 2 : 1);
	}

	if (accessibilityIsHudMessagesEnabled()
			&& state->secondary != secondary
			&& visiblefunctionname
			&& visiblefunctionname[0]) {
		char functionname[ACCESSIBILITY_WEAPON_TEXT_MAX];

		accessibilityWeaponCopyNormalized(functionname, sizeof(functionname),
				visiblefunctionname);

		if (functionname[0]) {
			accessibilityAnnouncementWeaponFunction(functionname, playernum);
			accessibilityLogEvent("weapon_function", "announced",
					"player=%d stage=%d weapon=%d function=%s text=%s",
					playernum, stagenum, weaponnum,
					secondary ? "secondary" : "primary", functionname);
		}
	}

	state->secondary = secondary;
	state->dual = dual;
}

void accessibilityWeaponFunctionReset(const char *reason)
{
	memset(g_AccessibilityWeaponFunctionStates, 0,
			sizeof(g_AccessibilityWeaponFunctionStates));
	accessibilityToneStopWeaponFunction();
	accessibilityLogEvent("weapon_function", "reset", "reason=%s",
			reason ? reason : "unspecified");
	accessibilityLogEvent("weapon_change", "reset", "reason=%s",
			reason ? reason : "unspecified");
}
