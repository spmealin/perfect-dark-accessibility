#include <stdio.h>
#include <ultra64.h>
#include "constants.h"
#include "game/lang.h"
#include "accessibility/accessibility_menu.h"

struct accessibilityheadlabel {
	s32 textid;
	const char *fallback;
};

/* Indexed by the native head ID, not by the carousel's version-dependent
 * order. Generic faces use model aliases until their looks can be reviewed.
 * Non-native labels belong in the future accessibility translation catalog. */
static const struct accessibilityheadlabel g_AccessibilityHeadLabels[] = {
	[HEAD_DARK_COMBAT] = { L_OPTIONS_016, "Joanna Dark, combat" },
	[HEAD_DARK_FROCK] = { L_OPTIONS_018, "Joanna Dark, party frock" },
	[HEAD_DARKAQUA] = { L_OPTIONS_021, "Joanna Dark, wetsuit" },
	[HEAD_DARK_SNOW] = { L_OPTIONS_023, "Joanna Dark, arctic" },
	[HEAD_ELVIS] = { 0, "Elvis" },
	[HEAD_ELVIS_GOGS] = { 0, "Elvis with goggles" },
	[HEAD_CARRINGTON] = { L_OPTIONS_027, "Daniel Carrington" },
	[HEAD_MRBLONDE] = { L_OPTIONS_029, "Mr. Blonde" },
	[HEAD_CASSANDRA] = { L_OPTIONS_030, "Cassandra De Vries" },
	[HEAD_TRENT] = { L_OPTIONS_031, "Trent Easton" },
	[HEAD_JONATHAN] = { L_OPTIONS_032, "Jonathan" },
	[HEAD_VD] = { 0, "V. D." },
	[HEAD_PRESIDENT] = { L_OPTIONS_064, "The President" },
	[HEAD_DDSHOCK] = { L_OPTIONS_036, "dataDyne Shock Trooper" },
	[HEAD_BIOTECH] = { L_OPTIONS_058, "Biotechnician" },
	[HEAD_DDSNIPER] = { L_OPTIONS_045, "dataDyne Sniper" },
	[HEAD_A51FACEPLATE] = { 0, "Area 51 faceplate" },
	[HEAD_SECRETARY] = { L_OPTIONS_041, "Secretary" },
	[HEAD_FEM_GUARD] = { 0, "dataDyne female guard, first model" },
	[HEAD_FEM_GUARD2] = { 0, "dataDyne female guard, second model" },
	[HEAD_MAIAN_S] = { L_OPTIONS_026, "Maian" },
	[HEAD_JON] = { 0, "Jon" },
	[HEAD_BEAU1] = { 0, "Beau" },
	[HEAD_ROSS] = { 0, "Ross" },
	[HEAD_MARK2] = { 0, "Mark" },
	[HEAD_CHRIST] = { 0, "Christ" },
	[HEAD_RUSS] = { 0, "Russ" },
	[HEAD_DARLING] = { 0, "Darling" },
	[HEAD_BRIAN] = { 0, "Brian" },
	[HEAD_JAMIE] = { 0, "Jamie" },
	[HEAD_DUNCAN2] = { 0, "Duncan" },
	[HEAD_KEITH] = { 0, "Keith" },
	[HEAD_STEVEM] = { 0, "Steve M." },
	[HEAD_GRANT] = { 0, "Grant" },
	[HEAD_PENNY] = { 0, "Penny" },
	[HEAD_DAVEC] = { 0, "Dave C." },
	[HEAD_JONES] = { 0, "Jones" },
	[HEAD_GRAHAM] = { 0, "Graham" },
	[HEAD_ROBERT] = { 0, "Robert" },
	[HEAD_NEIL2] = { 0, "Neil" },
	[HEAD_SHAUN] = { 0, "Shaun" },
	[HEAD_ROBIN] = { 0, "Robin" },
	[HEAD_COOK] = { 0, "Cook" },
	[HEAD_PRYCE] = { 0, "Pryce" },
	[HEAD_SILKE] = { 0, "Silke" },
	[HEAD_SMITH] = { 0, "Smith" },
	[HEAD_GARETH] = { 0, "Gareth" },
	[HEAD_MURCHIE] = { 0, "Murchie" },
	[HEAD_WONG] = { 0, "Wong" },
	[HEAD_CARTER] = { 0, "Carter" },
	[HEAD_TINTIN] = { 0, "Tintin" },
	[HEAD_MUNTON] = { 0, "Munton" },
	[HEAD_STAMPER] = { 0, "Stamper" },
	[HEAD_PHELPS] = { 0, "Phelps" },
	[HEAD_ALEX] = { 0, "Alex" },
	[HEAD_JULIANNE] = { 0, "Julianne" },
	[HEAD_LAURA] = { 0, "Laura" },
	[HEAD_EDMCG] = { 0, "Ed McG." },
	[HEAD_ANKA] = { 0, "Anka" },
	[HEAD_LESLIE_S] = { 0, "Leslie S." },
	[HEAD_MATT_C] = { 0, "Matt C." },
	[HEAD_PEER_S] = { 0, "Peer S." },
	[HEAD_EILEEN_T] = { 0, "Eileen T." },
	[HEAD_ANDY_R] = { 0, "Andy R." },
	[HEAD_BEN_R] = { 0, "Ben R." },
	[HEAD_STEVE_K] = { 0, "Steve K." },
	[HEAD_SANCHEZ] = { 0, "Sanchez" },
	[HEAD_TIM] = { 0, "Tim" },
	[HEAD_KEN] = { 0, "Ken" },
	[HEAD_EILEEN_H] = { 0, "Eileen H." },
	[HEAD_SCOTT_H] = { 0, "Scott H." },
	[HEAD_JOEL] = { 0, "Joel" },
	[HEAD_GRIFFEY] = { 0, "Griffey" },
	[HEAD_MOTO] = { 0, "Moto" },
	[HEAD_WINNER] = { 0, "Winner" },
};

s32 accessibilityMenuFormatHeadOption(char *buffer, u32 bufferlen,
		s32 headid, s32 optionindex)
{
	const char *label = NULL;

	if (!buffer || !bufferlen || optionindex < 0) {
		return false;
	}

	if (headid >= 0 && headid < ARRAYCOUNT(g_AccessibilityHeadLabels)) {
		const struct accessibilityheadlabel *entry = &g_AccessibilityHeadLabels[headid];

		if (entry->textid) {
			label = langGet(entry->textid);
		}
		if (!label || !label[0]) {
			label = entry->fallback;
		}
	}

	if (label) {
		snprintf(buffer, bufferlen, "%s", label);
	} else {
		snprintf(buffer, bufferlen, "Head %d", optionindex + 1);
	}
	return buffer[0] != '\0';
}
