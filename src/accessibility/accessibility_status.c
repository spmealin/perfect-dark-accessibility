#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/lv.h"
#include "game/player.h"
#include "game/mplayer/mplayer.h"
#include "game/mplayer/setup.h"
#include "lib/vars.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_status.h"
#ifndef PLATFORM_N64
#include "input.h"
#endif

#define ACCESSIBILITY_STATUS_TEXT_MAX 1024

static s32 g_AccessibilityStatusF1WasDown;

static void accessibilityStatusAppend(char *buffer, size_t bufferlen,
		const char *format, ...)
{
	size_t used = strlen(buffer);
	va_list args;

	if (used >= bufferlen - 1) {
		return;
	}

	if (used > 0) {
		snprintf(buffer + used, bufferlen - used, ", ");
		used = strlen(buffer);
	}

	va_start(args, format);
	vsnprintf(buffer + used, bufferlen - used, format, args);
	va_end(args);
}

static s32 accessibilityStatusPercent(f32 fraction)
{
	s32 percent = (s32)(fraction * 100.0f + 0.5f);

	if (percent < 0) {
		return 0;
	}
	if (percent > 100) {
		return 100;
	}
	return percent;
}

static const char *accessibilityStatusScopeReason(void)
{
	if (!accessibilityIsPlayerStatusEnabled()) {
		return "feature_disabled";
	}
	if (PLAYERCOUNT() != 1) {
		return "unsupported_player_count";
	}
	if (!g_Vars.currentplayer || !g_Vars.currentplayer->prop
			|| !g_Vars.currentplayerstats) {
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
	if (g_Vars.currentplayer->isdead) {
		return "player_dead";
	}
	return NULL;
}

static const char *accessibilityStatusCleanName(const char *source,
		char *buffer, size_t bufferlen)
{
	size_t i = 0;

	if (!source || !source[0]) {
		return "unknown";
	}

	while (*source && *source != '\n' && i + 1 < bufferlen) {
		buffer[i++] = *source++;
	}
	buffer[i] = '\0';
	return buffer;
}

static const char *accessibilityStatusTeamName(s32 team,
		char *buffer, size_t bufferlen)
{
	if (team < 0 || team >= MAX_TEAMS) {
		return "unknown";
	}
	return accessibilityStatusCleanName(g_BossFile.teamnames[team],
			buffer, bufferlen);
}

static const char *accessibilityStatusPropCarrierName(struct prop *prop,
		char *buffer, size_t bufferlen)
{
	s32 i;

	if (!prop) {
		return NULL;
	}

	for (i = 0; i < MAX_MPCHRS; i++) {
		if ((g_MpSetup.chrslots & (1 << i)) && g_MpAllChrPtrs[i]
				&& g_MpAllChrPtrs[i]->prop == prop) {
			return accessibilityStatusCleanName(MPCHR(i)->name,
					buffer, bufferlen);
		}
	}

	return NULL;
}

static void accessibilityStatusAppendTime(char *buffer, size_t bufferlen)
{
	s32 timelimit60 = lvGetMpTimeLimit60();
	s32 seconds;

	if (!g_Vars.normmplayerisrunning) {
		return;
	}

	if (timelimit60 > 0) {
		s32 remaining60 = TICKS(timelimit60) - lvGetStageTime60();

		if (remaining60 < 0) {
			remaining60 = 0;
		}
		seconds = remaining60 / TICKS(60);
		accessibilityStatusAppend(buffer, bufferlen, "%d:%02d left",
				seconds / 60, seconds % 60);
	} else {
		seconds = lvGetStageTime60() / TICKS(60);
		accessibilityStatusAppend(buffer, bufferlen, "%d:%02d elapsed",
				seconds / 60, seconds % 60);
	}
}

static void accessibilityStatusFindPlayerRanking(
		struct mpchrconfig *current, s32 chrnum, s32 *score, s32 *rank,
		s32 *count)
{
	u32 currentrankable;
	s32 currentdeaths;
	s32 i;

	scenarioCalculatePlayerScore(current, chrnum, score, &currentdeaths);
	currentrankable = ((u32)(*score + 0x8000) << 16)
			| (u32)(0xffff - currentdeaths);
	*rank = 1;
	*count = 0;

	for (i = 0; i < MAX_MPCHRS; i++) {
		s32 otherscore;
		s32 otherdeaths;
		u32 otherrankable;

		if (!(g_MpSetup.chrslots & (1 << i))) {
			continue;
		}

		(*count)++;
		if (i == chrnum) {
			continue;
		}

		scenarioCalculatePlayerScore(MPCHR(i), i, &otherscore,
				&otherdeaths);
		otherrankable = ((u32)(otherscore + 0x8000) << 16)
				| (u32)(0xffff - otherdeaths);
		if (otherrankable > currentrankable
				|| (otherrankable == currentrankable && i < chrnum)) {
			(*rank)++;
		}
	}
}

static s32 accessibilityStatusFindTeamRanking(s32 team, s32 *score,
		s32 *rank, s32 *count, struct ranking *rankings)
{
	s32 i;

	*rank = 0;
	*score = 0;
	*count = mpGetTeamRankings(rankings);

	for (i = 0; i < *count; i++) {
		if ((s32)rankings[i].teamnum == team) {
			*score = rankings[i].score;
			*rank = i + 1;
			return true;
		}
	}
	return false;
}

static void accessibilityStatusAppendPlayerScenario(char *buffer,
		size_t bufferlen, struct mpchrconfig *current, s32 chrnum)
{
	s32 seconds;

	switch (g_MpSetup.scenario) {
	case MPSCENARIO_HOLDTHEBRIEFCASE:
		if (g_ScenarioData.htb.token == g_Vars.currentplayer->prop) {
			s32 remaining240 = TICKS(7200)
					- g_Vars.currentplayerstats->tokenheldtime;

			if (remaining240 < 0) {
				remaining240 = 0;
			}
			seconds = (remaining240 + TICKS(240) - 1) / TICKS(240);
			accessibilityStatusAppend(buffer, bufferlen,
					"carrying briefcase, point in %d:%02d",
					seconds / 60, seconds % 60);
		}
		break;
	case MPSCENARIO_CAPTURETHECASE:
		{
			s32 team;

			for (team = 0; team < ARRAYCOUNT(g_ScenarioData.ctc.tokens);
					team++) {
				if (team != current->team
						&& g_ScenarioData.ctc.tokens[team]
								== g_Vars.currentplayer->prop) {
					char teamname[16];
					accessibilityStatusAppend(buffer, bufferlen,
							"carrying %s case",
							accessibilityStatusTeamName(team, teamname,
									sizeof(teamname)));
				}
			}
		}
		break;
	case MPSCENARIO_HACKERCENTRAL:
		if (g_ScenarioData.htm.uplink == g_Vars.currentplayer->prop) {
			accessibilityStatusAppend(buffer, bufferlen, "carrying uplink");
		}
		if (g_ScenarioData.htm.dlplayernum == g_Vars.currentplayernum
				&& g_ScenarioData.htm.dlterminalnum >= 0) {
			s32 progress = g_ScenarioData.htm.dltime240[
					g_Vars.currentplayernum] * 100 / TICKS(4800);

			if (progress > 100) {
				progress = 100;
			}
			accessibilityStatusAppend(buffer, bufferlen,
					"download %d%%", progress);
		}
		break;
	case MPSCENARIO_POPACAP:
		if (g_ScenarioData.pac.victimindex >= 0
				&& g_ScenarioData.pac.victims[
						g_ScenarioData.pac.victimindex] == chrnum) {
			s32 remaining240 = TICKS(60 * 240)
					- g_ScenarioData.pac.age240;

			if (remaining240 < 0) {
				remaining240 = 0;
			}
			seconds = (remaining240 + TICKS(240) - 1) / TICKS(240);
			accessibilityStatusAppend(buffer, bufferlen,
					"you are the target, survival point in %d:%02d",
					seconds / 60, seconds % 60);
		}
		break;
	case MPSCENARIO_KINGOFTHEHILL:
		if (!g_ScenarioData.koh.movehill
				&& current->team == g_ScenarioData.koh.occupiedteam) {
			s32 remaining240 = g_Vars.mphilltime * TICKS(240)
					- g_ScenarioData.koh.elapsed240
					+ (VERSION >= VERSION_PAL_BETA
							? TICKS(2640) - 1 : 2400);

			if (remaining240 < 0) {
				remaining240 = 0;
			}
			seconds = (remaining240 + TICKS(240) - 1) / TICKS(240);
			accessibilityStatusAppend(buffer, bufferlen,
					"your team controls hill, point in %d:%02d",
					seconds / 60, seconds % 60);
		}
		break;
	}
}

static void accessibilityStatusAppendTeamScenario(char *buffer,
		size_t bufferlen)
{
	char name[16];
	char teamname[16];
	const char *carrier;

	switch (g_MpSetup.scenario) {
	case MPSCENARIO_HOLDTHEBRIEFCASE:
		carrier = accessibilityStatusPropCarrierName(
				g_ScenarioData.htb.token, name, sizeof(name));
		if (carrier) {
			accessibilityStatusAppend(buffer, bufferlen,
					"briefcase held by %s", carrier);
		} else {
			accessibilityStatusAppend(buffer, bufferlen,
					"briefcase available");
		}
		break;
	case MPSCENARIO_CAPTURETHECASE:
		if (g_MpSetup.options & MPOPTION_CTC_SHOWONRADAR) {
			s32 team;

			for (team = 0; team < ARRAYCOUNT(g_ScenarioData.ctc.tokens);
					team++) {
				carrier = accessibilityStatusPropCarrierName(
						g_ScenarioData.ctc.tokens[team], name,
						sizeof(name));
				if (carrier) {
					accessibilityStatusAppend(buffer, bufferlen,
							"%s case held by %s",
							accessibilityStatusTeamName(team, teamname,
									sizeof(teamname)), carrier);
				}
			}
		}
		break;
	case MPSCENARIO_HACKERCENTRAL:
		carrier = accessibilityStatusPropCarrierName(
				g_ScenarioData.htm.uplink, name, sizeof(name));
		if (carrier) {
			accessibilityStatusAppend(buffer, bufferlen,
					"uplink held by %s", carrier);
		} else {
			accessibilityStatusAppend(buffer, bufferlen,
					"uplink available");
		}
		if (g_ScenarioData.htm.dlterminalnum >= 0) {
			accessibilityStatusAppend(buffer, bufferlen,
					"download active");
		}
		break;
	case MPSCENARIO_POPACAP:
		if (g_ScenarioData.pac.victimindex >= 0) {
			s32 victim = g_ScenarioData.pac.victims[
					g_ScenarioData.pac.victimindex];

			if (victim >= 0 && victim < MAX_MPCHRS) {
				accessibilityStatusAppend(buffer, bufferlen, "target %s",
						accessibilityStatusCleanName(MPCHR(victim)->name,
								name, sizeof(name)));
			}
		}
		break;
	case MPSCENARIO_KINGOFTHEHILL:
		if (g_ScenarioData.koh.movehill) {
			accessibilityStatusAppend(buffer, bufferlen, "hill moving");
		} else if (g_ScenarioData.koh.occupiedteam < 0) {
			accessibilityStatusAppend(buffer, bufferlen,
					"hill uncontrolled");
		} else {
			accessibilityStatusAppend(buffer, bufferlen,
					"hill controlled by %s",
					accessibilityStatusTeamName(
							g_ScenarioData.koh.occupiedteam, teamname,
							sizeof(teamname)));
		}
		break;
	}
}

static void accessibilityStatusSpeakPlayer(void)
{
	char text[ACCESSIBILITY_STATUS_TEXT_MAX] = "";
	char teamname[16];
	struct mpchrconfig *current = NULL;
	s32 chrnum = -1;
	f32 shield = playerGetShieldFrac();

	accessibilityStatusAppend(text, sizeof(text), "health %d%%",
			accessibilityStatusPercent(playerGetHealthFrac()));
	if (shield > 0.0f) {
		accessibilityStatusAppend(text, sizeof(text), "shields %d%%",
				accessibilityStatusPercent(shield));
	}

	if (g_Vars.normmplayerisrunning) {
		s32 score;
		s32 rank;
		s32 count;

		chrnum = g_Vars.currentplayerstats->mpindex;
		if (chrnum >= 0 && chrnum < MAX_MPCHRS) {
			current = MPCHR(chrnum);
			if (g_MpSetup.options & MPOPTION_TEAMSENABLED) {
				accessibilityStatusAppend(text, sizeof(text), "team %s",
						accessibilityStatusTeamName(current->team,
								teamname, sizeof(teamname)));
			}
			accessibilityStatusAppendPlayerScenario(text, sizeof(text),
					current, chrnum);
			accessibilityStatusFindPlayerRanking(current, chrnum, &score,
					&rank, &count);
			accessibilityStatusAppend(text, sizeof(text), "score %d", score);
			if (rank > 0) {
				accessibilityStatusAppend(text, sizeof(text),
						"rank %d of %d", rank, count);
			}
			if (lvGetMpScoreLimit() > 0) {
				accessibilityStatusAppend(text, sizeof(text), "limit %d",
						lvGetMpScoreLimit());
			}
		}
		accessibilityStatusAppendTime(text, sizeof(text));
	}

	accessibilityLogEvent("status", "query",
			"kind=player accepted=1 player=%d stage=%d scenario=%d text=%s",
			g_Vars.currentplayernum, g_Vars.stagenum,
			g_Vars.normmplayerisrunning ? g_MpSetup.scenario : -1, text);
	accessibilityAnnouncementStatus(text, "player_status",
			g_Vars.currentplayernum, true);
}

static void accessibilityStatusSpeakTeam(void)
{
	char text[ACCESSIBILITY_STATUS_TEXT_MAX] = "";
	char teamname[16];
	char leadername[16];
	struct ranking rankings[MAX_MPCHRS];
	struct mpchrconfig *current;
	s32 chrnum = g_Vars.currentplayerstats->mpindex;
	s32 score;
	s32 rank;
	s32 count;
	s32 kills = 0;
	s32 deaths = 0;
	s32 i;
	s32 j;

	current = MPCHR(chrnum);
	accessibilityStatusFindTeamRanking(current->team, &score, &rank,
			&count, rankings);
	accessibilityStatusAppend(text, sizeof(text), "%s",
			accessibilityStatusTeamName(current->team, teamname,
					sizeof(teamname)));
	accessibilityStatusAppend(text, sizeof(text), "score %d", score);
	if (rank > 0) {
		accessibilityStatusAppend(text, sizeof(text), "rank %d of %d",
				rank, count);
	}

	for (i = 0; i < MAX_MPCHRS; i++) {
		struct mpchrconfig *member;

		if (!(g_MpSetup.chrslots & (1 << i))) {
			continue;
		}
		member = MPCHR(i);
		if (member->team != current->team) {
			continue;
		}
		deaths += member->numdeaths;
		for (j = 0; j < MAX_MPCHRS; j++) {
			if ((g_MpSetup.chrslots & (1 << j))
					&& MPCHR(j)->team != current->team) {
				kills += member->killcounts[j];
			}
		}
	}
	accessibilityStatusAppend(text, sizeof(text), "kills %d", kills);
	accessibilityStatusAppend(text, sizeof(text), "deaths %d", deaths);

	if (count > 0 && (s32)rankings[0].teamnum != current->team) {
		accessibilityStatusAppend(text, sizeof(text), "%s leads by %d",
				accessibilityStatusTeamName(rankings[0].teamnum,
						leadername, sizeof(leadername)),
				rankings[0].score - score);
	}

	accessibilityStatusAppendTeamScenario(text, sizeof(text));
	if (lvGetMpTeamScoreLimit() > 0) {
		accessibilityStatusAppend(text, sizeof(text), "limit %d",
				lvGetMpTeamScoreLimit());
	}
	accessibilityStatusAppendTime(text, sizeof(text));

	accessibilityLogEvent("status", "query",
			"kind=team accepted=1 player=%d team=%d stage=%d scenario=%d text=%s",
			g_Vars.currentplayernum, current->team, g_Vars.stagenum,
			g_MpSetup.scenario, text);
	accessibilityAnnouncementStatus(text, "team_status",
			g_Vars.currentplayernum, true);
}

void accessibilityStatusTick(void)
{
#ifndef PLATFORM_N64
	s32 f1down = inputKeyPressed(VK_F1);

	if (f1down && !g_AccessibilityStatusF1WasDown) {
		const char *reason = accessibilityStatusScopeReason();
		u32 modifiers = inputGetKeyModState();
		s32 shift = (modifiers & KM_SHIFT) != 0;

		if (modifiers & (KM_ALT | KM_CTRL)) {
			reason = "unsupported_modifier";
		}

		if (reason) {
			accessibilityLogEvent("status", "query",
					"kind=%s accepted=0 reason=%s player=%d stage=%d",
					shift ? "team" : "player", reason,
					g_Vars.currentplayernum, g_Vars.stagenum);
		} else if (shift && (!g_Vars.normmplayerisrunning
				|| !(g_MpSetup.options & MPOPTION_TEAMSENABLED))) {
			accessibilityLogEvent("status", "query",
					"kind=team accepted=0 reason=teams_not_enabled player=%d stage=%d",
					g_Vars.currentplayernum, g_Vars.stagenum);
		} else if (shift) {
			accessibilityStatusSpeakTeam();
		} else {
			accessibilityStatusSpeakPlayer();
		}
	}

	g_AccessibilityStatusF1WasDown = f1down;
#endif
}

void accessibilityStatusReset(const char *reason)
{
	g_AccessibilityStatusF1WasDown = false;
	accessibilityLogEvent("status", "reset", "reason=%s",
			reason ? reason : "unknown");
}
