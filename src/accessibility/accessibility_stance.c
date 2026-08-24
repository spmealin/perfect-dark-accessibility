#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "game/bondmove.h"
#include "game/lv.h"
#include "lib/vars.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_stance.h"
#include "accessibility/accessibility_tone.h"

static s32 g_AccessibilityStanceInitialized;
static s32 g_AccessibilityStancePlayerNum = -1;
static s32 g_AccessibilityStanceStageNum = -1;
static s32 g_AccessibilityStanceCrouchPos = CROUCHPOS_STAND;

static const char *accessibilityStanceName(s32 crouchpos)
{
	switch (crouchpos) {
	case CROUCHPOS_SQUAT:
		return "double_crouching";
	case CROUCHPOS_DUCK:
		return "crouching";
	default:
		return "standing";
	}
}

static const char *accessibilityStanceScopeReason(void)
{
	if (!accessibilityIsStanceCuesEnabled()) {
		return "feature_disabled";
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
	if (g_Vars.in_cutscene || g_Vars.tickmode != TICKMODE_NORMAL) {
		return "non_gameplay_tickmode";
	}
	if (g_Vars.currentplayer->isdead) {
		return "player_dead";
	}
	if (g_Vars.currentplayer->bondmovemode != MOVEMODE_WALK) {
		return "unsupported_movement_mode";
	}
	return NULL;
}

void accessibilityStanceTick(void)
{
	const char *reason = accessibilityStanceScopeReason();
	s32 crouchpos;
	s32 pulses;

	if (reason) {
		return;
	}

	crouchpos = bmoveGetCrouchPos();

	if (crouchpos < CROUCHPOS_SQUAT || crouchpos > CROUCHPOS_STAND) {
		accessibilityLogEvent("stance", "invalid_state",
				"stage=%d player=%d crouch_pos=%d",
				g_Vars.stagenum, g_Vars.currentplayernum, crouchpos);
		return;
	}

	if (!g_AccessibilityStanceInitialized
			|| g_AccessibilityStancePlayerNum != g_Vars.currentplayernum
			|| g_AccessibilityStanceStageNum != g_Vars.stagenum) {
		g_AccessibilityStanceInitialized = true;
		g_AccessibilityStancePlayerNum = g_Vars.currentplayernum;
		g_AccessibilityStanceStageNum = g_Vars.stagenum;
		g_AccessibilityStanceCrouchPos = crouchpos;
		accessibilityLogEvent("stance", "baseline",
				"stage=%d player=%d state=%s crouch_pos=%d manual_crouch_pos=%d auto_crouch_pos=%d",
				g_Vars.stagenum, g_Vars.currentplayernum,
				accessibilityStanceName(crouchpos), crouchpos,
				g_Vars.currentplayer->crouchpos,
				g_Vars.currentplayer->autocrouchpos);
		return;
	}

	if (g_AccessibilityStanceCrouchPos == crouchpos) {
		return;
	}

	pulses = CROUCHPOS_STAND - crouchpos + 1;
	accessibilityTonePlayStanceConfirmation(crouchpos);
	accessibilityLogEvent("stance", "state_change",
			"stage=%d player=%d previous=%s current=%s previous_crouch_pos=%d crouch_pos=%d manual_crouch_pos=%d auto_crouch_pos=%d pulses=%d base_frequency_hz=880 changed_frequency_hz=1320 beep_ms=35 gap_ms=25 lane=toggle_confirmation",
			g_Vars.stagenum, g_Vars.currentplayernum,
			accessibilityStanceName(g_AccessibilityStanceCrouchPos),
			accessibilityStanceName(crouchpos),
			g_AccessibilityStanceCrouchPos, crouchpos,
			g_Vars.currentplayer->crouchpos,
			g_Vars.currentplayer->autocrouchpos, pulses);
	g_AccessibilityStanceCrouchPos = crouchpos;
}

void accessibilityStanceReset(const char *reason)
{
	g_AccessibilityStanceInitialized = false;
	g_AccessibilityStancePlayerNum = -1;
	g_AccessibilityStanceStageNum = -1;
	g_AccessibilityStanceCrouchPos = CROUCHPOS_STAND;
	accessibilityLogEvent("stance", "reset", "reason=%s",
			reason ? reason : "unspecified");
}
